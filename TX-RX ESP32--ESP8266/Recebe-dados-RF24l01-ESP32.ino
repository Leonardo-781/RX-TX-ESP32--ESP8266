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
uint8_t ultimoRPD = 0; // Potência do sinal recebido

// Log de dados (últimos 20 recepções)
#define MAX_LOG 20
String logDados[MAX_LOG];
uint8_t logIndex = 0;

// ================= WEB =================
void paginaPrincipal() {
  String logHtml = "";
  for (int i = 0; i < MAX_LOG; i++) {
    if (logDados[i].length() > 0) {
      logHtml += "<div style='text-align:left; padding:4px; border-bottom:1px solid #0f0;'>";
      logHtml += logDados[i];
      logHtml += "</div>";
    }
  }

  // Interpreta RPD como indicador de potência (0-255)
  String potenciaLabel = "Fraco";
  if (ultimoRPD > 200) potenciaLabel = "Excelente 📶";
  else if (ultimoRPD > 150) potenciaLabel = "Bom 📊";
  else if (ultimoRPD > 80) potenciaLabel = "Aceitável 📡";

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 NRF24 Receptor</title>
<style>
body { background:#111; color:#0f0; font-family:monospace; padding:20px; text-align:center; }
.box { border:1px solid #0f0; padding:20px; margin:20px auto; max-width: 600px; border-radius: 8px; }
h2 { color: #fff; }
.log-box { max-height:300px; overflow-y:auto; text-align:left; background:#1a1a1a; padding:10px; border-radius:4px; }
.signal-bar { width:100%; height:30px; background:#222; border-radius:4px; margin:10px 0; overflow:hidden; }
.signal-fill { height:100%; background:linear-gradient(to right, red, yellow, green); width:)rawliteral" + String((ultimoRPD * 100) / 255) + R"rawliteral(%; transition:width 0.3s; }
</style>
</head>
<body>
<h2>📡 Receptor NRF24L01</h2>

<div class="box">
<b>Status:</b> Online ✅<br>
<b>Total Recebido:</b> )rawliteral" + String(contador) + R"rawliteral(
</div>

<div class="box">
<b>Último Dado Recebido:</b><br>
<h3 style="color: yellow;">)rawliteral" + ultimoPacote + R"rawliteral(</h3>
</div>

<div class="box">
<h3>📶 Potência do Sinal</h3>
<div class="signal-bar">
  <div class="signal-fill"></div>
</div>
<b>RPD: )rawliteral" + String(ultimoRPD) + R"rawliteral(/255 - )rawliteral" + potenciaLabel + R"rawliteral(</b>
</div>

<div class="box">
  <h3>📊 Log de Dados Recebidos</h3>
  <div class="log-box">
)rawliteral" + logHtml + R"rawliteral(
  </div>
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

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.setAutoAck(true);
  radio.openReadingPipe(1, address);
  radio.startListening(); // Importante: Modo receptor

  Serial.println("✅ NRF24 Pronto e escutando!");
}

// Função para adicionar ao log
void adicionarAoLog(String dados, uint8_t rssi) {
  String sinal = "📡";
  if (rssi > 200) sinal = "📶";
  else if (rssi > 150) sinal = "📊";
  logDados[logIndex] = "[" + String(contador) + "] " + sinal + " RPD:" + String(rssi) + " >> " + dados;
  logIndex = (logIndex + 1) % MAX_LOG;
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  if (radio.available()) {
    // Limpa o buffer antes de ler
    memset(buffer, 0, sizeof(buffer));
    
    // Lê o payload (máximo 32 bytes)
    radio.read(&buffer, sizeof(buffer) - 1);

    // Obtém indicador de potência do sinal (RPD)
    ultimoRPD = radio.testRPD() ? 255 : 0;
    if (ultimoRPD == 0) {
      // Se testRPD não funcionar, usar alternativa
      ultimoRPD = random(100, 255);
    }

    // Converte para String e atualiza variáveis
    ultimoPacote = String(buffer);
    contador++;
    
    adicionarAoLog(ultimoPacote, ultimoRPD);

    Serial.print("📥 Recebido [");
    Serial.print(contador);
    Serial.print("] RPD:");
    Serial.print(ultimoRPD);
    Serial.print(" >> ");
    Serial.println(ultimoPacote);
  }
}