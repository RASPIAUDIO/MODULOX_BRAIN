bool stopaudio = false;

#include <modubrain.h>
#include <samplerfloat.h>
#include <lfofloat.h>
#include <webloader.h>
#include <ImprovWiFiLibrary.h>

#define TINY 0.000001f;
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"
#include <disto2.h>
#include <compressorf.h>
#include <delay.h>

#include "braindisplay.h"
//#include "phaser.h"

ImprovWiFi improvSerial(&Serial);

bool delay_on=false;
bool disto_on=false;
bool filter_on=false;
bool comp_on=false;
//bool phaser_on=false;
bool save_bool=false;
bool load_bool=false;
uint8_t audio_out = 0;      // 0: HP
                            // 1: Headphone

char linebuf[80];
int charcount = 0;

Sampler smp;
Lfo lfo;
Lfo lfo_phase;
TeeBeeFilter filter;
Compressor comp;
Disto2 disto;
//Phaser phaser;

bool display_wave=false;
bool display_par=true;
bool wifi_found=false;

uint8_t chan_voice[MAX_SAMPLE_NUM];
uint8_t note_voice[MAX_SAMPLE_NUM];
uint8_t vol_voice[MAX_SAMPLE_NUM];

bool wifiload=false;
bool wifijustload=false;
bool wifijustunload=false;

bool wifi_connected=false;

int previous_wifi_mode=0;




void onImprovWiFiErrorCb(ImprovTypes::Error err)
{
  //server.stop();
  //blink_led(2000, 3);
}

void onImprovWiFiConnectedCb(const char *ssid, const char *password)
{
  Serial.println("connected");
  wifiload=true;
  // Save ssid and password here
  //server.begin();
  //blink_led(100, 3);
}

bool connectWifi(const char *ssid, const char *password)
{
  WiFi.begin(ssid, password);

  while (!improvSerial.isConnected())
  {
    delay(100);
  }

  addFileSystems();
  setupFilemanager();

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  modubrainInit();
  disto.set_gain(10);
  param_focus[17]=2;
}

void loop() {
  if(stopaudio) delay(10);
  else
  {
    Midi_Process();  
    if(param_focus[17]==0) improvSerial.handleSerial();
    if(param_focus[17]==1) wifi_listener();
    if(param_focus[17]==2) modubrainProcess();
  }
}
