void Synth_Init()
{
  //f4 = new HuovilainenMoog(44100) ;
  for(int i=0; i<MAX_SYNTH; i++)
  {
    oscA[i].init();
    oscA[i].glideon = true;
    disto[i].init();
    Filter[i].Init((float)SAMPLE_RATE);
    Filter[i].SetMode(TeeBeeFilter::LP_24);
    lfo[0][i].init();
    //lfo[0][i].setfreq(param_lfo_freq[0]);
    lfo[1][i].init();
    //lfo[1][i].setfreq(param_lfo_freq[1]);
    lfo[2][i].init();
    //lfo[2][i].setfreq(param_lfo_freq[2]);
    delay_init();
    env[i].init();
    highpass1.setMode(OnePoleFilter::HIGHPASS);
    highpass1.setCutoff(24.167f);
  }
}


float volg=1.0;
float envamount_prev[2]={0,0};
float lfo1amount_prev[2]={0,0};


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

    

    if(multi_mode) // multi mode
    {
      float outvoice[2];
      outvoice[0]=0;
      outvoice[1]=0;
      for(int cur=0; cur<2; cur++)
      {
        if(env_dest>0)
        {
          float envamount=env[cur].amount();
          if(envamount!=envamount_prev[cur])
          {
            if(env_dest==1) oscA[cur].setPWM(0, (int)(1+126.0*envamount));
            if(env_dest==2) oscA[cur].setPWM(1, (int)(1+126.0*envamount));
            if(env_dest==3) oscA[cur].setPWM(2, (int)(1+126.0*envamount));
            if(env_dest==4) oscA[cur].set_pitch_lfo(envamount);
            display_wave=true;
            envamount_prev[cur]=envamount;
          }
        }
    
        for(int i=0; i<3; i++)
        {
          if(lfo[i][cur].dest>0)
          {
            float lfo1amount=lfo[i][cur].output();
            if(lfo1amount<0) lfo1amount=0;
            if(lfo1amount>=1.0) lfo1amount=1.0;
            if(lfo1amount>lfo1amount_prev[cur]+0.01 || lfo1amount<lfo1amount_prev[cur]-0.01)
            {
              if(lfo[i][cur].dest==1) oscA[cur].setPWM(0, 1+126.0*lfo1amount);
              if(lfo[i][cur].dest==2) oscA[cur].setPWM(1, 1+126.0*lfo1amount);
              if(lfo[i][cur].dest==3) oscA[cur].setPWM(2, 1+126.0*lfo1amount);
              if(lfo[i][cur].dest==4) Filter[cur].SetCutoff( 127.0*lfo1amount,true); 
              if(lfo[i][cur].dest==5) Filter[cur].SetResonance( 127.0*lfo1amount,true);  
              if(lfo[i][cur].dest==6) oscA[cur].set_pitch_lfo(lfo1amount);
              display_wave=true;
              lfo1amount_prev[cur]=lfo1amount;
              //rec1=micros()-save_rec1;
            }
          }
        }
        
        /* counter required to optimize processing */
        //count += 1;
    
        oscA[cur].next();
      
        out_l = oscA[cur].output();
    
        if(disto_on) out_l = disto[cur].out(out_l);
        else out_l=fast_tanh(out_l);
    
        if(param_focus[40]) out_l = Filter[cur].Process(out_l); 
        
        if(delay_on) out_l+=delay_output(out_l)*param_midi[44]/127;

        outvoice[cur]=out_l;
      }
      out_l=outvoice[0]+outvoice[1];
    }
    else
    {
      if(env_dest>0)
      {
        float envamount=env[current_synth].amount();
        if(envamount!=envamount_prev[current_synth])
        {
          if(env_dest==1) oscA[current_synth].setPWM(0, (int)(1+126.0*envamount));
          if(env_dest==2) oscA[current_synth].setPWM(1, (int)(1+126.0*envamount));
          if(env_dest==3) oscA[current_synth].setPWM(2, (int)(1+126.0*envamount));
          if(env_dest==4) oscA[current_synth].set_pitch_lfo(envamount);
          display_wave=true;
          envamount_prev[current_synth]=envamount;
        }
      }
  
      for(int i=0; i<3; i++)
      {
        if(lfo[i][current_synth].dest>0)
        {
          float lfo1amount=lfo[i][current_synth].output();
          if(lfo1amount<0) lfo1amount=0;
          if(lfo1amount>=1.0) lfo1amount=1.0;
          if(lfo1amount>lfo1amount_prev[current_synth]+0.01 || lfo1amount<lfo1amount_prev[current_synth]-0.01)
          {
            if(lfo[i][current_synth].dest==1) oscA[current_synth].setPWM(0, 1+126.0*lfo1amount);
            if(lfo[i][current_synth].dest==2) oscA[current_synth].setPWM(1, 1+126.0*lfo1amount);
            if(lfo[i][current_synth].dest==3) oscA[current_synth].setPWM(2, 1+126.0*lfo1amount);
            if(lfo[i][current_synth].dest==4) Filter[current_synth].SetCutoff( 127.0*lfo1amount,true); 
            if(lfo[i][current_synth].dest==5) Filter[current_synth].SetResonance( 127.0*lfo1amount,true);  
            if(lfo[i][current_synth].dest==6) oscA[current_synth].set_pitch_lfo(lfo1amount);
            display_wave=true;
            lfo1amount_prev[current_synth]=lfo1amount;
            //rec1=micros()-save_rec1;
          }
        }
      }
      
      
      
  
      /* counter required to optimize processing */
      //count += 1;
  
      oscA[current_synth].next();
   
      out_l = oscA[current_synth].output();

      
  
      if(disto_on) out_l = disto[current_synth].out(out_l);
  
      if(param_focus[40]) out_l = Filter[current_synth].Process(out_l); 

      if(delay_on) out_l+=delay_output(out_l)*param_midi[44]/127;

      //out_l = highpass1.getSample(out_l);
    }
  
    

    out_r=out_l;
 
    *left = 32768.0*out_l;
    *right = 32768.0*out_r;

    
}
