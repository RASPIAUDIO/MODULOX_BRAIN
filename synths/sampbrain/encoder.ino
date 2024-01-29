void param_action(int num)
{
  Serial.print("param_action : ");
  Serial.println(num);
  if(num==1) smp.param_start_sample[param_focus[0]]=param_midi[num];
  if(num==2) smp.param_end_sample[param_focus[0]]=param_midi[num];
  if(num==3) smp.sample_index_inc[param_focus[0]]=1.0-(param_midi[num]-64)/127.0;
  if(num==4) chan_voice[param_focus[0]]=param_midi[num]%16;
  if(num==5) note_voice[param_focus[0]]=param_midi[num];
  if(num==6) disto.set_gain(param_midi[num]);
  if(num==7) filter.SetCutoff((float)param_midi[num],true);
  if(num==8) filter.SetResonance((float)param_midi[num],true);
  if(num==9) set_time(param_midi[num]);
  if(num==10) set_feedback(param_midi[num]);
  if(num==11) comp.threshold=(float)param_midi[num]/127.0;
  if(num==12) comp.set_attack(param_midi[num]);
  if(num==13) comp.set_release(param_midi[num]);
  if(num==14) comp.set_ratio(param_midi[num]);
  if(num==15) comp.set_type(param_midi[num]);
  //if(num == 11) {lfo.setvol(param_midi[num]); lfo_phase.setvol(param_midi[num]);}
  //if(num == 12) {lfo.setfreq((float)param_midi[num]*(float)param_midi[num]/127); lfo_phase.setfreq((float)param_midi[num]*(float)param_midi[num]/127);}
}

void param_action_focus(int num)
{
  for(int i=0; i<6; i++)
  {
    param_focus[i]=param_focus[num];
  }
  if(num>=7 && num<=8) {
    for(int i=7; i<=8; i++) param_focus[i]=param_focus[num];
    if((param_focus[num]%4)==1) filter.SetMode(7);
    if((param_focus[num]%4)==2) filter.SetMode(9);
    if((param_focus[num]%4)==3) filter.SetMode(15);
    if((param_focus[num]%4)==0) filter_on=false;
    else filter_on=true;
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
