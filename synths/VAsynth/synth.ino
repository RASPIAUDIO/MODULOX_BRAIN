void Synth_Init()
{
  //f4 = new HuovilainenMoog(44100) ;
  for(int i=0; i<MAX_SYNTH; i++)
  {
    oscA[i].init();
    oscA[i].glideon = true;
    disto[i].init();
    disto_on[i]=false;
    Filter[i].Init((float)SAMPLE_RATE);
    Filter[i].SetMode(TeeBeeFilter::LP_24);
    Filter[i].SetResonance( 5,true);
    filter_on[i]=false;
    lfo[0][i].init();
    //lfo[0][i].setfreq(param_lfo_freq[0]);
    lfo[1][i].init();
    //lfo[1][i].setfreq(param_lfo_freq[1]);
    lfo[2][i].init();
    //lfo[2][i].setfreq(param_lfo_freq[2]);
    delay_init();
    env[0][i].init();
    env[1][i].init();
    highpass1.setMode(OnePoleFilter::HIGHPASS);
    highpass1.setCutoff(24.167f);
  }
}


float volg=1.0;
float envamount_prev[2][2]={{0,0},{0,0}};
float lfo1amount_prev[3][2]={{0,0},{0,0},{0,0}};


void test_matrix(int desti, float amoun)
{
  if(desti==1) oscA[current_synth].set_pitch_lfo(amoun);
  if(desti==2) volg=amoun;
  if(desti==3) oscA[current_synth].setPWM(0, (int)(1+126.0*amoun));
  if(desti==4) oscA[current_synth].setPWM(1, (int)(1+126.0*amoun));
  if(desti==5) oscA[current_synth].setPWM(2, (int)(1+126.0*amoun));
  if(desti==6) oscA[current_synth].setVolOsc(0, (int)(127.0*amoun));
  if(desti==7) oscA[current_synth].setVolOsc(1, (int)(127.0*amoun));
  if(desti==8) oscA[current_synth].setVolOsc(2, (int)(127.0*amoun));
  if(desti==9) oscA[current_synth].set_pitch_lfo(0, amoun);
  if(desti==10) oscA[current_synth].set_pitch_lfo(1, amoun);
  if(desti==11) oscA[current_synth].set_pitch_lfo(2, amoun);
  if(desti==12) Filter[current_synth].SetCutoff( (int)(127.0*amoun),true); 
  if(desti==13) Filter[current_synth].SetResonance( (int)(127.0*amoun),true);
  if(desti==14) set_time((int)(127.0*amoun));
  if(desti==15) oscA[current_synth].compute_detune((int)(127.0*amoun));
  if(desti==16) oscA[current_synth].compute_unisson_vol((int)(127.0*amoun));
}


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
    float glbvol=1.0;

    

    if(multi_mode) // multi mode
    {
      float outvoice[2];
      outvoice[0]=0;
      outvoice[1]=0;
      for(int cur=0; cur<2; cur++)
      {
        for(int i=0; i<2; i++)
        {
          if(env[i][cur].dest>0)
          {
            float envamount=env[i][cur].amount();
            if(envamount<envamount_prev[i][cur]-0.01 || envamount>envamount_prev[i][cur]+0.01 )
            {
              test_matrix(env[i][cur].dest, envamount);
              display_wave=true;
              envamount_prev[i][cur]=envamount;
            }
          }
        }
    
        for(int i=0; i<3; i++)
        {
          if(lfo[i][cur].dest>0)
          {
            float lfo1amount=lfo[i][cur].output();
            if(lfo1amount<0) lfo1amount=0;
            if(lfo1amount>=1.0) lfo1amount=1.0;
            if(lfo1amount>lfo1amount_prev[i][cur]+0.01 || lfo1amount<lfo1amount_prev[i][cur]-0.01)
            {
              test_matrix(lfo[i][current_synth].dest, lfo1amount);
              display_wave=true;
              lfo1amount_prev[i][cur]=lfo1amount;
              //rec1=micros()-save_rec1;
            }
          }
        }
        
        /* counter required to optimize processing */
        //count += 1;
    
        oscA[cur].next();
      
        out_l = oscA[cur].output();
    
        if(disto_on[cur]) out_l = disto[cur].out(out_l);
        else out_l=fast_tanh(out_l);
    
        if(filter_on[cur]) out_l = Filter[cur].Process(out_l); 
        
        if(delay_on) out_l+=delay_output(out_l)*param_midi[44]/127;

        outvoice[cur]=out_l;
      }
      out_l=outvoice[0]+outvoice[1];
    }
    else
    {
      for(int i=0; i<2; i++)
      {
        if(env[i][current_synth].dest>0)
        {
          float envamount=env[i][current_synth].amount();
          if(envamount<envamount_prev[i][current_synth]-0.01 || envamount>envamount_prev[i][current_synth]+0.01 )
          {
            test_matrix(env[i][current_synth].dest, envamount);            
            display_wave=true;
            envamount_prev[i][current_synth]=envamount;
          }
        }
      }
  
      for(int i=0; i<3; i++)
      {
        if(lfo[i][current_synth].dest>0)
        {
          float lfo1amount=lfo[i][current_synth].output();
          if(lfo1amount<0) lfo1amount=0;
          if(lfo1amount>=1.0) lfo1amount=1.0;
          if(lfo1amount>lfo1amount_prev[i][current_synth]+0.01 || lfo1amount<lfo1amount_prev[i][current_synth]-0.01)
          {
            test_matrix(lfo[i][current_synth].dest, lfo1amount);
            display_wave=true;
            lfo1amount_prev[i][current_synth]=lfo1amount;
            //rec1=micros()-save_rec1;
          }
        }
      }
      
      
      
  
      /* counter required to optimize processing */
      //count += 1;
  
      oscA[current_synth].next();
   
      out_l = oscA[current_synth].output();

      
  
      if(disto_on[current_synth]) out_l = disto[current_synth].out(out_l);
  
      if(filter_on[current_synth]) out_l = Filter[current_synth].Process(out_l); 

      if(delay_on) out_l+=delay_output(out_l)*delay_mix;

      out_l*=volg;

      //out_l = highpass1.getSample(out_l);
    }
  
    
    out_l=fast_tanh(out_l);
    out_r=out_l;
 
    *left = 32767.0*out_l;
    *right = 32767.0*out_r;

    
}
