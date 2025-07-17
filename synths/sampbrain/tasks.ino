void taskOther(void *parameter) {
  Serial.println("----------taskother");
  param_displayed=0;
  
  core0_init();
  Serial.println("----------1");
  Serial.println(disp.control_focus);
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
    
    if((coun%300)==0) disp.cpu_usage(cpuaudio);

    coun++;

    
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Pour éviter de surcharger le CPU
  }
}