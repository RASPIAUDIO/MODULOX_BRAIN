                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    String sync_list[]={"Internal clock","Sync IN","MIDI Sync"};
String audio_list[]={"Headphone","Speaker"};
String multi_list[]={"Solo","Multi"};
String dest_list[]={"None","Global pitch","Global volume","Osc1 PWM","Osc2 PWM","Osc3 PWM","Osc1 vol","Osc2 vol","Osc3 vol","Osc1 tune","Osc2 tune","Osc3 tune","Filter cutoff","Filter res","Delay time","Unisson detune","Unisson voulme"};

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
  Serial.println("dpm");
  Serial.println("display_param");
  Serial.println(param_displayed);
  disp.clear();
  Serial.println("cleared");
  if(param_displayed == 0) 
  {
    if(filefound) disp.draw_string_number_center("User ", savenum);
    else  disp.draw_string_number_center_red("User ", savenum);
  }
  if(param_displayed>0 && param_displayed<16)
  {
    int ind=(param_displayed-1)/5;
    int tes=(param_displayed-1)%5;
    if(tes == 0) disp.draw_waveform_osc(oscA[current_synth],ind);
    if(tes == 1) {disp.draw_waveform_osc(oscA[current_synth],ind); disp.draw_number_string_low(param_midi[param_displayed]*100/127, " %");}
    if(tes == 2) disp.draw_number_string_center((int)param_midi[param_displayed]-64, " semi");
    if(tes == 3) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
    if(tes == 4) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  }
  if(param_displayed == 16) {disp.draw_adsr(param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], param_midi[param_displayed+3], 0); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 17) {disp.draw_adsr(param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], 1);  disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 18) {disp.draw_adsr(param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], 2); disp.draw_number_string_low(param_midi[param_displayed]*100/127, " %");}
  if(param_displayed == 19) {disp.draw_adsr(param_midi[param_displayed-3], param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], 3); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 20) disp.draw_string_center(dest_list[param_midi[param_displayed]%16]);
  if(param_displayed == 21) {disp.draw_adsr(param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], param_midi[param_displayed+3], 0); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 22) {disp.draw_adsr(param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], 1);  disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 23) {disp.draw_adsr(param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], 2); disp.draw_number_string_low(param_midi[param_displayed]*100/127, " %");}
  if(param_displayed == 24) {disp.draw_adsr(param_midi[param_displayed-3], param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], 3); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 25) disp.draw_string_center(dest_list[param_midi[param_displayed]%16]);
  if(param_displayed == 26) {disp.draw_adsr(param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], param_midi[param_displayed+3], 0); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 27) {disp.draw_adsr(param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], 1);  disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 28) {disp.draw_adsr(param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], 2); disp.draw_number_string_low(param_midi[param_displayed]*100/127, " %");}
  if(param_displayed == 29) {disp.draw_adsr(param_midi[param_displayed-3], param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], 3); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed>=30 && param_displayed<=44)
  {
    int ind=(param_displayed-30)/5;
    int tes=(param_displayed-30)%5;
    if(tes == 0) disp.draw_string_center(dest_list[param_midi[param_displayed]%16]);
    if(tes == 1) disp.draw_waveform_lfo(lfo[ind][current_synth].wave, lfo[ind][current_synth].lfovol, lfo[ind][current_synth].maincutoff);
    if(tes == 2) disp.draw_number_string_center(param_midi[param_displayed]*param_midi[param_displayed]*0.0034724, " Hz");
    if(tes == 3) disp.draw_waveform_lfo(lfo[ind][current_synth].wave, lfo[ind][current_synth].lfovol, lfo[ind][current_synth].maincutoff);
    if(tes == 4) disp.draw_waveform_lfo(lfo[ind][current_synth].wave, lfo[ind][current_synth].lfovol, lfo[ind][current_synth].maincutoff);

  }
  if(param_displayed == 45) {disp.draw_filter(Filter[current_synth].GetCutoff(), param_midi[param_displayed+1], param_focus[param_displayed]%4); }
  if(param_displayed == 46) {disp.draw_filter(Filter[current_synth].GetCutoff(), param_midi[param_displayed], param_focus[param_displayed]%4);}
  if(param_displayed == 47) disp.draw_number_string_center(param_midi[param_displayed]*1000/64*param_midi[param_displayed]/127, " ms");
  if(param_displayed == 48) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 49) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 50) disp.draw_number_string_center((param_midi[param_displayed]%8)+1, " voice");
  if(param_displayed == 51) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 52) disp.draw_number_string_center(param_midi[param_displayed]+50, " bpm");
  if(param_displayed == 53) disp.draw_string_center(oscA[current_synth].arpmodename(param_midi[param_displayed]%3));
  if(param_displayed == 54) disp.draw_number_string_center(param_midi[param_displayed]%16, " steps");
  if(param_displayed == 55) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 56) disp.draw_number_string_center(param_midi[param_displayed]%5, " octaves");
  if(param_displayed == 57) disp.draw_number_string_center((param_midi[param_displayed]%8)+1, " voices");
  if(param_displayed == 58) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 59) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  //if(param_displayed == 55) draw_digit(param_disto_type);
  if(param_displayed == 60) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 61) disp.draw_string_center(sync_list[tempo_source]);
  if(param_displayed == 62) {
    if(param_midi[param_displayed]==0) disp.draw_string_center("Omni");
    else disp.draw_string_number_center("Channel ", param_midi[param_displayed]%16);
  }
  if(param_displayed == 63) disp.draw_string_center(audio_list[param_midi[param_displayed]%2]);
  if(param_displayed == 64) disp.draw_string_number_center("User ", savenum);
  if(param_displayed == 65) disp.draw_string_number_center("User ", savenum);
  if(param_displayed == 66) disp.draw_string_center(multi_list[multi_mode]);
  if(param_displayed == 67) disp.draw_string_number_center("User ", presetmulti1);
  if(param_displayed == 68) disp.draw_string_number_center("User ", presetmulti2);
  if(param_displayed == 69) disp.draw_string_number_center("Channel ", param_midi[param_displayed]%16);
  if(param_displayed == 70) disp.draw_string_number_center("Channel ", param_midi[param_displayed]%16);
  disp.display_window();  
}
