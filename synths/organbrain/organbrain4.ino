#define HAMMOND

#include <modubrain.h>
#include <oschammond.h>
#include <lfofloat.h>
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"
#include <disto2.h>
#include <delay.h>
#include "braindisplay.h"


uint8_t changlob=0;
bool delay_on=false;
float delay_mix=0.0;
bool save_bool=false;
bool load_bool=false;
uint8_t audio_out = 0;      // 0: HP
                            // 1: Headphone

OscMonoPoly oscA;
Lfo lfo;
Lfo lfo_phase;
Disto2 disto;
TeeBeeFilter filter;

bool disto_on = false;
bool display_wave=false;
bool display_par=true;
bool audio_start=true;

void setup() {
  Serial.begin(115200);
  delay(1000);
  exclude_load=24;
  modubrainInit();
}

void loop() {
  if(audio_start)
  {
    Midi_Process();
    modubrainProcess();
  }
}
