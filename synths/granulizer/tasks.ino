void taskOther(void *parameter) {
  
  core0_init();
  //disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[0], 127-param_midi[0]);
  disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], 0, 0);
  disp.load_all();
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

    if((param_displayed == 6 || param_displayed == 7) && (coun%20)==0 && enco_focus==0)
    {
      disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[1], param_midi[2]);
      disp.draw_bar(granulizer.sample_pos[0]/granulizer.sample_length[0]*260);
      disp.display_wave();
    }
    
    if((coun%300)==0) disp.cpu_usage(cpuaudio);

    coun++;

    
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Pour éviter de surcharger le CPU
  }
}