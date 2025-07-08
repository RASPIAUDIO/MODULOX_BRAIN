#include "mb.h"
#include "mbdisplay.h"
#include "granulizer.h"
#include <TFT_eSPI.h>
#include <lfofloat.h>
#include <envfloat.h>
#define TINY 0.000001f;
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"
#include <VL53L0X.h>

#define DELAY_SAMPLES 100000
#include "delaystereo.h"

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
float envamount_prev=0;
float volglobal=0.0;
float delay_mix=0.0;

int16_t maxaudio=0;
int lastind=0;

Env env2;

#define PERIOD_MS 20  // sensor cadence
#define MAX_MM 2000   // clip anything beyond 2 m
#define BAR_COLS 80   // full-width bar at MAX_MM

VL53L0X tof;
bool tof_connected=true;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.setDebugOutput(true);

  modubrainInit();

  delay(500);
  if (!tof.init()) {
    Serial.println("VL53L0X not found – check wiring");
    tof_connected=false;
  }
  else
  {
    tof.setMeasurementTimingBudget(20000);  // 20 000 µs budget
    tof.startContinuous(PERIOD_MS);
    tof_connected=true;
  }
  
  delay(500);

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

void change_matrix(int desti)
{
  if(desti==1) {
    granulizer.density_change(param_midi[5]);;
  }
  if(desti==2) 
  {
    granulizer.size_change(param_midi[3]);
    granulizer.sample_rate_change(param_midi[6]);
  }
  if(desti==3) 
  {
    granulizer.density_change(param_midi[5]);
    filter.SetCutoff((float)param_midi[12],true);
  }
  if(desti==4) 
  {
    granulizer.sample_rate_change(param_midi[6]);
  }
}


int cou=0;
// Fonction pour lire les grains et les mixer
//void mixGrains(int16_t *outputBuffer, size_t numSamples) {
void mixGrains(int16_t *outL, int16_t *outR, size_t numSamples) {
  //memset(outputBuffer, 0, numSamples * sizeof(int16_t));  // Initialisation du buffer de sortie à zéro
  //granulizer.load_buffers();
  for (int j = 0; j < numSamples; j++) {
    if(env2.dest>0)
    {
      float env2amount=env2.amount();
      if(env2amount<envamount_prev-0.01 || env2amount>envamount_prev+0.01 )
      {
        test_matrix(env2.dest, env2amount);
        envamount_prev=env2amount;
      }
    }
    float lfoamount=lfo.output();
    if(lfoamount<0) lfoamount=0;
    if(lfoamount>=1.0) lfoamount=1.0;
    if(lfoamount>lfoamount_prev+0.01 || lfoamount<lfoamount_prev-0.01)
    {
      test_matrix(lfo.dest, lfoamount);
      lfoamount_prev=lfoamount;
    }
    /*outputBuffer[j] = granulizer.process();
    outputBuffer[j]=32767.0*filter.Process((float)outputBuffer[j]/32767.0);
    //if(outputBuffer[j]>16000) Serial.println(outputBuffer[j]);
    outputBuffer[j]=outputBuffer[j]*(1.0-delay_mix) + delay_output(outputBuffer[j])*delay_mix;*/
    int16_t dry = granulizer.process();
    dry=32767.0*filter.Process((float)dry/32767.0);
    int16_t dl, dr;
    delay_output(dry, dry, &dl, &dr);    
    outL[j] = dry*(1.0-delay_mix) + dl*delay_mix;
    outR[j] = dry*(1.0-delay_mix) + dr*delay_mix;
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
        
      /*int16_t monoBuffer[bufferLen];
      int16_t testbuf[bufferLen];
      if(playglb) mixGrains(monoBuffer, bufferLen);*/
      int16_t leftBuffer[bufferLen];
      int16_t rightBuffer[bufferLen];
      if(playglb) mixGrains(leftBuffer, rightBuffer, bufferLen);
      else
      {
        for (int i = 0; i < bufferLen; i++) {
          //monoBuffer[i]=0; 
          leftBuffer[i]=0;
          rightBuffer[i]=0;         
        }
      }
      
      for (int i = 0; i < bufferLen; i++) {
        
        /*audioBuffer[2 * i] = monoBuffer[i];     // Canal gauche
        audioBuffer[2 * i + 1] = monoBuffer[i]; // Canal droit*/
        audioBuffer[2 * i] = leftBuffer[i];
        audioBuffer[2 * i + 1] = rightBuffer[i];
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

int prev_mm=0;
uint32_t t_prev=0;

void loop() {
  // put your main code here, to run repeatedly:
    Midi_Process();
    USB_Midi_Process();
    enco_turned();
    if(tof_connected)
    {
      uint32_t t=millis();
      if(t-t_prev>=20)
      {
        uint16_t mm = tof.readRangeContinuousMillimeters();
        if(mm<25) mm=25;
        if(mm>533) mm=533;
        mm=127-(mm-25)/4;
        if(mm!=prev_mm)
        {
          data_from_MIDI=3;
          Midi_ControlChange(0, 129, mm);
          prev_mm=mm;
        }
        t_prev=t;
      }      
    }
    uint32_t t=millis();
    if(t-t_prev>=20)
    {
      uint16_t mm = tof.readRangeContinuousMillimeters();
      if(mm<25) mm=25;
      if(mm>533) mm=533;
      mm=127-(mm-25)/4;
      if(mm!=prev_mm)
      {
        data_from_MIDI=3;
        Midi_ControlChange(0, 129, mm);
        prev_mm=mm;
      }
      t_prev=t;
    }
    
    
    //Serial.println(127-(mm-25)/4);
}
