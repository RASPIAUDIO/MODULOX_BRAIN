#include "mb.h"
#include "mbdisplay.h"
#include "granulizer.h"
#include <TFT_eSPI.h>
#include <lfofloat.h>
#define TINY 0.000001f;
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"

#define DELAY_SAMPLES 100000
#include "delayint.h"

Granulizer granulizer;
TeeBeeFilter filter;
Lfo lfo;

bool record=false;

bool playglb=true;

#define bufferLen 32
int16_t sBuffer[bufferLen*2];

int sample_index=0;
int play_ind=0;

float lfoamount_prev=0;
float volglobal=0.0;
float delay_mix=0.0;

int16_t maxaudio=0;
int lastind=0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.setDebugOutput(true);

  modubrainInit();

  Serial.println("setup mbrain OK");

  //granulizer.noteOn(64);
}

// Fonction pour charger un grain depuis un fichier WAV

void test_matrix(int desti, float amoun)
{
  if(desti==1) granulizer.size_change(amoun*127.0);
  if(desti==2) granulizer.density_change(amoun*127.0);
  if(desti==3) granulizer.sample_rate_change(amoun*127.0);
  if(desti==4) filter.SetCutoff(amoun*127.0,true);
}


int cou=0;
// Fonction pour lire les grains et les mixer
void mixGrains(int16_t *outputBuffer, size_t numSamples) {
  //memset(outputBuffer, 0, numSamples * sizeof(int16_t));  // Initialisation du buffer de sortie à zéro
  //granulizer.load_buffers();
  for (int j = 0; j < numSamples; j++) {
    float lfoamount=lfo.output();
    if(lfoamount<0) lfoamount=0;
    if(lfoamount>=1.0) lfoamount=1.0;
    if(lfoamount>lfoamount_prev+0.01 || lfoamount<lfoamount_prev-0.01)
    {
      test_matrix(lfo.dest, lfoamount);
      lfoamount_prev=lfoamount;
    }
    outputBuffer[j] = granulizer.process();
    //if(outputBuffer[j]>16000) Serial.println(outputBuffer[j]);
    outputBuffer[j]=outputBuffer[j]*(1.0-delay_mix) + delay_output(outputBuffer[j])*delay_mix;
    outputBuffer[j]=32767.0*filter.Process((float)outputBuffer[j]/32767.0);
    
    cou++;
  }
}

void taskAudio(void *parameter) {
  int16_t audioBuffer[bufferLen*2];
  int numpass=0;
  play_ind=granulizer.sample_start_index[0];
  while (1) {
    if(!record) 
    {
      // Début de la mesure
      uint32_t start = micros();
        
      int16_t monoBuffer[bufferLen];
      int16_t testbuf[bufferLen];
      if(playglb) mixGrains(monoBuffer, bufferLen);
      else
      {
        for (int i = 0; i < bufferLen; i++) {
          monoBuffer[i]=0;          
        }
      }
      
      for (int i = 0; i < bufferLen; i++) {
        
        audioBuffer[2 * i] = monoBuffer[i];     // Canal gauche
        audioBuffer[2 * i + 1] = monoBuffer[i]; // Canal droit
      }
      // Fin de la mesure
      uint32_t end = micros();
      // On accumule la durée
      audioCpuTime += (end - start);
      size_t bytes_written = 0;
      i2s_write(i2s_num, audioBuffer, sizeof(audioBuffer), &bytes_written, portMAX_DELAY);
    }
    else {
      size_t bytesIn = 0;
      size_t bytesOut = 0;
      esp_err_t result = i2s_read(i2s_num, &sBuffer, sizeof(sBuffer), &bytesIn, portMAX_DELAY);
      
      //numpass++;
      //Serial.println(bytesIn);
      for (int i = 0; i < bufferLen; i++) {
        sample[sample_index]=sBuffer[i*2];  
        audioBuffer[2 * i + 1] = sBuffer[i*2]; 
        audioBuffer[2 * i] = sBuffer[i*2];   
        sample_index++;
      }
      if(sample_index>200000) but_record();

      if (result == ESP_OK)
      {
        // Send what we just received back to the codec
        esp_err_t result_w = i2s_write(i2s_num, &audioBuffer, bytesIn, &bytesOut, portMAX_DELAY);
        // If there was an I2S write error, let us know on the serial terminal
        if (result_w != ESP_OK)
        {
          Serial.print("I2S write error.");
        }
      }
      else
      {
        Serial.print("I2S read error.");
      }
    }
    // Convertit le mono en stéréo en dupliquant chaque échantillon
    
    
  }
}


void loop() {
  // put your main code here, to run repeatedly:
    Midi_Process();
}
