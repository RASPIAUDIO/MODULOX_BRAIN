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
  if(param_displayed == 1) disp.draw_string_number_center("algo ", param_midi[param_displayed]%8);
  if(param_displayed == 2) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  if(param_displayed == 3) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  if(param_displayed == 4) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  if(param_displayed == 5) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  if(param_displayed == 6) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  if(param_displayed == 7) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  if(param_displayed == 8) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  if(param_displayed == 9) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  if(param_displayed == 10) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  if(param_displayed == 11) disp.draw_string_number_center(" ", param_midi[param_displayed]);
  disp.display_window();
}
