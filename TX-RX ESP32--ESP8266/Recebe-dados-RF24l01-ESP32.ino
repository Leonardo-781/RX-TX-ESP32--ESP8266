#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// ================= NRF24 =================
// Pinos padrão ESP32 (Pode variar conforme sua placa/fiação)
#define CE_PIN  4
#define CSN_PIN 5

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "CANAL";

// ================= WIFI AP =================
WebServer server(80);

// ================= VARIÁVEIS =================
char buffer[33]; // Max 32 bytes + 1 null terminator
String ultimoPacote = "Aguardando dados...";
unsigned long contador = 0;

// ================= WEB =================
void paginaPrincipal() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 NRF24 Receptor</title>
<style>
body { background:#111; color:#0f0; font-family:monospace; padding:20px; text-align:center; }
.box { border:1px solid #0f0; padding:20px; margin:20px auto; max-width: 400px; border-radius: 8px; }
h2 { color: #fff; }
</style>
</head>
<body>
<h2>📡 Receptor NRF24L01</h2>

<div class="box">
<b>Status:</b> Online<br>
<b>Pacotes Recebidos:</b> )rawliteral" + String(contador) + R"rawliteral(
</div>

<div class="box">
<b>Último Dado:</b><br>
<h3 style="color: yellow;">)rawliteral" + ultimoPacote + R"rawliteral(</h3>
</div>

<script>
// Recarrega a página a cada 2 segundos
setTimeout(()=>location.reload(), 2000);
</script>

</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nIniciando RECEPTOR ESP32 + NRF24");

  // WIFI Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-NRF24-RX", "12345678");

  Serial.print("IP local: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", paginaPrincipal);
  server.begin();

  // NRF24 SETUP
  if (!radio.begin()) {
    Serial.println("❌ NRF24 NÃO encontrado! Verifique a fiação.");
    while (1); // Trava se não achar o rádio
  }

  radio.setPALevel(RF24_PA_LOW); // Baixa potência para testes próximos
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(1, address);
  radio.startListening(); // Importante: Modo receptor

  Serial.println("✅ NRF24 Pronto e escutando!");
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  if (radio.available()) {
    // Limpa o buffer antes de ler
    memset(buffer, 0, sizeof(buffer));
    
    // Lê o payload (máximo 32 bytes)
    radio.read(&buffer, sizeof(buffer) - 1);

    // Converte para String e atualiza variáveis
    ultimoPacote = String(buffer);
    contador++;

    Serial.print("📥 Recebido [");
    Serial.print(contador);
    Serial.print("]: ");
    Serial.println(ultimoPacote);
  }
}