# RX-TX ESP32/ESP8266 com nRF24L01

Este projeto implementa comunicação entre um transmissor ESP8266 e um receptor ESP32 via nRF24L01. O transmissor envia, a cada 2 segundos, uma linha CSV longa no padrão solicitado, fragmentada em pacotes de 32 bytes (limite do nRF24). Ambos hospedam páginas web em modo Access Point; o ESP8266 exibe status em uma tela OLED SSD1306.

## Visão Geral

- Transmissor (ESP8266): `TX-RX ESP32--ESP8266/Envia-dados-RF24l01-ESP8266.ino`
	- Gera CSV: `AAAA-MM-DD HH:MM:SS,1,<18 valores ou NAN>`
	- Envia em chunks de até 32 bytes, com cabeçalho `P{i}/{N} `
	- OLED mostra IP, total enviados, último pacote e status (`OK`/`Falha`)
	- Página web com contador e log

- Receptor (ESP32): `TX-RX ESP32--ESP8266/Recebe-dados-RF24l01-ESP32.ino`
	- Recebe e exibe últimos dados
	- Página web com contador, log e indicador de potência (RPD)

## Hardware

### ESP8266 (Transmissor)
- nRF24L01 (SPI)
	- CE → D2 (GPIO 4)
	- CSN → D1 (GPIO 5)
	- SCK → D5 (GPIO 14)
	- MOSI → D7 (GPIO 13)
	- MISO → D6 (GPIO 12)
	- VCC (3.3V) + GND — recomendar capacitor 10µF–47µF

- OLED SSD1306 (I2C)
	- SDA → D6 (GPIO 14)
	- SCL → D5 (GPIO 12)
	- Endereço: 0x3C

Observação: D5/D6 são pinos do SPI e podem conflitar; mantidos aqui por compatibilidade com seu hardware atual. Se houver instabilidade, mover OLED para D3/D4 ou D1/D2.

### ESP32 (Receptor)
- nRF24L01 (SPI)
	- CE → GPIO 4
	- CSN → GPIO 5
	- SCK/MOSI/MISO conforme a placa

## Bibliotecas
- RF24, nRF24L01, SPI
- ESP8266WiFi, ESP8266WebServer (TX)
- WiFi, WebServer (RX)
- Wire, Adafruit_GFX, Adafruit_SSD1306

## Configuração do Rádio
- `RF24_250KBPS` (robusto)
- `setChannel(76)`
- `setPALevel(RF24_PA_LOW)`
- `setAutoAck(true)` em ambos
- `setRetries(5, 15)` no TX
- Pipe: TX `openWritingPipe("CANAL")`; RX `openReadingPipe(1, "CANAL")`

## Formato da Mensagem
Exemplo:
```
2025-09-19 16:35:00,1,12.64,29.12,51.99,49.23,43.67,NAN,53.36,NAN,0.674,47.44,41.29,43.16,42.81,NAN,NAN,45.38,43.10
```
Fragmentação: cada pacote inclui cabeçalho `P{i}/{N} ` seguido de até 27 bytes de conteúdo.

## Páginas Web (AP)
- ESP8266 TX: SSID `ESP8266-NRF24-TX`, senha `12345678`
- ESP32 RX: SSID `ESP32-NRF24-RX`, senha `12345678`
Ambas atualizam a página a cada 2 segundos e mostram logs.

## OLED (TX)
`atualizarOLED()` exibe IP, total enviados, último pacote (cortado) e status `OK/Falha`.

## Reconstituir Mensagens no RX (Opcional)
Para recuperar a linha completa, bufferize chunks usando o cabeçalho `P{i}/{N}` e concatene quando `i == N`. Posso implementar se desejar.

## Dicas de Estabilidade
- Capacitor 10µF–47µF no nRF24L01
- Teste a curta distância para validar ACK
- `testRPD()` pode não estar disponível em algumas placas; há fallback visual.

## Estrutura
```
README.md
TX-RX ESP32--ESP8266/
	Envia-dados-RF24l01-ESP8266.ino
	Recebe-dados-RF24l01-ESP32.ino
```

## Mudanças Principais
- Canal e AutoAck alinhados
- CSV longo gerado aleatoriamente com `NAN`
- Envio por chunks com cabeçalho
- OLED atualiza a cada envio
- Páginas web com logs