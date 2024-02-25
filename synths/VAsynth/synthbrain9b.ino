//INIT passe ok avec Xtal, en retirant commande   st += ES8960_Write_Reg(49, 0x0F7);
//  --> mais pas de son avec DOUT 4 seulemet son failbe avec 17



#include <modubrain.h>
#include <envfloat.h>
#include <oscfloat.h>
#include <lfofloat.h>
#include <disto2.h>
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"
#include <delay.h>

#include "braindisplay.h"



int env_dest=0;
bool delay_on=false;
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

bool disto_on[MAX_SYNTH];
bool display_wave=false;
bool display_par=true;
float delay_mix=0.0;
bool filter_on[MAX_SYNTH];

uint8_t poly=1;

bool audio_start=true;


void setup() {
  Serial.begin(115200);
  delay(1000);
  modubrainInit();
}

void loop() {
  if(audio_start)
  {
    Midi_Process();
    modubrainProcess();
  }
}
