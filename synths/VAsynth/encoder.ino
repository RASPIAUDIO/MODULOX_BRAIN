void param_action(int num)
{
  Serial.print("param_action : ");
  Serial.println(current_synth);
  Serial.println(delay_on[1]);
  
  if(!param_exclude[num] || exclude_load) 
  {
    if(num==0) {
      savenum=param_midi[num]; 
      audio_start=false; 
      delay(200);
      load_preset();
      delay(200);
      audio_start=true;
    }
    if(num>0 && num<16)
    {
      int ind=(num-1)/5;
      int tes=(num-1)%5;
      if(tes == 0) oscA[current_synth].setWaveform(ind, param_midi[num]/2);
      if(tes == 1) {
        oscA[current_synth].setPWM(ind, param_midi[num]*100/127);
        synth2_lfo_pwm[ind][current_synth]=param_midi[num];
      }
      if(tes == 2) oscA[current_synth].setFine(ind, ((int)param_midi[num]-64)*256 + param_midi[num+1]);
      if(tes == 3) oscA[current_synth].setFine(ind, ((int)param_midi[num-1]-64)*256 + param_midi[num]);
      if(tes == 4) {
        oscA[current_synth].setVolOsc(ind, param_midi[num]);
        synth2_vol_osc[ind][current_synth]=param_midi[num];
      }
    }
    
    
    
    
    if(num==16) oscA[current_synth].setenvA(param_midi[num]);
    if(num==17) oscA[current_synth].setenvD(param_midi[num]);
    if(num==18) oscA[current_synth].setenvS(param_midi[num]);
    if(num==19) oscA[current_synth].setenvR(param_midi[num]);
    if(num==20) {
      env[0][current_synth].dest=param_midi[num]%16;
      change_matrix(env[0][current_synth].dest, current_synth);
    }
    if(num==21) env[0][current_synth].setA(param_midi[num]);
    if(num==22) env[0][current_synth].setD(param_midi[num]);
    if(num==23) env[0][current_synth].setS(param_midi[num]);
    if(num==24) env[0][current_synth].setR(param_midi[num]);
    if(num==25) {
      env[1][current_synth].dest=param_midi[num]%16;
      change_matrix(env[1][current_synth].dest, current_synth);
    }
    if(num==26) env[1][current_synth].setA(param_midi[num]);
    if(num==27) env[1][current_synth].setD(param_midi[num]);
    if(num==28) env[1][current_synth].setS(param_midi[num]);
    if(num==29) env[1][current_synth].setR(param_midi[num]);
    if(num>=30 && num<=44)
    {
      int ind=(num-30)/5;
      int tes=(num-30)%5;
      if(tes == 0) {
        lfo[ind][current_synth].dest=param_midi[num]%16;
        change_matrix(lfo[ind][current_synth].dest, current_synth);
      }
      if(tes == 1) lfo[ind][current_synth].setWaveform(param_midi[num]%3);
      if(tes == 2) lfo[ind][current_synth].setfreq((float)param_midi[num]*(float)param_midi[num]/127);
      if(tes == 3) lfo[ind][current_synth].setvol(param_midi[num]);
      if(tes == 4) lfo[ind][current_synth].setmaincutoff(param_midi[num]);
  
    }
    if(num==45) 
    {
      Filter[current_synth].SetCutoff((float)param_midi[num],true);
      synth2_cutoff[current_synth]=param_midi[num];
    }
    if(num==46) 
    {
      Filter[current_synth].SetResonance((float)param_midi[num],true);
      synth2_reso[current_synth]=param_midi[num];
    }
    if(num==47 && current_synth==0) {set_time(param_midi[num]); synth2_delay_time[0]=param_midi[num]; Serial.print("delay0 "); Serial.println(param_midi[num]);}
    if(num==47 && current_synth==1) {set_time2(param_midi[num]); synth2_delay_time[1]=param_midi[num]; Serial.print("delay1 "); Serial.println(param_midi[num]);}
    if(num==48 && current_synth==0) {set_feedback(param_midi[num]); synth2_delay_release[0]=param_midi[num];}
    if(num==48 && current_synth==1) {set_feedback2(param_midi[num]); synth2_delay_release[1]=param_midi[num];}
    if(num==49) delay_mix[current_synth]=(float)param_midi[num]/127.0;
    if(num==50) oscA[current_synth].update_polyphony((param_midi[num]%8)+1);
    if(num==51) oscA[current_synth].setGlideTime(param_midi[num]);
    if(num==52) oscA[current_synth].update_bpm(param_midi[num]);
    if(num==53) {oscA[current_synth].arpmode=param_midi[num]%3; oscA[current_synth].compute_notes();}
    if(num==54) oscA[current_synth].arprate=param_midi[num]%16;
    if(num==55) oscA[current_synth].update_gate(param_midi[num]);
    if(num==56) {oscA[current_synth].arpstep=param_midi[num]%5; oscA[current_synth].compute_notes();}
    if(num==57) oscA[current_synth].unisson = (param_midi[num]%8)+1;
    if(num==58) {oscA[current_synth].compute_detune(param_midi[num]); synth2_detune[current_synth]=param_midi[num];}
    if(num==59) {oscA[current_synth].compute_unisson_vol(param_midi[num]); synth2_unisson[current_synth]=param_midi[num];}
    if(num==60) disto[current_synth].set_gain(param_midi[num]);
    if(num==61) tempo_source=param_midi[num]%3;
    if(num==62) changlob=param_midi[num]%16;
    if(num==63) {if(param_midi[num]%2==0) ES8960_Init2();
                  if(param_midi[num]%2==1) hp_spk();}
    if(num==64) savenum=param_midi[num]%16; 
    if(num==65) savenum=param_midi[num]%16;
    if(num==66) multi_mode=param_midi[num]%2;
    if(num==67) {savenum=param_midi[num]%16;
                  presetmulti1=savenum;
                  audio_start=false; 
                  delay(200);
                  current_synth=0;
                  delay(5);
                  load_preset();
                  delay(200);
                  current_synth=0;
                  audio_start=true;
                  }
    if(num==68) {savenum=param_midi[num]%16;
                  presetmulti2=savenum;
                  audio_start=false; 
                  delay(200);
                  current_synth=1;
                  delay(5);
                  load_preset();
                  delay(200);
                  current_synth=0;
                  audio_start=true;
                  }
    if(num==69) chansynth1=(param_midi[num]%16)+1;
    if(num==70) chansynth2=(param_midi[num]%16)+1;
    Serial.println(delay_on[1]);
  }
}

void param_action_focus(int num)
{
  Serial.println("param_action_focus");
  Serial.println(num);
  Serial.println(delay_on[1]);
  if(num>=45 && num<=46) {
    for(int i=45; i<=46; i++) param_focus[i]=param_focus[num];
    
    if((param_focus[num]%4)==1) Filter[current_synth].SetMode(7);
    if((param_focus[num]%4)==2) Filter[current_synth].SetMode(9);
    if((param_focus[num]%4)==3) Filter[current_synth].SetMode(15);
    if((param_focus[num]%4)==0) filter_on[current_synth]=false;
    else filter_on[current_synth]=true;
  }
  if(num>=47 && num<=49) {
    delay_on[current_synth]=param_focus[num]%2;
    Serial.print("delay");
    Serial.println(current_synth);
    for(int i=47; i<=49; i++) param_focus[i]=delay_on[current_synth];
  }
  if(num==60) {
    disto_on[current_synth]=param_focus[num]%2;
    param_focus[num]=disto_on[current_synth];
  }
  Serial.println(filter_on[current_synth]);
  
  if(num==51) oscA[current_synth].glideon=param_focus[num]%2;
  if(num>=52 && num<=56) {
    for(int i=52; i<=56; i++) param_focus[i]=param_focus[num];
    oscA[current_synth].arpon=param_focus[num]%2;
  }  
  Serial.println(delay_on[1]);
}


void change_enco(int sens)
{
  //tft.fillScreen(TFT_BLACK);
  Serial.println("change_enco");
  Serial.println(enco_focus);
  if(enco_focus==-1)
  {
    disp.change_menu_select(sens);
  }
  if(enco_focus==1)
  {
    disp.line_selected[disp.menu_level]+=sens;
    display_menu();
    disp.menu_hierarchy();
  }
  if(enco_focus==2)
  {
    if(param_focus[param_displayed]<=-sens) param_focus[param_displayed]=0;
    else param_focus[param_displayed]+=sens;
    if(param_focus[param_displayed]<0) param_focus[param_displayed]=0;
    if(param_focus[param_displayed]>param_focus_max) param_focus[param_displayed]=param_focus_max;
    param_action_focus(param_displayed);
    //display_menu();
    disp.menu_bottom(param_displayed,param_focus[param_displayed]);
    disp.display_bottom();
    disp.display_list(param_focus[param_displayed],0,10,100);
  }
  if(enco_focus==3)
  {
    disp.valida=!disp.valida;
    disp.draw_validate();
  }
  if(enco_focus==0)
  {
    Serial.println(param_midi[param_displayed]);
    if(sens<0) 
    {
       if(param_midi[param_displayed]>=-sens) param_midi[param_displayed]+=sens;
    }
    else
    {
      if((127-param_midi[param_displayed])>=sens) param_midi[param_displayed]+=sens;
    }
    Serial.println("param " + String(param_displayed) + " changed : " + String(param_midi[param_displayed]));
    param_action(param_displayed);
    display_param();
  }
    
  //if((param_displayed != 2 || env_dest!=1)&&display_par) display_param();
  Serial.println("end change_enco");
}
