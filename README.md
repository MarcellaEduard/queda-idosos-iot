# Sistema IoT de Detecção de Quedas para Idosos (ODS 3)

Protótipo de sistema de Internet das Coisas (IoT) para detecção de quedas em idosos, com sensor MPU6050, atuadores (LED e buzzer) e comunicação via MQTT. Alinhado ao ODS 3 — Saúde e Bem-Estar.

## Estrutura do projeto

```
Maarcella/
├── firmware/
│   └── queda_idosos.ino          # Firmware ESP32 (Arduino)
├── simulacao/
│   └── diagram.json              # Circuito para Wokwi
├── simulacao_web/
│   ├── index.html                # Simulação 3D interativa (MQTT real)
│   └── OldMan.glb                # Modelo 3D do idoso (rigged, Mixamo)
├── README.md
└── .gitignore
```

## Como simular (Wokwi)

1. Acesse https://wokwi.com e crie um novo projeto ESP32.
2. Cole o conteúdo de `firmware/queda_idosos.ino` no editor.
3. Substitua o `diagram.json` do projeto pelo arquivo `simulacao/diagram.json`.
4. Clique em "Start Simulation".
5. Acompanhe o Serial Monitor (leituras de SVM e estado).
6. Para monitorar MQTT, use o cliente web do HiveMQ:
   http://www.hivemq.com/demos/websocket-client/
   - Host: broker.hivemq.com, Port: 8000, Path: /mqtt
   - Subscribe: `ods3/queda/#`
7. Para silenciar o alarme remotamente, publique `"silenciar"` no tópico `ods3/queda/comando`.

## Simulação Web 3D (MQTT real)

Uma simulação visual está disponível em `simulacao_web/index.html`. O idoso é controlado pelas setas do teclado (↑↓←→) ou WASD; ao esbarrar em um obstáculo, ele cai e aciona o sensor, publicando evento MQTT.

Para executar:
1. Sirva a pasta do projeto: `python3 -m http.server 8765`
2. Abra `http://localhost:8765/simulacao_web/index.html`
3. Mova o idoso com as setas; evite os móveis.
4. Em caso de colisão, o personagem cai, LED/buzzer virtuais acendem e o evento é publicado.
5. Use "R" ou o botão "Resetar" para silenciar.

## Componentes principais

- ESP32 DevKit V1 (Wi-Fi + processamento)
- MPU6050 (acelerômetro 3 eixos) — sensor
- LED vermelho + buzzer — atuadores
- Botão — silenciar alarme

## Broker MQTT

- Host: `broker.hivemq.com`
- Porta: `1883` (TCP) / `8884` (WebSocket)
- Tópicos: `ods3/queda/telemetria`, `ods3/queda/evento`, `ods3/queda/status`, `ods3/queda/comando`

## Repositório GitHub

O repositório contém apenas o código e a simulação. Os documentos acadêmicos (A3.docx, A4.docx) e as instruções de entrega não são versionados (ver `.gitignore`).

```bash
git clone https://github.com/SEU_USUARIO/queda-idosos-iot.git
```

## Licença

Uso acadêmico.
