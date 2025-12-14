#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// ================= NRF24 =================
// ESP8266: CE no D2 (GPIO4), CSN no D1 (GPIO5)
RF24 radio(4, 5); 
const byte address[6] = "CANAL";

// ================= WIFI AP =================
ESP8266WebServer server(80);

// ================= VARIÁVEIS =================
String ultimoPacote = "Nenhum envio ainda";
unsigned long contador = 0;
unsigned long ultimoEnvio = 0;

// ================= FUNÇÕES =================
String gerarPacoteCurto() {
  // ATENÇÃO: O limite do NRF24 é 32 Bytes por envio!
  // Estamos simulando um pacote curto: Hora + 1 Sensor
  
  static int minuto = 0;
  static int segundo = 0;

  segundo += 2;
  if (segundo >= 60) { segundo = 0; minuto++; }

  float temperatura = random(2000, 3500) / 100.0;

  // Formata: "M:MM:SS T:XX.XX" (aprox 15 caracteres, cabe tranquilo)
  char dados[32];
  sprintf(dados, "T:%02d:%02d Sen:%.2f", minuto, segundo, temperatura);

  return String(dados);
}

// ================= WEB =================
void paginaPrincipal() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP8266 NRF24 Monitor</title>
<style>
body { background:#222; color:#0ff; font-family:monospace; padding:20px; text-align:center;}
.box { border:1px solid #0ff; padding:20px; margin:20px auto; max-width: 400px; border-radius:8px;}
h2 { color: #fff; }
</style>
</head>
<body>
<h2>📡 Transmissor NRF24</h2>

<div class="box">
<b>Status Rádio:</b> OK<br>
<b>Envios Sucesso:</b> )rawliteral" + String(contador) + R"rawliteral(
</div>

<div class="box">
<b>Último Enviado:</b><br>
<h3 style="color: white;">)rawliteral" + ultimoPacote + R"rawliteral(</h3>
</div>

<script>
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

  Serial.println("\nIniciando ESP8266 + NRF24");

  // WIFI AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP8266-NRF24-TX", "12345678");

  Serial.print("IP local: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", paginaPrincipal);
  server.begin();

  // NRF24 SETUP
  if (!radio.begin()) {
    Serial.println("❌ NRF24 NÃO DETECTADO - Verifique D1/D2");
    while (1);
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address);
  radio.stopListening(); // Importante: Modo Transmissor

  Serial.println("✅ NRF24 Pronto para enviar!");
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  // Envia a cada 2 segundos (2000ms)
  if (millis() - ultimoEnvio > 2000) { 
    ultimoEnvio = millis();

    String pacote = gerarPacoteCurto();
    
    // Converte String para char array seguro
    char bufferEnvio[32];
    pacote.toCharArray(bufferEnvio, 32);

    // Envia os dados
    bool ok = radio.write(&bufferEnvio, strlen(bufferEnvio));

    if (ok) {
      contador++;
      ultimoPacote = pacote;
      Serial.print("📤 Enviado: ");
      Serial.println(pacote);
    } else {
      Serial.println("❌ Falha no envio (Sem ACK ou Rádio Ocupado)");
    }
  }
}