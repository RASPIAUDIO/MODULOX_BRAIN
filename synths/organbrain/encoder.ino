void param_action(int num)
{
  Serial.print("param_action : ");
  Serial.println(num);
  if(num == 1) oscA.setWaveform(param_midi[num]%64);
  if(num == 2) oscA.setVolOsc(0, param_midi[num]);
  if(num == 3) oscA.setVolOsc(1, param_midi[num]);
  if(num == 4) oscA.setVolOsc(2, param_midi[num]);
  if(num == 5) oscA.setVolOsc(3, param_midi[num]);
  if(num == 6) oscA.setVolOsc(4, param_midi[num]);
  if(num == 7) oscA.setVolOsc(5, param_midi[num]);
  if(num == 8) oscA.setVolOsc(6, param_midi[num]);
  if(num == 9) oscA.setVolOsc(7, param_midi[num]);
  if(num == 10) oscA.setVolOsc(8, param_midi[num]);
  if(num == 11) {lfo.setvol(param_midi[num]); lfo_phase.setvol(param_midi[num]);}
  if(num == 12) {lfo.setfreq((float)param_midi[num]*(float)param_midi[num]/127); lfo_phase.setfreq((float)param_midi[num]*(float)param_midi[num]/127);}
}

void param_action_focus(int num)
{
  
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
