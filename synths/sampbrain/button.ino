void but_mid_pressed()
{
  Serial.println("but_mid_pressed");
  if(param_displayed==6) {
    disto_on=!disto_on;
    param_focus[param_displayed]=disto_on;
    disp.menu_bottom(param_displayed,(int)disto_on);    
  }
  if(param_displayed>=9 && param_displayed<=10) {
    delay_on=!delay_on;
    for(int i=9; i<=10; i++) param_focus[i]=delay_on;
    disp.menu_bottom(param_displayed,(int)delay_on);    
  }
  if(param_displayed>=11 && param_displayed<=15) {
    comp_on=!comp_on;
    for(int i=11; i<=15; i++) param_focus[i]=comp_on;
    disp.menu_bottom(param_displayed,(int)comp_on);    
  }
  if(param_displayed==16) {
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
        
      }
      if(param_focus[param_displayed]==2)
      {
        i2s_start(i2s_num);
      }
    }
  }
  if(param_displayed==18 ) {
    disp.clear();
    disp.draw_warning("LOAD");
    disp.display_window();
    disp.clear();
    load_preset_sampler();
    display_param();
  }
  if(param_displayed==19 ) {
    disp.clear();
    disp.draw_warning("SAVE");
    disp.display_window();
    disp.clear();
    save_preset_sampler();
    display_param();
  }
}
