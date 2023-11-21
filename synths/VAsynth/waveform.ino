
void wavef_init()
{
  for(int fn=0; fn<44; fn++)
  {
    String nu = "/wavef" + String(fn+1) + ".wav";
    fs::File file = SPIFFS.open(nu, "r");
    Serial.println(nu);

    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }
    
    int i=0;
    int wav_size=0;
    int chan_num=0;
    int smp_rate=0;
    int bps=0;
    int find_data=0;
    int ind_data=10000;
    int n=0;
    
    while(file.available()){
      char val=file.read();
      if(i==0) Serial.write(val);
      if(i==1) Serial.write(val);
      if(i==2) Serial.write(val);
      if(i==3) Serial.write(val);
      if(i==4) wav_size+=val;
      if(i==5) wav_size+=val*256;
      if(i==22) chan_num+=val;
      if(i==23) chan_num+=val*256;
      if(i==24) smp_rate+=val;
      if(i==25) smp_rate+=val*256;
      if(i==26) smp_rate+=val*65536;
      if(i==27) smp_rate+=val*16777216;
      if(i==34) bps+=val;
      if(i==35) bps+=val*256;
      if(i<200)
      {
        if(val==0x64 && find_data==0) find_data=1;
        if(val==0x61 && find_data==1) find_data=2;
        if(val==0x74 && find_data==2) find_data=3;
        if(val==0x61 && find_data==3) {ind_data=i+5;find_data=0;}
      }
      if(i>=(ind_data))
      {
        if(chan_num==1 && i%2==0) waveformTab[n+fn*1024]=val&0xFF;
        if(chan_num==1 && i%2==1) {waveformTab[n+fn*1024]|=((int)val<<8)&0xFF00; waveformTab[n+fn*1024]=waveformTab[n+fn*1024]; n++;}
        if(fn<=1 && i%2==1) {Serial.print((n-1)+fn*1024); Serial.print(" : "); Serial.println((int)waveformTab[(n-1)+fn*1024]);}
      }
      i++;
    }
    Serial.println("waveformat");
    Serial.println(n);
    Serial.println(wav_size);
    Serial.println(chan_num);
    Serial.println(smp_rate);
    Serial.println(bps);
    Serial.println(ind_data);
    
    file.close();
  }
}
