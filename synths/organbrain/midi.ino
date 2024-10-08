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

inline void Midi_NoteOn(uint8_t ch, uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn");
  oscA.setNote(note, vol);   
}

inline void Midi_NoteOff(uint8_t ch, uint8_t note)
{
  Serial.println("Midi_NoteOff");
  oscA.setNote(note, 0);
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
    uint8_t type=data[0] & 0xF0;

    /*Serial.print("type : ");
    Serial.println(type);
    Serial.print("channel : ");
    Serial.println(ch);
    Serial.print("data : ");
    Serial.println(data[1]);
    Serial.println(data[2]);*/

    bool mustplay=false;
    //Serial.println("solo");
    //Serial.println(changlob);
    if(changlob==0) mustplay=true;
    if(changlob==ch) mustplay=true;

    switch (type)
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
