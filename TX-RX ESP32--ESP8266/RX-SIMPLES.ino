#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// CE=GPIO4 (D2), CSN=GPIO5 (D1)
RF24 radio(4, 5);
const byte address[6] = "CANAL";

unsigned long contador = 0;
String receivedData = "";
int expectedPacket = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n=== RX SIMPLES ===");
  
  if (!radio.begin()) {
    Serial.println("ERRO: NRF24 NAO ENCONTRADO!");
    while(1);
  }
  
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.setPayloadSize(32);
  radio.openReadingPipe(1, address);
  radio.startListening();
  
  Serial.println("RX pronto!");
  Serial.println("Aguardando dados...");
}

void loop() {
  if (radio.available()) {
    char payload[32] = {0};
    radio.read(&payload, 32);
    
    int packetNum = payload[0];
    
    // Se é o primeiro pacote, limpar dados anteriores
    if (packetNum == 0) {
      receivedData = "";
      expectedPacket = 0;
    }
    
    // Verificar se é o pacote esperado
    if (packetNum == expectedPacket) {
      // Adicionar dados (ignorando o primeiro byte que é o número do pacote)
      receivedData += String(&payload[1]);
      expectedPacket++;
      
      // Verificar se a linha está completa (termina com número após última vírgula)
      if (receivedData.indexOf("NAN") >= 0 || receivedData.indexOf(".00") >= 0) {
        // Contar vírgulas para saber se recebeu tudo (data,id,18valores = 20 vírgulas)
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
          receivedData = "";
          expectedPacket = 0;
        }
      }
    } else {
      Serial.println("Pacote fora de ordem!");
      receivedData = "";
      expectedPacket = 0;
    }
  }
  
  delay(10);
}
