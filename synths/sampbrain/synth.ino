void Synth_Init()
{
  smp.init();
  /*smp.load_file("/kick01.wav",0,0);
  smp.load_file("/snare01.wav",1,1);
  smp.load_file("/hihat04.wav",2,2);*/
  for(int i=0; i<8; i++)
  {
    smp.tune(64,i);
    chan_voice[i]=i+1;
  }
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
  delay_init();
  comp.init();
  //phaser.init();

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

    out_l = smp.sample_read();
    if(disto_on) out_l = disto.out(out_l);
    else out_l=fast_tanh(out_l);
    if(filter_on) out_l = filter.Process(out_l); 
    if(delay_on) out_l+=delay_output(out_l)*param_midi[9]/127;
    if(comp_on) out_l=comp.out(out_l);
    //if(phaser_on) out_l=phaser.ProcessSample(out_l);
    out_l=fast_tanh(out_l);
    *left = 32767.0*out_l;
    //Serial.println(*left);
    *right = *left;

}
