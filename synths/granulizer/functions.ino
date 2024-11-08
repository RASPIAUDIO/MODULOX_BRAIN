void but_mid_pressed()
{

}

void enco_pressed()
{

}

void param_action(int num)
{
  Serial.print("param_action : ");
  Serial.println(num);
  if(num == 1) granulizer.size_change(param_midi[num]);
  if(num == 2) granulizer.env_change(param_midi[num]);
  if(num == 3) granulizer.density_change(param_midi[num]);
  if(num == 4) granulizer.rate_change(param_midi[num]);

}

void param_action_focus(int num)
{
  
}

void display_param()
{
  disp.clear();
  
  if(param_displayed == 0) disp.draw_string_number_center("User ", savenum);
  if(param_displayed == 1) disp.draw_number_string_center(((int)param_midi[param_displayed]*(int)param_midi[param_displayed]+50)/44, " ms");
  if(param_displayed == 2) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 3) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 4) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");

  disp.display_window();
  
}

inline void Midi_NoteOn(uint8_t ch, uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn");
}

inline void Midi_NoteOff(uint8_t ch, uint8_t note)
{
  Serial.println("Midi_NoteOff");
}

void Synth_Init()
{
  granulizer.init();
  granulizer.load_file("/sample.wav", 0);
  granulizer.launch(0,100);

}

inline void Synth_Process(int16_t *left, int16_t *right)
{

}


