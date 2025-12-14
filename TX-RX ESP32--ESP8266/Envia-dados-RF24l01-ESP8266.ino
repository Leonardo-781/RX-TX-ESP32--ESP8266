#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
String ultimoStatusEnvio = "Aguardando...";

// (Removido envio de texto manual)

// Log de dados (últimos 20 envios)
#define MAX_LOG 20
String logDados[MAX_LOG];
uint8_t logIndex = 0;

// ================= OLED =================
#define OLED_SDA 14         // D6 (como você usava antes)
#define OLED_SCL 12         // D5 (como você usava antes)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void atualizarOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("ESP8266 NRF24");

  display.setCursor(0, 12);
  display.print("IP:");
  display.print(WiFi.softAPIP());

  display.setCursor(0, 24);
  display.print("Enviados:");
  display.print(contador);

  display.setCursor(0, 36);
  display.print("Ultimo:");
  String linha = ultimoPacote;
  if (linha.length() > 21) linha = linha.substring(0, 21);
  display.setCursor(0, 48);
  display.print(linha);

  // Linha de status do último envio
  int w = display.width();
  display.setCursor(w - 6 * 7, 0); // canto superior direito aprox
  String st = (ultimoStatusEnvio.length() > 7) ? ultimoStatusEnvio.substring(0,7) : ultimoStatusEnvio;
  display.print(st);

  display.display();
}

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

// ================= PADRÃO LONGO (CSV) =================
// Exemplo:
// 2025-09-19 16:35:00,1,12.64,29.12,51.99,49.23,43.67,NAN,53.36,NAN,0.674,47.44,41.29,43.16,42.81,NAN,NAN,45.38,43.10
// OBS: NRF24 suporta apenas 32 bytes por pacote. Vamos enviar em múltiplos pacotes (chunks) sequenciais.
String gerarLinhaPadraoLonga() {
  int year = 2025;
  int month = random(1, 13);
  int day = random(1, 29);
  int hour = random(0, 24);
  int minute = random(0, 60);
  int second = random(0, 60);

  String ts = String(year) + "-" + (month<10?"0":"") + String(month) + "-" + (day<10?"0":"") + String(day) +
              " " + (hour<10?"0":"") + String(hour) + ":" + (minute<10?"0":"") + String(minute) + ":" + (second<10?"0":"") + String(second);

  auto maybeVal = []() {
    if (random(0, 10) < 2) return String("NAN");
    float v = random(0, 6000) / 100.0;
    char b[16];
    dtostrf(v, 0, 2, b);
    return String(b);
  };

  String s = ts + ",1";
  for (int i = 0; i < 18; i++) s += "," + maybeVal();
  return s;
}

// Envia a string longa em chunks de até 32 bytes com cabeçalho curto "P{i}/{N} "
void enviarEmChunks(const String &msg) {
  int total = (msg.length() + 27) / 28; // 5 bytes cabeçalho + 27 de conteúdo
  for (int i = 0; i < total; i++) {
    String chunk = "P" + String(i+1) + "/" + String(total) + " ";
    int start = i * 27;
    chunk += msg.substring(start, min(start + 27, (int)msg.length()));

    char payload[32];
    memset(payload, 0, sizeof(payload));
    chunk.toCharArray(payload, sizeof(payload));

    bool ok = radio.write(&payload, strlen(payload));
    if (ok) {
      contador++;
      ultimoPacote = chunk;
      adicionarAoLog(chunk);
      Serial.print("📤 Enviado chunk (");
      Serial.print(i+1);
      Serial.print("/");
      Serial.print(total);
      Serial.print("): ");
      Serial.println(chunk);
      ultimoStatusEnvio = "OK";
      atualizarOLED();
    } else {
      Serial.println("❌ Falha no envio chunk");
      ultimoStatusEnvio = "Falha";
      atualizarOLED();
    }
    delay(20);
  }
}

// ================= WEB =================
void paginaPrincipal() {
  String logHtml = "";
  for (int i = 0; i < MAX_LOG; i++) {
    if (logDados[i].length() > 0) {
      logHtml += "<div style='text-align:left; padding:4px; border-bottom:1px solid #0ff;'>";
      logHtml += logDados[i];
      logHtml += "</div>";
    }
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP8266 NRF24 Monitor</title>
<style>
body { background:#222; color:#0ff; font-family:monospace; padding:20px; text-align:center;}
.box { border:1px solid #0ff; padding:20px; margin:20px auto; max-width: 600px; border-radius:8px;}
h2 { color: #fff; }
input, select { padding:6px; margin:6px; width: 90%; max-width: 500px; }
button { padding:8px 14px; margin:6px; }
label { display:block; margin-top:8px; }
.log-box { max-height:300px; overflow-y:auto; text-align:left; background:#111; padding:10px; border-radius:4px; }
</style>
</head>
<body>
<h2>📡 Transmissor NRF24</h2>

<div class="box">
<b>Status Rádio:</b> OK<br>
<b>Total Enviados:</b> )rawliteral" + String(contador) + R"rawliteral(
</div>

<div class="box">
<b>Último Enviado:</b><br>
<h3 style="color: yellow;">)rawliteral" + ultimoPacote + R"rawliteral(</h3>
</div>

<div class="box">
  <h3>📊 Log de Dados Enviados</h3>
  <div class="log-box">
)rawliteral" + logHtml + R"rawliteral(
  </div>
</div>

<script>
setTimeout(()=>location.reload(), 2000);
</script>

</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// (Removidos handlers de envio manual)

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
  radio.setChannel(76);
  radio.setAutoAck(true);
  radio.setRetries(5, 15); // melhora ACK
  radio.openWritingPipe(address);
  radio.stopListening(); // Importante: Modo Transmissor

  Serial.println("✅ NRF24 Pronto para enviar!");

  // OLED SETUP
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED não encontrado (I2C 0x3C)");
  } else {
    delay(50);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("ESP8266 NRF24");
    display.setCursor(0, 12);
    display.print("Inicializando...");
    display.display();
    atualizarOLED();
  }
}

// Função para adicionar ao log
void adicionarAoLog(String dados) {
  logDados[logIndex] = "[" + String(contador) + "] " + dados;
  logIndex = (logIndex + 1) % MAX_LOG;
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  // Envio automático a cada 2 segundos
  if (millis() - ultimoEnvio > 2000) { 
    ultimoEnvio = millis();

    String linha = gerarLinhaPadraoLonga();
    enviarEmChunks(linha);
  }
}