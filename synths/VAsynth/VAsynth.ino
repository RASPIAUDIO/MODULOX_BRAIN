#include "mb.h"
#include "mbdisplay.h"
#include <loader_guard.h>
#include <envfloat.h>
#include <oscfloat2.h>
#include <TFT_eSPI.h>
#include <lfofloat.h>
#include <disto2.h>
#define TINY 0.000001f;
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"
#include "arp2.h"

#define DELAY_SAMPLES 100000
#include "delayint.h"

int env_dest=0;
bool delay_on=true;
bool save_bool=false;
bool load_bool=false;
int current_instru = 0;
uint8_t current_bar = 0;
uint8_t step_selected = 0;
uint8_t tempo_source = 0;   // 0: interne
                            // 1: sync in
uint8_t audio_out = 0;      // 0: HP
                            // 1: Headphone

uint8_t multi_mode;
uint8_t changlob=0;
uint8_t chansynth1=0;
uint8_t chansynth2=0;

#define MAX_SYNTH       2
int current_synth = 0;

OscMonoPoly oscA[MAX_SYNTH];
Lfo lfo[3][MAX_SYNTH];
Disto2 disto[MAX_SYNTH];
TeeBeeFilter Filter[MAX_SYNTH];
Env env[2][MAX_SYNTH];
OnePoleFilter     highpass1;
Arp arp;

bool disto_on[MAX_SYNTH];
bool display_wave=false;
bool display_par=true;
float delay_mix=0.0;
bool filter_on[MAX_SYNTH];

bool record=false;

bool playglb=true;

#define bufferLen 32
int16_t sBuffer[bufferLen*2];

float lfoamount_prev=0;
float volglobal=0.0;

int16_t maxaudio=0;
int lastind=0;

bool arpon=false;

void setup() {
  Serial.begin(115200);
  startLoaderGuard();
  delay(2000);
  Serial.setDebugOutput(true);

  modubrainInit();

  Serial.println("setup mbrain OK");

  //granulizer.noteOn(64);
}


int cou=0;
// Fonction pour lire les grains et les mixer


void taskAudio(void *parameter) {
  int16_t audioBuffer[bufferLen*2];
  int numpass=0;
  while (1) {
    
      // Début de la mesure
      uint32_t start = micros();
        
      int16_t monoBuffer[bufferLen];
      int16_t testbuf[bufferLen];
      /*if(playglb) 
      {
        for (int i = 0; i < bufferLen; i++) {
          monoBuffer[i]=0;          
        }
      }
      else
      {
        for (int i = 0; i < bufferLen; i++) {
          monoBuffer[i]=0;          
        }
      }*/
      
      for (int i = 0; i < bufferLen; i++) {
        Synth_Process(&audioBuffer[2 * i], &audioBuffer[2 * i + 1]);
      }
      if(arpon) arp.update(&oscA[0]);
      // Fin de la mesure
      uint32_t end = micros();
      // On accumule la durée
      audioCpuTime += (end - start);
      size_t bytes_written = 0;
      i2s_write(i2s_num, audioBuffer, sizeof(audioBuffer), &bytes_written, portMAX_DELAY);
  }
}


void loop() {
  // put your main code here, to run repeatedly:
    Midi_Process();
    USB_Midi_Process();
    enco_turned();
    distance_sensor();
}
