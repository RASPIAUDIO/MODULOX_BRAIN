#include <arduino.h>
#include <tables.h>
#include <env.h>

// Change this to change the number of oscillators per voice
#define NUM_OSC 10
#define POLYPHONY 5

// Can adjust the Sample rate
#define SAMPLE_RATE 44100.0 

// Number of samples played in one cycle (a cycle depends on the frequency of the note played)
#define SAMPLES_PER_CYCLE 600 

#define SAMPLES_PER_CYCLE_FIXEDPOINT (SAMPLES_PER_CYCLE<<20) 
#define TICKS_PER_CYCLE (float)((float)SAMPLES_PER_CYCLE_FIXEDPOINT/(float)SAMPLE_RATE) 


// This class represents a voice of synth. It can have several oscillators
class Osc
{
	
public:
   int32_t volglb[POLYPHONY];                               // Volume global
   int32_t volglbsave;                           // Volume global temporary save  
   int32_t phase[NUM_OSC];
   int16_t waveform;                    // Waveform of each oscillator 
   int32_t volosc[NUM_OSC];                      // Volume of each oscillator
   float fine[NUM_OSC];                           // Fine of each oscillator
   uint32_t ulPhaseAccumulator[NUM_OSC][POLYPHONY];          // Position in the reading of each oscillator table 
   volatile uint32_t ulPhaseIncrement[NUM_OSC][POLYPHONY] ;  // Reading speed of each oscillaotr table 
   int8_t octave[NUM_OSC];                        // Octave of each oscillator

//GLIDE
   uint8_t noteplaying[POLYPHONY];                           // true : a note is already playing - false : no note is playing
   boolean glideon;                               // true : glide is ON - false : glide is OFF
   boolean glidestart[POLYPHONY];                            // true : glide already started - false : glide is not started
   boolean play[POLYPHONY];                                  
   uint32_t glidetime;                            // glide time in ms
   uint32_t Incrementglide[NUM_OSC];              // glide speed
   uint32_t Incrementfin[NUM_OSC];                // target frequency

   Env env[POLYPHONY];

   float fFrequency[POLYPHONY];
	 
   int16_t wave[SAMPLES_PER_CYCLE];
   uint8_t pwm_conv;
   float fact1, fact2;
   float fl;
   
   int pitch[NUM_OSC];
   
// initialize the synth. Here you can put your default values.   
   void init()
   {
     
     volglbsave = 0;
	 pwm_conv=0;
	 setPWM(0);
	 pitch[0]=0;
	 pitch[1]=12;
	 pitch[2]=19;
	 pitch[3]=24;
	 pitch[4]=28;
	 pitch[5]=31;
	 pitch[6]=34;
	 pitch[7]=36;
	 pitch[8]=38;
	 pitch[9]=40;
	 volosc[0] = 80;
	 volosc[1] = 110;
	 volosc[2] = 70;
	 volosc[3] = 64;
	 volosc[4] = 40;
	 volosc[5] = 35;
	 volosc[6] = 30;
	 volosc[7] = 64;
	 volosc[8] = 30;
	 volosc[9] = 30;
	 for(int j=0; j<SAMPLES_PER_CYCLE; j++)
		{
			wave[j]=nSineTable[j];
		}
 	 
		 for(int i=0; i<NUM_OSC; i++)
		 {
			waveform = 0;
			
			fine[i] = 0;
			phase[i] = 0;
			Incrementglide[i] = 0;
			Incrementfin[i] = 0;
			octave[i] = 0;
			Incrementfin[i]=0;
			
			
		 }

		 for(int i=0; i<POLYPHONY; i++)
		 {
			 for(int j=0; j<NUM_OSC; j++)
			 {
				 ulPhaseAccumulator[j][i] = 0; 
				 ulPhaseIncrement[j][i] = 0;
			 }
		 }
		 for(int i=0; i<POLYPHONY; i++)
		 {
			 glidestart[i] = false;
			 noteplaying[i] = 0;
			 play[i] = false;
			 volglb[i] = 64;
			 env[i].init();
		 }
	 
//GLIDE
     glideon = false;
     glidetime = 200; // (en ms)
	 
	 fact1=1;
	 fact2=100;
     
   }
   

// Set the note (frequency), and volume 
   void setNote(uint32_t note, int32_t vol)
   {		 

		 int numos = 128;
		 for(int j=0; j<POLYPHONY; j++)
		 {
			 if(note==noteplaying[j] && vol==0) {
				 //play[j]=false; 
				 env[j].stop();
				 numos=129;
			 }
			 /*if(note==noteplaying[j] && vol>0 && play[j]) {
				 //play[j]=false; 
				 env[j].start();
				 numos=129;
			 }*/
		 }
		 if(numos!=129)
		 {
			 bool oscnew = false;
			 for(int j=0; j<POLYPHONY; j++)
			 {
				 if(play[j] && note==noteplaying[j]) numos=j;
			 }
			 if(numos>=128)
			 {
				 for(int j=0; j<POLYPHONY; j++)
				 {
					 if(!play[j]) numos=j;
					 oscnew = true;
				 }
			 }
			 if(numos<128)
			 {
				 for(int i=0; i<NUM_OSC; i++)
				 {
					 fFrequency[numos] = ((pow(2.0,((note+12*octave[i]+pitch[i])+(fine[i]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
					 ulPhaseIncrement[i][numos] = fFrequency[numos]; 
					 if(POLYPHONY==1) Incrementfin[i] = fFrequency[numos]; 
					 if(oscnew) ulPhaseAccumulator[i][numos]=phase[i]<<20;
				 }
				 // The synth is playing
				 play[numos] = true;
				 if(vol!=0) 
				 {
					 volglb[numos] = vol;
					 noteplaying[numos] = note;
					 env[numos].start();
				 }
				 // If the volume = 0, we consider that the synth is stopped
				 else play[numos] = false;
			 }
		 }
		 //Fill the volume data
		 

   }

// Stop the playing of the synth
   void stop(uint8_t note)
   {
		 for(int j=0; j<POLYPHONY; j++)
		 {
			 if(note==noteplaying[j]) play[j] = false;
		 }
   }

// Set the volume of one oscillator
   void setVolOsc(uint8_t num, int32_t vol)
   {
     volosc[num] = vol>>1;
   }
   
   void setPhaseOsc(uint8_t num, int32_t ph)
   {
		 ph*=2;
		 for(int j=0; j<POLYPHONY; j++)
		 {
			 ulPhaseAccumulator[num][j]+=((ph-phase[num])*1048576);
		 }
		 phase[num] = ph;
   }

// Set the waveform of one oscillator
   void setWaveform(uint8_t val)
   {
     waveform = val;
		 for(int i=0; i<SAMPLES_PER_CYCLE; i++)
		 {
			 if(val == 0) wave[i]=nSineTable[i];
			 if(val == 1) wave[i]=nSawTable[i];
			 if(val == 2) wave[i]=nSqTable[i]; 
			 if(val == 3) wave[i]=AKWF_clavinet_0001[i]; 
			 if(val == 4) wave[i]=AKWF_flute_0001[i]; 
			 if(val == 5) wave[i]=AKWF_clavinet_0004[i]; 
			 if(val == 6) wave[i]=AKWF_R_asym_saw_01[i]; 
			 if(val == 7) wave[i]=AKWF_saw_0002[i]; 
			 if(val == 8) wave[i]=AKWF_tri_0002[i]; 
			 if(val == 9) wave[i]=AKWF_squ_0015[i]; 
			 if(val == 10) wave[i]=AKWF_squ_0067[i]; 
		 }
   }

// Set the fine tune of one oscillator
   void setFine(uint8_t num, int val)
   {
     fine[num] = val;
		 for(int j=0; j<POLYPHONY; j++)
		 {
			 fFrequency[j] = ((pow(2.0,((noteplaying[j]+12*octave[num]+pitch[num])+(fine[num]/128)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
			 ulPhaseIncrement[num][j] = fFrequency[j]; 
			 if(POLYPHONY==1) Incrementfin[num] = fFrequency[j];
		 }
   }
	 
	void setPWM(uint8_t val)
	 {
		 pwm_conv=val;
		 
			if(val<=64)
			{
				fl = 5+val*val*val/5;
				fact1=1+fl/600;
				fact2=-fl*fact1;
			}
			else
			{
				fl = 200000/((val-64)*(val-64)+8);
				//fl = 3850-val*30;
				fact1=-fl/600;
				fact2=fact1*(600+fl);
			}
			
	 }

//Set the glide time
   void setGlideTime(uint8_t glt)
   {
     glidetime = glt * 10+1;
   }
   
    void setenvA(uint32_t val)
	{
		for(int j=0; j<POLYPHONY; j++)
		{
			env[j].setA(val);
		}
	}

	void setenvD(uint32_t val)
	{
		for(int j=0; j<POLYPHONY; j++)
		{
			env[j].setD(val);
		}
	}

	void setenvS(uint32_t val)
	{
		for(int j=0; j<POLYPHONY; j++)
		{
			env[j].setS(val);
		}
	}

	void setenvR(uint32_t val)
	{
		for(int j=0; j<POLYPHONY; j++)
		{
			env[j].setR(val);
		}
	} 

// Compute the table position in the wavetable from increment values
   void next()
   {
     for(int j=0; j<POLYPHONY; j++)
	 {
		 if(play[j])
		 {
			 for(int i=0; i<NUM_OSC; i++)
			 {
			   ulPhaseAccumulator[i][j] += ulPhaseIncrement[i][j];   
			   if(ulPhaseAccumulator[i][j] > SAMPLES_PER_CYCLE_FIXEDPOINT) 
			   { 
				 ulPhaseAccumulator[i][j] -= SAMPLES_PER_CYCLE_FIXEDPOINT; 
			   } 
			   
			 }
		 }
	 }	 
   }

//Return the sample value from the wavetable 
   int16_t output()
   {
     int16_t ret=0;
		 //int voldesc;
		 for(int j=0; j<POLYPHONY; j++)
		 {
			 //voldesc=0;
			 int volenv=env[j].amount();
			 //if(volenv>0) Serial.println(volenv);
			 if(!env[j].started && play[j]) play[j]=false;
			 
			 for(int i=0; i<NUM_OSC; i++)
			 {
				 if(pwm_conv==0)
				 {
					 ret+= ((((int32_t)wave[ulPhaseAccumulator[i][j]>>20]* volosc[i])>>7)*volenv)>>7;
					 //voldesc+=volosc[i];
				 }
				  if(pwm_conv>64) 
				 {
					 int ind = (fact1-fact2/((600-(ulPhaseAccumulator[i][j]>>20)+fl)))*(float)(ulPhaseAccumulator[i][j]>>20);
					 ret+= ((((int32_t)wave[ind]* volosc[i])>>7)*volenv)>>7;
				 }
				 if(pwm_conv>0 && pwm_conv<=64)
				 {
					 int ind = (fact1+fact2/((ulPhaseAccumulator[i][j]>>20)+fl))*(float)(ulPhaseAccumulator[i][j]>>20);
					 ret+= ((((int32_t)wave[ind]* volosc[i])>>7)*volenv)>>7;
				 }
			 }
		 }
		 //if(voldesc<127) voldesc=127;
     //return (ret*127/voldesc);
	 return (ret);
   }
   
};
