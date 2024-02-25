#include <arduino.h>

#define DELAY_SAMPLES 44100 



class Delays
{
  
public:
   //int16_t *samp_rec = (int16_t *) ps_malloc(DELAY_SAMPLES * sizeof(int16_t));
   int16_t samp_rec[DELAY_SAMPLES/2];
   int pos;
   int feedback;
   int time;
   
   void init()
   {
     pos=0;
	 feedback = 10;
	 time=64;
   }
   
   void set_feedback(int feed)
   {
	   feedback = feed;
   }
   
   void set_time(int ti)
   {
	   time = ti;
   }
   
   int16_t output(int16_t input)
   {
	 int16_t ret = samp_rec[pos/2];
     if(pos%2==0) samp_rec[pos/2]=input+samp_rec[pos/2]*feedback/150;
	 pos++;
	 if(pos>=347*time) pos=0;
	 return ret;
   }
   
};
