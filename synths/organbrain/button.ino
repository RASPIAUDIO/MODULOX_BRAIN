void but_mid_pressed()
{
  if(param_displayed==25 && enco_focus==3) 
  {
    audio_start=false; 
    delay(200);
    load_preset();
    delay(200);
    audio_start=true;
  }
  if(param_displayed==26 && enco_focus==3) save_preset();
  if(param_displayed>=20 && param_displayed<=22) {
    param_action_focus(param_displayed); 
    disp.menu_bottom(param_displayed,(int)delay_on);      
  }
  if(param_displayed==19) {
    param_action_focus(param_displayed); 
    disp.menu_bottom(param_displayed,(int)disto_on);      
  }
}

void enco_pressed()
{

}
