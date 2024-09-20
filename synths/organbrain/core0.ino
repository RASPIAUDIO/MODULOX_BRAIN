int count5=0;

void Core0Task(void *parameter)
{
Serial.println("core 0");
Serial.println(digitalRead(0));

  core0_init();
  disp.init_buffer(22, 142);

  savenum=1;
  param_midi[0]=1;
  audio_start=false; 
  delay(50);
  load_preset();
  delay(200);
  audio_start=true;

  while (1)
  {
    
    Sync_Process();
    enco_turned(); 
    button_pressed();
    count5++;
    if(count5>200)
    {
      count5=0;
      disp.cpu_usage(rec2);
      
    }
    
    delay(1);
  }
}
