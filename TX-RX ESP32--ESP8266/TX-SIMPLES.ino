#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// CE=GPIO4 (D2), CSN=GPIO5 (D1)
RF24 radio(4, 5);
const byte address[6] = "CANAL";

unsigned long contador = 0;
unsigned long startTime = 0;

// Função para gerar valor de sensor (20% chance de NAN)
String getSensorValue() {
  if (random(0, 100) < 20) {
    return "NAN";
  }
  return String(random(1000, 6000) / 100.0, 2);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n=== TX COM DADOS DE SENSORES ===");
  
  if (!radio.begin()) {
    Serial.println("ERRO: NRF24 NAO ENCONTRADO!");
    while(1);
  }
  
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.setPayloadSize(32);
  radio.openWritingPipe(address);
  radio.stopListening();
  
  randomSeed(analogRead(0));
  startTime = millis();
  
  Serial.println("TX pronto!");
  Serial.println("Enviando dados de sensores...");
}

void loop() {
  // Calcular tempo decorrido
  unsigned long elapsedSeconds = (millis() - startTime) / 1000;
  int seconds = elapsedSeconds % 60;
  int minutes = (elapsedSeconds / 60) % 60;
  int hours = (elapsedSeconds / 3600) % 24 + 16;
  int day = 19 + (elapsedSeconds / 86400);
  
  // Construir linha completa de dados
  String dataLine = String("2025-09-") + String(day) + " " + 
                    String(hours) + ":" + String(minutes) + ":" + String(seconds) + "," +
                    String(contador) + ",";
  
  // Adicionar 18 valores de sensores
  for (int i = 0; i < 18; i++) {
    dataLine += getSensorValue();
    if (i < 17) dataLine += ",";
  }
  
  // Dividir em pacotes de 32 bytes
  int totalLength = dataLine.length();
  int numPackets = (totalLength / 31) + 1;
  
  Serial.print("TX [");
  Serial.print(contador);
  Serial.print("] Enviando ");
  Serial.print(numPackets);
  Serial.println(" pacotes...");
  
  bool success = true;
  for (int p = 0; p < numPackets; p++) {
    char payload[32];
    memset(payload, 0, 32);
    
    int startPos = p * 31;
    int len = min(31, totalLength - startPos);
    
    // Primeiro byte: número do pacote
    payload[0] = p;
    
    // Copiar dados
    for (int i = 0; i < len; i++) {
      payload[i + 1] = dataLine[startPos + i];
    }
    
    if (!radio.write(payload, 32)) {
      Serial.print("FALHA no pacote ");
      Serial.println(p);
      success = false;
      break;
    }
    delay(10); // Pequeno delay entre pacotes
  }
  
  if (success) {
    Serial.println("OK - Dados completos:");
    Serial.println(dataLine);
    contador++;
  }
  
  delay(3000);
}