#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "ota.h"
#include "serial_menu.h"

// libreria de I2C
#include <Wire.h>

// libreria para controlar el LCD por I2C
#include <LiquidCrystalIO.h>
#include <IoAbstractionWire.h>
// If your backpack is wired RS,RW,EN then use this version
LiquidCrystalI2C_RS_EN(lcd, 0x27, false)

// control de modulos expansores
#include <PCF8574.h>
    // nombre y direccion i2c de el expansor
    // PCF8574 pcf(0x20);
    PCF8574 pcf(0x38); // para los que terminan en A

#define LED_PIN D4
#define B_verde D5
#define B_rojo D6
#define E_inyectora 7
#define E_fin_carrera 6
#define S_verde 0
#define S_amarillo 1
#define S_rojo 2
#define S_alarma 3
#define S_cinta 4
#define S_calecita 5
#define Habilitacion D8

OTAUpdater updater;
AppConfig appCfg;

unsigned long lastCheck = 0;
unsigned long lastReconnectAttempt = 0;
#define espera_e_cinta_calesita 2
byte equipo;                  // el numero del equio
int cantidad;                 // creo la variable cantidad
unsigned int limite_caja;     // cantidad total para cada caja
unsigned int cantidad_alarma; // en la ultima caja suena la alarma cuando pasa de esta cantidad
unsigned long tiempo_actual;
unsigned long tiempo_pasado_cinta;
unsigned long tiempo_pasado_parpadeo;
unsigned long tiempo_pasado_Fcontacto;
unsigned long tiempo_luz;
unsigned long ciclo;
unsigned long tiempo_suspender;
unsigned long tiempo_pasado_antirebote_inyectora;
unsigned long tiempo_pasado_boton_verde;
unsigned long tiempo_pasado_boton_rojo;
unsigned long final_carrera_antirebote;
unsigned int Tcinta;
unsigned int girando_en;
unsigned long tiempo_giro;
unsigned long tiempo_obstruido;
unsigned long tiempo_giro_pasado,
    tiempo_enciendo_cinta;
unsigned int buttonMillis;
byte cantidad_de_cajas;
byte numero_caja = 1;
byte cavidades;
byte Tbloqueo;    // tiempo para bloquear
byte Tdesbloqueo; // tiempo para desbloquear
byte Treset;
byte demora;
byte calculo_comp;
byte caja_comp = 1;
byte T_aviso_giro = 5; // aviso que esta por cambiar de caja 5 segundo antes
byte entradas;
byte mas_rapido;
bool aviso_giro;
bool parpadeo;
bool parpadeo_viejo;
bool parpadeo_viejo2;
bool inyectora;
bool inyectoraViejo;
bool muestra;
bool automatico;
bool automatico_viejo;
// bool boton_suma;
// bool boton_suma_viejo;
// bool boton_resta;
// bool boton_resta_viejo;
byte menu;
bool final_carrera; // fin de carrera de la calecita
bool final_carrera_viejo;
bool girar_calecita;
bool girar_calecita_viejo;
bool rotacion_manual;
bool cambio_caja_OK;
bool cambio_caja_viejo;
bool caja_llena;
bool ultima_caja;
// bool guradamotor;
bool guradamotor_viejo;
// bool reset;
// bool reset_viejo;
bool escribo_LCD; // dato para actualizar los valores del LCD
bool salida_calecita;
bool salida_calecita_viejo;
bool salida_cinta = 1;
bool salida_cinta_viejo;
bool antirebote_muestra = 1; // dato para restar una muestra por cada inyeccion
bool boton_verde;
bool boton_verde_viejo;
bool boton_rojo;
bool boton_rojo_viejo;
bool pregunta;
bool Fcontacto;
bool obstruido;
bool obstruido_viejo;
bool L_verde = 1; // nuevas variables para semaforo RGB
bool L_amarillo = 0;
bool L_rojo = 0;
bool pausa = 0;
bool guradamotorCinta,
    guradamotorCalecita,
    guardamotores,
    timerExterno;
// bool placaDeEntradasVieja;

byte N_inyecciones = 0;

void blinkLed(int times, int onMs, int offMs)
{
    for (int i = 0; i < times; i++)
    {
        digitalWrite(LED_PIN, LOW);
        delay(onMs);
        digitalWrite(LED_PIN, HIGH);
        delay(offMs);
    }
}

static void connectWifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(appCfg.wifiSsid.c_str(), appCfg.wifiPass.c_str());

    Serial.printf("Conectando a WiFi: %s\n", appCfg.wifiSsid.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
    {
        delay(500);
        Serial.print(".");
        digitalWrite(LED_PIN, LOW);
        delay(100);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi conectado.");
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        blinkLed(5, 50, 50);
    }
    else
    {
        Serial.println("No se pudo conectar al WiFi.");
        blinkLed(5, 200, 200);
    }
}

void setup()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    pinMode(Habilitacion, OUTPUT);
    digitalWrite(Habilitacion, true);
    pinMode(B_verde, INPUT_PULLUP);
    pinMode(B_rojo, INPUT_PULLUP);

    Serial.begin(115200);
    delay(200);

    Serial.println();
    Serial.println("=====================================");
    Serial.println("  ESP8266 OTA GitHub Updater");
    Serial.printf("  Version: %s\n", FIRMWARE_VERSION);
    Serial.println("=====================================");

    blinkLed(3, 100, 100);

    // Cargar configuración desde NVS
    configLoad(appCfg);

    // Menú serial — si el usuario cambió el WiFi reconectar con los nuevos datos
    bool wifiChanged = serialMenuRun(appCfg);
    (void)wifiChanged; // siempre reconectamos después del menú

    // Conectar WiFi con la config activa (default o la guardada en NVS)
    connectWifi();

    updater.begin(appCfg);

    // Primer check OTA a los 30s para que el sistema esté estable
    lastCheck = millis() - OTA_CHECK_INTERVAL_MS + 30000UL;
}

void loop()
{
    // Parpadeo lento del LED como heartbeat
    static unsigned long lastToggle = 0;
    if (millis() - lastToggle > 2000)
    {
        lastToggle = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        if (millis() - lastCheck >= OTA_CHECK_INTERVAL_MS)
        {
            lastCheck = millis();
            Serial.println("\nIniciando comprobacion OTA...");
            Serial.printf("Free heap antes de OTA: %u bytes\n", ESP.getFreeHeap());
            updater.checkForUpdate(appCfg);
        }
    }
    else if (millis() - lastReconnectAttempt > 30000)
    {
        lastReconnectAttempt = millis();
        Serial.println("WiFi desconectado, intentando reconectar...");
        WiFi.reconnect();
    }

    delay(100);
}
