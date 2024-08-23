void display_menu()
{
  Serial.println("display_menu");
  if(enco_focus>0) disp.menu_top(param_displayed, 0x5ACF); 
  else disp.menu_top(param_displayed);
  disp.display_top();
  Serial.println("background");
  //compute_filter_curve();
  disp.menu_bottom(param_displayed,param_focus[param_displayed]);
  disp.display_bottom();
}

void display_param()
{
  disp.clear();
  
  if(param_displayed == 0) disp.draw_string_number_center("User ", savenum);
  //if(param_displayed == 1) disp.draw_waveform_osc(oscA,0);
  if(param_displayed >= 2 && param_displayed <= 10)
  {
    for(int i=0; i<9; i++) disp.drawBmpParam("/tirette1.bmp",10+i*28, param_midi[i+2]-100);
  }
    
  //if(param_displayed >= 2 && param_displayed <= 10) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 11) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 12) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 18) 
  {
    if(param_focus[param_displayed]==0)
    {
      //String wifi_disp = WiFi.localIP().toString();
      disp.draw_centered(20, "choose wifi on modulox website", SMALLCHAR);
      disp.draw_centered(40, "choose firmware et click on 'set Wifi'", SMALLCHAR);
    }
    if(param_focus[param_displayed]==1)
    {
      if(wifi_connected)
      {
        String wifi_disp = "Go to " + WiFi.localIP().toString() + ":" + (String)filemanagerport + "/ on your browser";
        disp.draw_centered(40, wifi_disp, SMALLCHAR);
      }
      else
      {
        disp.draw_centered(40, "Connect to Wifi first", SMALLCHAR);
      }
    }
    if(param_focus[param_displayed]==2)
    {
      disp.draw_centered(60, "Wifi Off - Audio On", SMALLCHAR);
    }
  }
  disp.display_window();
  
  
}
