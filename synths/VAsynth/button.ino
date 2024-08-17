void enco_pressed()
{
  if(param_displayed != 0 && param_displayed!=64 && param_displayed!=65) learn_midi();
}

void but_mid_pressed()
{
  Serial.println("but mid");
  if(param_displayed==64 && enco_focus==3) 
  {
    audio_start=false; 
    delay(200);
    load_preset();
    delay(200);
    audio_start=true;
  }
  if(param_displayed==65 && enco_focus==3) save_preset();
  if(param_displayed>=47 && param_displayed<=49) {
    param_action_focus(param_displayed); 
    disp.menu_bottom(param_displayed,(int)delay_on);      
  }
  if(param_displayed==60) {
    param_action_focus(param_displayed);  
    disp.menu_bottom(param_displayed,(int)disto_on[current_synth]);     
  }
}
