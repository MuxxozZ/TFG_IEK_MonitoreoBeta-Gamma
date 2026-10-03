#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "DHT.h"

// --- Configuración LoRa (Emisor y Receptor) ---
#define RF_FREQUENCY                915000000
#define TX_OUTPUT_POWER             5
#define LORA_BANDWIDTH              0
#define LORA_SPREADING_FACTOR       7
#define LORA_CODINGRATE             1
#define LORA_PREAMBLE_LENGTH        8

// ---- Pines ---
#define DHTPIN          7
#define DHTTYPE         DHT22
#define GEIGER_OUT_PIN  6

DHT dht(DHTPIN, DHTTYPE);

// ========================
//  ESTRUCTURA BINARIA
//  Packed = 21 bytes
// ========================
typedef struct __attribute__((packed)) {
    uint8_t  node_id;           // 1 B  identificador del nodo
    float    temperatura;       // 4 B  °C
    float    humedad;           // 4 B  %HR
    uint16_t cpm;               // 2 B  cuentas por minuto
    uint32_t cuentas_totales;   // 4 B  acumulado desde el arranque
    float    radiacion;         // 4 B  uSv/h derivado de cpm
    int16_t  contador_paquetes; // 2 B  nº de paquete (detector de perdidas)
} DatosLoRa;
DatosLoRa datos;

bool lora_idle = true;
static RadioEvents_t RadioEvents;

// ----- Conteo de pulsos por interrupción -----
volatile uint32_t      acumuladorPulsos  = 0;   // cuentas del minuto en curso
volatile unsigned long ultimoPulso_us    = 0;

// Filtro dead-time para evitar doble conteo por rebote en flanco
const unsigned long DEADTIME_US = 5000;   // 5 ms

uint32_t totalGlobal = 0;   // acumulado global

unsigned long tiempoInicioMinuto = 0;
const unsigned long UN_MINUTO = 60000;

void IRAM_ATTR alDetectarPulso() {
    unsigned long ahora = micros();
    if (ahora - ultimoPulso_us > DEADTIME_US) {
        acumuladorPulsos++;
        ultimoPulso_us = ahora;
    }
}

void OnTxDone(void);
void OnTxTimeout(void);

void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    dht.begin();

    pinMode(GEIGER_OUT_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(GEIGER_OUT_PIN), alDetectarPulso, RISING);

    datos.node_id          = 1;
    datos.contador_paquetes = 0;
    datos.cuentas_totales   = 0;

    RadioEvents.TxDone    = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH, false, true, 0, 0, false, 3000);

    tiempoInicioMinuto = millis();
    Serial.printf("--- EMISOR LISTO --- sizeof(DatosLoRa)=%u bytes\n",
                  (unsigned)sizeof(DatosLoRa));
}

void loop() {
    unsigned long tiempoActual = millis();

    if (tiempoActual - tiempoInicioMinuto >= UN_MINUTO) {

        uint16_t cpmMinuto;
        noInterrupts();
        cpmMinuto = (uint16_t)acumuladorPulsos;
        acumuladorPulsos = 0;
        interrupts();

        totalGlobal += cpmMinuto;

        datos.cpm             = cpmMinuto;
        datos.cuentas_totales = totalGlobal;

        // SparkFun usa un LND712, generalmente se utiliza 0.00812 como factor, pero puede calibrarse posteriormente en nodered.
        datos.radiacion       = cpmMinuto * 0.00812f;

        float h = dht.readHumidity();
        float t = dht.readTemperature();
        if (!isnan(h) && !isnan(t)) {
            datos.temperatura = t;
            datos.humedad     = h;
        } else {
            // Aviso de lectura fallida en monitor serial
            Serial.println("[AVISO] Lectura DHT22 fallida, uso ultimo valor valido");
        }

        datos.contador_paquetes++;

        Serial.printf("[MIN] CPM:%u  Total:%lu  Rad:%.4f uSv/h  T:%.1f C  H:%.1f %%  Pkt:%d\n",
                      datos.cpm, (unsigned long)datos.cuentas_totales,
                      datos.radiacion, datos.temperatura, datos.humedad,
                      datos.contador_paquetes);

        if (lora_idle) {
            Radio.Send((uint8_t*)&datos, sizeof(DatosLoRa));
            lora_idle = false;
        }
        tiempoInicioMinuto = tiempoActual;
    }
    Radio.IrqProcess();
}

void OnTxDone(void)    { Serial.println(">>> LoRa OK"); lora_idle = true; }
void OnTxTimeout(void) { Serial.println(">>> LoRa TIMEOUT"); Radio.Sleep(); lora_idle = true; }
