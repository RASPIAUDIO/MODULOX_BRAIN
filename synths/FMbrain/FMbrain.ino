#define HAMMOND

#include <modubrain.h>
#include "braindisplay.h"

#define REV_MUL(a)  (a)
#define l_CB0 REV_MUL(3460)
#define l_CB1 REV_MUL(2988)
#define l_CB2 REV_MUL(3882)
#define l_CB3 REV_MUL(4312)
#define l_AP0 REV_MUL(480)
#define l_AP1 REV_MUL(161)
#define l_AP2 REV_MUL(46)
#define REV_BUFF_SIZE   (l_CB0 + l_CB1 + l_CB2 + l_CB3 + l_AP0 + l_AP1 + l_AP2)

static float m1_sample[SAMPLE_BUFFER_SIZE];

void modubrainProcess2()
{
  uint32_t rec1=micros();
  FmSynth_Process(m1_sample, SAMPLE_BUFFER_SIZE);
  Reverb_Process(m1_sample, SAMPLE_BUFFER_SIZE);
  for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
    {
        fl_sample[n] = 10000.0*m1_sample[n];
        fr_sample[n] = 10000.0*m1_sample[n];
    }
 //Serial.println(fl_sample[0]);

  rec2=micros()-rec1;
  countz++;
  if(countz>1000) {countz=0;}

  Audio_Output(fl_sample, fr_sample);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Sine_Init();
  FmSynth_Init();
  modubrainInit();
  static float *revBuffer = (float *)malloc(sizeof(float) * REV_BUFF_SIZE);
  Reverb_Setup(revBuffer);
  Reverb_SetLevel(0, 0.4);
}

void loop() {
  Midi_Process();
  modubrainProcess2();
}
