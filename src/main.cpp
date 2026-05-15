#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "ota.h"
#include "serial_menu.h"

// libreria de I2C
#include <Wire.h>

// libreria para usar la memoria y no perder datos al apagar
#include <EEPROM.h>

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
    timerExterno,
    toglePcf;
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

    // ------------------------------------------inicio el LCD--------------
    Wire.begin();
    lcd.begin(20, 4);
    // lcd.setBacklightPin(3, POSITIVE);
    // lcd.setBacklight(HIGH);// enciendo la luz del LCD
    lcd.configureBacklightPin(3);
    lcd.backlight();
    lcd.print(F("Calecita rotador de "));
    lcd.setCursor(0, 1);
    lcd.print(F("cajas Version: "));
    lcd.print(FIRMWARE_VERSION);
    lcd.setCursor(0, 2);
    lcd.print(F(" Alejandro A Balonga"));
    lcd.setCursor(0, 3);
    lcd.print(F("desde el 2017"));
    // delay(5000);
    escribo_LCD = 1;

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

    //------------------------------leo los datos guardados en la memoria y lo cargo en los datos------------------------------
    EEPROM.begin(10);
    equipo = EEPROM.read(0);
    limite_caja = EEPROM.read(1) * 10;
    // cantidad_alarma = EEPROM.read(2) * 10;
    cantidad_alarma = limite_caja - EEPROM.read(1); // ahora la cantidad para la alarma es el limite de la caja menos el 10% de esta
    cantidad_de_cajas = EEPROM.read(3);
    cavidades = EEPROM.read(4);
    Tbloqueo = EEPROM.read(5);
    Tdesbloqueo = EEPROM.read(6);
    Treset = EEPROM.read(7);
    Tcinta = EEPROM.read(8);

    automatico = 1;   // inicio en automatico
    tiempo_giro = 50; // tiempo vace de cambio de caja, despues se toma el tiempo real
}

void loop()
{
    // Parpadeo lento del LED como heartbeat
    /*static unsigned long lastToggle = 0;
    if (millis() - lastToggle > 2000)
    {
        lastToggle = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }//*/

    if (WiFi.status() == WL_CONNECTED)
    {
        if (millis() - lastCheck >= OTA_CHECK_INTERVAL_MS && pausa)
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

    // delay(100);

    //------------------------------aviso si algun modulo de expansion esta desconectado
    if (!pcf.isConnected() && Serial.available() == 0)
    {
        lcd.clear();                                    // limpio todo el LCD
        lcd.print(F("Expansor PCF8574 not connected")); // primer renglon el numero del equipo
        escribo_LCD = 1;
        pausa = 1;
        Serial.println("Expansor PCF8574 not connected");
        if (toglePcf)
        {
            PCF8574 pcf(0x20);
            toglePcf = false;
        }
        else
        {
            PCF8574 pcf(0x38); // para los que terminan en A
            toglePcf = true;
        }
        delay(2000);
        // return;
    }
    else
    {
        //------------------------------------------------------------------------LECTURA DE ENTRADAS DIGITALES----------------------

        tiempo_actual = millis() / 100; // guardo el valor del tiempo para las funciones que usan timer
        // 1 menos la lectura es porque despues de pasar la entradas a HIGH para no necesitar resistencias, estas se leen como 1 cuando deberia ser 0
        unsigned int entradasBufer = pcf.read8();
        // boton_suma = 1 - bitRead(entradasBufer, Esuma);
        // boton_resta = 1 - bitRead(entradasBufer, Eresta);
        //  guradamotor = !bitRead(entradasBufer, E_guradamotor);
        //  muestra = !bitRead(entradasBufer, E_muestra);            //leo la entrada para la extraccion de muestras
        // rotacion_manual = 1 - bitRead(entradasBufer, Erotacion_manual);
        // cambio_caja_OK = 1 - bitRead(entradasBufer, Ecambio_caja_OK);
        // reset = 1 - bitRead(entradasBufer, Ereset);
        inyectora = bitRead(entradasBufer, E_inyectora);       // leo la entrada para la inyectora
        final_carrera = bitRead(entradasBufer, E_fin_carrera); // fin de carrera de la calecita
        if ((tiempo_actual - buttonMillis) >= 20)
        {
            buttonMillis = tiempo_actual;
            guradamotorCinta = !pcf.readButton(S_cinta);
            guradamotorCalecita = !pcf.readButton(S_calecita);
            timerExterno = !pcf.readButton(S_amarillo) && (guradamotorCinta || guradamotorCalecita);
            // pcf.write(E_inyectora, LOW);
            // pcf.write(E_fin_carrera, LOW);
            // pcf.write(S_cinta, 1);
            // pcf.write(S_calecita, 1);
            // pcf.write(S_amarillo, 1);
            /*pcf.write8(0xFF);
            delay(2);
            guradamotorCinta = !pcf.read(S_cinta);
            guradamotorCalecita = !pcf.read(S_calecita);
            timerExterno = !pcf.read(S_amarillo) && (guradamotorCinta || guradamotorCalecita);
            pcf.write(S_cinta, bitRead(entradasBufer, S_cinta));
            pcf.write(S_calecita, bitRead(entradasBufer, S_calecita));
            pcf.write(S_amarillo, bitRead(entradasBufer, S_amarillo));
            pcf.write8(entradasBufer);//*/
        }
        // boton_verde = 1 - bitRead(entradasBufer, B_verde);
        boton_verde = !digitalRead(B_verde);
        // boton_rojo = 1 - bitRead(entradasBufer, B_rojo);
        boton_rojo = !digitalRead(B_rojo);

        //----------------------------------aviso de sacar el Timer---------------------------------------
        if (timerExterno)
        {
            lcd.clear(); // limpio todo el LCD
            lcd.print(F("RETIRAR EL TIMER"));
            escribo_LCD = 1;
            pausa = 1;
            pcf.write8(0xFF); // apago todas las salidas para que no quede nada activo mientras este el timer
            delay(2000);
            return;
        }

        //------------------------------------------------------------------------------ESCRITURA EN LCD----------------------

        if (escribo_LCD == 1)
        {
            escribo_LCD = 0;
            /* Serial.print(F("automatico="));
            Serial.println(automatico);
            Serial.print(F("menu="));
            Serial.println(menu);
            Serial.print(F("pregunta="));
            Serial.println(pregunta);
            Serial.print(F("boton_verde="));
            Serial.println(boton_verde);
            Serial.print(F("boton_rojo="));
            Serial.println(boton_rojo);
            Serial.println();
          */

            lcd.clear(); // limpio todo el LCD

            lcd.print(F("EQ ")); // primer renglon el numero del equipo
            lcd.print(equipo);

            lcd.setCursor(0, 1); // escrivo en en el segundo renglon
            if (menu == 1)
            {
                lcd.print(F(">> CAVIDADES "));
                lcd.print(cavidades);
                lcd.setCursor(0, 2); // escrivo en en el LCD el numero de limite_caja
                lcd.print(F("   CANT x CAJA "));
                lcd.print(limite_caja);
            }
            else if (menu == 2)
            {
                lcd.print(F("   CAVIDADES "));
                lcd.print(cavidades);
                lcd.setCursor(0, 2); // escrivo en en el LCD el numero de limite_caja
                lcd.print(F(">> CANT x CAJA "));
                lcd.print(limite_caja);
            }
            else
            {
                lcd.print(F("CAV "));
                lcd.print(cavidades);

                lcd.setCursor(8, 1); // escrivo en en el LCD el numero de CAJA
                lcd.print(F("CxCAJA "));
                lcd.print(limite_caja);

                lcd.setCursor(0, 2); // escrivo la cantidad en el LCD
                // lcd.print(F("N.iny "));
                // lcd.print(N_inyecciones);
                lcd.print(F("Comp "));
                lcd.print(caja_comp);
                lcd.print(F("/"));
                lcd.print(calculo_comp);
                lcd.setCursor(10, 2); // escrivo la cantidad en el LCD
                lcd.print(F("CANT "));
                lcd.print(cantidad);
            }

            lcd.setCursor(0, 3);
            if (automatico == 1 && menu == 0 && pregunta == 0)
            {
                lcd.print(F("MANUAL"));
                lcd.setCursor(11, 3);
                lcd.print(F("REINICIAR"));
            }
            else if (automatico == 1 && menu == 0 && pregunta == 1)
            {
                lcd.print(F("  NO"));
                lcd.setCursor(16, 3);
                lcd.print(F("SI  "));
            }
            else if (automatico == 0 && menu == 0)
            {
                if (girar_calecita == 0)
                {
                    lcd.print(F("GIRAR"));
                }
                else
                {
                    lcd.print(F("DETENER"));
                }
                lcd.setCursor(10, 3);
                if (guardamotores)
                    lcd.print(F("REINICIAR"));
                else
                    lcd.print(F("AUTOMATICO"));
            }
            else if (menu != 0)
            {
                lcd.print(F("RESTA"));
                lcd.setCursor(16, 3);
                lcd.print(F("SUMA"));
            }
            /*
              lcd.setCursor(0, 3); //ESCRIBO EL ESTADO DE LAS ENTRADAS
              lcd.print(F("E"));
              lcd.print(automatico);
              lcd.print(inyectora);
              lcd.print(muestra);
              lcd.print(boton_suma);
              lcd.print(boton_resta);
              lcd.print(final_carrera);
              lcd.print(cambio_caja_OK);
              lcd.print(rotacion_manual);
              lcd.print(reset);
              lcd.print(guradamotor);

              lcd.print(F(" S"));//escribo el estado de las salidas
              lcd.print(1 - bitRead(entradasBufer, S_verde));
              lcd.print(1 - bitRead(entradasBufer, S_amarillo));
              lcd.print(1 - bitRead(entradasBufer, S_rojo));
              //lcd.print(salida_cinta);
              lcd.print(1 - bitRead(entradasBufer, S_cinta));
              lcd.print(1 - bitRead(entradasBufer, S_calecita));
              lcd.print(1 - bitRead(entradasBufer, S_alarma));
          */

            lcd.setCursor(6, 0);
            if (guradamotorCalecita)
            {
                lcd.print(F("Gu.Mo.CALESITA"));
                tiempo_luz = tiempo_actual; // enciendo el back light del LCD
            }
            else if (guradamotorCinta)
            {
                lcd.print(F("Gu.Mo.CINTA"));
                tiempo_luz = tiempo_actual; // enciendo el back light del LCD
            }
            else if (obstruido == 1)
            {
                lcd.print(F("GIRO OBSTRUIDO"));
            }
            else if (girar_calecita == 1)
            {
                lcd.print(F("GIRANDO"));
                tiempo_luz = tiempo_actual; // enciendo el back light del LCD
            }
            else if (caja_llena == 1 && ultima_caja == 0)
            {
                lcd.print(F("GIRANDO EN"));
                tiempo_luz = tiempo_actual; // enciendo el back light del LCD
            }
            else if (automatico == 1 && caja_llena == 0 && ultima_caja == 0 && pregunta == 0 && pausa == true)
            {
                lcd.print(F("PAUSA"));
            }
            else if (automatico == 1 && caja_llena == 0 && ultima_caja == 0 && pregunta == 0)
            {
                lcd.print(F("AUTOMATICO"));
            }
            else if (automatico == 1 && caja_llena == 0 && ultima_caja == 0 && pregunta == 1)
            {
                lcd.print(F("CAMBIO MANUAL?"));
            }
            else if (automatico == 0)
            {
                lcd.print(F("MANUAL"));
            }
            else if (caja_llena == 1 && ultima_caja == 1)
            {
                lcd.print(F("CALECITA LLENA"));
                tiempo_luz = tiempo_actual; // enciendo el back light del LCD
            }
            else if (ultima_caja == 1 && caja_llena == 0)
            {
                lcd.print(F("ULTIMA CAJA"));
                tiempo_luz = tiempo_actual; // enciendo el back light del LCD
            }
        }

        //------------------------------------------------------------------CONDICION AUTOMATICO O MANUAL---------------------

        if (automatico != automatico_viejo)
        {
            if (automatico == 1)
            { // AUTOMATICO
                // digitalWrite(S_verde, LOW); //PRENDO LA LUZ VERDE
                L_verde = 1;
                // digitalWrite(S_amarillo, HIGH); //APAGO LA LUZ AMARILLO
                L_amarillo = 0;
                // salida_cinta = 1;//enciendo la cinta
                tiempo_pasado_cinta = tiempo_actual; // reinicio el tiempo de espera para detener la cinta
            }
            else
            { // MANUAL
                // digitalWrite(S_amarillo, LOW); //PRENDO LA LUZ AMARILLO
                L_amarillo = 1;
                // digitalWrite(S_verde, HIGH); //APAGO LA LUZ VERDE
                L_verde = 0;
                salida_cinta = 1; // enciendo la cinta
            }
            automatico_viejo = automatico;
            escribo_LCD = 1;
        }
        //----------------------------------------------------------------------------ALARMA DE GUARDAMOTORES-------------------

        if (guradamotorCalecita || guradamotorCinta) // si se habre un guardamotor
        {
            // automatico = 0;  // paso a manual
            guardamotores = true;
            // if (guradamotorCalecita) pcf.write(S_calecita, HIGH);  //apago la calecita
            // if (guradamotorCinta) pcf.write(S_cinta, HIGH);        //apago la cinta
            // if (parpadeo == 0)
            {
                // digitalWrite(S_rojo, LOW);
                L_rojo = 1;
                if (parpadeo)
                    pcf.write(S_alarma, LOW);
                else
                    pcf.write(S_alarma, HIGH);
                tiempo_luz = tiempo_actual; // enciendo el back light del LCD
            }
            /*else
            {
            //digitalWrite(S_rojo, HIGH);
            L_rojo = 0;
            digitalWrite(S_alarma, HIGH);
            }*/
        }
        if (salida_cinta != salida_cinta_viejo && !obstruido)
        {
            salida_cinta_viejo = salida_cinta;
            tiempo_enciendo_cinta = tiempo_actual;
            if (salida_cinta == 1)
            {
                // digitalWrite(S_calecita, HIGH); //apago la calecita
                // delay (500);
                //  digitalWrite(S_cinta, LOW); //enciendo la cinta
            }
            else
            {
                pcf.write(S_cinta, HIGH); // apago la cinta
                Fcontacto = 1;
                // delay (500);
            }
        }
        if (tiempo_actual - tiempo_enciendo_cinta == espera_e_cinta_calesita * 10)
        {
            // tiempo_enciendo_cinta--;     //para que no lo vuelva a hacer
            if (salida_cinta)
                pcf.write(S_cinta, 0); // enciendo la cinta con LOW si no tiene saltado el guardamotor
            if (salida_calecita)
                pcf.write(S_calecita, 0); // enciendo la calesita con LOW si no tiene saltado el guardamotor
        }
        if (salida_calecita != salida_calecita_viejo)
        {
            // salida_calecita_viejo = salida_calecita;
            salida_calecita_viejo = salida_calecita;
            if (salida_calecita == 1)
            {
                // digitalWrite(S_cinta, HIGH); //apago la cinta
                // delay (500);
                Fcontacto = 1;
                if (automatico == 0)
                    pcf.write(S_calecita, guradamotorCalecita); // enciendo la calecita
                else
                    tiempo_enciendo_cinta = tiempo_actual;
            }
            else if (salida_calecita == 0)
            {
                pcf.write(S_calecita, HIGH); // apago la calecita
            }
        }
        if (bitRead(entradasBufer, S_calecita) == 0 && salida_calecita == 0)
            pcf.write(S_calecita, HIGH); // apago la calecita

        if (guardamotores != guradamotor_viejo && Fcontacto == 0)
        {
            guradamotor_viejo = guardamotores;
            escribo_LCD = 1; // con esto se vuelve loco la pantalla
            Fcontacto = 1;
            if (!guardamotores)
            {
                // digitalWrite(S_rojo, HIGH);//apago la luz roja
                L_rojo = 0;
                pcf.write(S_alarma, HIGH); // apago la alarma
                // automatico = 1;
            }
        }

        //-------------------FUNCION DE GIRO OBSTRUIDO-------------

        if (obstruido != obstruido_viejo)
        {
            obstruido_viejo = obstruido;
            if (obstruido == 1)
            {
                automatico = 0;
                girar_calecita = 0; // apago la calecita
                // digitalWrite(S_rojo, LOW);
                L_rojo = 1;
                tiempo_luz = tiempo_actual; // enciendo el back light del LCD
            }
            else
            {
                pcf.write(S_alarma, HIGH);
                // digitalWrite(S_rojo, HIGH);
                L_rojo = 0;
            }
            escribo_LCD = 1;
        }
        if (obstruido == 1)
        {
            if (parpadeo)
                pcf.write(S_alarma, LOW);
            else
                pcf.write(S_alarma, HIGH);
        }

        //---------------------------------------------------------------------FUNCION PARA CAMBIO DE CAJA--------------

        if (girar_calecita != girar_calecita_viejo)
        {
            girar_calecita_viejo = girar_calecita;
            escribo_LCD = 1;
            if (girar_calecita == 1 && tiempo_actual - tiempo_giro_pasado >= 5)
            {
                if (automatico == 0)
                {
                    salida_cinta = 0; // apago la cinta
                }
                salida_calecita = 1; // enciendo la calecita
                // tiempo_giro = (tiempo_actual - tiempo_obstruido);// obtengo el tiempo del giro
                tiempo_obstruido = tiempo_actual;
            }
            else
            {
                salida_calecita = 0; // apago la calecita
                if (obstruido == 1)
                {
                    salida_cinta = 0; // apago la cinta
                }
                else
                {
                    salida_cinta = 1; // enciendo la cinta
                }
            }
        }
        if (salida_calecita == 1 || girar_calecita == 1)
        {
            if (tiempo_actual - final_carrera_antirebote == (tiempo_giro / 3)) //(tiempo_giro/3) era 10 cambio el 19/3/18
            {
                final_carrera_viejo = final_carrera; // solucion a la desigualdad momentanea que puede tener el final de carrera despues de la mod. espera 1/3 de giro de abajo
            }
            if (tiempo_actual - final_carrera_antirebote > (tiempo_giro / 3)) // espero a que este en 1 por mas de 1/3 de giro para evitar una falsa señal al iniciar el giro
            {
                if (final_carrera != final_carrera_viejo) // final de carrera
                {
                    final_carrera_viejo = final_carrera;
                    if (obstruido == 1) // si estaba obstruido apago las alarmas
                    {
                        obstruido = 0;
                    }
                    tiempo_giro = (tiempo_actual - tiempo_obstruido); // obtengo el tiempo del giro
                    escribo_LCD = 1;
                    // if (final_carrera == LOW)
                    //{

                    girar_calecita = 0; // habilito de nuevo la funcion de girar calecita
                    salida_calecita = 0;
                    pcf.write(S_alarma, HIGH);
                    tiempo_giro_pasado = tiempo_actual; // reinicio el tiempo de espera para hacer otro giro
                    if (automatico == 1)
                    {
                        caja_llena = 0;                      // habilito la funcion de cambio de caja
                        tiempo_pasado_cinta = tiempo_actual; // reinicio el tiempo de espera para detener la cinta
                        // numero_caja ++;//paso a la siguiente caja----------------------------------------------------------HABILITAR ESTE RENGLON PARA QUE CUENTE CAJAS
                        caja_comp++; // agrego una caja al contador de compensacion de cajas que se muestra en LCD
                        // digitalWrite(S_verde, LOW);//mantengo la luz verde encendida
                        L_verde = 1;
                        if (numero_caja > cantidad_de_cajas) // esto proboca que vuelva a empezar la posicion de la calecita
                        {
                            numero_caja = 1;
                        }
                        else if (numero_caja == cantidad_de_cajas)
                        {
                            ultima_caja = 1;
                        }
                    }
                    else
                    {
                        numero_caja = 1;  // si esta en manual y cambian de caja esa sera la primera
                        caja_comp = 1;    // pongo en 1 el contador de compensacion de cajas que se muestra en LCD
                        salida_cinta = 1; // enciendo la cinta
                    }
                    //}
                }
            }
            /*if (obstruido == 1)
            {
            if (parpadeo == 1)
            {
              //salida_calecita = 1;//enciendo la calecita
              digitalWrite(S_alarma, LOW);
              //digitalWrite(S_rojo, LOW);
            L_rojo = 1;
            }
            else
            {
              //salida_calecita = 0;//apago la calecita
              digitalWrite(S_alarma, HIGH);
              //digitalWrite(S_rojo, HIGH);
            L_rojo = 0;
            }
            }*/
        }
        else
        {
            final_carrera_antirebote = tiempo_actual; // pongo a 0 la espera
            final_carrera_viejo = final_carrera;
        }

        if (girar_calecita == 1 && obstruido == 0 && (tiempo_actual - tiempo_obstruido >= (tiempo_giro * 3))) // si esta girando y todavia no llego al final de carrera en el tiempo que suele tardar
        {
            obstruido = 1;
        }

        //--------------------------------------------------------------TIEMPO DE ESPERA PARA DETENER LA CINTA y control de inicio del giro------------

        if (automatico == 1 /* && girar_calecita == 0*/)
        {
            if (Tcinta * 10 < 10)
                Tcinta = ciclo / 10; // si Tcinta es mas bajo que 10s lo pongo en el timpo que tarde cada siclo de la inyectora como tiempo minimo
            // if (tiempo_actual - tiempo_pasado_cinta == Tcinta)//original
            if (tiempo_actual - tiempo_pasado_cinta >= Tcinta * 10 && salida_cinta != 0) // cambiado el 8/7/2019
            {
                salida_cinta = 0; // apago la cinta
                aviso_giro = 0;
                if (caja_llena == 1 && ultima_caja == 0)
                {
                    girar_calecita = 1; // cuando termino el tiempo de la cinta y la caja tiene que cambiar enciendo la calecita
                }
            }
            if (tiempo_actual - tiempo_pasado_cinta >= (Tcinta * 10) - T_aviso_giro * 10)
            {
                if (caja_llena == 1 && ultima_caja == 0)
                {
                    aviso_giro = 1; // un tiempo antes de que cambie de caja aviso con sonido y un cartel en la pantalla, de que esta por girar
                    if (parpadeo)
                        pcf.write(S_alarma, LOW);
                    else
                        pcf.write(S_alarma, HIGH);
                }
            }
            else if (T_aviso_giro > Tcinta)
                T_aviso_giro = Tcinta;

            if (caja_llena == 1 && ultima_caja == 0 && girar_calecita == 0)
            {
                girando_en = Tcinta * 10 - (tiempo_actual - tiempo_pasado_cinta);
                lcd.setCursor(17, 0);
                if (girando_en < 100)
                    lcd.print(F(" "));
                lcd.print(girando_en / 10);
            }

            if (tiempo_actual - tiempo_pasado_cinta < Tcinta * 10 && salida_calecita == 0)
            {
                salida_cinta = 1; // enciendo la cinta
            }
        }

        // if (tiempo_actual < tiempo_pasado_cinta)//en el caso de que el contador tiempo_actual llege al maximo y se reinicie
        //{
        //   tiempo_pasado_cinta = tiempo_actual;
        // }

        //-----------------------------------------FUNCION PARA AUMENTAR LA CANTIDAD CON EL VALOR DE CAVIDADES CUANDO LA INYECTORA EXPULSA--------------

        if (inyectora)
        {
            if (tiempo_actual - tiempo_pasado_antirebote_inyectora >= 10 && inyectoraViejo != inyectora) // espero a que este en HIGH por 1 segundo
            {
                // tiempo_pasado_antirebote_inyectora = tiempo_pasado_antirebote_inyectora - 10;  //evito que el IF vuelva a pasar porque el tiempo_actual es = a 10 por varios siclos
                inyectoraViejo = inyectora;
                escribo_LCD = 1;
                ciclo = 0; // reinicio la funcion de suspender
                if (pausa)
                {
                    L_amarillo = 1;
                }
                else
                {
                    cantidad = cantidad + cavidades; // sumo a la cantidad el valor de cavidades
                    N_inyecciones++;
                    antirebote_muestra = 0; // habilito la funcion para sacar muestras de calidad
                    // pcf.write(S_luz_calidad, HIGH);              // apago la luz del boton de calidad
                    if (caja_llena == 0 && girar_calecita == 0) // solo si no lleno la caja
                    {
                        tiempo_pasado_cinta = tiempo_actual; // reinicio el tiempo de espera para detener la cinta
                    }
                }
            }
        }
        else
        {
            inyectoraViejo = inyectora;
            tiempo_pasado_antirebote_inyectora = tiempo_actual; // pongo a 0 la espera
        }

        //--------------------------------------FUNCION PARA DISMINUIR LA CANTIDAD CON EL VALOR DE CAVIDADES CUANDO SACAN UNA MUESTRA DE CALIDAD---------------
        /*
      if (muestra == HIGH && antirebote_muestra == 0) {
        antirebote_muestra = 1;
        cantidad = cantidad - cavidades;  // resto a la cantidad el valor de cavidades
        N_inyecciones--;
        escribo_LCD = 1;
        pcf.write(S_luz_calidad, LOW);  // enciando la luz del boton de calidad
      }//*/

        //------------------------------------------------Funciion de calculo de compensacion por caja-----------------
        // calculo de compensacion que muestra cuantas cajas se nececitan para que el promedio de la cantidad que tiene que tener cada caja
        if (!calculo_comp && cavidades && limite_caja)
        {
            unsigned int agregar = 0;
            unsigned int B = 0;
            // escribo_LCD = 1;
            if (cavidades <= 0)
                cavidades = 1;
            agregar = limite_caja / cavidades; // ej 1700 / 31 = 54,83......
            agregar = agregar * cavidades;     // ej 54 x 31 = 1674
            while (agregar > limite_caja)      // en caso de que el redondeo de un valor mayor a la cant. por caja
            {
                agregar -= cavidades; // de esta forma da el valor a usar para  ej 1674
            }
            do
            {
                if (B < agregar)
                {
                    B += agregar; // ej B + 1674
                }
                if (B < limite_caja)
                {
                    B += cavidades; // ej + 31
                }
                else
                {
                    B -= limite_caja; // ej - 1700
                    calculo_comp++;   // sumo una caja a la cantidad de cajas necesarias para compensar las cantidades
                }
                /*if (calculo_comp == 254) {
                break;
              }//*/
                // lcd.setCursor(0, 2);
                // lcd.print(F("                    "));
                // lcd.setCursor(0, 2);
                // lcd.print(F("Calc> "));
                // lcd.print(B);
                // lcd.print(F(" Com "));
                // lcd.print(calculo_comp);
                // lcd.print(F(" AG"));
                // lcd.print(agregar);
            } while (B != 0);
        }
        if (caja_comp > calculo_comp)
        {
            caja_comp = 1;
        }

        //-----------------------------------------------------------------------------CAMBIO DE CAJA---------------------
        if (cantidad >= limite_caja && automatico == 1) // decido cuando cambiar de caja
        {
            cantidad = cantidad - limite_caja;
            N_inyecciones = 0;
            caja_llena = 1;
        }
        if (caja_llena == 1 && automatico == 1)
        {
            if (parpadeo == 1) // aviso que va a cambiar de caja
            {
                // digitalWrite(S_verde, HIGH);
                L_verde = 0;
            }
            else
            {
                // digitalWrite(S_verde, LOW);
                L_verde = 1;
            }
        }

        //-----------------------------------------------------------------FUNCION DE REINICIO DEL CONTADOR DE CAJAS----------------
        /*
        if (boton_verde != boton_verde_viejo && automatico == 1 && pregunta == 0 && menu == 0)
        {
          boton_verde_viejo = boton_verde;
          if (boton_verde == 0 && Fcontacto == 0)
          {
            Fcontacto = 1;
            escribo_LCD = 1;

            if (caja_llena == 1 && ultima_caja == 1)
            {
              girar_calecita = 1;
              numero_caja = 0;//pongo el contador de cajas en 0 para que cuando termina de girar este en la caja 1
            }
            else if (ultima_caja == 1)
            {
              ultima_caja = 0;
              numero_caja = 1;
            }
            else
            {
              numero_caja = 1;//pongo el contador de cajas en 1 - SI NO SE QUIERE QUE EL CONTADOR DE CAJAS VUELVA A 1 OMITIR ESTA LINEA Y BUSCAR SOLUCION
            }
            digitalWrite(S_alarma, HIGH);
            //digitalWrite(S_amarillo, HIGH);
          L_amarillo = 0;
            //digitalWrite(S_rojo, HIGH);
          L_rojo = 0;
          }
        }
      */
        //----------------------------------------------------------------BOTONES Y CONFIGURACION: CAMBIO DE CANTIDAD DE CAVIDADES--------------------

        // if (boton_verde != boton_verde_viejo)//aumento la cantidad de cavidades si esta dentro del menu
        {
            // boton_verde_viejo = boton_verde;
            if (boton_verde == 1 && Fcontacto == 0 && boton_rojo == 0)
            {
                if (tiempo_actual - tiempo_pasado_boton_verde == 1) // espero que este en 1 por 1 decima de segundo
                {
                    demora = 0;
                    Fcontacto = 1;
                    escribo_LCD = 1;

                    if (menu == 1) // aumento la cantidad de cavidades si esta dentro del menu
                    {
                        cavidades += mas_rapido;
                        mas_rapido++;
                        pregunta = 0;
                    }
                    else if (menu == 2)
                    {
                        limite_caja += mas_rapido * 10;
                        mas_rapido++;
                        pregunta = 0;
                    }
                    else if (guardamotores)
                    {
                        guardamotores = false;
                    }
                    else if (automatico == 0) // si no estaba en automatico lo paso a automatico
                    {
                        automatico = 1;
                    }
                    else if (pregunta == 1) // si esta preguntando para cambiar a manual y elijo que si
                    {
                        automatico = 0;
                        pregunta = 0;
                    }
                    //-----------------------------------------------FUNCION DE REINICIO DEL CONTADOR DE CAJAS
                    else if (caja_llena == 1 && ultima_caja == 1)
                    {
                        girar_calecita = 1;
                        ultima_caja = 0;
                        numero_caja = 0; // pongo el contador de cajas en 0 para que cuando termina de girar este en la caja 1
                    }
                    else if (ultima_caja == 1)
                    {
                        ultima_caja = 0;
                        numero_caja = 1;
                    }
                    else if (pausa)
                    { // si esta pausado lo reinicio
                        pausa = false;
                    }
                    else
                    {
                        numero_caja = 1; // pongo el contador de cajas en 1 - SI NO SE QUIERE QUE EL CONTADOR DE CAJAS VUELVA A 1 OMITIR ESTA LINEA Y BUSCAR SOLUCION
                    }
                    pcf.write(S_alarma, HIGH);
                    // digitalWrite(S_amarillo, HIGH);
                    L_amarillo = 0;
                    // digitalWrite(S_rojo, HIGH);
                    L_rojo = 0;
                }
            }
            else
            {
                tiempo_pasado_boton_verde = tiempo_actual;
            }
        }

        // if (boton_rojo != boton_rojo_viejo)//disminuyo la cantidad de cavidades si esta dentro del menu

        // boton_rojo_viejo = boton_rojo;
        if (boton_rojo == 1 && Fcontacto == 0 && boton_verde == 0)
        {
            if (tiempo_actual - tiempo_pasado_boton_rojo == 1) // espero que este en 1 por 1 decima de segundo
            {
                demora = 0;
                Fcontacto = 1;
                escribo_LCD = 1;
                if (menu == 1)
                {
                    cavidades -= mas_rapido;
                    mas_rapido++;
                    pregunta = 0;
                }
                else if (menu == 2)
                {
                    limite_caja -= mas_rapido * 10;
                    mas_rapido++;
                    pregunta = 0;
                }
                else if (automatico == 1 && pregunta == 0)
                {
                    pregunta = 1;
                }
                else if (pregunta == 1)
                {
                    pregunta = 0;
                }
                else if (automatico == 0)
                {
                    if (girar_calecita == 0)
                    {
                        girar_calecita = 1;
                    }
                    else
                    {
                        girar_calecita = 0;
                    }
                    if (obstruido == 1) // si estaba obstruido apago las alarmas
                    {
                        tiempo_giro = 50; // tiempo vace de cambio de caja, despues se toma el tiempo real
                        obstruido = 0;
                    }
                    cantidad = 0;
                    N_inyecciones = 0;
                }
            }
        }
        else
        {
            tiempo_pasado_boton_rojo = tiempo_actual;
        }

        if (boton_verde == 0 && boton_rojo == 0)
            mas_rapido = 1;

        if (boton_verde == 1 && boton_rojo == 1)
        {
            pregunta = 0;
            if (parpadeo != parpadeo_viejo)
            {
                escribo_LCD = 1;
                parpadeo_viejo = parpadeo;
                if (menu == 0)
                {
                    if (demora == Tdesbloqueo)
                    {
                        menu = 1; // acedo a cambiar la cantidad de cavidades
                        pregunta = 0;
                        demora = 0;
                        lcd.begin(20, 4);
                    }
                    else
                    {
                        demora++;
                    }
                }
                else if (menu == 1)
                {
                    menu = 2; // acedo a cambiar la cantidad que llena una caja
                }
                else if (menu == 2)
                {
                    menu = 1; // regreso a cambiar la cantidad de cavidades
                }
            }
        }

        if (menu != 0) // esperando para cerrar el menu
        {
            if (parpadeo != parpadeo_viejo)
            {
                parpadeo_viejo = parpadeo;
                if (demora == Tbloqueo)
                {
                    menu = 0;
                    demora = 0;
                    EEPROM.write(4, cavidades); // guardo en la memoria la actualizacion al numero de cavidades
                    EEPROM.write(1, limite_caja / 10);
                    EEPROM.commit();
                    calculo_comp = 0; // inicio el calculo de compensacion que muestra cuantas cajas se nececitan para que el promedio de la cantidad que tiene que tener cada caja
                    escribo_LCD = 1;
                    cantidad_alarma = limite_caja - EEPROM.read(1); // ahora la cantidad para la alarma es el limite de la caja menos el 10% de esta
                                                                    // EEPROM.write(2, cantidad_alarma / 10);
                }
                else
                {
                    demora++;
                }
            }
        }

        //--------------------------------------------------------------------------FUNCION PARA EVITAR FALSOS CONTACTOS-------------------------

        if (Fcontacto == 1)
        {
            if (tiempo_actual - tiempo_pasado_Fcontacto >= 5) // espero 5 desimas de segundo
            {
                Fcontacto = 0;
                tiempo_pasado_Fcontacto = tiempo_actual;
            }
            tiempo_luz = tiempo_actual;
        }
        else
        {
            tiempo_pasado_Fcontacto = tiempo_actual;
        }

        //----------------------------------------------------------------------FUNCION PARA EL BACKLIGHT DEL LCD-----------------

        if (tiempo_actual - tiempo_luz == 600 && automatico == 1) // espero 60 segundospara apagar la luz del LCD
        {
            lcd.begin(20, 4);
            // lcd.setBacklight(LOW);// apago la luz del LCD
            pregunta = 0;
            escribo_LCD = 1;
        }
        else if (tiempo_actual == tiempo_luz)
        {
            lcd.setBacklight(HIGH); // enciendo la luz del LCD
        }

        //--------------------------------------------------FUNCION SUSPENDER EQUIPO DESPUES DE NO TENER SEÑAL DE INYECTORA POR UN TIEMPO---------------------------

        if (ciclo == 0)
        {
            ciclo = (tiempo_actual - tiempo_suspender); // obtengo el tiempo del ciclo
            tiempo_suspender = tiempo_actual;
            // digitalWrite(S_verde, LOW); // ENCIENDO LA LUZ
            L_verde = 1;
            lcd.setBacklight(HIGH); // PRENDO la luz del LCD
        }
        else if (tiempo_actual - tiempo_suspender >= (ciclo * 2) && tiempo_actual - tiempo_luz >= 400 && automatico && !guradamotorCalecita && !guradamotorCinta && !pausa) // espero el doble del tiempo de ciclo para susupender
        {
            pausa = true;
            lcd.clear();           // limpio todo el LCD
            lcd.setBacklight(LOW); // apago la luz del LCD
            // digitalWrite(S_verde, HIGH); //APAGO LA LUZ VERDE
            L_verde = 0;
            // pcf.write(S_luz_calidad, HIGH);  // apago la luz del boton de calidad
        }

        //------------------------------------------------------ALARMA EN PAUSA----------------------------------------------------------
        if (pausa && L_amarillo)
        {
            if (parpadeo)
                pcf.write(S_alarma, LOW);
            else
                pcf.write(S_alarma, HIGH);
        }
        //----------------------------------------------------------------FUNCION DE RESET DE CONTADORES DE CAJA Y CANTIDAD-----------------------------------

        /* if (reset == 1)
        {
         if (parpadeo != parpadeo_viejo)
         {
           parpadeo_viejo = parpadeo;
           if (demora >= Treset)
           {
             numero_caja = 1;
             cantidad = 0;
             N_inyecciones = 0;
           }
           else
           {
             demora ++;
           }
         }
        }

        if (reset != reset_viejo)
        {
         reset_viejo = reset;
         escribo_LCD = 1;
         if (reset == 0)
         {
           demora = 0;
         }
        }
      */
        //-----------------------------------------------------------------------------FUNCION PARPADEO-------------

        if (tiempo_actual - tiempo_pasado_parpadeo >= 10) // cambio de estado cada 1 segundos
        {
            tiempo_pasado_parpadeo = tiempo_actual;
            if (parpadeo == 1)
            {
                parpadeo = 0;
            }
            else
            {
                parpadeo = 1;
            }
        }

        //------------------------------------------------------------------------ALARMAS FIN DE CALECITA-----------------------------------------

        if (automatico == 1)
        {
            if (ultima_caja == 1)
            {
                /*if (parpadeo == 1)
                {
                //digitalWrite(S_amarillo, HIGH);
                L_amarillo = 0;
                }
                else*/
                //{
                // digitalWrite(S_amarillo, LOW);
                L_amarillo = 1;
                //}

                if (cantidad >= cantidad_alarma || caja_llena == 1)
                {
                    if (parpadeo)
                        pcf.write(S_alarma, LOW);
                    else
                        pcf.write(S_alarma, HIGH);
                }

                if (caja_llena == 1)
                {
                    // digitalWrite(S_rojo, LOW);
                    L_rojo = 1;
                }
            }
            else
            {
                if (aviso_giro == 0 && !pausa)
                {
                    pcf.write(S_alarma, HIGH);
                }
                // digitalWrite(S_amarillo, HIGH);
                if (!pausa)
                    L_amarillo = 0;
                // digitalWrite(S_rojo, HIGH);
                L_rojo = 0;
            }
        }
        //------------------- prendo el LED en la placa con cada entrada-----------

        if (entradas != parpadeo + inyectora + muestra + final_carrera + boton_verde + boton_rojo)
        {
            entradas = parpadeo + inyectora + muestra + final_carrera + boton_verde + boton_rojo;
            digitalWrite(LED_BUILTIN, entradas % 2);
            // lcd.setCursor(8, 3);
            // if (final_carrera) lcd.print("FC1");
            // else lcd.print("FC0");
        }
        //---------------------------------------------------------------funcion para el nuevo semaforo RGB-----------------------------
        if (parpadeo != parpadeo_viejo2)
        {
            parpadeo_viejo2 = parpadeo;
            /*
          Serial.print(F("L_verde="));
          Serial.print(L_verde);
          Serial.print(F(" S_verde="));
          Serial.print(bitRead(entradasBufer, S_verde));
          Serial.print(F(" L_amarillo="));
          Serial.print(L_amarillo);
          Serial.print(F(" S_amarillo="));
          Serial.print(bitRead(entradasBufer, S_amarillo));
          Serial.print(F(" L_rojo="));
          Serial.print(L_rojo);
          Serial.print(F(" S_rojo="));
          Serial.println(bitRead(entradasBufer, S_rojo));*/

            if (L_verde == 1 && L_amarillo == 1 && L_rojo == 1)
            {
                // si es luz verde, amarillo y rojo
                if (pcf.readButton(S_verde) == LOW)
                {
                    pcf.write(S_verde, HIGH);
                    pcf.write(S_amarillo, LOW);
                    // Serial.println(F(" amarillo "));
                }
                else if (pcf.readButton(S_amarillo) == LOW)
                {
                    pcf.write(S_amarillo, HIGH);
                    pcf.write(S_rojo, LOW);
                    // Serial.println(F(" rojo "));
                }
                else if (pcf.readButton(S_rojo) == LOW)
                {
                    pcf.write(S_verde, LOW);
                    // Serial.println(F(" verde "));
                    pcf.write(S_rojo, HIGH);
                }
            }
            else if (L_verde == 1 && L_amarillo == 1 && L_rojo == 0)
            {
                // si es luz verde y amarillo
                // if (parpadeo != parpadeo_viejo2) {
                // parpadeo_viejo2 = parpadeo;
                if (parpadeo)
                {
                    // if (pcf.readButton(S_verde) == LOW) {
                    pcf.write(S_verde, HIGH);
                    pcf.write(S_amarillo, LOW);
                    // Serial.println(F(" amarillo "));
                }
                else
                { /*if (pcf.readButton(S_amarillo) == LOW)*/
                    pcf.write(S_verde, LOW);
                    Serial.println(F(" verde "));
                    pcf.write(S_amarillo, HIGH);
                }
                //}
                pcf.write(S_rojo, HIGH);
            }
            else if (L_verde == 1 && L_amarillo == 0 && L_rojo == 1)
            {
                // si es luz verde y rojo
                //  if (parpadeo != parpadeo_viejo2) {
                //  parpadeo_viejo2 = parpadeo;
                if (parpadeo)
                {
                    // if (pcf.readButton(S_verde) == LOW) {
                    pcf.write(S_verde, HIGH);
                    pcf.write(S_rojo, LOW);
                    // Serial.println(F(" rojo "));
                }
                else
                { /* if (pcf.readButton(S_rojo) == LOW)*/
                    pcf.write(S_verde, LOW);
                    // Serial.println(F(" verde "));
                    pcf.write(S_rojo, HIGH);
                }
                //}
                pcf.write(S_amarillo, HIGH);
            }
            else if (L_verde == 0 && L_amarillo == 1 && L_rojo == 1)
            {
                // si es luz amarillo y rojo
                //  if (parpadeo != parpadeo_viejo2) {
                //  parpadeo_viejo2 = parpadeo;
                if (parpadeo)
                {
                    // if (pcf.readButton(S_rojo) == LOW) {
                    pcf.write(S_rojo, HIGH);
                    pcf.write(S_amarillo, LOW);
                    // Serial.println(F(" amarillo "));
                }
                else
                { /* if (pcf.readButton(S_amarillo) == LOW) */
                    pcf.write(S_rojo, LOW);
                    // Serial.println(F(" rojo "));
                    pcf.write(S_amarillo, HIGH);
                }
                //}
                pcf.write(S_verde, HIGH);
            }
            else if (L_rojo == 1)
            {
                pcf.write(S_verde, HIGH);
                pcf.write(S_amarillo, HIGH);
                pcf.write(S_rojo, LOW);
                // Serial.print(F(" rojo "));
            }
            else if (L_amarillo == 1)
            {
                pcf.write(S_verde, HIGH);
                pcf.write(S_amarillo, LOW);
                // Serial.println(F(" amarillo "));
                pcf.write(S_rojo, HIGH);
            }
            else if (L_verde == 1)
            {
                pcf.write(S_verde, LOW);
                // Serial.println(F(" verde "));
                pcf.write(S_amarillo, HIGH);
                pcf.write(S_rojo, HIGH);
            }
            else
            {
                pcf.write(S_verde, HIGH);
                pcf.write(S_amarillo, HIGH);
                pcf.write(S_rojo, HIGH);
            }
        }

        //---------------------------------------------------------------------COMUNICACION POR EL PUERTO SERIE-------------
    }
    if (Serial.available() > 0)
    {
        while (Serial.available() > 0)
        {
            int dato;
            dato = Serial.parseInt();
            if (dato == 0)
            {
                Serial.println(F(""));
                Serial.println(F("DATOS CONSTANTES (estos datos se guardan en la memoria)"));
                Serial.print(F("1- EQUIPO N° "));
                Serial.println(EEPROM.read(0));
                Serial.print(F("2- Cantidad por caja = "));
                Serial.println(EEPROM.read(1) * 10);
                Serial.print(F("3- Cantidad para alarma = "));
                // Serial.println(EEPROM.read(2) * 10);
                Serial.println(cantidad_alarma);
                Serial.print(F("4- Cantidad de cajas en calecita = "));
                Serial.println(EEPROM.read(3));
                Serial.print(F("5- Numero de cavidades = "));
                Serial.println(EEPROM.read(4));
                Serial.print(F("6- Tiempo de bloqueo = "));
                Serial.print(EEPROM.read(5));
                Serial.println(F("s"));
                Serial.print(F("7- tiempo de desbloqueo = "));
                Serial.print(EEPROM.read(6));
                Serial.println(F("s"));
                Serial.print(F("8- tiempo de reset de caja y cantidad = "));
                Serial.print(EEPROM.read(7));
                Serial.println(F("s"));

                Serial.print(F("9- tiempo de cinta transportadora = "));
                Serial.print(EEPROM.read(8));
                Serial.println(F("s"));

                Serial.println(F(""));
                Serial.println(F("DATOS VARIABLES (estos datos se pueden cambiar pero no se guardan)"));
                Serial.print(F("10- Caja actual = "));
                Serial.println(numero_caja);
                Serial.print(F("11- Cantidad actual = "));
                Serial.println(cantidad);
                Serial.println(F(""));
                Serial.println(F("Para cambiar estos datos se deve escribir el numero del DATO y ESPACIO y VALOR"));
                Serial.println(F("Tambien puede escribir DATO ESPACIO VALOR ESPACIO DATO y etc para hacer mas de un cambio"));
                Serial.println(F("El tiempo esta en segundo, El valor maximo para los datos 1,4,5,6,7,8,9 es 255"));
                Serial.println(F(""));
            }
            else if (dato == 1)
            {
                equipo = Serial.parseInt();
                Serial.print(F("1- EQUIPO N° "));
                EEPROM.write(0, equipo);
                Serial.println(EEPROM.read(0));
            }
            else if (dato == 2)
            {
                limite_caja = Serial.parseInt();
                Serial.print(F("2- Cantidad por caja = "));
                EEPROM.write(1, limite_caja / 10);
                Serial.println(EEPROM.read(1) * 10);
            }
            else if (dato == 3)
            {
                cantidad_alarma = Serial.parseInt();
                Serial.print(F("3- Cantidad para alarma = "));
                // EEPROM.write(2, cantidad_alarma / 10);
                // Serial.println(EEPROM.read(2) * 10);
                Serial.println(cantidad_alarma);
            }
            else if (dato == 4)
            {
                cantidad_de_cajas = Serial.parseInt();
                Serial.print(F("4- Cantidad de cajas en calecita = "));
                EEPROM.write(3, cantidad_de_cajas);
                Serial.println(EEPROM.read(3));
            }
            else if (dato == 5)
            {
                cavidades = Serial.parseInt();
                Serial.print(F("5- Numero de cavidades = "));
                EEPROM.write(4, cavidades);
                Serial.println(EEPROM.read(4));
            }
            else if (dato == 6)
            {
                Tbloqueo = Serial.parseInt();
                Serial.print(F("6- Tiempo de bloqueo = "));
                EEPROM.write(5, Tbloqueo);
                Serial.print(EEPROM.read(5));
                Serial.println(F("s"));
            }
            else if (dato == 7)
            {
                Tdesbloqueo = Serial.parseInt();
                Serial.print(F("7- Tiempo de desbloqueo = "));
                EEPROM.write(6, Tdesbloqueo);
                Serial.print(EEPROM.read(6));
                Serial.println(F("s"));
            }
            else if (dato == 8)
            {
                Treset = Serial.parseInt();
                Serial.print(F("8- tiempo de reset de caja y cantidad = "));
                EEPROM.write(7, Treset);
                Serial.print(EEPROM.read(7));
                Serial.println(F("s"));
            }
            else if (dato == 9)
            {
                Tcinta = Serial.parseInt();
                Serial.print(F("9- tiempo de cinta transportadora = "));
                EEPROM.write(8, Tcinta);
                Serial.print(EEPROM.read(8));
                Serial.println(F("s"));
            }
            else if (dato == 10)
            {
                numero_caja = Serial.parseInt();
                Serial.print(F("10- caja actual = "));
                Serial.println(numero_caja);
            }
            else if (dato == 11)
            {
                cantidad = Serial.parseInt();
                Serial.print(F("11- cantidad actual = "));
                Serial.println(cantidad);
            }
            else
            {
                Serial.print(dato);
                Serial.println(F(" ? DATO NO VALIDO, INGRECE 0 PARA VER LA GUIA"));
            }
        }
        // if (dato) {
        EEPROM.commit();
        Serial.println(F(" EEPROM <- GUARDADO"));
        // }
    }

    //---------------------------------------------------------------------COMUNICACION POR EL PUERTO SERIE 3 PARA ESP8266 -------------

    /*if (Serial3.available() > 0) {
      int dato;
      dato = Serial3.parseInt();
      if (dato == 123)  //si el mensaje no empieza con este numero se lo debe ignorar
      {
        dato = Serial3.parseInt();
        if (dato == 0) {
          // Serial3.println(F(""));
          //Serial3.println(F("DATOS CONSTANTES (estos datos se guardan en la memoria)"));
          //Serial3.print(F(" "));//- EQUIPO N°
          Serial3.print(EEPROM.read(0));
          Serial3.print(F(" "));  //- Cantidad por caja =
          Serial3.print(EEPROM.read(1) * 10);
          Serial3.print(F(" "));  //- Cantidad para alarma =
          //Serial3.print(EEPROM.read(2) * 10);
          Serial3.println(cantidad_alarma);
          Serial3.print(F(" "));  //- Cantidad de cajas en calecita =
          Serial3.print(EEPROM.read(3));
          Serial3.print(F(" "));  //- Numero de cavidades =
          Serial3.print(EEPROM.read(4));
          Serial3.print(F(" "));  //- Tiempo de bloqueo =
          Serial3.print(EEPROM.read(5));
          //Serial3.println(F("s"));
          Serial3.print(F(" "));  //- tiempo de desbloqueo =
          Serial3.print(EEPROM.read(6));

          Serial3.print(F(" "));  //- tiempo de cinta transportadora =
          Serial3.print(EEPROM.read(8));
          //Serial3.println(F("s"));

          //Serial3.println(F(""));
          //Serial3.println(F("DATOS VARIABLES (estos datos se pueden cambiar pero no se guardan)"));
          Serial3.print(F(" "));  //- Caja actual =
          Serial3.print(numero_caja);
          Serial3.print(F(" "));  //- Cantidad actual =
          Serial3.print(cantidad);
        } else if (dato == 1) {
          equipo = Serial3.parseInt();
          EEPROM.write(0, equipo);
          //dato = Serial3.parseInt();
          //}
          //if (dato == 2)
          {
            limite_caja = Serial3.parseInt();
            EEPROM.write(1, limite_caja / 10);
            //dato = Serial3.parseInt();
          }
          //if (dato == 3)
          {
            cantidad_alarma = Serial3.parseInt();
            //EEPROM.write(2, cantidad_alarma / 10);
            //dato = Serial3.parseInt();
          }
          //if (dato == 4)
          {
            cantidad_de_cajas = Serial3.parseInt();
            EEPROM.write(3, cantidad_de_cajas);
            //dato = Serial3.parseInt();
          }
          //if (dato == 5)
          {
            cavidades = Serial3.parseInt();
            EEPROM.write(4, cavidades);
            //dato = Serial3.parseInt();
          }
          //if (dato == 6)
          {
            Tbloqueo = Serial3.parseInt();
            EEPROM.write(5, Tbloqueo);
            //dato = Serial3.parseInt();
          }
          //if (dato == 7)
          {
            Tdesbloqueo = Serial3.parseInt();
            EEPROM.write(6, Tdesbloqueo);
            //dato = Serial3.parseInt();
          }
          //if (dato == 9)
          {
            Tcinta = Serial3.parseInt();
            EEPROM.write(8, Tcinta);
            // dato = Serial3.parseInt();
          }
          //if (dato == 10)
          {
            numero_caja = Serial3.parseInt();
            //dato = Serial3.parseInt();
          }
          //if (dato == 11)
          {
            cantidad = Serial3.parseInt();
          }
        }
      }
    }//*/
}
