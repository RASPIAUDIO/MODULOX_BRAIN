#ifndef synth_h
#define synth_h

#include <setup.h>
#include <envfloat.h>
#include <oscfloat.h>
#include <lfofloat.h>
#include <reverb.h>
#include <disto2.h>
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"

class Synth
{
  
public:
  uint8_t param_osc_waveform[3]={0,0,0};
  uint8_t param_osc_vol[3]={64,64,64};
  uint8_t param_osc_pitch[3]={0,0,0};
  uint8_t param_osc_fine[3]={0,0,0};
  uint8_t param_osc_phase[3]={0,0,0};
  uint8_t param_osc_pwm[3]={0,0,0};
  uint8_t param_env_A[3]={10,10,10};
  uint8_t param_env_D[3]={10,10,10};
  uint8_t param_env_S[3]={100,100,100};
  uint8_t param_env_R[3]={10,10,10};
  uint8_t param_filter_mode[2]={0,0};
  uint8_t param_filter_freq[2]={100,100};
  uint8_t param_filter_res[2]={10,10};
  uint8_t param_lfo_waveform[3]={0,0,0};
  uint8_t param_lfo_freq[3]={5,5,5};
  uint8_t param_lfo_vol[3]={64,64,64};
  uint8_t param_lfo_cut[3]={64,64,64};
  uint8_t param_lfo_dest[3]={0,0,0};
  uint8_t param_delay_time=64;
  uint8_t param_delay_feedback=10;
  uint8_t param_delay_wet=50;
  bool delay_on=false;
  uint8_t param_disto_gainin=10;
  uint8_t param_disto_gainout=64;
  uint8_t param_disto_type=0;
  uint8_t param_arp_mode=0;
  uint8_t param_arp_rate=1;
  uint8_t param_arp_gate=100;
  uint8_t param_arp_step=1;
  uint8_t param_start_sample[4]={0,0,0,0};
  uint8_t param_end_sample[4]={0,0,0,0};
  uint8_t param_unisson_num=1;
  uint8_t param_unisson_vol=64;
  uint8_t param_unisson_detune=20;
  bool param_unisson_on=false;
  uint8_t env_dest=0;
  uint8_t lfo_dest[3]={0,0,0};
  uint8_t glide_time=64;  
  bool poly=false;
  
  OscMonoPoly oscA;
  Lfo lfo[3];
  Disto2 dist;
  TeeBeeFilter      Filter;
  Env env;
  
  float* samp_rec;
  int pos;
  float feedback;
  float deltime;
  
  bool disto_on = false;


  Synth()
  {
    oscA.init();
    oscA.glideon = true;
    dist.init();
    Filter.Init((float)SAMPLE_RATE);
    Filter.SetMode(TeeBeeFilter::LP_24);
    lfo[0].init();
    lfo[0].setfreq(param_lfo_freq[0]);
    lfo[1].init();
    lfo[1].setfreq(param_lfo_freq[1]);
    lfo[2].init();
    lfo[2].setfreq(param_lfo_freq[2]);
    delay_init();
    env.init();
  }

  void init()
  {
    oscA.init();
    oscA.glideon = true;
    dist.init();
    Filter.Init((float)SAMPLE_RATE);
    Filter.SetMode(TeeBeeFilter::LP_24);
    lfo[0].init();
    lfo[0].setfreq(param_lfo_freq[0]);
    lfo[1].init();
    lfo[1].setfreq(param_lfo_freq[1]);
    lfo[2].init();
    lfo[2].setfreq(param_lfo_freq[2]);
    delay_init();
    env.init();
  }

  void sprocess(int16_t *left, int16_t *right)
  {
  
      /* gerenate a noise signal */
      //float noise_signal = ((random(1024) / 512.0f) - 1.0f) * soundNoiseLevel;
  
      /*
       * generator simulation, rotate all wheels
       */
      float out_l, out_r;
      out_l = 0;
      out_r = 0;
  
      if(env_dest>0)
      {
        float envamount=env.amount();
        if(envamount!=envamount_prev)
        {
          if(env_dest==1) oscA.setPWM(0, param_osc_pwm[0]*envamount);
          if(env_dest==2) oscA.setPWM(1, param_osc_pwm[1]*envamount);
          if(env_dest==3) oscA.setPWM(2, param_osc_pwm[2]*envamount);
          if(env_dest==4) oscA.set_pitch_lfo(envamount);
          display_wave=true;
          envamount_prev=envamount;
        }
      }
  
      for(int i=0; i<3; i++)
      {
        if(lfo_dest[i]>0)
        {
          float lfo1amount=lfo[i].output();
          if(lfo1amount<0) lfo1amount=0;
          if(lfo1amount>=1.0) lfo1amount=1.0;
          if(lfo1amount!=lfo1amount_prev)
          {
            if(lfo_dest[i]==1) oscA.setPWM(0, 1+127.0*lfo1amount);
            if(lfo_dest[i]==2) oscA.setPWM(1, 1+127.0*lfo1amount);
            if(lfo_dest[i]==3) oscA.setPWM(2, 1+127.0*lfo1amount);
            if(lfo_dest[i]==4) Filter.SetCutoff( 127.0*lfo1amount,true); 
            if(lfo_dest[i]==5) Filter.SetResonance( 127.0*lfo1amount,true);  
            if(lfo_dest[i]==6) oscA.set_pitch_lfo(lfo1amount);
            display_wave=true;
            lfo1amount_prev=lfo1amount;
            //rec1=micros()-save_rec1;
          }
        }
      }
      
      
      
  
      /* counter required to optimize processing */
      //count += 1;
  
      oscA.next();
      
  
      //if(disto_on) out_l = dist.out(synth[current_synth].oscA.output());
      //else out_l = synth[current_synth].oscA.output();
  
      out_l = oscA.output();
  
      
  
      if(disto_on) out_l = dist.out(out_l);
  
      /*count++;
      if(count>32000)
      {
        count=0;
        Serial.println("synth");
        Serial.println(out_l);
        //Serial.println(*left);
      }*/
  
      if(param_filter_mode[0]) out_l = Filter.Process(out_l); 
   
      //f4.Process(&out_float[0], 1);
      
      if(delay_on) out_l+=delay_output(out_l)*param_delay_wet/127;
   
      out_r=out_l;
   
      *left += 32768.0*out_l;
      *right += 32768.0*out_r;
  }
  
    void delay_init()
  {
   pos=0;
   feedback = 0.4;
   deltime=0.3;
   samp_rec = (float *)ps_malloc(DELAY_SAMPLES * sizeof(float));
   for(int i=0; i<DELAY_SAMPLES; i++)
   {
    samp_rec[i]=0.0;
   }
  }
  
  void set_feedback(int feed)
  {
   feedback = feed/127.0;
  }
  
  void set_time(int ti)
  {
   deltime = ti/127.0;
  }
  
  float delay_output(float input)
  {
    float ret = samp_rec[pos];
    samp_rec[pos]=input+samp_rec[pos]*feedback;
    pos++;
    if(pos>=DELAY_SAMPLES*deltime*deltime) pos=0;
    return ret;
  }

  

private:
  float volg=1.0;
  float envamount_prev=0;
  float lfo1amount_prev=0;



};



#endif