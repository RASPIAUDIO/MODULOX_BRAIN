void save_preset_sampler()
{
  String aff = String(savenum);
  aff = "/save" + aff + ".txt";
  Serial.println(aff);
  fs::File file = FFat.open(aff, FILE_WRITE);
  Serial.println(file.name());
  for(int i=0; i<128; i++)
  {
    int numsamp=i/6;
    int parsamp=i%6;
    Serial.println(param_midi[i]);
    file.write(param_midi[i]);
  }
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
    Serial.println(param_midi[i]);
    param_midi[i] = file.read();
  }
  file.close();
  Serial.println("file closed");
  
  for(int i=0; i<6*MAX_SAMPLE_NUM; i++)
  {
    Serial.println(i);
    Serial.println(param_midi[i]);
    param_focus[0]=i/6;
    if((i%6)==0) smp.load_file("/"+disp.display_fatfiles(param_midi[i]),param_focus[0],param_midi[i]);
    else param_action(i%6);
  }
  for(int i=6*MAX_SAMPLE_NUM; i<128; i++)
  {
    Serial.println(i);
    Serial.println(param_midi[i]);
    param_action(i);
  }
}
