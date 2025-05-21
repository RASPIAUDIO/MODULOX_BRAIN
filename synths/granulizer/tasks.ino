void taskOther(void *parameter) {
  
  core0_init();
  disp.load_all();
  //disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[0], 127-param_midi[0]);
  disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], 0, 0);
  
  disp.load_screen(0);
  disp.display();
  disp.display_controllers();
  Serial.println("taskOther");
    
  int coun=0;
  while (1) {
    uint32_t start = micros();
    Sync_Process();
    enco_turned(); 
    button_pressed();
    //Handle USB communication
    USB_Midi_Process();

    uint32_t end = micros();
    midiCpuTime += (end - start);

    if((param_displayed == 6 || param_displayed == 7) && (coun%30)==0 && enco_focus==0 &&!midi_learn)
    {
      disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[1], param_midi[2]);
      disp.draw_bar(granulizer.sample_pos[0]/granulizer.sample_length[0]*260);
      disp.display_wave();
    }
    if((param_displayed == 11 || param_displayed == 12 || param_displayed == 13) && (coun%60)==0 && enco_focus==0 &&!midi_learn)
    {
      disp.draw_filter(filter.GetCutoff(), param_midi[13], filt_mode, filt_pente, filt_pente2);
      disp.display_wave();
    }
    if((param_displayed == 3 || param_displayed == 4 || param_displayed == 5) && (coun%100)==0 && enco_focus==0 &&!midi_learn)
    {
      disp.draw_grain(20, param_midi[3], param_midi[4], param_midi[5]);
      disp.display_wave();
    }

    
    if((coun%300)==0) disp.cpu_usage(cpuaudio);

    coun++;

    
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Pour éviter de surcharger le CPU
  }
}