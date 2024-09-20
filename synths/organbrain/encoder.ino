void param_action(int num)
{
  Serial.print("param_action : ");
  Serial.println(num);
  if(num==0) {
    savenum=param_midi[num]; 
    audio_start=false; 
    delay(200);
    exclude_load=false;
    load_preset();
    delay(200);
    exclude_load=true;
    audio_start=true;
  }
  if(num == 1) oscA.setWaveform(param_midi[num]%4);
  if(num == 2) oscA.setVolOsc(0, param_midi[num]);
  if(num == 3) oscA.setVolOsc(1, param_midi[num]);
  if(num == 4) oscA.setVolOsc(2, param_midi[num]);
  if(num == 5) oscA.setVolOsc(3, param_midi[num]);
  if(num == 6) oscA.setVolOsc(4, param_midi[num]);
  if(num == 7) oscA.setVolOsc(5, param_midi[num]);
  if(num == 8) oscA.setVolOsc(6, param_midi[num]);
  if(num == 9) oscA.setVolOsc(7, param_midi[num]);
  if(num == 10) oscA.setVolOsc(8, param_midi[num]);
  if(num == 11) {lfo.setvol(param_midi[num]/16.0); lfo_phase.setvol(param_midi[num]/16.0);}
  if(num == 12) {lfo.setfreq((float)param_midi[num]*(float)param_midi[num]/127); lfo_phase.setfreq((float)param_midi[num]*(float)param_midi[num]/127);}
  if(num == 13) oscA.setenvA(param_midi[num]);
  if(num == 14) oscA.setenvD(param_midi[num]);
  if(num == 15) oscA.setenvS(param_midi[num]);
  if(num == 16) oscA.setenvR(param_midi[num]);
  if(num == 17) oscA.setenvAacc(param_midi[num]);
  if(num == 18) oscA.setvolacc(param_midi[num]);
  if(num == 19) disto.set_gain(param_midi[num]+1);
  if(num==20) set_time(param_midi[num]);
  if(num==21) set_feedback(param_midi[num]);
  if(num==22) delay_mix=(float)param_midi[num]/127.0;
  if(num==23) changlob=param_midi[num]%8;
  if(num==24 && exclude_load) {if(param_midi[num]%2==0) ES8960_Init2();
                if(param_midi[num]%2==1) hp_spk();}
  if(num==25 && exclude_load) savenum=param_midi[num]%16; 
  if(num==26 && exclude_load) savenum=param_midi[num]%16;
}

void param_action_focus(int num)
{
  if(num>=20 && num<=22) {
    delay_on=!delay_on;
    for(int i=20; i<=22; i++) param_focus[i]=delay_on;
  }
  if(num==19) {
    disto_on=!disto_on;
    param_focus[19]=disto_on;
  }
}

void change_enco(int sens)
{
  Serial.println("change_enco");
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
    disp.display_window();
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
