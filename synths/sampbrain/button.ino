void but_mid_pressed()
{
  if(param_displayed==6) {
    disto_on=!disto_on;
    disp.menu_bottom(param_displayed,(int)disto_on);    
  }
  if(param_displayed>=9 && param_displayed<=10) {
    delay_on=!delay_on;
    disp.menu_bottom(param_displayed,(int)delay_on);    
  }
  if(param_displayed==15) {
    if(previous_wifi_mode != param_focus[17]) 
    {
      previous_wifi_mode = param_focus[17];
      if(param_focus[17]==0)
      {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
      
        improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "ImprovWiFiLib", "1.0.0", "BasicWebServer", "http://{LOCAL_IPV4}?name=Guest");
        improvSerial.onImprovError(onImprovWiFiErrorCb);
        improvSerial.onImprovConnected(onImprovWiFiConnectedCb);
        improvSerial.setCustomConnectWiFi(connectWifi);  // Optional
      }
      if(param_focus[17]==1)
      {
        i2s_stop(i2s_num);
        if(setup_wifi()) {wifi_connected=true; Serial.println("connected");}
        else {wifi_connected=false; Serial.println("not connected");}
        
      }
      if(param_focus[17]==2)
      {
        i2s_start(i2s_num);
      }
    }
  }
}
