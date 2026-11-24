int count5=0;

void Core0Task(void *parameter)
{
Serial.println("core 0");

  core0_init();
  

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
