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
#include "phaser.h"

ImprovWiFi improvSerial(&Serial);

bool delay_on=false;
bool disto_on=false;
bool filter_on=false;
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
Phaser phaser;

bool display_wave=false;
bool display_par=true;
bool wifi_found=false;

uint8_t chan_voice[MAX_SAMPLE_NUM];
uint8_t note_voice[MAX_SAMPLE_NUM];

bool wifiload=true;
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
  delay(1000);
  modubrainInit();
  param_focus[17]=2;
  //if(wifiload) setup_wifi();
  /*WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "ImprovWiFiLib", "1.0.0", "BasicWebServer", "http://{LOCAL_IPV4}?name=Guest");
  improvSerial.onImprovError(onImprovWiFiErrorCb);
  improvSerial.onImprovConnected(onImprovWiFiConnectedCb);
  improvSerial.setCustomConnectWiFi(connectWifi);  // Optional*/
}

void loop() {
  Midi_Process();  
  /*if(wifijustload) {
      wifijustload=false;
      i2s_stop(i2s_num);
      WiFi.begin("Gaetan","");
      while (WiFi.status() != WL_CONNECTED) {
        delay(10);
      }

      if (!filemgr.AddFS(FFat, "Flash/FFat", false)) {
        Serial.println(F("Adding FFAT failed."));
      }
      setupFilemanager();
      //delay(1000);
      //setup_wifi();
    }
    if(wifijustunload) 
    {
      wifijustunload=false;
      //WiFi.disconnect(true);
      //delay(1000);
      i2s_start(i2s_num);
    }*/
  if(param_focus[17]==0) improvSerial.handleSerial();
  if(param_focus[17]==1) wifi_listener();
  if(param_focus[17]==2) modubrainProcess();
  //if(wifiload) wifi_listener();
  //else modubrainProcess();

}
