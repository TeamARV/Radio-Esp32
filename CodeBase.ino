#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>

#define I2S_DOUT     25
#define I2S_BCLK     27
#define I2S_LRC      26

Audio audio;

String ssid = "xxxxx";
String password = "xxxx";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -5 * 3600); // Zona horaria GMT-5 (Colombia)

const int botonPinSubir = 21;   // Pin para aumentar el volumen
const int botonPinBajar = 23;   // Pin para disminuir el volumen
const int botonPinEmisora = 22; // Pin para cambiar de emisora
int currentStation = 0;
int volume = 5;         // Valor inicial del volumen
int volumeStep = 1;    // Incremento/decremento del volumen

String stationInfo;
String streamTitle;
String bitrateInfo;

// Ganancias iniciales para los filtros
int gainLowPass = 4;
int gainBandPass = 0;
int gainHighPass = -1;

// Update the callback functions to store the information
void audio_showstation(const char *info) {
  stationInfo = info;
}

void audio_showstreamtitle(const char *info) {
  streamTitle = info;
}

void audio_bitrate(const char *info) {
  bitrateInfo = info;
}
/////////////////

// Arreglo de URLs de las emisoras
String radioStations[] = {
  "http://ibizaglobalradio.streaming-pro.com:8024/",     // Emisora 1
  "https://secure.tempo-radio.com:1090/stream",  // estado trance
  "https://stream.live.vc.bbcmedia.co.uk/bbc_world_service", // BBC
  "http://nodo03-cloud01.streaming-pro.com:8057/vaughanradio.mp3",
  "http://playerservices.streamtheworld.com/api/livestream-redirect/RADIO_ACTIVA.mp3"
};

// Cantidad total de emisoras
int totalStations = sizeof(radioStations) / sizeof(radioStations[0]);

AsyncWebServer server(80);

void reproducirMensaje() {
  // Reproduce el mensaje de texto aquí
  // Puedes utilizar la función audio.connecttospeech() para reproducir el mensaje
  audio.connecttospeech("Bienvenido Amo Andrress", "Ru");
}

void handleRootRequest(AsyncWebServerRequest *request) {

  // Obtener la intensidad de señal WiFi
  int32_t rssi = WiFi.RSSI();

  // Envía la página HTML con el formulario y la información actual
  String html = "<html><body>";

  html += "<head>";
  html += "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body { background-color: #f0f0f0; }</style>";  // Ejemplo de estilo de fondo
  html += "<style>.rotate { animation: rotation 2s infinite linear; } @keyframes rotation { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }</style>";  // Estilo para rotación constante
  html += "</head>";

  html += "<h1 style='text-align: center;'>Radio 📻</h1>";
  html += "<p style='text-align: center;'><strong>Intensidad de señal WiFi: 📶" + String(rssi) + " dBm</strong></p>";
  html += "<div style='border: 4px solid black; padding: 10px; display: flex; flex-direction: column; align-items: center;'>";

  html += "<img src='https://i.imgur.com/CfZbjvs.jpg' />";  // Agregar clase 'rotate' para rotación constante

  html += "<form action='/setstation' method='post' style='display: flex; flex-direction: column; align-items: center; margin-top: 10px;'>"
          "<input type='text' name='station' placeholder='Ingrese emisora🎶' style='text-align: center; font-size: 20px; width: 80%;'>"
          "<br><br><input type='submit' value='Enviar emisora▶️' style='font-size: 17px; width: 70%;'>"
          "</form>";

   // Agregar controles deslizantes para las ganancias de los filtros
html += "<h2 style='text-align: center;'>Control de Ganancias</h2>";
html += "<form action='/setgain' method='post' style='display: flex; flex-direction: column; align-items: center; margin-top: 10px;'>";

// Ganancia Low Pass
html += "<label for='lowpass'>Ganancia Low Pass:</label>";
html += "<input type='range' id='lowpass' name='lowpass' min='-40' max='6' step='1' value='" + String(gainLowPass) + "' onchange='updateValue(this)'>";
html += "<span id='lowpass-value'>" + String(gainLowPass) + "</span>";

// Ganancia Band Pass
html += "<label for='bandpass'>Ganancia Band Pass:</label>";
html += "<input type='range' id='bandpass' name='bandpass' min='-40' max='6' step='1' value='" + String(gainBandPass) + "' onchange='updateValue(this)'>";
html += "<span id='bandpass-value'>" + String(gainBandPass) + "</span>";

// Ganancia High Pass
html += "<label for='highpass'>Ganancia High Pass:</label>";
html += "<input type='range' id='highpass' name='highpass' min='-40' max='6' step='1' value='" + String(gainHighPass) + "' onchange='updateValue(this)'>";
html += "<span id='highpass-value'>" + String(gainHighPass) + "</span>";

// Script para actualizar los valores al mover los controles deslizantes
html += "<script>";
html += "function updateValue(slider) {";
html += "var valueSpan = document.getElementById(slider.id + '-value');";
html += "valueSpan.textContent = slider.value;";
html += "}";
html += "</script>";

html += "<br><br><input type='submit' value='Aplicar ganancias'>";
html += "</form>";

  

  // Agregar información de reproducción actual
  html += "<h2 style='text-align: center;'>Información de reproducción 🎧 </h2>"
          "<p style='text-align: center;'><strong>Estación:</strong> " + stationInfo + "</p>"
          "<p style='text-align: center;'><strong>Título de la emisora:</strong> " + streamTitle + "</p>"
          "<p style='text-align: center;'><strong>Bitrate:</strong> " + bitrateInfo + "</p>";
  // Agregar más datos de reproducción según sea necesario

  html += "</div>";
  html += "<footer style='text-align: center; position: fixed; bottom: 0; left: 0; width: 100%; background-color: #61b3e6;'>Radio By Andrés <div class='rotate'> 💿 </div> </footer>";
  html += "</body></html>";

  request->send(200, "text/html", html);
}

void handleSetStation(AsyncWebServerRequest *request) {
  // Obtener la nueva emisora desde los parámetros del formulario
  String newStation = request->arg("station");

  // Conectarse a la nueva emisora
  audio.connecttohost(newStation.c_str());

  // Redirigir al usuario de vuelta a la página principal
  request->redirect("/");
}

void handleSetGain(AsyncWebServerRequest *request) {
  // Obtener las nuevas ganancias de los filtros desde los parámetros del formulario
  int newGainLowPass = request->arg("lowpass").toInt();
  int newGainBandPass = request->arg("bandpass").toInt();
  int newGainHighPass = request->arg("highpass").toInt();

  // Aplicar las nuevas ganancias a la función setTone
  audio.setTone(newGainLowPass, newGainBandPass, newGainHighPass);

  // Redirigir al usuario de vuelta a la página principal
  request->redirect("/");
}

void setup() {
  Serial.begin(115200);

  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // Habilitar el modo de estación
  WiFi.begin(ssid.c_str(), password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.print(".");
  }

  // WiFi Connected, print IP to serial monitor
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Intensidad de señal: ");
  Serial.println(WiFi.RSSI());

  // Establecer las ganancias iniciales de los filtros
  int gainLowPass = 0;
  int gainBandPass = 0;
  int gainHighPass = 0;

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolumeSteps(20);
  audio.setVolume(volume);
  audio.setTone(gainLowPass, gainBandPass, gainHighPass);

  //reproducirMensaje();

  Serial.println("Hora actual obtenida desde el servidor NTP:");
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  Serial.println(formattedTime);

  // Conectarse a la primera emisora
  audio.connecttohost(radioStations[currentStation].c_str());

  pinMode(botonPinSubir, INPUT_PULLUP);   // Configurar el pin del botón como entrada con resistencia pull-up
  pinMode(botonPinBajar, INPUT_PULLUP);   // Configurar el pin del botón como entrada con resistencia pull-up
  pinMode(botonPinEmisora, INPUT_PULLUP); // Configurar el pin del botón como entrada con resistencia pull-up

  server.on("/", HTTP_GET, handleRootRequest);
  server.on("/setstation", HTTP_POST, handleSetStation);
  server.on("/setgain", HTTP_POST, handleSetGain);

  server.begin();
}

void loop() {

  audio.loop();

  //Serial.print("loop() running on core ");
  //Serial.println(xPortGetCoreID());

  // Verificar si se ha presionado el botón de subir volumen
  if (digitalRead(botonPinSubir) == LOW) {
    volume += volumeStep;
    if (volume > 20)
      volume = 20;
    audio.setVolume(volume);
    Serial.println(audio.getVolume());
    delay(200);  // Retardo para evitar rebotes del botón
  }

  // Verificar si se ha presionado el botón de bajar volumen
  if (digitalRead(botonPinBajar) == LOW) {
    volume -= volumeStep;
    if (volume < 1)
      volume = 1;
    audio.setVolume(volume);
    Serial.println(audio.getVolume());
    delay(200);  // Retardo para evitar rebotes del botón
  }

  // Verificar si se ha presionado el botón de cambiar de emisora
  if (digitalRead(botonPinEmisora) == LOW) {
    currentStation = (currentStation + 1) % totalStations;
    audio.connecttohost(radioStations[currentStation].c_str());
    delay(200);  // Retardo para evitar rebotes del botón
    Serial.println("click cambio emisora");
  }
}
