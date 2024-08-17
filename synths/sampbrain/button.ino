void enco_pressed()
{
  Serial.println("enco_pressed");
  if(param_displayed == 0) {
    disp.draw_warning("LOAD");
    disp.display_window();
    disp.clear();
    String st = "/"+disp.display_fatfiles(param_midi[param_displayed+7*param_focus[0]]);
    Serial.println("name : ");
    Serial.println(st);
    Serial.println(param_midi[param_displayed+7*param_focus[0]]);
    if(!smp.load_file(st,param_focus[0],param_midi[param_displayed+7*param_focus[0]])) 
    {
      disp.draw_warning("Not enough memory");
      disp.display_window();  
    }
    smp.sample_launch_raw(param_focus[0], 64);
    display_param();
  }
  else
  {
    learn_midi();
  }
}


void but_mid_pressed()
{
  Serial.println("but_mid_pressed");
  if(param_displayed==7) {
    disto_on=!disto_on;
    param_focus[param_displayed]=disto_on;
    disp.menu_bottom(param_displayed,(int)disto_on);    
  }
  if(param_displayed>=10 && param_displayed<=11) {
    delay_on=!delay_on;
    for(int i=10; i<=11; i++) param_focus[i]=delay_on;
    disp.menu_bottom(param_displayed,(int)delay_on);    
  }
  if(param_displayed>=12 && param_displayed<=15) {
    comp_on=!comp_on;
    for(int i=12; i<=16; i++) param_focus[i]=comp_on;
    disp.menu_bottom(param_displayed,(int)comp_on);    
  }
  if(param_displayed==17) {
    if(previous_wifi_mode != param_focus[param_displayed]) 
    {
      previous_wifi_mode = param_focus[param_displayed];
      if(param_focus[param_displayed]==0)
      {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
      
        improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "ImprovWiFiLib", "1.0.0", "BasicWebServer", "http://{LOCAL_IPV4}?name=Guest");
        improvSerial.onImprovError(onImprovWiFiErrorCb);
        improvSerial.onImprovConnected(onImprovWiFiConnectedCb);
        improvSerial.setCustomConnectWiFi(connectWifi);  // Optional
      }
      if(param_focus[param_displayed]==1)
      {
        i2s_stop(i2s_num);
        if(setup_wifi()) {wifi_connected=true; Serial.println("connected");}
        else {wifi_connected=false; Serial.println("not connected");}
        delay(500);
        
      }
      if(param_focus[param_displayed]==2)
      {
        i2s_start(i2s_num);
      }
    }
  }
  if(param_displayed==19 ) {
    disp.clear();
    disp.draw_warning("LOAD");
    disp.display_window();
    disp.clear();
    load_preset_sampler();
    display_param();
  }
  if(param_displayed==20 ) {
    disp.clear();
    disp.draw_warning("SAVE");
    disp.display_window();
    disp.clear();
    save_preset_sampler();
    display_param();
  }
}
