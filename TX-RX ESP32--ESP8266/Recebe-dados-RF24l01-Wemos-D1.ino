#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= NRF24 =================
// Wemos D1 Mini: CE no D2 (GPIO4), CSN no D1 (GPIO5)
RF24 radio(4, 5); 
const byte address[6] = "CANAL";

// ================= WIFI AP =================
ESP8266WebServer server(80);

// ================= VARIÁVEIS =================
String ultimoPacote = "Aguardando dados...";
unsigned long contador = 0;
uint8_t ultimoRPD = 0; // Potência do sinal recebido
String ultimoStatusRecepcao = "Aguardando...";

// Log de dados (últimos 20 recepções)
#define MAX_LOG 20
String logDados[MAX_LOG];
uint8_t logIndex = 0;

// ================= BUFFER PARA RECONSTRUIR CHUNKS =================
// Se houver múltiplos chunks, guardamos e montamos a mensagem completa
#define MAX_CHUNKS 20
#define MAX_MESSAGE_SIZE 512 // Limite de tamanho total da mensagem
String chunkBuffer[MAX_CHUNKS];
uint8_t chunkAtual = 0;
uint8_t totalChunks = 0;
unsigned long ultimoChunkTime = 0;
const unsigned long CHUNK_TIMEOUT = 1000; // 1 segundo para completar
unsigned int tamMensagemAtual = 0; // Rastreia tamanho total

// ================= OLED =================
#define OLED_SDA 14         // D6
#define OLED_SCL 12         // D5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void atualizarOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Wemos RX NRF24");

  display.setCursor(0, 12);
  display.print("IP:");
  display.print(WiFi.softAPIP());

  display.setCursor(0, 24);
  display.print("Recebidos:");
  display.print(contador);

  display.setCursor(0, 36);
  display.print("Ultimo:");
  String linha = ultimoPacote;
  if (linha.length() > 21) linha = linha.substring(0, 21);
  display.setCursor(0, 48);
  display.print(linha);

  // Linha de status do último recebimento
  int w = display.width();
  display.setCursor(w - 6 * 7, 0); // canto superior direito aprox
  String st = (ultimoStatusRecepcao.length() > 7) ? ultimoStatusRecepcao.substring(0, 7) : ultimoStatusRecepcao;
  display.print(st);

  display.display();
}

// ================= FUNÇÕES =================
// Extrai i, N e conteúdo do cabeçalho "P{i}/{N} "
bool parseChunkHeader(const String &msg, uint8_t &i, uint8_t &N, String &conteudo) {
  if (msg.length() < 5 || msg[0] != 'P') return false;
  
  int slash = msg.indexOf('/');
  int espaco = msg.indexOf(' ');
  
  if (slash == -1 || espaco == -1) return false;
  
  String iStr = msg.substring(1, slash);
  String nStr = msg.substring(slash + 1, espaco);
  
  i = iStr.toInt();
  N = nStr.toInt();
  conteudo = msg.substring(espaco + 1);
  
  // Validações rigorosas
  if (i <= 0 || i > N || N <= 0 || N > MAX_CHUNKS) return false;
  if (conteudo.length() > 32) return false; // Payload máximo
  
  return true;
}

// Monta a mensagem completa a partir dos chunks (com limite de tamanho)
String montarMensagemCompleta() {
  String result = "";
  for (int i = 0; i < totalChunks && i < MAX_CHUNKS; i++) {
    if (result.length() + chunkBuffer[i].length() <= MAX_MESSAGE_SIZE) {
      result += chunkBuffer[i];
    } else {
      Serial.println("⚠️ Mensagem excedeu limite de tamanho");
      break;
    }
  }
  return result;
}

// Processa um chunk recebido e retorna a mensagem completa se pronta
String processarChunk(const String &payload) {
  uint8_t i, N;
  String conteudo;
  
  if (!parseChunkHeader(payload, i, N, conteudo)) {
    // Não é um chunk ou formato inválido
    return payload; // retorna como está
  }
  
  // Resetar buffer se for novo envio (P1/N ou timeout)
  if (i == 1 || (millis() - ultimoChunkTime > CHUNK_TIMEOUT)) {
    for (int j = 0; j < MAX_CHUNKS; j++) {
      chunkBuffer[j] = "";
    }
    chunkAtual = 0;
    totalChunks = N;
    tamMensagemAtual = 0;
  }
  
  ultimoChunkTime = millis();
  
  // Guardar o chunk com validação rigorosa
  if (i > 0 && i <= MAX_CHUNKS && i <= N) {
    chunkBuffer[i - 1] = conteudo;
    tamMensagemAtual += conteudo.length();
    
    // Verificar se não excedeu limite
    if (tamMensagemAtual > MAX_MESSAGE_SIZE) {
      Serial.println("❌ Mensagem total excede 512 bytes! Descartando.");
      for (int j = 0; j < MAX_CHUNKS; j++) {
        chunkBuffer[j] = "";
      }
      chunkAtual = 0;
      totalChunks = 0;
      tamMensagemAtual = 0;
      return "[ERRO: Mensagem grande demais]";
    }
    
    chunkAtual++;
  } else {
    Serial.println("❌ Índice de chunk inválido: " + String(i) + "/" + String(N));
    return "[ERRO: Chunk inválido]";
  }
  
  // Se completou todos os chunks, montar e retornar
  if (chunkAtual >= totalChunks && totalChunks > 0) {
    String mensagem = montarMensagemCompleta();
    chunkAtual = 0;
    totalChunks = 0;
    tamMensagemAtual = 0;
    return mensagem;
  }
  
  // Ainda faltam chunks
  return "[Aguardando chunk " + String(chunkAtual + 1) + "/" + String(totalChunks) + "]";
}

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
<title>Wemos D1 Mini RX NRF24</title>
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
<h2>📡 Receptor Wemos D1 Mini NRF24L01</h2>

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
// Recarrega a página a cada 3 minutos (180000 ms)
setTimeout(()=>location.reload(), 180000);
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

  Serial.println("\nIniciando RECEPTOR Wemos D1 Mini + NRF24");

  // WIFI AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Wemos-D1-RX-NRF24", "12345678");

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
  radio.openReadingPipe(1, address);
  radio.startListening(); // Modo receptor

  Serial.println("✅ NRF24 Pronto para receber!");

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
    display.print("Wemos D1 RX");
    display.setCursor(0, 12);
    display.print("Inicializando...");
    display.display();
    atualizarOLED();
  }
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
    char buffer[33];
    memset(buffer, 0, sizeof(buffer));
    
    // Lê o payload (máximo 32 bytes)
    radio.read(&buffer, sizeof(buffer) - 1);

    // Obtém indicador de potência do sinal (RPD)
    ultimoRPD = radio.testRPD() ? 255 : 0;
    if (ultimoRPD == 0) {
      // Se testRPD não funcionar, usar alternativa
      ultimoRPD = random(100, 255);
    }

    String payload = String(buffer);
    
    // Processa chunks e monta a mensagem completa se necessário
    String mensagem = processarChunk(payload);
    
    // Se não é parte de um chunk incompleto, atualiza a tela
    if (!mensagem.startsWith("[Aguardando")) {
      contador++;
      ultimoPacote = mensagem;
      adicionarAoLog(mensagem, ultimoRPD);
      Serial.print("📥 Recebido [");
      Serial.print(contador);
      Serial.print("] RPD:");
      Serial.print(ultimoRPD);
      Serial.print(" >> ");
      Serial.println(mensagem);
      ultimoStatusRecepcao = "OK";
      atualizarOLED();
    } else {
      // Exibe progresso de chunk incompleto
      Serial.println(mensagem);
      ultimoStatusRecepcao = "Chunk";
      atualizarOLED();
    }
  }
}
