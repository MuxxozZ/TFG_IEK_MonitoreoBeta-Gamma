#include "LoRaWan_APP.h"
#include "Arduino.h"

// --- Configuración LoRa (Emisor y Receptor) ---
#define RF_FREQUENCY                915000000
#define LORA_BANDWIDTH              0
#define LORA_SPREADING_FACTOR       7
#define LORA_CODINGRATE             1
#define LORA_PREAMBLE_LENGTH        8
#define LORA_SYMBOL_TIMEOUT         0
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false

static RadioEvents_t RadioEvents;
bool lora_idle = true;

// ========================
//  ESTRUCTURA BINARIA
//  Packed = 21 bytes
// ========================
typedef struct __attribute__((packed)) {
    uint8_t  node_id;            // 1 B
    float    temperatura;       // 4 B
    float    humedad;           // 4 B
    uint16_t cpm;               // 2 B
    uint32_t cuentas_totales;   // 4 B
    float    radiacion;         // 4 B
    int16_t  contador_paquetes; // 2 B
} DatosLoRa;
DatosLoRa datosRecibidos;

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

    RadioEvents.RxDone = OnRxDone;
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                      LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                      0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
}

void loop() {
    if (lora_idle) {
        lora_idle = false;
        Radio.Rx(0);
    }
    Radio.IrqProcess();
}

// CALLBACK RX -> SALIDA JSON PARA RASPBERRY PI 4
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {

    if (size == sizeof(DatosLoRa)) {
        memcpy(&datosRecibidos, payload, sizeof(DatosLoRa));

        Serial.print("{\"node\":");   Serial.print(datosRecibidos.node_id);
        Serial.print(",\"temp\":");   Serial.print(datosRecibidos.temperatura, 2);
        Serial.print(",\"hum\":");    Serial.print(datosRecibidos.humedad, 2);
        Serial.print(",\"cpm\":");    Serial.print(datosRecibidos.cpm);
        Serial.print(",\"total\":");  Serial.print(datosRecibidos.cuentas_totales);
        Serial.print(",\"rad\":");    Serial.print(datosRecibidos.radiacion, 4);
        Serial.print(",\"pkt\":");    Serial.print(datosRecibidos.contador_paquetes);
        Serial.print(",\"rssi\":");   Serial.print(rssi);
        Serial.print(",\"snr\":");    Serial.print(snr);
        Serial.println("}");
    } else {
        // Si las structs están desincronizadas:
        Serial.print("{\"error\":\"size\",\"recibido\":");
        Serial.print(size);
        Serial.print(",\"esperado\":");
        Serial.print((unsigned)sizeof(DatosLoRa));
        Serial.println("}");
    }

    Radio.Rx(0);
    lora_idle = true;
}
