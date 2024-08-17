void param_action(int num)
{
  //Serial.println("param_action");
  if(num==0) {
    savenum=param_midi[num]%16; 
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
    if(tes == 0) oscA[0].setWaveform(ind, param_midi[num]/2);
    if(tes == 1) oscA[0].setPWM(ind, param_midi[num]*100/127);
    if(tes == 2) oscA[0].setFine(ind, ((int)param_midi[num]-64)*256 + param_midi[num+1]);
    if(tes == 3) oscA[0].setFine(ind, ((int)param_midi[num-1]-64)*256 + param_midi[num]);
    if(tes == 4) oscA[0].setVolOsc(ind, param_midi[num]);
  }
  
  
  
  
  if(num==16) oscA[0].setenvA(param_midi[num]);
  if(num==17) oscA[0].setenvD(param_midi[num]);
  if(num==18) oscA[0].setenvS(param_midi[num]);
  if(num==19) oscA[0].setenvR(param_midi[num]);
  if(num==20) env[1][current_synth].dest=param_midi[num]%16;
  if(num==21) env[1][current_synth].setA(param_midi[num]);
  if(num==22) env[1][current_synth].setD(param_midi[num]);
  if(num==23) env[1][current_synth].setS(param_midi[num]);
  if(num==24) env[1][current_synth].setR(param_midi[num]);
  if(num==25) env[2][current_synth].dest=param_midi[num]%16;
  if(num==26) env[2][current_synth].setA(param_midi[num]);
  if(num==27) env[2][current_synth].setD(param_midi[num]);
  if(num==28) env[2][current_synth].setS(param_midi[num]);
  if(num==29) env[2][current_synth].setR(param_midi[num]);
  if(num>=30 && num<=44)
  {
    int ind=(num-30)/5;
    int tes=(num-30)%5;
    if(tes == 0) lfo[ind][current_synth].dest=param_midi[num]%16;
    if(tes == 1) lfo[ind][current_synth].setWaveform(param_midi[num]%3);
    if(tes == 2) lfo[ind][current_synth].setfreq((float)param_midi[num]*(float)param_midi[num]/127);
    if(tes == 3) lfo[ind][current_synth].setvol(param_midi[num]);
    if(tes == 4) lfo[ind][current_synth].setmaincutoff(param_midi[num]);

  }
  if(num==45) Filter[current_synth].SetCutoff((float)param_midi[num],true);
  if(num==46) Filter[current_synth].SetResonance((float)param_midi[num],true);
  if(num==47) set_time(param_midi[num]);
  if(num==48) set_feedback(param_midi[num]);
  if(num==49) delay_mix=(float)param_midi[num]/127.0;
  if(num==50) oscA[0].update_polyphony((param_midi[num]%8)+1);
  if(num==51) oscA[0].setGlideTime(param_midi[num]);
  if(num==52) oscA[0].update_bpm(param_midi[num]);
  if(num==53) {oscA[0].arpmode=param_midi[num]%3; oscA[0].compute_notes();}
  if(num==54) oscA[0].arprate=param_midi[num]%16;
  if(num==55) oscA[0].update_gate(param_midi[num]);
  if(num==56) {oscA[0].arpstep=param_midi[num]%5; oscA[0].compute_notes();}
  if(num==57) oscA[0].unisson = (param_midi[num]%8)+1;
  if(num==58) oscA[0].compute_detune(param_midi[num]);
  if(num==59) oscA[0].compute_unisson_vol(param_midi[num]);
  if(num==60) disto[current_synth].set_gain(param_midi[num]);
  if(num==61) tempo_source=param_midi[num]%3;
  if(num==62) changlob=(param_midi[num]%8)+1;
  if(num==63) {if(param_midi[num]%2==0) ES8960_Init2();
                if(param_midi[num]%2==1) hp_spk();}
  if(num==64) savenum=param_midi[num]%16; 
  if(num==65) savenum=param_midi[num]%16;
  if(num==66) multi_mode=param_midi[num]%2;
  if(num==67) {savenum=param_midi[num]%8;
                load_preset();
                current_synth=0;
                init_synth_param();}
  if(num==68) {savenum=param_midi[num]%8;
                load_preset();
                current_synth=0;
                init_synth_param();}
  if(num==69) chansynth1=(param_midi[num]%8)+1;
  if(num==70) chansynth2=(param_midi[num]%8)+1;
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
  Serial.println("param_action_focus");
  Serial.println(num);
  if(num>=45 && num<=46) {
    for(int i=45; i<=46; i++) param_focus[i]=param_focus[num];
    
    if((param_focus[num]%4)==1) Filter[current_synth].SetMode(7);
    if((param_focus[num]%4)==2) Filter[current_synth].SetMode(9);
    if((param_focus[num]%4)==3) Filter[current_synth].SetMode(15);
    if((param_focus[num]%4)==0) filter_on[current_synth]=false;
    else filter_on[current_synth]=true;
  }
  if(num>=47 && num<=49) {
    delay_on=!delay_on;
    for(int i=47; i<=48; i++) param_focus[i]=delay_on;
  }
  if(num==60) {
    disto_on[current_synth]=!disto_on[current_synth];
    param_focus[num]=disto_on[current_synth];
  }
  Serial.println(filter_on[current_synth]);
  
  if(num==51) oscA[0].glideon=param_focus[num]%2;
  if(num>=52 && num<=56) {
    for(int i=52; i<=56; i++) param_focus[i]=param_focus[num];
    oscA[current_synth].arpon=param_focus[num]%2;
  }  
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
