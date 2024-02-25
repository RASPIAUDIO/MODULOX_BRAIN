String audio_list[]={"Headphone","Speaker"};

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
  Serial.println("display_param");
  disp.clear();
  if(param_displayed == 0) {
    disp.draw_warning("LOAD");
    disp.display_window();
    disp.clear();
    smp.load_file("/"+disp.display_fatfiles(param_midi[param_displayed+6*param_focus[0]]),param_focus[0],param_midi[param_displayed+6*param_focus[0]]);  
    smp.sample_launch(param_focus[0], 64);
  }

  
  if(param_displayed == 1) disp.draw_wave(sample, smp.sample_start_index[param_focus[0]], smp.sample_length[param_focus[0]], smp.param_start_sample[param_focus[0]], smp.param_end_sample[param_focus[0]]);
  if(param_displayed == 2) disp.draw_wave(sample, smp.sample_start_index[param_focus[0]], smp.sample_length[param_focus[0]], smp.param_start_sample[param_focus[0]], smp.param_end_sample[param_focus[0]]);
  if(param_displayed == 3) disp.draw_number_string_center(param_midi[param_displayed+6*param_focus[0]]-64, " semi");
  if(param_displayed == 4) disp.draw_string_number_center("Channel ", param_midi[param_displayed+6*param_focus[0]]%16);
  if(param_displayed == 5) 
  {
    if(note_voice[param_focus[0]]==0) disp.draw_string_center("Pitched");
    else disp.draw_string_number_center("Note ", note_voice[param_focus[0]]);
  }
  if(param_displayed == 6) disp.draw_number_string_center(param_midi[param_displayed+6*MAX_SAMPLE_NUM-6]*100/127, " %");
  if(param_displayed == 7) disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed+6*MAX_SAMPLE_NUM-6+1], param_focus[param_displayed]%4); 
  if(param_displayed == 8) disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed+6*MAX_SAMPLE_NUM-6], param_focus[param_displayed]%4);
  if(param_displayed == 9) disp.draw_number_string_center(param_midi[param_displayed+6*MAX_SAMPLE_NUM-6]*1000/64*param_midi[param_displayed+6*MAX_SAMPLE_NUM-6]/127, " ms");
  if(param_displayed == 10) disp.draw_number_string_center(param_midi[param_displayed+6*MAX_SAMPLE_NUM-6]*100/127, " %");
  if(param_displayed == 11) disp.draw_number_string_center(param_midi[param_displayed+6*MAX_SAMPLE_NUM-6]*100/127, " %");
  if(param_displayed == 12) disp.draw_number_string_center(param_midi[param_displayed+6*MAX_SAMPLE_NUM-6], " ms");
  if(param_displayed == 13) disp.draw_number_string_center(param_midi[param_displayed+6*MAX_SAMPLE_NUM-6], " ms");
  if(param_displayed == 14) disp.draw_number_string_center(param_midi[param_displayed+6*MAX_SAMPLE_NUM-6], " dB");
  
  if(param_displayed == 16) 
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
  if(param_displayed == 17) disp.draw_string_center(audio_list[param_midi[param_displayed+6*MAX_SAMPLE_NUM-6]%2]);
  if(param_displayed == 18) disp.draw_string_number_center("User ", savenum);
  if(param_displayed == 19) disp.draw_string_number_center("User ", savenum);
  disp.display_window();
}
