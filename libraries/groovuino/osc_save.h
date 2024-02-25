#include <arduino.h>
#include <tables.h>
#include <env.h>

// Change this to change the number of oscillators per voice
#define NUM_OSC 3
#define POLYPHONY 1

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
   int16_t waveform[NUM_OSC];                    // Waveform of each oscillator 
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
	 
   int16_t wave[NUM_OSC][SAMPLES_PER_CYCLE];
   uint16_t pwm_conv[SAMPLES_PER_CYCLE];
   
// initialize the synth. Here you can put your default values.   
   void init()
   {
     
     volglbsave = 0;
 	 
		 for(int i=0; i<NUM_OSC; i++)
		 {
			waveform[i] = 0;
			volosc[i] = 64;
			fine[i] = 0;
			phase[i] = 0;
			Incrementglide[i] = 0;
			Incrementfin[i] = 0;
			octave[i] = 0;
			Incrementfin[i]=0;
			for(int j=0; j<SAMPLES_PER_CYCLE; j++)
			{
				wave[i][j]=nSineTable[j];
			}
		 }
		 float pwm = 128.0/600.0+0.1;
		 float factor1 = 100/pwm-400;
		 float factor2=600/((600*pwm+factor1)*(600*pwm+factor1)*(600*pwm+factor1)-factor1*factor1*factor1);
		 float factor3=factor1*factor1*factor1*factor2;
		 
		 //setWaveform(num, waveform[num]);
		 for(int i=0; i<SAMPLES_PER_CYCLE; i++)
		 {
			pwm_conv[i]=(i*pwm+factor1)*(i*pwm+factor1)*(i*pwm+factor1)*factor2-factor3;
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
     glidetime = 1000; // (en ms)
     
   }
   

// Set the note (frequency), and volume 
   void setNote(uint32_t note, int32_t vol)
   {
// If we are in glide mode and the synth is not playing, we have to compute the glide increment (= glide speed)	
// We do not have to change the frequency value, it's the glide increment which will change it
		 
     if(note!=noteplaying[0] && glideon && play[0] && POLYPHONY==1)
     {
       for(int i=0; i<NUM_OSC; i++)
       {
         fFrequency[0] = ((pow(2.0,((note+12*octave[i])+(fine[i]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
         Incrementfin[i] = fFrequency[0];
				 Incrementglide[i] = abs((int)Incrementfin[i] - (int)ulPhaseIncrement[i][0])*1000/44100/glidetime;
				 if(Incrementglide[i]==0) Incrementglide[i] = 1;
       }
     }
//If we are not in glide mode, compute the frequency of the new note
     else
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
			 for(int j=0; j<POLYPHONY; j++)
			 {
				 if(!play[j] || (play[j] && note==noteplaying[j])) numos=j;
				 
			 }
			 if(numos<128)
			 {
				 for(int i=0; i<NUM_OSC; i++)
				 {
					 fFrequency[numos] = ((pow(2.0,((note+12*octave[i])+(fine[i]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
					 ulPhaseIncrement[i][numos] = fFrequency[numos]; 
					 if(POLYPHONY==1) Incrementfin[i] = fFrequency[numos]; 
					 ulPhaseAccumulator[i][numos]=phase[i]<<20;
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
   void setWaveform(uint8_t num, uint8_t val)
   {
     waveform[num] = val;
		 for(int i=0; i<SAMPLES_PER_CYCLE; i++)
		 {
			 if(val == 0) wave[num][i]=nSineTable[i];
			 if(val == 1) wave[num][i]=nSawTable[i];
			 if(val == 2) wave[num][i]=nSqTable[i]; 
			 if(val == 3) wave[num][i]=AKWF_clavinet_0001[i]; 
			 if(val == 4) wave[num][i]=AKWF_flute_0001[i]; 
			 if(val == 5) wave[num][i]=AKWF_clavinet_0004[i]; 
			 if(val == 6) wave[num][i]=AKWF_R_asym_saw_01[i]; 
			 if(val == 7) wave[num][i]=AKWF_saw_0002[i]; 
			 if(val == 8) wave[num][i]=AKWF_tri_0002[i]; 
			 if(val == 9) wave[num][i]=AKWF_squ_0015[i]; 
			 if(val == 10) wave[num][i]=AKWF_squ_0067[i]; 
		 }
   }

// Set the fine tune of one oscillator
   void setFine(uint8_t num, int val)
   {
     fine[num] = val;
		 for(int j=0; j<POLYPHONY; j++)
		 {
			 fFrequency[j] = ((pow(2.0,((noteplaying[j]+12*octave[num])+(fine[num]/128)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
			 ulPhaseIncrement[num][j] = fFrequency[j]; 
			 if(POLYPHONY==1) Incrementfin[num] = fFrequency[j];
		 }
   }
	 
	 void setPWM(uint8_t num, uint8_t val)
	 {
/*		 float pwm = (float)val/600.0+0.1;
		 float factor1 = 100/pwm-400;
		 float factor2=600/((600*pwm+factor1)*(600*pwm+factor1)*(600*pwm+factor1)-factor1*factor1*factor1);
		 float factor3=factor1*factor1*factor1*factor2;*/
		 
		 
		 for(int i=0; i<SAMPLES_PER_CYCLE; i++)
		 {
			 //int i2 =  (i*pwm+factor1)*(i*pwm+factor1)*(i*pwm+factor1)*factor2-factor3;
			 int i2 =  (pwm_conv[i]*val + i*(128-val))>>7;
			 if(i2<0) i2=0;
			 if(waveform[num] == 0) wave[num][i]=nSineTable[i2];
			 if(waveform[num] == 1) wave[num][i]=nSawTable[i2];
			 if(waveform[num] == 2) wave[num][i]=nSqTable[i2]; 
			 if(waveform[num] == 3) wave[num][i]=AKWF_clavinet_0001[i2]; 
			 if(waveform[num] == 4) wave[num][i]=AKWF_flute_0001[i2]; 
			 if(waveform[num] == 5) wave[num][i]=AKWF_clavinet_0004[i2]; 
			 if(waveform[num] == 6) wave[num][i]=AKWF_R_asym_saw_01[i2]; 
			 if(waveform[num] == 7) wave[num][i]=AKWF_saw_0002[i2]; 
			 if(waveform[num] == 8) wave[num][i]=AKWF_tri_0002[i2]; 
			 if(waveform[num] == 9) wave[num][i]=AKWF_squ_0015[i2]; 
			 if(waveform[num] == 10) wave[num][i]=AKWF_squ_0067[i2]; 
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
	 if(POLYPHONY==1)
	 {
	   for(int i=0; i<NUM_OSC; i++)
	   {
		   if(ulPhaseIncrement[i][0]<Incrementfin[i]) 
		   {
			 if((Incrementfin[i]-ulPhaseIncrement[i][0])<=Incrementglide[i])
			 {
			   ulPhaseIncrement[i][0] = Incrementfin[i];
			 }
			 else ulPhaseIncrement[i][0] += Incrementglide[i];
		   }
		   if(ulPhaseIncrement[i][0]>Incrementfin[i]) 
		   {
			 if((ulPhaseIncrement[i][0]-Incrementfin[i])<=Incrementglide[i])
			 {
			   ulPhaseIncrement[i][0] = Incrementfin[i];
			 }
			 else ulPhaseIncrement[i][0] -= Incrementglide[i];
		   }
	   }
	 }
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
		 int voldesc;
		 for(int j=0; j<POLYPHONY; j++)
		 {
			 voldesc=0;
			 int volenv=env[j].amount();
			 //if(volenv>0) Serial.println(volenv);
			 if(!env[j].started) play[j]=false;
			 for(int i=0; i<NUM_OSC; i++)
			 {
				 ret+= ((((int32_t)wave[i][ulPhaseAccumulator[i][j]>>20]* volosc[i])>>7)*volenv)>>7;
				 voldesc+=volosc[i];
			 }
		 }
		 if(voldesc<127) voldesc=127;
     return (ret*127/voldesc);
   }
   
};
