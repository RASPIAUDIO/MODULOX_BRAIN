int count5=0;

void Core0Task(void *parameter)
{
Serial.println("core 0");

  core0_init();

  disp.clear();
  disp.draw_warning("LOAD");
  disp.display_window();
  disp.clear();

  savenum=1;
  load_preset_sampler();
  param_focus[0]=0;

  display_menu();
  display_param();

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

    if(param_displayed == 15) 
    {
      disp.display_compressor(comp.cmax, comp.duty);
      disp.display_window();
    }
    
    
    delay(1);
  }
}
