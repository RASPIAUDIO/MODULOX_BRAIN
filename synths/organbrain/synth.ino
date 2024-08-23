void Synth_Init()
{
  oscA.init();
  oscA.update_polyphony(4);
  /*oscA.setVolOsc(0, 80);
  oscA.setVolOsc(1, 110);
  oscA.setVolOsc(2, 70);
  oscA.setVolOsc(3, 64);
  oscA.setVolOsc(4, 40);
  oscA.setVolOsc(5, 35);
  oscA.setVolOsc(6, 30);
  oscA.setVolOsc(7, 64);*/
  filter.Init((float)SAMPLE_RATE);
  filter.SetMode(TeeBeeFilter::LP_24);
  filter.SetResonance( 5,true);
  lfo.init();
  lfo_phase.init();
  lfo.setmaincutoff(64);
  lfo_phase.setmaincutoff(64);
  lfo.setvol(8);
  lfo_phase.setvol(8);
  lfo_phase.phase_accu=256;
  lfo.start(); 
  lfo_phase.start();

}

float lfo1amount_prev=0;

inline void Synth_Process(int16_t *left, int16_t *right)
{

    /* gerenate a noise signal */
    //float noise_signal = ((random(1024) / 512.0f) - 1.0f) * soundNoiseLevel;

    /*
     * generator simulation, rotate all wheels
     */
    float out_l, out_r;
    
    out_l = 0;
    out_r = 0;

    float lfo1amount=lfo.output();
    float lfo2amount=lfo_phase.output();
    //Serial.println(lfo1amount);
    if(lfo1amount<0) lfo1amount=0;
    if(lfo1amount>=1.0) lfo1amount=1.0;
    if(lfo2amount<0) lfo2amount=0;
    if(lfo2amount>=1.0) lfo2amount=1.0;
    oscA.set_pitch_lfo(lfo1amount);
    if(lfo2amount>lfo1amount_prev+0.01 || lfo2amount<lfo1amount_prev-0.01)
    {
      //Serial.println(127.0*lfo2amount);
      filter.SetCutoff( 127.0*lfo2amount+40.0,true); 
      lfo1amount_prev=lfo2amount;
    }


    oscA.next();
  
    out_l = oscA.output()+lfo2amount-0.5;
    out_l = filter.Process(out_l); 
    out_r=out_l;

    *left = 32768.0*out_l;
    //Serial.println(*left);
    *right = *left;

}
