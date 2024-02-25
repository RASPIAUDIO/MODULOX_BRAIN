void param_action(int num)
{
  Serial.print("param_action : ");
  if(num==1) FmSynth_SetAlgorithm(param_midi[num]%8, 1);
  if(num==2) FmSynth_VelToLev(0, (float)param_midi[num]/127.0);
  if(num==3) FmSynth_Attack(0, (float)param_midi[num]/127.0);
  if(num==4) FmSynth_Decay1(0, (float)param_midi[num]/127.0);
  if(num==5) FmSynth_Decay2(0, (float)param_midi[num]/127.0);
  if(num==6) FmSynth_DecayL(0, (float)param_midi[num]/127.0);
  if(num==7) FmSynth_Release(0, (float)param_midi[num]/127.0);
  if(num==8) FmSynth_ChangeParam(0, (float)param_midi[num]/127.0);
  if(num==9) FmSynth_Feedback(0, (float)param_midi[num]/127.0);
  if(num==10) FmSynth_ChangeParam(1, (float)param_midi[num]/127.0);
  if(num==11) FmSynth_ChangeParam(2, (float)param_midi[num]/127.0);
  Serial.println(num);

}

void param_action_focus(int num)
{
  if(num>=2 && num<=11) {
    for(int i=2; i<=11; i++) param_focus[i]=param_focus[num];
    if((param_focus[num]%4)>0) FmSynth_SelectOp(param_focus[num]%4, 1);
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
  Serial.println("end change_enco");
}
