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
    
    delay(1);
  }
}
