void param_action(int num)
{
  Serial.println("param_action");
  if(num==0) {savenum=param_midi[num]; load_preset();}
  if(num>0 && num<16)
  {
    int ind=(num-1)/5;
    int tes=(num-1)%5;
    if(tes == 0) oscA[0].setWaveform(ind, param_midi[num]/2);
    if(tes == 1) oscA[0].setPWM(ind, param_midi[num]*100/127);
    if(tes == 2) oscA[0].setFine(ind, ((int)param_midi[num]-64)*256 + param_midi[num+1]);
    if(tes == 3) oscA[0].setFine(ind, ((int)param_midi[num-1]-64)*256 + param_midi[num]);
    if(tes == 4) oscA[0].setVolOsc(ind, param_midi[num]);
  }
  if(num>=16 && num<=27)
  {
    int ind=(num-16)/4;
    int tes=(num-16)%4;
    if(ind==0) update_env(current_synth);
    if(ind==1) update_env_filter();
    if(ind==2)
    {
      if(tes == 0) env[current_synth].setA(param_midi[num]);
      if(tes == 1) env[current_synth].setD(param_midi[num]);
      if(tes == 2) env[current_synth].setS(param_midi[num]);
      if(tes == 3) env[current_synth].setR(param_midi[num]);
    }
  }
  if(num>=28 && num<=39)
  {
    int ind=(num-28)/4;
    int tes=(num-28)%4;
    if(tes == 0) lfo[ind][current_synth].setWaveform(param_midi[num]%3);
    if(tes == 1) lfo[ind][current_synth].setfreq((float)param_midi[num]*(float)param_midi[num]/127);
    if(tes == 2) lfo[ind][current_synth].setvol(param_midi[num]);
    if(tes == 3) lfo[ind][current_synth].setmaincutoff(param_midi[num]);

  }
  if(num==40) Filter[current_synth].SetCutoff((float)param_midi[num],true);
  if(num==41) Filter[current_synth].SetResonance((float)param_midi[num],true);
  if(num==42) set_time(param_midi[num]);
  if(num==43) set_feedback(param_midi[num]);
  if(num==45) oscA[0].update_polyphony((param_midi[num]%8)+1);
  if(num==46) oscA[0].setGlideTime(param_midi[num]);
  if(num==47) oscA[0].update_bpm(param_midi[num]);
  if(num==48) {oscA[0].arpmode=param_midi[num]%3; oscA[0].compute_notes();}
  if(num==49) oscA[0].arprate=param_midi[num]%16;
  if(num==50) oscA[0].update_gate(param_midi[num]);
  if(num==51) {oscA[0].arpstep=param_midi[num]%5; oscA[0].compute_notes();}
  if(num==52) oscA[0].unisson = (param_midi[num]%8)+1;
  if(num==53) oscA[0].compute_detune(param_midi[num]);
  if(num==54) oscA[0].compute_unisson_vol(param_midi[num]);
  if(num==56) disto[current_synth].set_gain(param_midi[num]);
  if(num==57) tempo_source=param_midi[num]%3;
  if(num==58) changlob=(param_midi[num]%8)+1;
  if(num==59) {if(param_midi[num]%2==0) ES8960_Init2();
                if(param_midi[num]%2==1) hp_spk();}
  if(num==60) savenum=param_midi[num]; 
  if(num==61) savenum=param_midi[num];
  if(num==62) multi_mode=param_midi[num]%2;
  if(num==63) {savenum=param_midi[num]%8;
                load_preset();
                current_synth=0;
                init_synth_param();}
  if(num==64) {savenum=param_midi[num]%8;
                load_preset();
                current_synth=0;
                init_synth_param();}
  if(num==65) chansynth1=(param_midi[num]%8)+1;
  if(num==66) chansynth2=(param_midi[num]%8)+1;
}

void init_lfo_dest(int num)
{
  oscA[current_synth].setPWM(0, 0);
  oscA[current_synth].setPWM(1, 0);
  oscA[current_synth].setPWM(2, 0);
  Filter[current_synth].SetCutoff( param_midi[40],true); 
  Filter[current_synth].SetResonance( param_midi[41],true);
  oscA[current_synth].set_pitch_lfo(0.5);
}

void param_action_focus(int num)
{
  if(num>=24 && num<=27) {
    for(int i=24; i<=27; i++) param_focus[i]=param_focus[num];
    env_dest=param_focus[num]%4;
  }
  if(num>=28 && num<=31) {
    for(int i=28; i<=31; i++) param_focus[i]=param_focus[num];
    lfo[0][current_synth].dest==param_focus[num]%7;
  }
  if(num>=32 && num<=35) {
    for(int i=32; i<=35; i++) param_focus[i]=param_focus[num];
    lfo[1][current_synth].dest==param_focus[num]%7;
  }
  if(num>=36 && num<=39) {
    for(int i=36; i<=39; i++) param_focus[i]=param_focus[num];
    lfo[2][current_synth].dest==param_focus[num]%7;
  }
  if(num>=40 && num<=41) {
    for(int i=40; i<=41; i++) param_focus[i]=param_focus[num];
    if((param_focus[num]%4)>0) Filter[current_synth].SetMode((param_focus[num]%4)-1);
  }
  if(num>=42 && num<=43) {
    for(int i=42; i<=43; i++) param_focus[i]=param_focus[num];
    delay_on=param_focus[num]%2;
  }
  if(num==46) oscA[0].glideon=param_focus[num]%2;
  if(num>=48 && num<=51) {
    for(int i=48; i<=51; i++) param_focus[i]=param_focus[num];
    oscA[0].arpon=param_focus[num]%2;
  }
  if(num >= 55 && num <= 56) 
  {
    for(int i=55; i<=56; i++) param_focus[i]=param_focus[num];
    disto_on=param_focus[num]%2;
  }
  if(num ==60) 
  {
    load_bool=param_focus[num]%2;
  }
  if(num ==61) 
  {
    save_bool=param_focus[num]%2;
  }
  disp.menu_bottom(num,param_focus[num]);
  disp.display_bottom();
  
}


void change_enco(int sens)
{
  //tft.fillScreen(TFT_BLACK);
  Serial.println("change_enco");
  if(enco_focus==-1)
  {
    disp.change_menu_select(sens);
  }
  if(enco_focus==1)
  {
    line_selected[menu_level]+=sens;
    display_menu();
    disp.menu_hierarchy();
  }
  if(enco_focus>1)
  {
    param_focus[param_displayed]+=sens;
    if(param_focus[param_displayed]<0) param_focus[param_displayed]=0;
    if(param_focus[param_displayed]>param_focus_max) param_focus[param_displayed]=param_focus_max;
    param_action_focus(param_displayed);
    //display_menu();
    disp.display_list(param_focus[param_displayed],90,100,100);
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

void change_CC(int sens)
{

  param_midi[param_displayed]=sens;  
  if(param_midi[param_displayed]>127) param_midi[param_displayed]=127;
  if(param_midi[param_displayed]<0) param_midi[param_displayed]=0;
  param_action(param_displayed);
}

void init_synth_param()
{
  Serial.println("init_synth_param");
  for(int i=0; i<128; i++)
  {
    Serial.println(i);
    Serial.println(param_midi[i]);
    if(i!=59 && i!=60 && i!=61 && i!=63 && i!=64 && i!=0) param_action(i);
  }
}
