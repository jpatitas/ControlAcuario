#include "constantes.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ThingerESP8266.h>

// Conexión del sensor de temperatura DS18B20
#define ONE_WIRE_BUS D1

// Constantes para histéresis
#define MAX_TEMP 27.5
#define MARGEN_TEMP 0.5

ThingerESP8266 thing(usuario, device_Id, device_credentials);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

char temperatureString[6];

int relayInput = D0; // Relé ventiladores

// Variables para control de ventiladores
bool ventiladoresOn = false;
bool forzarVentiladores = false;
unsigned long tiempoVentiladoresForzados = 0;
int maxTimeVentiladoresForzados = 30*60000; // A la media hora de que se hayan forzado los ventiladores, estos se apagan

void setup() {
  // Iniciar la conexión serie
  Serial.begin(115200);
  while(! Serial);

  Serial.print("Conectando a Thinger.io");

  // Inicialización de la WiFi para comunicarse con la API de Thinger.io
  thing.add_wifi(WIFI_SSID, WIFI_PASS);  

  // Inicializar Arduino OTA
  ArduinoOTA.setPassword((const char *)CLAVE_OTA);//Clave para acceso reprogramación  
  ArduinoOTA.begin();
  
  Serial.println("Listo");
  Serial.print("Direccion IP: ");
  Serial.println(WiFi.localIP());

  // Conectar sensor de temperatura
  DS18B20.begin();

  // Inicializar pin del relé como salida
  pinMode(relayInput, OUTPUT);

  thing["ventilador"] << [](pson& in) {
    if (in.is_empty()) {
      in = (bool) forzarVentiladores;    
    }
    else {
      forzarVentiladores = in;
    }
  };
  
  thing["NodeMCU"] >> [](pson& out){
    out["TemperaturaAgua"] = getTemperature();
    //String(temperatureString);
    out["Ventiladores"] = ventiladoresOn || forzarVentiladores;
  };
}

void loop() {
  // Lo primero mirar si hay una actualización vía OTA
  ArduinoOTA.handle();

  // Leer temperatura del sensor
  float temperature = getTemperature();

  dtostrf(temperature, 2, 2, temperatureString);
  
  Serial.println(temperatureString);

  if (forzarVentiladores) {
    // Forzado de ventiladores - arrancar
    if  (not ventiladoresOn) {
      digitalWrite(relayInput, HIGH);
      ventiladoresOn = true;
      tiempoVentiladoresForzados = millis();
      Serial.println("Forzando ventiladores");
    }
    // Fin del tiempo máximo de forzado de ventiladores
    else if (millis() - tiempoVentiladoresForzados >= maxTimeVentiladoresForzados) {
      digitalWrite(relayInput, LOW);
      ventiladoresOn = false;
      forzarVentiladores = false;
      Serial.println("Terminando forzado de ventiladores");      
    }
  }
  // Si se desactiva el forzado de ventiladores externamente
  else if ((not forzarVentiladores) and (tiempoVentiladoresForzados > 0)) {
    digitalWrite(relayInput, LOW);
    tiempoVentiladoresForzados = 0;
    ventiladoresOn = false;
    Serial.println("Terminando forzado de ventiladores");      
  }
  else {
    // Si se ha superado la temperatura máxima y no están encendidos los ventiladores, se encienden
    if ((temperature >= (MAX_TEMP + MARGEN_TEMP)) and (not ventiladoresOn))
    {
      digitalWrite(relayInput, HIGH); // turn relay on
      ventiladoresOn = true;
      Serial.println("Encendiendo ventiladores");
    }
    // Si se ha bajado de la temperatura mínima y están encendidos los ventiladores, se apagan
    else if ((temperature <= (MAX_TEMP - MARGEN_TEMP)) and (ventiladoresOn))
    {
      digitalWrite(relayInput, LOW); // turn relay off
      ventiladoresOn = false;
      Serial.println("Apagando ventiladores");
    }
  }

  // Manejador para la interfaz con Thinger IO
  thing.handle();

  // Esperar 4 segundos para volver a chequear
  delay(4000);
}

float getTemperature() {
  float temp;
  
  while (temp == 85.0 || temp == (-127.0)) {
    DS18B20.requestTemperatures(); 
    temp = DS18B20.getTempCByIndex(0);
  }

  return temp;
}

