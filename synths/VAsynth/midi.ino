
inline void Midi_NoteOn(uint8_t chan, uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn");
  Serial.println(current_synth);
  Serial.println(note);
  Serial.println(vol);
  
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
    //if(oscA[0].setNote(note, 127)) { }
    //Serial.println("end Midi_NoteOn");
    
}

inline void Midi_NoteOff(uint8_t chan, uint8_t note)
{
  Serial.println("Midi_NoteOff");
  Serial.println(current_synth);
  Serial.println(note);
    oscA[current_synth].setNote(note, 0);
}



/*
 * this function will be called when a control change message has been received
 */
inline void Midi_ControlChange(uint8_t channel, uint8_t data1, uint8_t data2)
{
    /*if (data1 == 17)
    {
        if (channel < 10)
        {
            Synth_SetSlider(channel,  data2 * NORM127MUL);
        }
    }
    if ((data1 == 18) && (channel == 1))
    {
        Synth_SetSlider(8,  data2 * NORM127MUL);
    }

    if ((data1 == 16) && (channel < 9))
    {
        Synth_SetRotary(channel, data2 * NORM127MUL);

    }
    if ((data1 == 18) && (channel == 0))
    {
        Synth_SetRotary(8,  data2 * NORM127MUL);
    }*/
    //Synth_SetRotary(data1,  data2 * NORM127MUL);
    //Serial.println("Midi_ControlChange");
    //Serial.println(data1);
    //Serial.println(data2);
    //Serial.println(param_displayed);
    if(midi_learn)
    {
      //display_menu();
      //display_param();
      midi_learn=false;
      midi_cc_val[param_displayed]=data1;
      //Serial.println(midi_cc_val[param_displayed]);
      disp.midi_learned(data1);
      disp.display_window();
    }
    else
    {
      //Serial.println(data1);
      //Serial.println(midi_cc_val[0]);
      for(int i=0; i<128; i++)
      {
        if(data1==midi_cc_val[i])  
        {
          param_displayed=i;
          change_CC(data2);
          
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

    //Serial.println(data[0] & 0xF0);
    //Serial.print("channel : ");
    //Serial.println(ch);

    bool mustplay=false;
    if(multi_mode) // multi mode
    {
      if(ch==chansynth1) {current_synth=0; mustplay=true;}
      if(ch==chansynth2) {current_synth=1; mustplay=true;}
    }
    else
    {
      if(changlob==0) mustplay=true;
      if(changlob==ch) mustplay=true;
      current_synth=0;
    }

    switch (data[0] & 0xF0)
    {
    /* note on */
      
    case 0x90:
        
        if (data[2] > 0)
        {
          if(mustplay) Midi_NoteOn(ch, data[1],data[2]);
        }
        else
        {
          if(mustplay) Midi_NoteOff(ch, data[1]);
        }
        break;
    /* note off */
    case 0x80:
        if(mustplay) Midi_NoteOff(ch, data[1]);
        break;
    case 0xb0:
        if(mustplay) Midi_ControlChange(ch, data[1], data[2]);
        break;
    }
}
