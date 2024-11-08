int16_t* samp_rec;
int pos;
float feedback;
float deltime;

void delay_init()
{
 pos=0;
 feedback = 0.4;
 deltime=0.3;
 samp_rec = (int16_t *)ps_malloc(DELAY_SAMPLES * sizeof(int16_t));
 for(int i=0; i<DELAY_SAMPLES; i++)
 {
  samp_rec[i]=0;
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

int16_t delay_output(int16_t input)
{
  int16_t ret = samp_rec[pos];
  samp_rec[pos]=input+samp_rec[pos]*feedback;
  pos++;
  if(pos>=DELAY_SAMPLES*deltime*deltime) {pos=0;}
  return ret;
}