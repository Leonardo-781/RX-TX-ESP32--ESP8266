#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

const char* ssid = "ESP-TX";
const char* password = "12345678";

RF24 radio(4, 5);
const byte address[6] = "CANAL";

ESP8266WebServer server(80);

unsigned long contador = 0;
unsigned long startTime = 0;
String ultimosEnvios[10];
int indiceEnvios = 0;

String getSensorValue() {
  if (random(0, 100) < 20) {
    return "NAN";
  }
  return String(random(1000, 6000) / 100.0, 2);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>TX Monitor</title>";
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;background:#f5f5f5}";
  html += ".container{max-width:900px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  html += "h1{color:#333;text-align:center}";
  html += ".stats{background:#FF9800;color:white;padding:20px;border-radius:5px;text-align:center;margin:20px 0}";
  html += ".data{background:#f9f9f9;padding:12px;margin:8px 0;border-left:4px solid #FF9800;font-family:monospace;font-size:12px;overflow-x:auto}";
  html += ".btn{background:#2196F3;color:white;padding:12px 24px;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin:10px 0;display:block;width:100%}";
  html += "</style>";
  html += "<script>setInterval(()=>location.reload(),5000)</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>TX Monitor</h1>";
  html += "<div class='stats'><h2>" + String(contador) + " pacotes enviados</h2></div>";
  html += "<button class='btn' onclick='location.reload()'>Atualizar</button>";
  html += "<h3>Ultimos envios:</h3>";
  
  for(int i = indiceEnvios - 1; i >= 0; i--) {
    if (ultimosEnvios[i].length() > 0) {
      html += "<div class='data'>" + ultimosEnvios[i] + "</div>";
    }
  }
  
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n=== TX COM WEB SERVER ===");
  
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
  radio.openWritingPipe(address);
  radio.stopListening();
  
  randomSeed(analogRead(0));
  startTime = millis();
  
  Serial.println("TX pronto!");
}

void loop() {
  unsigned long elapsedSeconds = (millis() - startTime) / 1000;
  int seconds = elapsedSeconds % 60;
  int minutes = (elapsedSeconds / 60) % 60;
  int hours = (elapsedSeconds / 3600) % 24 + 16;
  int day = 19 + (elapsedSeconds / 86400);
  
  String dataLine = String("2025-09-") + String(day) + " " + 
                    String(hours) + ":" + String(minutes) + ":" + String(seconds) + "," +
                    String(contador) + ",";
  
  for (int i = 0; i < 18; i++) {
    dataLine += getSensorValue();
    if (i < 17) dataLine += ",";
  }
  
  int totalLength = dataLine.length();
  int numPackets = (totalLength / 31) + 1;
  
  bool success = true;
  for (int p = 0; p < numPackets; p++) {
    char payload[32];
    memset(payload, 0, 32);
    
    int startPos = p * 31;
    int len = min(31, totalLength - startPos);
    
    payload[0] = p;
    
    for (int i = 0; i < len; i++) {
      payload[i + 1] = dataLine[startPos + i];
    }
    
    if (!radio.write(payload, 32)) {
      success = false;
      break;
    }
    delay(10);
  }
  
  if (success) {
    Serial.print("TX [");
    Serial.print(contador);
    Serial.println("]: ");
    Serial.println(dataLine);
    contador++;
    
    if (indiceEnvios < 10) {
      ultimosEnvios[indiceEnvios] = dataLine;
      indiceEnvios++;
    } else {
      for(int i = 0; i < 9; i++) {
        ultimosEnvios[i] = ultimosEnvios[i + 1];
      }
      ultimosEnvios[9] = dataLine;
    }
  }
  
  server.handleClient();
  delay(3000);
}
