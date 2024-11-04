void Synth_Init()
{
  //f4 = new HuovilainenMoog(44100) ;
  param_exclude[0]=true;
  param_exclude[61]=true;
  param_exclude[62]=true;
  param_exclude[63]=true;
  param_exclude[64]=true;
  param_exclude[65]=true;
  param_exclude[66]=true;
  param_exclude[67]=true;
  param_exclude[68]=true;
  param_exclude[69]=true;
  param_exclude[70]=true;
  for(int i=0; i<MAX_SYNTH; i++)
  {
    Serial.print("---init ");
    Serial.println(i);
    oscA[i].init();
    oscA[i].glideon = true;
    disto[i].init();
    disto_on[i]=false;
    delay_on[i]=false;
    
    delay_mix[i]=0.0;
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
    delay(50);
    delay2_init();
    delay(50);
    env[0][i].init();
    env[1][i].init();
    highpass1.setMode(OnePoleFilter::HIGHPASS);
    highpass1.setCutoff(24.167f);
  }
}


float volg=1.0;
float envamount_prev[2][2]={{0,0},{0,0}};
float lfo1amount_prev[3][2]={{0,0},{0,0},{0,0}};


void test_matrix(int desti, float amoun, int cur)
{
  if(desti==1) oscA[cur].set_pitch_lfo(amoun);
  if(desti==2) volg=amoun;
  if(desti==3) oscA[cur].setPWM(0, (int)(1+126.0*amoun));
  if(desti==4) oscA[cur].setPWM(1, (int)(1+126.0*amoun));
  if(desti==5) oscA[cur].setPWM(2, (int)(1+126.0*amoun));
  if(desti==6) oscA[cur].setVolOsc(0, (int)(127.0*amoun));
  if(desti==7) oscA[cur].setVolOsc(1, (int)(127.0*amoun));
  if(desti==8) oscA[cur].setVolOsc(2, (int)(127.0*amoun));
  if(desti==9) oscA[cur].set_pitch_lfo(0, amoun);
  if(desti==10) oscA[cur].set_pitch_lfo(1, amoun);
  if(desti==11) oscA[cur].set_pitch_lfo(2, amoun);
  if(desti==12) Filter[cur].SetCutoff( (int)(127.0*amoun),true); 
  if(desti==13) Filter[cur].SetResonance( (int)(127.0*amoun),true);
  if(desti==14) {
    if(cur==0) set_time((int)(127.0*amoun));
    if(cur==1) set_time2((int)(127.0*amoun));    
  }
  if(desti==15) oscA[cur].compute_detune((int)(127.0*amoun));
  if(desti==16) oscA[cur].compute_unisson_vol((int)(127.0*amoun));
}

void change_matrix(int desti, int cur)
{
  if(desti==1) {
    volg=1.0;
  }
  if(desti==2) 
  {
    oscA[cur].set_pitch_lfo(0.5);
    oscA[cur].setPWM(0, synth2_lfo_pwm[0][cur]);
  }
  if(desti==3) 
  {
    oscA[cur].setPWM(1, synth2_lfo_pwm[1][cur]);
    volg=1.0;
  }
  if(desti==4) 
  {
    oscA[cur].setPWM(0, synth2_lfo_pwm[0][cur]);
    oscA[cur].setPWM(2, synth2_lfo_pwm[2][cur]);
  }
  if(desti==5) 
  {
    oscA[cur].setVolOsc(0, synth2_vol_osc[0][cur]);
    oscA[cur].setPWM(1, synth2_lfo_pwm[1][cur]);
  }
  if(desti==6) 
  {
    oscA[cur].setPWM(2, synth2_lfo_pwm[2][cur]);
    oscA[cur].setVolOsc(1, synth2_vol_osc[1][cur]);
  }
  if(desti==7) 
  {
    oscA[cur].setVolOsc(0, synth2_vol_osc[0][cur]);
    oscA[cur].setVolOsc(2, synth2_vol_osc[2][cur]);
  }
  if(desti==8) 
  {
    oscA[cur].setVolOsc(1, synth2_vol_osc[1][cur]);
    oscA[cur].set_pitch_lfo(0,0.5);
  }
  if(desti==9) 
  {
    oscA[cur].setVolOsc(2, synth2_vol_osc[2][cur]);
    oscA[cur].set_pitch_lfo(1,0.5);
  }
  if(desti==10) 
  {
    oscA[cur].set_pitch_lfo(0,0.5);
    oscA[cur].set_pitch_lfo(2,0.5);
  }
  if(desti==11) 
  {
    oscA[cur].set_pitch_lfo(1,0.5);
    Filter[cur].SetCutoff(synth2_cutoff[cur],true); 
  }
  if(desti==12) 
  {
    oscA[cur].set_pitch_lfo(2,0.5);
    Filter[cur].SetResonance( synth2_reso[cur],true);
  }
  if(desti==13) 
  {
    Filter[cur].SetCutoff(synth2_cutoff[cur],true); 
    if(cur==0) set_time(synth2_delay_time[0]);
    if(cur==1) set_time2(synth2_delay_time[1]);
  }
  if(desti==14) 
  {
    Filter[cur].SetResonance(synth2_reso[cur],true);
    oscA[cur].compute_detune(synth2_detune[cur]);
  }
  if(desti==15) 
  {
    oscA[cur].compute_unisson_vol(synth2_unisson[cur]);
    if(cur==0) set_time(synth2_delay_time[0]);
    if(cur==1) set_time2(synth2_delay_time[1]);
  }
  if(desti==16) 
  {
    oscA[cur].compute_detune(synth2_detune[cur]);
  }
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
              test_matrix(env[i][cur].dest, envamount, cur);
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
              test_matrix(lfo[i][cur].dest, lfo1amount, cur);
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

        //if(cur==0 && filter_on[0]) out_l = Filter[0].Process(out_l);
    
        if(filter_on[cur]) out_l = Filter[cur].Process(out_l); 
        
        if(delay_on[cur]) 
        {
          if(cur==0) out_l+=delay_output(out_l)*delay_mix[cur];
          if(cur==1) out_l+=delay2_output(out_l)*delay_mix[cur];
        }

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
            test_matrix(env[i][current_synth].dest, envamount, current_synth);            
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
            test_matrix(lfo[i][current_synth].dest, lfo1amount, current_synth);
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
      if(delay_on[current_synth]) out_l+=delay_output(out_l)*delay_mix[current_synth];

      out_l*=volg;

      //out_l = highpass1.getSample(out_l);
    }
  
    
    out_l=fast_tanh(out_l);
    //if(out_l>=0.95) Serial.println(out_l);
    out_r=out_l;
 
    *left = 16383.0*out_l;
    *right = 16383.0*out_r;

    
}
