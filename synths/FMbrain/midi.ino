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

inline void Midi_NoteOn(uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn"); 
  FmSynth_NoteOn(0, note, vol/127.0);
}

inline void Midi_NoteOff(uint8_t note)
{
  Serial.println("Midi_NoteOff");
  FmSynth_NoteOff(0, note);
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
          Midi_NoteOn(data[1],data[2]);
        }
        else
        {
          Midi_NoteOff(data[1]);
        }
        break;
    /* note off */
    case 0x80:
        Midi_NoteOff(data[1]);
        break;
    case 0xb0:
        Midi_ControlChange(ch, data[1], data[2]);
        break;
    }
}
