/*
 * a simple implementation to use midi
 *
 * Author: Marcel Licence
 */


/*
 * look for midi interface using 1N136
 * to convert the MIDI din signal to
 * a uart compatible signal
 */
#define RXD2 21 /* U2RRXD */
#define TXD2 47
#define SYNCRX 48
/* use define to dump midi data */
#define DUMP_SERIAL2_TO_SERIAL

/* constant to normalize midi value to 0.0 - 1.0f */
#define NORM127MUL  0.007874f

int value_sync=0;

inline void Midi_NoteOn(uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn");
  Serial.println(current_synth);
  Serial.println(note);
  Serial.println(vol);
  
    if(oscA[current_synth].setNote(note, vol)) { 
      //f3.start(); 
      if(env_dest>0) env[current_synth].start(); 
      lfo[0][current_synth].start();
      Serial.print("dest : ");
      Serial.println(lfo[0][current_synth].dest);
    }
    //if(oscA[0].setNote(note, 127)) { }
    //Serial.println("end Midi_NoteOn");
    
}

inline void Midi_NoteOff(uint8_t note)
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
    //Serial.println("midi learn");
    //Serial.println(midi_learn);
    if(midi_learn)
    {
      display_menu();
      display_param();
      midi_learn=false;
      midi_cc_val[param_displayed]=data1;
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
    Serial.print("channel : ");
    Serial.println(ch);

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
          if(mustplay) Midi_NoteOn(data[1],data[2]);
        }
        else
        {
          if(mustplay) Midi_NoteOff(data[1]);
        }
        break;
    /* note off */
    case 0x80:
        if(mustplay) Midi_NoteOff(data[1]);
        break;
    case 0xb0:
        if(mustplay) Midi_ControlChange(ch, data[1], data[2]);
        break;
    }
}

void Midi_Setup()
{
    Serial2.begin(31250, SERIAL_8N1, RXD2, TXD2);
    pinMode(SYNCRX,INPUT_PULLDOWN);
}

/*
 * this function should be called continuously to ensure that incoming messages can be processed
 */

void Sync_Process()
{
  int v = digitalRead(SYNCRX);
  if(v!=value_sync)  {
    //Serial.println(v); 
    //if(v==1) current_step++;
    value_sync=v;
  }
}

void Midi_Process()
{
    /*
     * watchdog to avoid getting stuck by receiving incomplete or wrong data
     */
    static uint32_t inMsgWd = 0;
    static uint8_t inMsg[3];
    static uint8_t inMsgIndex = 0;

    //Serial.println(digitalRead(RXD2));

    //Choose Serial1 or Serial2 as required

    if (Serial2.available())
    {
        uint8_t incomingByte = Serial2.read();
        //Serial.println("read");

#ifdef DUMP_SERIAL2_TO_SERIAL
        //Serial.printf("%02x", incomingByte);
#endif
        /* ignore live messages */
        if ((incomingByte & 0xF0) == 0xF0)
        {
            return;
        }

        if (inMsgIndex == 0)
        {
            if ((incomingByte & 0x80) != 0x80)
            {
                inMsgIndex = 1;
            }
        }

        inMsg[inMsgIndex] = incomingByte;
        inMsgIndex += 1;

        if (inMsgIndex >= 3)
        {
            HandleShortMsg(inMsg);
            inMsgIndex = 0;
        }

      
        inMsgWd = 0;
    }

    /*else
    {
        if (inMsgIndex > 0)
        {
            inMsgWd++;
            if (inMsgWd == 0xFFF)
            {
                inMsgIndex = 0;
            }
        }
    }*/

}
