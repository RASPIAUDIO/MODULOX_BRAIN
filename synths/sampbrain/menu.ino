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
  if(param_displayed == 0) smp.load_file("/"+disp.display_fatfiles(param_midi[0]),param_focus[0]);
  if(param_displayed == 1) disp.draw_wave(sample, smp.sample_start_index[param_focus[0]], smp.sample_length[param_focus[0]], smp.param_start_sample[param_focus[0]], smp.param_end_sample[param_focus[0]]);
  if(param_displayed == 2) disp.draw_wave(sample, smp.sample_start_index[param_focus[0]], smp.sample_length[param_focus[0]], smp.param_start_sample[param_focus[0]], smp.param_end_sample[param_focus[0]]);
  if(param_displayed == 3) disp.draw_number_string_center(param_midi[param_displayed]-64, " tune");
  if(param_displayed == 4) disp.draw_string_number_center("Channel ", param_midi[param_displayed]%16);
  if(param_displayed == 5) 
  {
    if(param_midi[param_displayed]==0) disp.draw_string_center("Pitched");
    else disp.draw_string_number_center("Note ", param_midi[param_displayed]);
  }
  if(param_displayed == 6) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 7) disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed+1], param_focus[param_displayed]%4); 
  if(param_displayed == 8) disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed], param_focus[param_displayed]%4);
  if(param_displayed == 9) disp.draw_number_string_center(param_midi[param_displayed]*1000/64*param_midi[param_displayed]/127, " ms");
  if(param_displayed == 10) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 11) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 12) disp.draw_number_string_center(param_midi[param_displayed], " ms");
  if(param_displayed == 13) disp.draw_number_string_center(param_midi[param_displayed], " ms");
  if(param_displayed == 14) disp.draw_number_string_center(param_midi[param_displayed], " dB");
  if(param_displayed == 15) disp.draw_number_string_center(param_midi[param_displayed], "");
  
  if(param_displayed == 17) 
  {
    if(param_focus[17]==0)
    {
      //String wifi_disp = WiFi.localIP().toString();
      disp.draw_centered(20, "choose wifi on modulox website", SMALLCHAR);
      disp.draw_centered(40, "choose firmware et click on 'set Wifi'", SMALLCHAR);
    }
    if(param_focus[17]==1)
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
    if(param_focus[17]==2)
    {
      disp.draw_string_center("audio reactivated");
    }
  }
  disp.display_window();
}
