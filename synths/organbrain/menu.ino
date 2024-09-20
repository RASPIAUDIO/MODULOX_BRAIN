String audio_list[]={"Headphone","Speaker"};
String wave_list[]={"Sine","Saw","Square","Triangle"};

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
  disp.clear();
  if(param_displayed == 0) 
  {
    if(filefound) disp.draw_string_number_center("User ", savenum);
    else  disp.draw_string_number_center_red("User ", savenum);
  }
  if(param_displayed == 1) disp.draw_string_center(wave_list[param_midi[param_displayed]%4]);
  if(param_displayed >= 2 && param_displayed <= 10)
  {
    //for(int i=0; i<9; i++) disp.drawBmpParam("/tirette1.bmp",10+i*28, param_midi[i+2]-100);
    for(int i=0; i<9; i++) disp.display_buffer(10+i*28, param_midi[i+2]-100);
  }
    
  //if(param_displayed >= 2 && param_displayed <= 10) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 11) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 12) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 13) {disp.draw_adsr(param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], param_midi[param_displayed+3], 0); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 14) {disp.draw_adsr(param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], param_midi[param_displayed+2], 1);  disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 15) {disp.draw_adsr(param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], param_midi[param_displayed+1], 2); disp.draw_number_string_low(param_midi[param_displayed]*100/127, " %");}
  if(param_displayed == 16) {disp.draw_adsr(param_midi[param_displayed-3], param_midi[param_displayed-2], param_midi[param_displayed-1], param_midi[param_displayed], 3); disp.draw_number_string_low(5.24288*(param_midi[param_displayed]*100+1)/44.1, " ms");}
  if(param_displayed == 17) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 18) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 19) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 20) disp.draw_number_string_center(param_midi[param_displayed]*1000/64*param_midi[param_displayed]/127, " ms");
  if(param_displayed == 21) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 22) disp.draw_number_string_center(param_midi[param_displayed]*100/127, " %");
  if(param_displayed == 23) {
    if(param_midi[param_displayed]==0) disp.draw_string_center("Omni");
    else disp.draw_string_number_center("Channel ", param_midi[param_displayed]);
  }
  if(param_displayed == 24) disp.draw_string_center(audio_list[param_midi[param_displayed]%2]);
  if(param_displayed == 25) disp.draw_string_number_center("User ", savenum);
  if(param_displayed == 26) disp.draw_string_number_center("User ", savenum);

  disp.display_window();
}
