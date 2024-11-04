float* samp_rec2;
int pos2;
float feedback2;
float deltime2;

void delay2_init()
{
	Serial.print("delay2_init : ");
	Serial.println(DELAY_SAMPLES);
 pos2=0;
 feedback2 = 0.4;
 deltime2=0.3;
 samp_rec2 = (float *)ps_malloc(DELAY_SAMPLES * sizeof(float));
 for(int i=0; i<DELAY_SAMPLES; i++)
 {
  samp_rec2[i]=0.0;
 }
}

void set_feedback2(int feed)
{
 feedback2 = feed/127.0;
}

void set_time2(int ti)
{
	Serial.print("set_time2 : ");
	Serial.println(ti);
    deltime2 = (float)ti/127.0;
}

float delay2_output(float input)
{
  float ret = samp_rec2[pos2];
  samp_rec2[pos2]=input+samp_rec2[pos2]*feedback2;
  pos2++;
  if(pos2>=DELAY_SAMPLES*deltime2*deltime2) {pos2=0;}
  return ret;
}