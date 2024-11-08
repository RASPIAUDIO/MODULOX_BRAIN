#include "mbrain.h"
#include "braindisplay.h"
#include "granulizer.h"
#include <TFT_eSPI.h>

Granulizer granulizer;

#define bufferLen 64
int16_t sBuffer[bufferLen];

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.setDebugOutput(true);

  modubrainInit();

  Serial.println("setup mbrain OK");
}

// Fonction pour charger un grain depuis un fichier WAV



int cou=0;
// Fonction pour lire les grains et les mixer
void mixGrains(int16_t *outputBuffer, size_t numSamples) {
  //memset(outputBuffer, 0, numSamples * sizeof(int16_t));  // Initialisation du buffer de sortie à zéro
  

  for (int j = 0; j < numSamples; j++) {
    outputBuffer[j] = granulizer.process();
    //if(cou<1000) Serial.println(outputBuffer[j]);
    cou++;
  }
}

void taskAudio(void *parameter) {
  int16_t audioBuffer[64];
  while (1) {
    mixGrains(audioBuffer, 256);
    size_t bytes_written = 0;
    i2s_write(i2s_num, audioBuffer, sizeof(audioBuffer), &bytes_written, portMAX_DELAY);
    
    /*size_t bytes_written = 0;
    size_t bytes_read = 0;
    i2s_read(i2s_num, audioBuffer, sizeof(audioBuffer), &bytes_read, portMAX_DELAY);
    i2s_write(i2s_num, audioBuffer, sizeof(audioBuffer), &bytes_written, portMAX_DELAY);*/
  }
  USB_Midi_Process();
  /*size_t bytesIn = 0;
  size_t bytesOut = 0;
  i2s_read(i2s_num, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);
  i2s_write(i2s_num, &sBuffer, bytesIn, &bytesOut, portMAX_DELAY);*/

  //mixGrains(audioBuffer, 256);
}


void loop() {
  // put your main code here, to run repeatedly:
}
