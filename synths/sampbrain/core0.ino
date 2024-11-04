int count5=0;

void Core0Task(void *parameter)
{
Serial.println("core 0");

  core0_init();

  disp.clear();
  disp.draw_warning("LOAD");
  disp.display_window();
  disp.clear();

  //savenum=1;
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

    if(param_displayed == 16) 
    {
      disp.display_compressor(comp.cmax, comp.duty);
      disp.display_window();
    }

    if(midi_changed)
    {
      if(midi_learn)
      {
        Serial.println("midi learning");
        //dans Midi_ControlChange
        disp.midi_learned(midi_cc_val[param_displayed]);
        disp.display_window();
        Serial.println("end CC");
        midi_learn=false;
      }
      else
      {
        //dans change_CC
        param_action(param_CC);
        if(param_displayed==param_CC) display_param();
      }
      midi_changed=false;
    }
    
    delay(1);
  }
}
