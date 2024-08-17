void param_action(int num)
{
  Serial.print("param_action : ");
  Serial.println(num);
  if(num==1) smp.param_start_sample[param_focus[0]]=param_midi[num+7*param_focus[0]];
  if(num==2) smp.param_end_sample[param_focus[0]]=param_midi[num+7*param_focus[0]];
  //if(num==3) smp.tune(param_midi[num+7*param_focus[0]],param_focus[0]);
  if(num==4) chan_voice[param_focus[0]]=param_midi[num+7*param_focus[0]]%16;
  if(num==5) note_voice[param_focus[0]]=param_midi[num+7*param_focus[0]];
  if(num==6) vol_voice[param_focus[0]]=param_midi[num+7*param_focus[0]];
  if(num==7) disto.set_gain(param_midi[num+7*MAX_SAMPLE_NUM-7]);
  if(num==8) filter.SetCutoff((float)param_midi[num+7*MAX_SAMPLE_NUM-7],true);
  if(num==9) filter.SetResonance((float)param_midi[num+7*MAX_SAMPLE_NUM-7],true);
  if(num==10) set_time(param_midi[num+7*MAX_SAMPLE_NUM-7]);
  if(num==11) set_feedback(param_midi[num+7*MAX_SAMPLE_NUM-7]);
  if(num==12) comp.threshold=(float)param_midi[num+7*MAX_SAMPLE_NUM-7]/127.0;
  if(num==13) comp.set_attack(param_midi[num+7*MAX_SAMPLE_NUM-7]);
  if(num==14) comp.set_release(param_midi[num+7*MAX_SAMPLE_NUM-7]);
  if(num==15) comp.set_ratio(param_midi[num+7*MAX_SAMPLE_NUM-7]);
  if(num==18) {
                disp.clear();
                disp.draw_warning("LOAD");
                disp.display_window();
                disp.clear();
                if(param_midi[num+7*MAX_SAMPLE_NUM-7]%2==0) ES8960_Init2();
                if(param_midi[num+7*MAX_SAMPLE_NUM-7]%2==1) hp_spk();
                
              }
  if(num==19) savenum=param_midi[num+7*MAX_SAMPLE_NUM-7]; 
  if(num==20) savenum=param_midi[num+7*MAX_SAMPLE_NUM-7];
}

void param_action_focus(int num)
{
  Serial.println("param_action_focus");
  for(int i=0; i<7; i++)
  {
    param_focus[i]=param_focus[num];
  }
  if(num>=8 && num<=9) {
    for(int i=8; i<=9; i++) param_focus[i]=param_focus[num];
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
  if(enco_focus>1 )
  {
    if(param_focus[param_displayed]<=-sens) param_focus[param_displayed]=0;
    else param_focus[param_displayed]+=sens;
    if(param_focus[param_displayed]<0) param_focus[param_displayed]=0;
    if(param_focus[param_displayed]>param_focus_max) param_focus[param_displayed]=param_focus_max;
    param_action_focus(param_displayed);
    //display_menu();
    if(enco_focus==3) disp.valida=!disp.valida;
    else disp.display_list(param_focus[param_displayed],90,0,100);
  }
  if(enco_focus==0 && param_displayed<7)
  {
    Serial.println(param_midi[param_displayed+6*param_focus[0]]);
    if(sens<0) 
    {
       if(param_midi[param_displayed+7*param_focus[0]]>=-sens) param_midi[param_displayed+7*param_focus[0]]+=sens;
    }
    else
    {
      if((127-param_midi[param_displayed+7*param_focus[0]])>=sens) param_midi[param_displayed+7*param_focus[0]]+=sens;
    }
    Serial.println("param " + String(param_displayed+7*param_focus[0]) + " changed : " + String(param_midi[param_displayed+7*param_focus[0]]));
    param_action(param_displayed);
    display_param();
  }

  if(enco_focus==0 && param_displayed>=7)
  {
    Serial.println(param_midi[param_displayed+7*MAX_SAMPLE_NUM-7]);
    if(sens<0) 
    {
       if(param_midi[param_displayed+7*MAX_SAMPLE_NUM-7]>=-sens) param_midi[param_displayed+7*MAX_SAMPLE_NUM-7]+=sens;
    }
    else
    {
      if((127-param_midi[param_displayed+7*MAX_SAMPLE_NUM-7])>=sens) param_midi[param_displayed+7*MAX_SAMPLE_NUM-7]+=sens;
    }
    Serial.println("param " + String(param_displayed+7*MAX_SAMPLE_NUM-7) + " changed : " + String(param_midi[param_displayed+7*MAX_SAMPLE_NUM-7]));
    param_action(param_displayed);
    display_param();
  }
    
  //if((param_displayed != 2 || env_dest!=1)&&display_par) display_param();
  Serial.println("end change_enco");
}
