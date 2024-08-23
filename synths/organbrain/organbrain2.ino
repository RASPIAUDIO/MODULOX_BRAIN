#define HAMMOND

#include <modubrain.h>
#include <oschammond.h>
#include <lfofloat.h>
#include <webloader.h>
#include <ImprovWiFiLibrary.h>

#define TINY 0.000001f;
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"
#include <disto2.h>

#include "braindisplay.h"

ImprovWiFi improvSerial(&Serial);

bool wifi_found=false;
bool wifiload=false;
bool wifijustload=false;
bool wifijustunload=false;
bool wifi_connected=false;

bool delay_on=false;
bool save_bool=false;
bool load_bool=false;
uint8_t audio_out = 0;      // 0: HP
                            // 1: Headphone

OscMonoPoly oscA;
Lfo lfo;
Lfo lfo_phase;
TeeBeeFilter filter;

bool disto_on = false;
bool display_wave=false;
bool display_par=true;

void setup() {
  Serial.begin(115200);
  delay(1000);
  modubrainInit();
  param_focus[18]=2;
}

void loop() {
  Midi_Process();
  if(param_focus[18]==0) improvSerial.handleSerial();
  if(param_focus[18]==1) wifi_listener();
  if(param_focus[18]==2) modubrainProcess();
}
