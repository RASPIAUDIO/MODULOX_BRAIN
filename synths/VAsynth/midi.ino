
inline void Midi_NoteOn(uint8_t chan, uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn");
  Serial.println(note);
  Serial.println(vol);
  Serial.println(multi_mode);
  Serial.println(chan);
  Serial.println(chansynth1);
  Serial.println(chansynth2);

  bool mustplay=false;
  if(multi_mode) // multi mode
  {
    //Serial.println("multi");
    if(chan==chansynth1) {current_synth=0; mustplay=true;}
    if(chan==chansynth2) {current_synth=1; mustplay=true;}
  }
  else
  {
    //Serial.println("solo");
    //Serial.println(changlob);
    if(changlob==0) mustplay=true;
    if(changlob==chan) mustplay=true;
    current_synth=0;
  }

  Serial.println(current_synth);
  if(mustplay) 
  {
    if(oscA[current_synth].setNote(note, vol)) { 
      //f3.start(); 
      if(env[0][current_synth].dest>0) env[0][current_synth].start(); 
      if(env[1][current_synth].dest>0) env[1][current_synth].start();
      lfo[0][current_synth].start();
      lfo[1][current_synth].start();
      lfo[2][current_synth].start();
      Serial.print("dest : ");
      Serial.println(lfo[0][current_synth].dest);
    }
  }
    //if(oscA[0].setNote(note, 127)) { }
    //Serial.println("end Midi_NoteOn");
    
}

inline void Midi_NoteOff(uint8_t chan, uint8_t note)
{
  Serial.println("Midi_NoteOff");
  Serial.println(current_synth);
  Serial.println(note);
  bool mustplay=false;
  if(multi_mode) // multi mode
  {
    //Serial.println("multi");
    if(chan==chansynth1) {current_synth=0; mustplay=true;}
    if(chan==chansynth2) {current_synth=1; mustplay=true;}
  }
  else
  {
    //Serial.println("solo");
    //Serial.println(changlob);
    if(changlob==0) mustplay=true;
    if(changlob==chan) mustplay=true;
    current_synth=0;
  }
  if(mustplay) 
  {
    oscA[current_synth].setNote(note, 0);
    if(env[0][current_synth].dest>0) env[0][current_synth].stop(); 
    if(env[1][current_synth].dest>0) env[1][current_synth].stop();
  }
}



/*
 * this function will be called when a control change message has been received
 */
inline void Midi_ControlChange(uint8_t channel, uint8_t data1, uint8_t data2)
{
    Serial.println("Midi_ControlChange");
    Serial.println(data1);
    Serial.println(data2);
    Serial.println(param_displayed);
    if(midi_learn)
    {
      //display_menu();
      //display_param();
      
      midi_cc_val[param_displayed]=data1;
      //Serial.println(midi_cc_val[param_displayed]);
      //Serial.println("end CC");
      midi_changed=true;
    }
    else
    {
      for(int i=0; i<128; i++)
      {
        if(data1==midi_cc_val[i])  
        {
          param_CC=i;
          change_CC(data2);
          midi_changed=true;
          
        }
      }
      //change_enco(1);
    }
    
    
}

/*
 * function will be called when a short message has been received over midi
 */
inline void HandleShortMsg(uint8_t *data)
{
    uint8_t ch = data[0] & 0x0F;
    uint8_t type=data[0] & 0xF0;

    Serial.print("type : ");
    Serial.println(type);
    Serial.print("channel : ");
    Serial.println(ch);
    Serial.print("data : ");
    Serial.println(data[1]);
    Serial.println(data[2]);

    /*bool mustplay=false;
    if(multi_mode) // multi mode
    {
      //Serial.println("multi");
      if(ch==chansynth1) {current_synth=0; mustplay=true;}
      if(ch==chansynth2) {current_synth=1; mustplay=true;}
    }
    else
    {
      //Serial.println("solo");
      //Serial.println(changlob);
      if(changlob==0) mustplay=true;
      if(changlob==ch) mustplay=true;
      current_synth=0;
    }*/

    switch (type)
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
