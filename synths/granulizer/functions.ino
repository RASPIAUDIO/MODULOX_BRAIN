String dest_list[]={"None","Grain Size","Grain dens","Speed","Cutoff"};
int filt_mode=1;
int filt_pente=30;
int filt_pente2=30;

void but_record()
{

    Serial.println("rec");
    playglb=false;
    if(!record) 
    {
      disp.draw_warning("press enco to stop rec");
      disp.display_wave();
      sample_index=0; 
      maxaudio=0;
      record=true;
    }
    else 
    {
      granulizer.sample_start_index[0]=0;
      granulizer.sample_end_index[0]=sample_index;
      granulizer.sample_length[0]=sample_index;
      granulizer.init_grain();
      granulizer.sample_pos[0]=0;
      //for (int i = 0; i < MAX_GRAIN_NUM; i++) {grains[i].env=0; grains[i].active=false; grains[i].position=0;}
      //granulizer.launch(0, 127);
      Serial.println(granulizer.sample_start_index[0]);
      Serial.println(granulizer.sample_end_index[0]);
      Serial.println("sample");
      for(int i=2000; i<sample_index-2000; i++)
      {
        if(maxaudio<sample[i]) {maxaudio=sample[i]; lastind=i;} 
      }
      
      Serial.println("maxaudio : ");
      Serial.println(maxaudio);
      Serial.println(lastind);

      for(int i=lastind-50; i<lastind+50; i++)
      {
        Serial.println(sample[i]);
      }

      float normalize=65000.0/(float)maxaudio;
      for(int i=0; i<sample_index; i++)
      {
        sample[i]*=normalize;
      }
      Serial.println(normalize);

      

      sample_index=0;
      record=false;

      display_window(param_displayed);
    }
   
    granulizer.trace=0;
    playglb=true;
}

void but_mid_pressed()
{
  Serial.println("but mid pressed");
  Serial.println(param_displayed);
  if(param_displayed==24) 
  {
    disp.draw_warning("saving...");
    disp.display_wave();
    delay(500);
    save_preset();
    load_window(9);
    display_window(24);
  }
  if(param_displayed==23) 
  {
    disp.draw_warning("loading...");
    disp.display_wave();
    delay(500);
    load_preset();
    load_window(9);
    display_window(23);
  }
  if(param_displayed != 0 && param_displayed!=24 && param_displayed!=23 && enco_focus==0) learn_midi();
}

void enco_pressed()
{
  //if(param_displayed != 0 && param_displayed!=1 && param_displayed!=24) learn_midi();
  but_record();
}

void enco_released()
{
  //codec.enableSpeakers();
 
}

void load_window(int screen_n, bool init_s)
{
  Serial.print("load_window : ");
  Serial.println(screen_n);
  if(screen_n==0)
  {
    if(init_s) param_displayed=0;
    disp.controller_val[0]=param_midi[0];
    disp.controller_val[1]=param_midi[1];
    disp.controller_val[2]=param_midi[2];
    disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[1], param_midi[2]);
  }
  if(screen_n==1)
  {
    if(init_s) param_displayed=3;
    disp.controller_val[0]=param_midi[3];
    disp.controller_val[1]=param_midi[4];
    disp.controller_val[2]=param_midi[5];
    disp.draw_grain(20, param_midi[3], param_midi[4], param_midi[5]);
  }
  if(screen_n==2)
  {
    if(init_s) param_displayed=6;
    disp.controller_val[0]=param_midi[6];
    disp.controller_val[1]=param_midi[7];
    disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[1], param_midi[2]);
  }
  if(screen_n==3)
  {
    if(init_s) param_displayed=8;
    disp.controller_val[0]=param_midi[8];
    disp.controller_val[1]=param_midi[9];
    disp.controller_val[2]=param_midi[10];

    disp.init_window();
    String aff = String(param_midi[8]*1000/64*param_midi[8]/127);
    aff = aff+" ms";
    disp.load_param(0, aff);

    aff = String(param_midi[9]*100/127);
    aff = aff+" %";
    disp.load_param(1, aff);

    aff = String(param_midi[10]*100/127);
    aff = aff+" %";
    disp.load_param(2, aff);
  }
  if(screen_n==4)
  {
    if(init_s) param_displayed=11;
    disp.controller_val[0]=param_midi[11];
    disp.controller_val[1]=param_midi[12];
    disp.controller_val[2]=param_midi[13];
    disp.init_window();
    disp.draw_filter(filter.GetCutoff(), param_midi[13], filt_mode, filt_pente, filt_pente2);
  }
  if(screen_n==5)
  {
    if(init_s) param_displayed=14;
    disp.controller_val[0]=param_midi[14];
    disp.controller_val[1]=param_midi[15];
    disp.init_window();
    disp.draw_waveform_lfo(lfo.wave, lfo.lfovol, lfo.maincutoff);
  }
  if(screen_n==6) 
  {
    if(init_s) param_displayed=16;
    disp.controller_val[0]=param_midi[16];
    disp.controller_val[1]=param_midi[17];
    disp.controller_val[2]=param_midi[18];
    disp.init_window();
    disp.draw_waveform_lfo(lfo.wave, lfo.lfovol, lfo.maincutoff);
  }
  if(screen_n==7) 
  {
    if(init_s) param_displayed=19;
    disp.controller_val[0]=param_midi[19];
    disp.controller_val[1]=param_midi[20];
    disp.controller_val[2]=param_midi[21];
    disp.controller_val[3]=param_midi[22];
    disp.init_window();
    disp.draw_adsr(param_midi[19], param_midi[20], param_midi[21], param_midi[22], 0);
  }
  if(screen_n==8) 
  {
    if(init_s) param_displayed=23;
    disp.controller_val[0]=0;
    disp.controller_val[1]=0;
    disp.init_window();
  }
  if(screen_n==9) 
  {
    if(init_s) param_displayed=25;
    disp.init_window();
  }
}

void param_action(int num)
{
  Serial.print("param_action : ");
  Serial.println(num);
  if(num==0) {
    vTaskSuspend(TaskAudioHandle); 
    disp.draw_warning("loading...");
    disp.display_wave();
    String sfile = "/sample" + String(param_midi[num]) + ".wav"; 
    granulizer.load_file(sfile); 
    vTaskResume(TaskAudioHandle);
  }
  if(num == 1) {
    granulizer.change_start(param_midi[num]);
  }
  if(num == 2) {
    granulizer.change_end(127-param_midi[num]);
  }
  if(num == 3) {
    granulizer.size_change(param_midi[num]);
  }
  if(num == 4) {
    granulizer.env_change(param_midi[num]);
  }
  if(num == 5) {
    granulizer.density_change(param_midi[num]);
  }
  if(num == 6) granulizer.sample_rate_change(param_midi[num]);
  if(num == 7) granulizer.change_random_start_grain(param_midi[num]);
  if(num==8) set_time(param_midi[num]);
  if(num==9) set_feedback(param_midi[num]);
  if(num==10) delay_mix=(float)param_midi[num]/127.0;
  if(num==11) {
    filter.SetMode(param_midi[num]);
    if(param_midi[num]==0) {filt_mode=0;}
    if(param_midi[num]==1) {filt_mode=1;  filt_pente=40;}
    if(param_midi[num]==2) {filt_mode=1;  filt_pente=30;}
    if(param_midi[num]==3) {filt_mode=1;  filt_pente=20;}
    if(param_midi[num]==4) {filt_mode=1;  filt_pente=10;}
    if(param_midi[num]==5) {filt_mode=2;  filt_pente=40;}
    if(param_midi[num]==6) {filt_mode=2;  filt_pente=30;}
    if(param_midi[num]==7) {filt_mode=2;  filt_pente=20;}
    if(param_midi[num]==8) {filt_mode=2;  filt_pente=10;}
    if(param_midi[num]==9) {filt_mode=3;  filt_pente=20; filt_pente2=20;}
    if(param_midi[num]==10) {filt_mode=3;  filt_pente=30; filt_pente2=10;}
    if(param_midi[num]==11) {filt_mode=3;  filt_pente=10; filt_pente2=30;}
    if(param_midi[num]==12) {filt_mode=3;  filt_pente=20; filt_pente2=10;}
    if(param_midi[num]==13) {filt_mode=3;  filt_pente=10; filt_pente2=20;}
    if(param_midi[num]==14) {filt_mode=3;  filt_pente=10; filt_pente2=10;}
  }
  if(num==12) filter.SetCutoff((float)param_midi[num],true);
  if(num==13) {filter.SetResonance((float)param_midi[num],true);}
  if(num==14) lfo.dest=param_midi[num]%16;
  if(num==15) lfo.setWaveform(param_midi[num]%3);
  if(num==16) lfo.setfreq((float)param_midi[num]*(float)param_midi[num]/127);
  if(num==17) lfo.setvol(param_midi[num]);
  if(num==18) lfo.setmaincutoff(param_midi[num]);
  if(num==19) granulizer.setenvA(param_midi[num]);
  if(num==20) granulizer.setenvD(param_midi[num]);
  if(num==21) granulizer.setenvS(param_midi[num]);
  if(num==22) granulizer.setenvR(param_midi[num]);
  if(num==23) savenum=param_midi[num]%16;
  if(num==24) savenum=param_midi[num]%16;
  if(num==25) {volglobal=(float)param_midi[num]*80.0/127.0-74.0; codec.setHeadphoneVolumeDB(volglobal);}
}

void param_action_focus(int num)
{
  
}

void display_window(int num)
{
  Serial.println("display window");
  if(num==0) {
    disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[1], param_midi[2]);
    disp.display_wave();
  }
  if(num == 1) {
    disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[1], param_midi[2]);
    disp.display_wave();
  }
  if(num == 2) {
    disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[1], param_midi[2]);
    disp.display_wave();
  }
  if(num == 3) {
    disp.draw_grain(20, param_midi[3], param_midi[4], param_midi[5]);
    disp.display_wave();
  }
  if(num == 4) {
    disp.draw_grain(20, param_midi[3], param_midi[4], param_midi[5]);
    disp.display_wave();
  }
  if(num == 5) {
    disp.draw_grain(20, param_midi[3], param_midi[4], param_midi[5]);
    disp.display_wave();
  }
  if(num == 8) {
    String aff = String(param_midi[param_displayed]*1000/64*param_midi[param_displayed]/127);
		aff = aff+" ms";
    disp.draw_param(0, aff);
  }

  if(num == 9) {
    String aff = String(param_midi[param_displayed]*100/127);
		aff = aff+" %";
    disp.draw_param(1, aff);
  }

  if(num == 10) {
    String aff = String(param_midi[param_displayed]*100/127);
		aff = aff+" %";
    disp.draw_param(2, aff);
  }
  if(param_displayed == 11) 
  {
    disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed+2], filt_mode, filt_pente, filt_pente2); 
    disp.display_wave();
  }
  if(param_displayed == 12) 
  {
    disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed+1], filt_mode, filt_pente, filt_pente2); 
    disp.display_wave();
  }
  if(param_displayed == 13) 
  {
    disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed], filt_mode, filt_pente, filt_pente2);
    disp.display_wave();
  }
  if(param_displayed >= 14 && param_displayed <= 18) 
  {
    disp.draw_waveform_lfo(lfo.wave, lfo.lfovol, lfo.maincutoff);
    disp.display_wave();
  }
  if(param_displayed >= 19 && param_displayed <= 22) 
  {
    disp.draw_adsr(param_midi[19], param_midi[20], param_midi[21], param_midi[22], param_displayed-19);
    disp.display_wave();
  }
  if(param_displayed >= 23 && param_displayed <= 24) 
  {
    disp.display_wave();
  }
}

void display_param()
{
  //disp.clear();
  //disp.display_back();
  //disp.display();
  Serial.println("display_param");
  
  if(param_displayed == 0) disp.updateController(1);
  if(param_displayed == 1) {
    disp.draw_wave(sample, granulizer.sample_start_index[0], granulizer.sample_length[0], param_midi[param_displayed+1], 127-param_midi[param_displayed+2]);
    //disp.draw_bottom_menu("Rec");
  }
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
  if(param_displayed == 12) disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed+1], 1, filt_pente, filt_pente2); 
  if(param_displayed == 13) disp.draw_filter(filter.GetCutoff(), param_midi[param_displayed], 1, filt_pente, filt_pente2);
  if(param_displayed == 14) disp.draw_string_center(dest_list[param_midi[param_displayed]%16]);
  if(param_displayed == 15) disp.draw_waveform_lfo(lfo.wave, lfo.lfovol, lfo.maincutoff);
  if(param_displayed == 16) disp.draw_number_string_center(param_midi[param_displayed]*param_midi[param_displayed]*0.0034724, " Hz");
  if(param_displayed == 17) disp.draw_waveform_lfo(lfo.wave, lfo.lfovol, lfo.maincutoff);
  if(param_displayed == 18) disp.draw_waveform_lfo(lfo.wave, lfo.lfovol, lfo.maincutoff);
  if(param_displayed == 19) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 20) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 21) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 22) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 23) disp.draw_number_string_center((int)volglobal, " dB");
  if(param_displayed == 24) disp.draw_string_number_center("User ", savenum);
  //disp.display();
 
}

inline void Midi_NoteOn(uint8_t ch, uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn");
  if(param_midi[26]==ch || param_midi[26]==0) 
  {
    if(vol==0) granulizer.noteOff(note);
    else granulizer.noteOn(note);
  }
}

inline void Midi_NoteOff(uint8_t ch, uint8_t note)
{
  Serial.println("Midi_NoteOff");
  if(param_midi[26]==ch || param_midi[26]==0) granulizer.noteOff(note);
}

uint32_t previous_time = micros();

inline void Midi_ControlChange(uint8_t ch, uint8_t note, uint8_t val)
{
  Serial.println("Midi_CC");
  if(midi_learn)
    {
      //midi_learn=false;
      midi_cc_val[param_displayed]=note;
      disp.midi_learned(note);
      disp.display_wave();
    }
    else
    {
      for(int i=0; i<128; i++)
      {
        if(note==midi_cc_val[i])  
        {    
          change_CC(i, val);
          uint32_t CC_time = millis()-previous_time;
          if(param_screen[i]==disp.current_screen) disp.encoder(val,param_numinscreen[i]);
          if(CC_time>200) {
            display_window(i);
            previous_time=millis();
          }
          
        }
      }
    }
}

void Synth_Init()
{
  param_midi[3]=127;
  granulizer.init();
  granulizer.load_file("/sample0.wav");
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

inline void HandleShortMsg(uint8_t *data)
{
    uint8_t ch = data[0] & 0x0F;

    Serial.println(data[0] & 0xF0);
    Serial.print("channel : ");
    Serial.println(ch);

    switch (data[0] & 0xF0)
    {
    /* note on */
      
    case 0x90:
        
        if (data[2] > 0)
        {
          Midi_NoteOn(ch, data[1],data[2]);
        }
        else
        {
          Midi_NoteOff(ch, data[1]);
        }
        break;
    /* note off */
    case 0x80:
        Midi_NoteOff(ch, data[1]);
        break;
    case 0xb0:
        Midi_ControlChange(ch, data[1], data[2]);
        break;
    }
}