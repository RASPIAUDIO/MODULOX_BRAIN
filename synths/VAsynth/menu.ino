String sync_list[]={"Internal clock","Sync IN","MIDI Sync"};
String audio_list[]={"Headphone","Speaker"};
String multi_list[]={"Solo","Multi"};

void display_menu()
{
  Serial.println("display_menu");
  if(enco_focus>0) disp.menu_top(param_displayed, 0x5ACF); 
  else disp.menu_top(param_displayed);
  disp.display_top();
  Serial.println("background");
  //compute_filter_curve();
  disp.menu_bottom(param_displayed,param_focus[param_displayed]);
  disp.display_bottom();
}

void display_param()
{
  Serial.println("display_param");
  disp.clear();
  if(param_displayed == 0) disp.draw_string_number_center("User ", savenum);
  if(param_displayed>0 && param_displayed<16)
  {
    int ind=(param_displayed-1)/5;
    int tes=(param_displayed-1)%5;
    if(tes == 0) disp.draw_waveform_osc(oscA[0],ind);
    if(tes == 1) {disp.draw_waveform_osc(oscA[0],ind); disp.draw_number_string_low(param_midi[param_displayed]*100/127, " %");}
    if(tes == 2) disp.draw_number_string_center((int)param_midi[param_displayed]-64, " semi");
    if(tes == 3) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
    if(tes == 4) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  }
  if(param_displayed>=16 && param_displayed<=27)
  {
    int ind=(param_displayed-16)/4;
    int tes=(param_displayed-16)%4;
    if(tes == 0) {disp.draw_adsr(param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], param_midi[param_displayed+3], 0); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
    if(tes == 1) {disp.draw_adsr(param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], 1);  disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
    if(tes == 2) {disp.draw_adsr(param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], 2); disp.draw_number_string_low(param_midi[param_displayed]*100/127, " %");}
    if(tes == 3) {disp.draw_adsr(param_midi[param_displayed-3], param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], 3); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  }
  if(param_displayed>=28 && param_displayed<=39)
  {
    int ind=(param_displayed-28)/4;
    int tes=(param_displayed-28)%4;
    if(tes == 0) disp.draw_waveform_lfo(lfo[ind][current_synth].wave, lfo[ind][current_synth].lfovol, lfo[ind][current_synth].maincutoff);
    if(tes == 1) disp.draw_number_string_center(param_midi[param_displayed]*param_midi[param_displayed]*0.0034724, " Hz");
    if(tes == 2) disp.draw_waveform_lfo(lfo[ind][current_synth].wave, lfo[ind][current_synth].lfovol, lfo[ind][current_synth].maincutoff);
    if(tes == 3) disp.draw_waveform_lfo(lfo[ind][current_synth].wave, lfo[ind][current_synth].lfovol, lfo[ind][current_synth].maincutoff);

  }
  if(param_displayed == 40) {disp.draw_filter(Filter[current_synth].GetCutoff(), param_midi[param_displayed+1]); }
  if(param_displayed == 41) {disp.draw_filter(Filter[current_synth].GetCutoff(), param_midi[param_displayed]);}
  if(param_displayed == 42) disp.draw_number_string_center(param_midi[param_displayed]*1000/64*param_midi[param_displayed]/127, " ms");
  if(param_displayed == 43) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 44) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 45) disp.draw_number_string_center((param_midi[param_displayed]%8)+1, " voice");
  if(param_displayed == 46) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 47) disp.draw_number_string_center(param_midi[param_displayed]+50, " bpm");
  if(param_displayed == 48) disp.draw_string_center(oscA[current_synth].arpmodename(param_midi[param_displayed]%3));
  if(param_displayed == 49) disp.draw_number_string_center(param_midi[param_displayed]%16, " steps");
  if(param_displayed == 50) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 51) disp.draw_number_string_center(param_midi[param_displayed]%5, " octaves");
  if(param_displayed == 52) disp.draw_number_string_center((param_midi[param_displayed]%8)+1, " voices");
  if(param_displayed == 53) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 54) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  //if(param_displayed == 55) draw_digit(param_disto_type);
  if(param_displayed == 56) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 57) disp.draw_string_center(sync_list[tempo_source]);
  if(param_displayed == 58) disp.draw_string_number_center("Channel ", param_midi[param_displayed]);
  if(param_displayed == 59) disp.draw_string_center(audio_list[param_midi[param_displayed]%2]);
  if(param_displayed == 60) disp.draw_string_number_center("User ", savenum);
  if(param_displayed == 61) disp.draw_string_number_center("User ", savenum);
  if(param_displayed == 62) disp.draw_string_center(multi_list[param_midi[param_displayed]]);
  if(param_displayed == 63) disp.draw_string_number_center("User ", param_midi[param_displayed]);
  if(param_displayed == 64) disp.draw_string_number_center("User ", param_midi[param_displayed]);
  if(param_displayed == 65) disp.draw_string_number_center("Channel ", param_midi[param_displayed]);
  if(param_displayed == 66) disp.draw_string_number_center("Channel ", param_midi[param_displayed]);
  disp.display_window();  
}

void display_midi_learn()
{
  disp.midi_learn();
}
