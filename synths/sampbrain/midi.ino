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

inline void Midi_NoteOn(uint8_t chan, uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn");
  Serial.println(chan);
  Serial.println(chan_voice[0]);
  Serial.println(note);
  for(int i=0; i<MAX_SAMPLE_NUM; i++)
  {
    if(chan==chan_voice[i] && note_voice[i]==0)  smp.sample_launch(i, vol,note);
    if(chan==chan_voice[i] && note_voice[i]==note)  smp.sample_launch(i, vol);
  }   
}

inline void Midi_NoteOff(uint8_t chan, uint8_t note)
{
  Serial.println("Midi_NoteOff");
  for(int i=0; i<MAX_SAMPLE_NUM; i++)
  {
    if(chan==chan_voice[i])  smp.sample_stop(i);
  }
  
}



/*
 * this function will be called when a control change message has been received
 */
inline void Midi_ControlChange(uint8_t channel, uint8_t data1, uint8_t data2)
{
 
    
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
