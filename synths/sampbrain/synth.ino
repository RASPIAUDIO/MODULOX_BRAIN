void Synth_Init()
{
  Serial.println("Synth_Init");
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
  delay_init();
  comp.init();
  //phaser.init();

}

float lfo1amount_prev=0;

inline void Synth_Process(int16_t *left, int16_t *right)
{
  float out_l, out_r;
    
  out_l = 0;
  out_r = 0;

  out_l = smp.sample_read();
  if(disto_on) out_l = disto.out(out_l);
  else out_l=fast_tanh(out_l);
  if(filter_on) out_l = filter.Process(out_l); 
  if(comp_on) out_l=comp.out(out_l);
  out_l=fast_tanh(out_l);
  int16_t dry=32767.0*out_l;
  int16_t dl, dr;
  delay_output(dry, dry, &dl, &dr);    
  out_l = dry*(1.0-delay_mix) + dl*delay_mix;
  out_r = dry*(1.0-delay_mix) + dr*delay_mix;
  //if(phaser_on) out_l=phaser.ProcessSample(out_l);
  //out_l=fast_tanh(out_l);
  //out_r=fast_tanh(out_r);
  *left = out_l;
  //Serial.println(*left);
  *right = out_r;

}