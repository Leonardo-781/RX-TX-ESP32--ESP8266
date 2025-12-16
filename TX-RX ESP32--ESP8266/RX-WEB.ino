#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

const char* ssid = "ESP-RX";
const char* password = "12345678";

RF24 radio(4, 5);
const byte address[6] = "CANAL";

ESP8266WebServer server(80);

unsigned long contador = 0;
String receivedData = "";
int expectedPacket = 0;
String ultimosDados[10];
int indiceDados = 0;

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>RX Monitor</title>";
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;background:#f5f5f5}";
  html += ".container{max-width:900px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  html += "h1{color:#333;text-align:center}";
  html += ".stats{background:#4CAF50;color:white;padding:20px;border-radius:5px;text-align:center;margin:20px 0}";
  html += ".data{background:#f9f9f9;padding:12px;margin:8px 0;border-left:4px solid #4CAF50;font-family:monospace;font-size:12px;overflow-x:auto}";
  html += ".nan{color:#ff5722;font-weight:bold}";
  html += ".btn{background:#2196F3;color:white;padding:12px 24px;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin:10px 0;display:block;width:100%}";
  html += "</style>";
  html += "<script>setInterval(()=>location.reload(),5000)</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>RX Monitor</h1>";
  html += "<div class='stats'><h2>" + String(contador) + " pacotes recebidos</h2></div>";
  html += "<button class='btn' onclick='location.reload()'>Atualizar</button>";
  html += "<h3>Ultimos dados:</h3>";
  
  for(int i = indiceDados - 1; i >= 0; i--) {
    if (ultimosDados[i].length() > 0) {
      String linha = ultimosDados[i];
      linha.replace("NAN", "<span class='nan'>NAN</span>");
      html += "<div class='data'>" + linha + "</div>";
    }
  }
  
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n=== RX COM WEB SERVER ===");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("Rede WiFi: ");
  Serial.println(ssid);
  Serial.print("Acesse: http://");
  Serial.println(WiFi.softAPIP());
  
  server.on("/", handleRoot);
  server.begin();
  
  if (!radio.begin()) {
    Serial.println("ERRO: NRF24 NAO ENCONTRADO!");
    while(1);
  }
  
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.setPayloadSize(32);
  radio.openReadingPipe(1, address);
  radio.startListening();
  
  Serial.println("RX pronto!");
}

void loop() {
  if (radio.available()) {
    char payload[32] = {0};
    radio.read(&payload, 32);
    
    int packetNum = payload[0];
    
    if (packetNum == 0) {
      receivedData = "";
      expectedPacket = 0;
    }
    
    if (packetNum == expectedPacket) {
      receivedData += String(&payload[1]);
      expectedPacket++;
      
      if (receivedData.indexOf("NAN") >= 0 || receivedData.indexOf(".00") >= 0) {
        int commaCount = 0;
        for (int i = 0; i < receivedData.length(); i++) {
          if (receivedData[i] == ',') commaCount++;
        }
        
        if (commaCount >= 19) {
          contador++;
          Serial.print("RX [");
          Serial.print(contador);
          Serial.print("]: ");
          Serial.println(receivedData);
          
          if (indiceDados < 10) {
            ultimosDados[indiceDados] = receivedData;
            indiceDados++;
          } else {
            for(int i = 0; i < 9; i++) {
              ultimosDados[i] = ultimosDados[i + 1];
            }
            ultimosDados[9] = receivedData;
          }
          
          receivedData = "";
          expectedPacket = 0;
        }
      }
    } else {
      receivedData = "";
      expectedPacket = 0;
    }
  }
  
  server.handleClient();
  delay(10);
