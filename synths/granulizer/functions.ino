String dest_list[]={"None","Grain Size","Grain dens","Speed","Cutoff"};


void but_mid_pressed()
{
  

}

void enco_pressed()
{
  playglb=false;
  
  if(!record) {sample_index=0; record=true;}
  granulizer.trace=0;
  playglb=true;
}

void enco_released()
{
  playglb=false;
  if(record) 
  {
    granulizer.sample_start_index[0]=5000;
    granulizer.sample_end_index[0]=sample_index-5000;
    granulizer.sample_length[0]=sample_index-10000;
    granulizer.init_grain();
    granulizer.sample_pos[0]=0;
    //for (int i = 0; i < MAX_GRAIN_NUM; i++) {grains[i].env=0; grains[i].active=false; grains[i].position=0;}
    granulizer.launch(0, 127);
    Serial.println(granulizer.sample_start_index[0]);
    Serial.println(granulizer.sample_end_index[0]);
    Serial.println("sample");
    /*for(int i=0; i<sample_index; i++)
    {
      Serial.println(sample[i]);
    }*/
    sample_index=0;
    record=false;
  }
  granulizer.trace=0;
  
  playglb=true;
 
}

void param_action(int num)
{
  Serial.print("param_action : ");
  Serial.println(num);
  if(num == 1) {String sfile = "/sample" + String(param_midi[num]) + ".wav"; granulizer.load_file(sfile, 0);}
  if(num == 2) granulizer.change_start(param_midi[num]);
  if(num == 3) granulizer.change_end(param_midi[num]);
  if(num == 4) granulizer.size_change(param_midi[num]);
  if(num == 5) granulizer.env_change(param_midi[num]);
  if(num == 6) granulizer.density_change(param_midi[num]);
  if(num == 7) granulizer.rate_change(param_midi[num]);
  if(num == 8) granulizer.sample_rate_change(param_midi[num]);
  if(num == 9) granulizer.change_random_start_grain(param_midi[num]);
  if(num==10) set_time(param_midi[num]);
  if(num==11) set_feedback(param_midi[num]);
  if(num==12) filter.SetCutoff((float)param_midi[num],true);
  if(num==13) filter.SetResonance((float)param_midi[num],true);
  if(num==14) lfo.dest=param_midi[num]%16;
  if(num==15) lfo.setWaveform(param_midi[num]%3);
  if(num==16) lfo.setfreq((float)param_midi[num]*(float)param_midi[num]/127);
  if(num==17) lfo.setvol(param_midi[num]);
  if(num==18) lfo.setmaincutoff(param_midi[num]);
  if(num==19) granulizer.setenvA(param_midi[num]);
  if(num==20) granulizer.setenvD(param_midi[num]);
  if(num==21) granulizer.setenvS(param_midi[num]);
  if(num==22) granulizer.setenvR(param_midi[num]);

}

void param_action_focus(int num)
{
  
}

void display_param()
{
  disp.clear();
  
  if(param_displayed == 0) disp.draw_string_number_center("User ", savenum);
  if(param_displayed == 1) disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[param_displayed+1], 127-param_midi[param_displayed+2]);
  if(param_displayed == 2) disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[param_displayed], 127-param_midi[param_displayed+1]);
  if(param_displayed == 3) disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[param_displayed-1], 127-param_midi[param_displayed]);
  if(param_displayed == 4) disp.draw_number_string_center(((int)param_midi[param_displayed]*(int)param_midi[param_displayed]+50)/44, " ms");
  if(param_displayed == 5) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 6) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 7) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 8) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 9) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 10) disp.draw_number_string_center(param_midi[param_displayed]*1000/64*param_midi[param_displayed]/127, " ms");
  if(param_displayed == 11) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 12) disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed+1], 1); 
  if(param_displayed == 13) disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed], 1);
  if(param_displayed == 14) disp.draw_string_center(dest_list[param_midi[param_displayed]%16]);
  if(param_displayed == 15) disp.draw_waveform_lfo(lfo.wave, lfo.lfovol, lfo.maincutoff);
  if(param_displayed == 16) disp.draw_number_string_center(param_midi[param_displayed]*param_midi[param_displayed]*0.0034724, " Hz");
  if(param_displayed == 17) disp.draw_waveform_lfo(lfo.wave, lfo.lfovol, lfo.maincutoff);
  if(param_displayed == 18) disp.draw_waveform_lfo(lfo.wave, lfo.lfovol, lfo.maincutoff);
  if(param_displayed == 19) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 20) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 21) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 22) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");

  disp.display_window();
  
}

inline void Midi_NoteOn(uint8_t ch, uint8_t note, uint8_t vol)
{
  //Serial.println("Midi_NoteOn");
  granulizer.noteOn(note);
}

inline void Midi_NoteOff(uint8_t ch, uint8_t note)
{
  //Serial.println("Midi_NoteOff");
  granulizer.noteOff(note);
}

inline void Midi_ControlChange(uint8_t ch, uint8_t note, uint8_t val)
{
  //Serial.println("Midi_CC");
}

void Synth_Init()
{
  param_midi[3]=127;
  granulizer.init();
  granulizer.load_file("/sample1.wav", 0);
  granulizer.change_end(127);
  granulizer.launch(0,100);
  delay_init();
  filter.Init((float)SAMPLE_RATE);
  filter.SetMode(TeeBeeFilter::LP_24);
  filter.SetResonance( 5,true);
  lfo.init();
  lfo.setmaincutoff(64);
  lfo.setvol(8);
  lfo.start(); 

}

inline void Synth_Process(int16_t *left, int16_t *right)
{

}


