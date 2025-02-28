void taskOther(void *parameter) {
  core0_init();
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

    if((coun%300)==0) disp.cpu_usage(cpuaudio);

    coun++;

    
    vTaskDelay(1 / portTICK_PERIOD_MS);  // Pour éviter de surcharger le CPU
  }
}