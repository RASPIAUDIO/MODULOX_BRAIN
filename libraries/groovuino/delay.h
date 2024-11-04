float* samp_rec;
int pos;
float feedback;
float deltime;

void delay_init()
{
 pos=0;
 feedback = 0.4;
 deltime=0.3;
 samp_rec = (float *)ps_malloc(DELAY_SAMPLES * sizeof(float));
 for(int i=0; i<DELAY_SAMPLES; i++)
 {
  samp_rec[i]=0.0;
 }
}

void set_feedback(int feed)
{
 feedback = feed/127.0;
}

void set_time(int ti)
{
	Serial.print("set_time : ");
	Serial.println(ti);
    deltime = ti/127.0;
}

float delay_output(float input)
{
  float ret = samp_rec[pos];
  samp_rec[pos]=input+samp_rec[pos]*feedback;
  pos++;
  if(pos>=DELAY_SAMPLES*deltime*deltime) {pos=0;}
  return ret;
}