void taskOther(void *parameter) {
  Serial.println("----------taskother");
  param_displayed=0;
  
  core0_init();
  Serial.println("----------1");
  Serial.println(disp.control_focus);
  lfo[0][current_synth].start();
  lfo[1][current_synth].start();
  lfo[2][current_synth].start();
  //disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[0], 127-param_midi[0]);
  disp.load_all();
  disp.control_focus=0;
  Serial.println("----------2");
  Serial.println(disp.control_focus);
  disp.load_screen(0);
  Serial.println("----------3");
  Serial.println(disp.control_focus);
  load_window(0);
  Serial.println("----------4");
  Serial.println(disp.control_focus);
  disp.display();
  disp.display_controllers();
  Serial.println("----------5");
  Serial.println(disp.control_focus);
  disp.display_wave();
  Serial.println("taskOther");
    
  int coun=0;
  while (1) {
    uint32_t start = micros();
    Sync_Process();
    button_pressed();
    core0_process();

    uint32_t end = micros();
    midiCpuTime += (end - start);

    /*if((param_displayed == 6 || param_displayed == 7) && (coun%20)==0 && enco_focus==0)
    {
      disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[1], param_midi[2]);
      disp.draw_bar(granulizer.sample_pos[0]/granulizer.sample_length[0]*260);
      disp.display_wave();
    }*/
    
    if((coun%300)==0) disp.cpu_usage(cpuaudio);

    coun++;

    
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Pour éviter de surcharger le CPU
  }
}