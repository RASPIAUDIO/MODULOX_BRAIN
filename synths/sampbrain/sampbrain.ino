bool stopaudio = false;

#include "mb.h"
#include "mbdisplay.h"
#include <TFT_eSPI.h>
#include <samplerfloat.h>
#include <disto2.h>
#include <compressorf.h>
#define TINY 0.000001f;
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"

#define DELAY_SAMPLES 100000
#include "delaystereo.h"

int env_dest=0;
bool delay_on=true;
bool disto_on=false;
bool filter_on=true;
bool comp_on=false;
bool save_bool=false;
bool load_bool=false;
int current_instru = 0;
uint8_t current_bar = 0;
uint8_t step_selected = 0;
uint8_t tempo_source = 0;   // 0: interne
                            // 1: sync in
uint8_t audio_out = 0;      // 0: HP
                            // 1: Headphone

uint8_t chan_voice[MAX_SAMPLE_NUM];
uint8_t note_voice[MAX_SAMPLE_NUM];
uint8_t pitch_voice[MAX_SAMPLE_NUM];
uint8_t vol_voice[MAX_SAMPLE_NUM];

Sampler smp;
TeeBeeFilter filter;
Compressor comp;
Disto2 disto;

bool playglb=true;

#define bufferLen 32
int16_t sBuffer[bufferLen*2];

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
      
      for (int i = 0; i < bufferLen; i++) {
        Synth_Process(&audioBuffer[2 * i], &audioBuffer[2 * i + 1]);
      }

      uint32_t end = micros();
      // On accumule la durée
      audioCpuTime += (end - start);
      size_t bytes_written = 0;
      i2s_write(i2s_num, audioBuffer, sizeof(audioBuffer), &bytes_written, portMAX_DELAY);
  }
}


void loop() {
  // put your main code here, to run repeatedly:
  if(!stopaudio)
  {
    Midi_Process();
    USB_Midi_Process();
    enco_turned();
    distance_sensor();
  }
    
}
