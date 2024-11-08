#include <arduino.h>

#define SAMPLE_LFO_RATE 500.0 
#define SAMPLES_LFO_PER_CYCLE 600 
#define SAMPLES_LFO_PER_CYCLE_FIXEDPOINT (SAMPLES_LFO_PER_CYCLE<<20) 
#define TICKS_LFO_PER_CYCLE (float)((float)SAMPLES_LFO_PER_CYCLE_FIXEDPOINT/(float)SAMPLE_LFO_RATE) 



class Lfo
{
  
public:
   uint16_t waveform;
   uint32_t ulPhaseAccumulator; 
   volatile uint32_t ulPhaseIncrement ;   
   float fFrequency;
   boolean play;
   uint32_t lfovol;
   uint32_t maincutoff;
   int16_t wave[SAMPLES_LFO_PER_CYCLE];
   
   void init()
   {
    
     waveform = 0;

     ulPhaseAccumulator = 0; 
     ulPhaseIncrement = 0; 
     maincutoff = 0;	 

     lfovol = 100;	 
	 for(int j=0; j<SAMPLES_LFO_PER_CYCLE; j++)
		{
			wave[j]=nSineTable[j];
		}
	 setfreq(5);
	 play=true;
   }

   void setfreq(float freq)
   {
     fFrequency = freq*TICKS_LFO_PER_CYCLE/200;
     ulPhaseIncrement = fFrequency; 
   }

   void stop()
   {
     play = false;
   }
   
   void start()
   {
     play = true;
   }
   
   void setvol(uint32_t _vol)
   {
     lfovol = _vol;
   }
   
   void setmaincutoff(uint32_t data)
   {
     maincutoff = data;
   }
   
   void setWaveform(uint32_t val)
   {
     waveform = val;
	 for(int i=0; i<SAMPLES_LFO_PER_CYCLE; i++)
	 {
		 if(val == 0) wave[i]=nSineTable[i];
		 if(val == 1) wave[i]=nSawTable[i];
		 if(val == 2) wave[i]=nSqTable[i]; 
	 }
   }
   
/*   void next()
   {
     if(play)
	 {
       ulPhaseAccumulator += ulPhaseIncrement;   
       if(ulPhaseAccumulator > SAMPLES_LFO_PER_CYCLE_FIXEDPOINT) 
       { 
         ulPhaseAccumulator -= SAMPLES_LFO_PER_CYCLE_FIXEDPOINT; 
       }
     }	   
   }	*/	 

   
   uint16_t output()
   {
     int32_t ret=128;
	 
     if(play)
	 {
	   ulPhaseAccumulator += ulPhaseIncrement;   
       if(ulPhaseAccumulator > SAMPLES_LFO_PER_CYCLE_FIXEDPOINT) 
       { 
         ulPhaseAccumulator -= SAMPLES_LFO_PER_CYCLE_FIXEDPOINT; 
       }
       ret= (((wave[ulPhaseAccumulator>>20]+2048)*lfovol)>>12)+maincutoff;
	   if(ret>=128) ret = 128;
	   if(ret<=1) ret = 1;
     }
     return ret;
   }
   
};
