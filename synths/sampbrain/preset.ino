void save_preset_sampler()
{
  String aff = String(savenum);
  aff = "/save" + aff + ".txt";
  Serial.println(aff);
  fs::File file = FFat.open(aff, FILE_WRITE);
  Serial.println(file.name());
  for(int i=0; i<121; i++)
  {
    int numsamp=i/7;
    int parsamp=i%7;
    Serial.println(param_midi[i]);
    file.write(param_midi[i]);
  }
  if(disto_on) file.write(1); 
  else file.write(0); 
  if(delay_on) file.write(1); 
  else file.write(0); 
  file.write(param_focus[8]); 
  if(comp_on) file.write(1); 
  else file.write(0); 
  file.write(0); 
  file.write(0); 
  file.write(0); 
  file.close();
  Serial.println("file closed");
}

void load_preset_sampler()
{
  Serial.println("load preset");
  String aff = String(savenum);
  aff = "/save" + aff + ".txt";
  Serial.println(aff);
  fs::File file = FFat.open(aff, "r");
  Serial.println(file.name());
  for(int i=0; i<128; i++)
  {
    param_midi[i] = file.read();
    Serial.println(param_midi[i]);
  }
  file.close();
  Serial.println("file closed");
  
  for(int i=0; i<7*MAX_SAMPLE_NUM; i++)
  {
    Serial.println(i);
    Serial.println(param_midi[i]);
    param_focus[0]=i/7;
    if((i%7)==0) {smp.load_file("/"+disp.display_fatfiles(param_midi[i]),param_focus[0],param_midi[i]); delay(20);}
    else param_action(i%7);
  }
  for(int i=7*MAX_SAMPLE_NUM; i<121; i++)
  {
    Serial.println(i);
    Serial.println(param_midi[i]);
    param_action(i+7-7*MAX_SAMPLE_NUM);
  }
  if(param_midi[121]>0) {disto_on=true; param_focus[7]=1;}
  if(param_midi[122]>0) {delay_on=true; param_focus[10]=1; param_focus[11]=1;}
  if(param_midi[123]>0) {filter_on=true; param_focus[8]=param_midi[123]; param_focus[9]=param_midi[123];param_action_focus(8);}
  if(param_midi[124]>0) {comp_on=true; param_focus[12]=1; param_focus[13]=1;param_focus[14]=1; param_focus[15]=1;param_focus[16]=1;}

  param_focus[0]=0;
  param_focus[1]=0;
  param_focus[2]=0;
  param_focus[3]=0;
  param_focus[4]=0;
  param_focus[5]=0;
  param_focus[6]=0;
}
