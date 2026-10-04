import json
import time
import serial
import paho.mqtt.client as mqtt

# --- Configuración ---
SERIAL_PORT     = "/dev/esp32_rx_geiger"
BAUD_RATE       = 115200
MQTT_HOST       = "localhost"
MQTT_PORT       = 1883
MQTT_TOPIC      = "tfg/geiger_counter"
MQTT_QOS        = 1
RECONNECT_DELAY = 3


def crear_cliente_mqtt():
    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    except (AttributeError, TypeError):
        client = mqtt.Client()
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()
    return client


def main():
    client = crear_cliente_mqtt()
    print(f"[MQTT] Conectado a {MQTT_HOST}:{MQTT_PORT}, publicando en '{MQTT_TOPIC}'",
          flush=True)

    while True:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
            print(f"[SERIAL] {SERIAL_PORT} abierto a {BAUD_RATE} bps", flush=True)

            while True:
                linea = ser.readline().decode("utf-8", errors="ignore").strip()
                if not linea:
                    continue


                try:
                    datos = json.loads(linea)
                except json.JSONDecodeError:
                    continue

                payload = json.dumps(datos)
                client.publish(MQTT_TOPIC, payload, qos=MQTT_QOS)
                print(f"[PUB] {payload}", flush=True)

        except serial.SerialException as e:
            print(f"[SERIAL] Error: {e}. Reintento en {RECONNECT_DELAY}s...", flush=True)
            time.sleep(RECONNECT_DELAY)
        except Exception as e:
            print(f"[ERROR] {e}. Reintento en {RECONNECT_DELAY}s...", flush=True)
            time.sleep(RECONNECT_DELAY)

if __name__ == "__main__":
    main()
