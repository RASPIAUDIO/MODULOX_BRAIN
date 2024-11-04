int bpm_rect=10;

int count5=0;

void Core0Task(void *parameter)
{
  
  Serial.println("core 0");
  Serial.println(digitalRead(0));
  //Midi_Setup();

  core0_init();

  lfo[0][current_synth].start();
  lfo[1][current_synth].start();
  lfo[2][current_synth].start();

  while (1)
  {
    //Midi_Process();
    Sync_Process();
    //if(arp.play) arp.update();

    
    if(bpm_rect<50) bpm_rect++;
    if(bpm_rect==50) {disp.tempo_on(); Serial.println("end tempo"); bpm_rect++;}
    

    enco_turned(); 
    button_pressed();
    /*if(param_displayed == 1 && (env_dest==1||param_focus[3]==1) && display_wave) {display_param(); display_wave=false;}
    if(param_displayed == 2 && (env_dest==1||param_focus[3]==1) && display_wave) {display_param(); display_wave=false;}
    if(param_displayed == 40 && (param_focus[3]==4) && display_wave) {display_param(); display_wave=false;}
    if(param_displayed == 41 && (param_focus[3]==4) && display_wave) {display_param(); display_wave=false;}*/

    count5++;
    if(count5>200)
    {
      count5=0;
      disp.cpu_usage(rec2);
      
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
