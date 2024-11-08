void taskOther(void *parameter) {
  core0_init();
  Serial.println("taskOther");
    
  
  while (1) {
    Sync_Process();
    enco_turned(); 
    button_pressed();
    //Handle USB communication

    
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Pour éviter de surcharger le CPU
  }
}