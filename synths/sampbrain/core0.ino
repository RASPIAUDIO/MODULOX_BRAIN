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

    //disp.display_compressor(comp.maxmax, (comp.env_value-comp.ratio_conv*comp.env_value));
    
    //disp.display_window();
    //if(param_displayed == 16) disp.display_compressor(comp.maxmax, comp.env_value);
    //yield();
    delay(1);
  }
}
