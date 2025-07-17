int filt_mode=1;
int filt_pente=30;
int filt_pente2=30;
int SampNum=0;

void but_record()
{

    
}

void but_mid_pressed()
{
  Serial.println("but mid pressed");
  Serial.println(param_displayed);
  
  if(param_displayed==19) 
  {
    disp.draw_warning("loading...");
    disp.display_wave();
    delay(500);
    load_preset();
    load_window(10);
    display_window(19);
  }
  if(param_displayed==20) 
  {
    disp.draw_warning("saving...");
    disp.display_wave();
    delay(500);
    save_preset();
    load_window(10);
    display_window(20);
  }
  if(param_displayed!=19 && param_displayed!=20 && enco_focus==0) learn_midi();
}

void enco_pressed()
{
  
}

void enco_released()
{
  //codec.enableSpeakers();
 
}

float volg=1.0;

void load_window(int screen_n, bool init_s)
{
  Serial.print("load_window : ");
  Serial.println(screen_n);
  if(screen_n<16)
  {
    int SampNum=screen_n/2;
    if(screen_n%2==0) 
    {
      if(init_s) param_displayed=0;
      disp.controller_val[0]=param_midi[SampNum*7];
      disp.controller_val[1]=param_midi[1+SampNum*7];
      disp.controller_val[2]=param_midi[2+SampNum*7];
      disp.init_window();
      disp.draw_wave(sample, smp.sample_start_index[SampNum], smp.sample_length[SampNum], param_midi[1+SampNum*6], param_midi[2+SampNum*6]);
    }
    if(screen_n%2==1) 
    {
      if(init_s) param_displayed=4;
      disp.controller_val[0]=param_midi[3+SampNum*7];
      disp.controller_val[1]=param_midi[4+SampNum*7];
      disp.controller_val[2]=param_midi[5+SampNum*7];
      disp.controller_val[2]=param_midi[6+SampNum*7];

      disp.init_window();

      String aff = String(param_midi[4+SampNum*7]);
      disp.load_param(1, aff);

      aff = String(param_midi[5+SampNum*7]-63);
      disp.load_param(2, aff);

      aff = String(param_midi[6+SampNum*7]*100/127);
      aff = aff+" %";
      disp.load_param(3, aff);
    }
  }
  
  if(screen_n==16) 
  {
    if(init_s) param_displayed=56;
    disp.controller_val[0]=param_midi[56];
    disp.controller_val[1]=param_midi[57];
    disp.init_window();
  }
  if(screen_n==17) 
  {
    if(init_s) param_displayed=58;
    disp.controller_val[0]=param_midi[58];
    disp.controller_val[1]=param_midi[59];
    disp.controller_val[2]=param_midi[60];
    disp.controller_val[3]=param_midi[61];
    disp.controller_val[4]=param_midi[62];

    if(!param_midi[58]) disp.load_param(0, "Normal");
    else disp.load_param(0, "PingPong");

    disp.init_window();
    String aff = String(param_midi[59]*1000/64*param_midi[59]/127);
    aff = aff+" ms";
    disp.load_param(1, aff);

    aff = String(param_midi[60]*1000/64*param_midi[60]/127);
    aff = aff+" ms";
    disp.load_param(2, aff);

    aff = String(param_midi[61]*100/127);
    aff = aff+" %";
    disp.load_param(3, aff);

    aff = String(param_midi[62]*100/127);
    aff = aff+" %";
    disp.load_param(4, aff);
  }
  if(screen_n==18)
  {
    if(init_s) param_displayed=63;
    disp.controller_val[0]=param_midi[63];
    disp.controller_val[1]=param_midi[64];
    disp.controller_val[2]=param_midi[65];
    disp.init_window();
    disp.draw_filter(filter.GetCutoff(), param_midi[63], filt_mode, filt_pente, filt_pente2);
  }
  if(screen_n==19) 
  {
    if(init_s) param_displayed=66;
    disp.controller_val[0]=param_midi[66];
    disp.controller_val[1]=param_midi[67];
    disp.controller_val[2]=param_midi[68];
    disp.controller_val[3]=param_midi[69];
    disp.controller_val[4]=param_midi[70];

    if(!param_midi[66]) disp.load_param(0, "Off");
    else disp.load_param(0, "On");

    disp.init_window();
    String aff = String(param_midi[67]*100/127);
    aff = aff+" ms";
    disp.load_param(1, aff);

    aff = String(param_midi[68]*100/127);
    aff = aff+" ms";
    disp.load_param(2, aff);

    aff = String(param_midi[69]*100/127);
    aff = aff+" %";
    disp.load_param(3, aff);

    aff = String(param_midi[70]*100/127);
    aff = aff+" %";
    disp.load_param(4, aff);
  }
  if(screen_n==20) 
  {
    if(init_s) param_displayed=71;
    disp.controller_val[0]=0;
    disp.controller_val[1]=0;
    disp.init_window();
  }
  if(screen_n==21)
  {
    if(init_s) param_displayed=72;
    disp.controller_val[0]=param_midi[72];
    disp.init_window();
  }
}

void param_action(int num)
{
  Serial.print("param_action : ");
  Serial.println(num);
  if(num<48) 
  {
    int SampNum=num/7;
    int ParamNum=num%7;
    if(ParamNum==0) {
      data_from_enco=10;
      stopaudio=true;
      delay(200);
      vTaskSuspend(TaskAudioHandle);
      vTaskSuspend(TaskOtherHandle);  
      delay(10);
      //Serial.println("data enco put to 10");
      
      //disp.draw_warning("loading...");
      //disp.display_wave();
      Serial.println(disp.fileList[0][param_midi[num]]);
      //String sfile = "sample" + String(param_midi[num]) + ".wav"; 
      smp.load_file(disp.fileList[0][param_midi[num]],SampNum,param_midi[num]);
      smp.sample_launch_raw(SampNum, 64);
      data_from_enco=1;
      vTaskResume(TaskAudioHandle);
      vTaskResume(TaskOtherHandle);
      stopaudio=false;
    }
    if(ParamNum==1) smp.param_start_sample[SampNum]=param_midi[num];
    if(ParamNum==2) smp.param_end_sample[SampNum]=param_midi[num];
    if(ParamNum==3) chan_voice[SampNum]=param_midi[num];
    if(ParamNum==4) note_voice[SampNum]=param_midi[num];
    if(ParamNum==5) pitch_voice[SampNum]=param_midi[num];
    if(ParamNum==6) vol_voice[SampNum]=param_midi[num];
  }
  if(num==56) disto_on = param_midi[num];
  if(num==57) disto.set_gain(param_midi[num]);
  if(num==58) set_pingpong(param_midi[num]);
  if(num==59) set_time_l(param_midi[num]);
  if(num==60) set_time_r(param_midi[num]);
  if(num==61) set_feedback(param_midi[num]);
  if(num==62) delay_mix=(float)param_midi[num]/127.0;
  if(num==63) {
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
  if(num==64) filter.SetCutoff((float)param_midi[num],true);
  if(num==65) filter.SetResonance((float)param_midi[num],true);
  if(num==66) comp_on=param_midi[num];
  if(num==67) comp.threshold=(float)param_midi[num]/127.0;
  if(num==68) comp.set_attack(param_midi[num]);
  if(num==69) comp.set_release(param_midi[num]);
  if(num==70) comp.set_ratio(param_midi[num]);
  if(num==71) savenum=param_midi[num]; 
  if(num==72) savenum=param_midi[num];
  if(num==73) {volglobal=(float)param_midi[num]*80.0/127.0-74.0; codec.setHeadphoneVolumeDB(volglobal);}
}

void param_action_focus(int num)
{
  
}

void display_window(int num)
{
  Serial.print("display window : ");
  Serial.println(num);
  int SampNum=num/7;
  if((num%7)>=0 && (num%7)<=2 && num<56) {
    disp.draw_wave(sample, smp.sample_start_index[SampNum], smp.sample_length[SampNum], param_midi[1+SampNum*7], param_midi[2+SampNum*7]);
    disp.display_wave();
  }
  if((num%7)==4 && num<56) {
    String aff;
    if(param_midi[num]==0) aff = "Pitch";
    else aff = String(param_midi[num]);
    disp.draw_param(1, aff);
  }
  if((num%7)==5 && num<56) {
    String aff = String(param_midi[num]-63);
    disp.draw_param(2, aff);
  }
  if((num%7)==6 && num<56) {
    String aff = String(param_midi[num]*100/127);
    aff = aff+" %";
    disp.draw_param(3, aff);
  }
  if(num==57) 
  {
    String aff = String(param_midi[num]*100/127);
    aff = aff+" %";
    disp.draw_param(1, aff);
  }
  if(num == 58) {
    if(!param_midi[num]) disp.draw_param(0, "Normal");
    else disp.draw_param(0, "PingPong");
  }
  if(num == 59) {
    String aff = String(param_midi[59]*1000/64*param_midi[59]/127);
		aff = aff+" ms";
    disp.draw_param(1, aff);
  }
  if(num == 60) {
    String aff = String(param_midi[60]*1000/64*param_midi[60]/127);
		aff = aff+" ms";
    disp.draw_param(2, aff);
  }
  if(num == 61) {
    String aff = String(param_midi[61]*100/127);
		aff = aff+" %";
    disp.draw_param(3, aff);
  }
  if(num == 62) {
    String aff = String(param_midi[62]*100/127);
		aff = aff+" %";
    disp.draw_param(4, aff);
  }
  if(num == 63) 
  {
    disp.draw_filter(filter.GetCutoff(), param_midi[num+2], filt_mode, filt_pente, filt_pente2); 
    disp.display_wave();
  }
  if(num == 64) 
  {
    disp.draw_filter(filter.GetCutoff(), param_midi[num+1], filt_mode, filt_pente, filt_pente2); 
    disp.display_wave();
  }
  if(num == 65) 
  {
    disp.draw_filter(filter.GetCutoff(), param_midi[num], filt_mode, filt_pente, filt_pente2);
    disp.display_wave();
  }
  if(num == 67) {
    String aff = String(param_midi[67]*100/127);
		aff = aff+" %";
    disp.draw_param(1, aff);
  }
  if(num == 68) {
    String aff = String(param_midi[68]);
		aff = aff+" ms";
    disp.draw_param(2, aff);
  }
  if(num == 69) {
    String aff = String(param_midi[69]);
		aff = aff+" ms";
    disp.draw_param(3, aff);
  }
  if(num == 70) {
    String aff = String(param_midi[70]);
		aff = aff+" dB";
    disp.draw_param(4, aff);
  }
}


inline void Midi_NoteOn(uint8_t ch, uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn");
  Serial.println(note);
  Serial.println(ch);
  Serial.println(vol);
  for(int i=0; i<MAX_SAMPLE_NUM; i++)
  {
    int v = vol*vol_voice[i]/127;
    //int v=64;
    Serial.println(i);
    Serial.print("chan : ");
    Serial.print(chan_voice[i]);
    Serial.print(", note : ");
    Serial.println(note_voice[i]);
    
    if(ch==chan_voice[i] && note_voice[i]==0)  {Serial.println("launch1"); smp.sample_launch(i,v ,note); }
    //if(ch==chan_voice[i] && note_voice[i]==note)  {smp.sample_launch(i, v, param_midi[3+7*i]); Serial.println(v);}
    if(ch==chan_voice[i] && note_voice[i]==note)  {Serial.println("launch2"); smp.sample_launch(i, v, pitch_voice[i]); }
  } 
  
}

inline void Midi_NoteOff(uint8_t ch, uint8_t note)
{
  Serial.println("Midi_NoteOff");
  Serial.println(note);
  for(int i=0; i<MAX_SAMPLE_NUM; i++)
  {
    if(ch==chan_voice[i] && note_voice[i]==0)  smp.sample_stop(i);
    if(ch==chan_voice[i] && note_voice[i]==note)  smp.sample_stop(i);
  }
  
}

uint32_t previous_time = micros();

inline void Midi_ControlChange(uint8_t ch, uint8_t note, uint8_t val)
{
  Serial.println("Midi_CC");
  if(midi_learn)
    {
      midi_cc_val[param_displayed]=note;
      num_from_MIDI=note;
    }
    else
    {
      for(int i=0; i<128; i++)
      {
        if(note==midi_cc_val[i])  
        {    
          change_CC(i, val);
          param_action(i);
          num_from_MIDI=i;
          val_from_MIDI=val;
          
        }
      }
    }
}

