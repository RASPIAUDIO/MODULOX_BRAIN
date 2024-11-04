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
#define DELAY_SAMPLES 60000
#include <delay.h>
#include <delay2.h>

#include "braindisplay.h"

#define MAX_SYNTH       2

int env_dest=0;
bool delay_on[MAX_SYNTH];
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

uint8_t presetmulti1=0;
uint8_t presetmulti2=0;

int current_synth = 0;

OscMonoPoly oscA[MAX_SYNTH];
Lfo lfo[3][MAX_SYNTH];
Disto2 disto[MAX_SYNTH];
TeeBeeFilter Filter[MAX_SYNTH];
Env env[2][MAX_SYNTH];
OnePoleFilter     highpass1;

uint8_t synth2_lfo_pwm[3][MAX_SYNTH];
uint8_t synth2_vol_osc[3][MAX_SYNTH];
uint8_t synth2_cutoff[MAX_SYNTH];
uint8_t synth2_reso[MAX_SYNTH];
uint8_t synth2_delay_time[MAX_SYNTH];
uint8_t synth2_delay_release[MAX_SYNTH];
uint8_t synth2_detune[MAX_SYNTH];
uint8_t synth2_unisson[MAX_SYNTH];

bool disto_on[MAX_SYNTH];
bool display_wave=false;
bool display_par=true;
float delay_mix[MAX_SYNTH];
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
