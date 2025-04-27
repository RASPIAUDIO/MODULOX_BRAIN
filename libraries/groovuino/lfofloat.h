#ifndef lfofloat_h
#define lfofloat_h

#include <arduino.h>
#include <tablesfloat.h>

#define SAMPLE_LFO_RATE 44100.0 
#define SAMPLES_LFO_PER_CYCLE 1024 
#define TICKS_LFO_PER_CYCLE (float)((float)SAMPLES_LFO_PER_CYCLE/(float)SAMPLE_LFO_RATE) 



class Lfo
{
  
public:
	uint16_t waveform;
	float phase_accu; 
	float phase_inc ;   
	float fFrequency;
	boolean play;
	float lfovol;
	float maincutoff;
	float wave[SAMPLES_LFO_PER_CYCLE];
	uint8_t dest;

	void init()
	{
		waveform = 0;
		phase_accu = 0; 
		phase_inc = 0; 
		maincutoff = 0.5;	 
		lfovol = 0.5;	 
		dest=0;
		for(int j=0; j<SAMPLES_LFO_PER_CYCLE; j++)
		{
			wave[j]=nSineTable[j];
		}
		setfreq(5);
		play=true;
	}

	void setfreq(float freq)
	{
		fFrequency = (freq*TICKS_LFO_PER_CYCLE+TICKS_LFO_PER_CYCLE)/8.0;
		phase_inc = fFrequency; 
	}

	void stop()
	{
		play = false;
	}

	void start()
	{
		play = true;
	}

	void setvol(float _vol)
	{
		lfovol = _vol/127.0;
	}

	void setmaincutoff(float data)
	{
		maincutoff = data/127.0;
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

   
	float output()
	{
		float ret=1.0;
	 
		if(play)
		{
			phase_accu += phase_inc;   
			if(phase_accu > SAMPLES_LFO_PER_CYCLE) 
			{ 
				phase_accu -= SAMPLES_LFO_PER_CYCLE; 
			}
			ret= wave[(int)phase_accu]*lfovol+maincutoff;
			if(ret>=1.0) ret = 1.0;
			if(ret<=0) ret = 0;
		}
		return ret;
	}
   
};

#endif