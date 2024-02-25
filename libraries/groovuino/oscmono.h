#include <arduino.h>


// Change this to change the number of oscillators per voice
#define NUM_OSC 3

// Can adjust the Sample rate
#define SAMPLE_RATE 44100.0 

// Number of samples played in one cycle (a cycle depends on the frequency of the note played)
#define SAMPLES_PER_CYCLE 1024 
#define TICKS_PER_CYCLE (float)((float)SAMPLES_PER_CYCLE/(float)SAMPLE_RATE)

// This class represents a voice of synth. It can have several oscillators
class OscMono
{
public:
   int32_t volglb;                               // Volume global
   int32_t volglbsave;                           // Volume global temporary save  
   int32_t phase[NUM_OSC];
   int16_t waveform[NUM_OSC];                    // Waveform of each oscillator 
   int32_t volosc[NUM_OSC];                      // Volume of each oscillator
   float fine[NUM_OSC];                           // Fine of each oscillator
   float ulPhaseAccumulator[NUM_OSC];          // Position in the reading of each oscillator table 
   float ulPhaseIncrement[NUM_OSC];  // Reading speed of each oscillaotr table 
   int8_t octave[NUM_OSC];                        // Octave of each oscillator

//GLIDE
   uint8_t noteplaying;                           // true : a note is already playing - false : no note is playing
   boolean glideon;                               // true : glide is ON - false : glide is OFF
   boolean glidestart;                            // true : glide already started - false : glide is not started
   boolean play;                                  
   uint32_t glidetime;                            // glide time in ms
   float Incrementglide[NUM_OSC];              // glide speed
   float Incrementfin[NUM_OSC];                // target frequency

   Env env;

   float fFrequency;
	 
   int16_t wave[NUM_OSC][SAMPLES_PER_CYCLE];
   uint8_t pwm_conv[NUM_OSC];
   float fact1, fact2;
   float fl;
   
   float convp[64];
   
   struct notepressed{
		bool          on;
		bool          isplaying;
		uint8_t       pitch; 
   };
   typedef struct notepressed Notepressed;
   
   Notepressed notepressed[8];
   int testinc;
   
// initialize the synth. Here you can put your default values.   
   void init()
   {
     testinc = 0;
     volglbsave = 0;
	 float convp2[]={0,3,3.8,4.4,4.9,5.2,5.7,6,6.3,6.5,6.7,6.9,7.2,7.4,7.6,7.9,8.1,8.3,8.5,8.7,8.9,9.2,9.4,9.6,9.8,10.1,10.3,10.6,10.8,11.1,11.3,11.6,11.8,12,12.3,12.6,12.8,13,13.3,13.6,13.9,14.2,14.5,14.9,15.2,15.6,15.9,16.4,16.9,17.5,18,18.8,19.3,20.2,20.8,21.6,23.5,25,26,29,32,36,51,64};
     for(int i=0; i<64; i++)
		 {
			 convp[i]=convp2[i];
		 }
 	 
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
			pwm_conv[i]=0;
			for(int j=0; j<SAMPLES_PER_CYCLE; j++)
			{
				wave[i][j]=waveformTab[j];
			}
			setPWM(i, 0);
		 }

		 for(int j=0; j<NUM_OSC; j++)
		 {
			 ulPhaseAccumulator[j] = 0; 
			 ulPhaseIncrement[j] = 0;
		 }
		
		 glidestart = false;
		 noteplaying = 0;
		 play = false;
		 volglb = 64;
		 env.init();
	 
//GLIDE
     glideon = false;
     glidetime = 200; // (en ms)
	 
	 fact1=1;
	 fact2=100;
     
   }
   

// Set the note (frequency), and volume 
   bool setNote(uint32_t note, int32_t vol)
   {
// If we are in glide mode and the synth is not playing, we have to compute the glide increment (= glide speed)	
// We do not have to change the frequency value, it's the glide increment which will change it
     bool ret=true;
	 Serial.println(notepressed[0].pitch);
	 Serial.println(notepressed[1].pitch);
	 Serial.println(notepressed[2].pitch);
	 int note2=128;
	 if(vol>0)
	 {
		 for(int i=0; i<8; i++) notepressed[i].isplaying=false;
		 for(int i=0; i<8; i++)
		 {
			 if(!notepressed[i].on)
			 {
				 notepressed[i].on=true;
				 notepressed[i].pitch=note;
				 notepressed[i].isplaying=true;
				 note2=note;
				 i=100;
			 }
		 }
	 }
	 if(vol==0)
	 {
		 for(int i=0; i<8; i++)
		 {
			 if(notepressed[i].on && notepressed[i].pitch==note)
			 {
				 notepressed[i].on=false;
				 if(notepressed[i].isplaying) 
				 {
					 if(i>0)
		             {
						 Serial.println("previous");
						 notepressed[i-1].isplaying=true; 
						 note2=notepressed[i-1].pitch;
						 ret=false;
					 }
					 else note2=129;
				 }
				 for(int j=i; j<7; j++)
				 {
					 notepressed[j].pitch=notepressed[j+1].pitch;
					 notepressed[j].on=notepressed[j+1].on;
					 notepressed[j].isplaying=notepressed[j+1].isplaying;
				 }
			 }
		 }
	 }
	 
	 Serial.println("infoplay");
	 Serial.println(note2);
	 Serial.println(glideon);
	 Serial.println(play);
		 
     if(note2<128 && glideon && play)
     {
       for(int i=0; i<NUM_OSC; i++)
       {
         fFrequency = ((pow(2.0,((note2+12*octave[i])+(fine[i]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
         Incrementfin[i] = fFrequency;
		 Incrementglide[i] = abs(Incrementfin[i] - ulPhaseIncrement[i])*1000.0/44100.0/glidetime;
		 if(Incrementglide[i]==0) Incrementglide[i] = 1;
		 Serial.println("setnote glide");
		 Serial.println(Incrementfin[i]);
		 Serial.println(ulPhaseIncrement[i]);
		 Serial.println(Incrementglide[i]);
       }
	   ret=false;
     }
     if(note2==129) {
			 env.stop();
	 }
	 if(note2<128 && (!glideon || (glideon && !play)))
     {
		 Serial.println("setnote no glide");
		 for(int i=0; i<NUM_OSC; i++)
		 {
			 fFrequency = ((pow(2.0,((note2+12*octave[i])+(fine[i]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
			 ulPhaseIncrement[i] = fFrequency; 
			 Incrementfin[i] = fFrequency; 
			 ulPhaseAccumulator[i]=phase[i];
		 }
		 // The synth is playing
		 play = true;
		 volglb = vol;
		 env.start();		 
     }
	 return ret;

   }

// Set the volume of one oscillator
   void setVolOsc(uint8_t num, int32_t vol)
   {
     volosc[num] = vol>>1;
   }
   
   void setPhaseOsc(uint8_t num, int32_t ph)
   {
		 ph*=2;
		 ulPhaseAccumulator[num]+=(ph-phase[num]);
		 phase[num] = ph;
   }

// Set the waveform of one oscillator
   void setWaveform(uint8_t num, uint8_t val)
   {
     waveform[num] = val;
		 for(int i=0; i<SAMPLES_PER_CYCLE; i++)
		 {
			 wave[num][i]=waveformTab[i+val*1024];
		 }
   }

// Set the fine tune of one oscillator
   void setFine(uint8_t num, int val)
   {
     fine[num] = val;
	 fFrequency = ((pow(2.0,((noteplaying+12*octave[num])+(fine[num]/128)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
	 ulPhaseIncrement[num] = fFrequency; 
	 Incrementfin[num] = fFrequency;
   }
	 
	void setPWM(uint8_t num, uint8_t val)
	 {
		 float val2=val;
		 if(val2<64)
		 {
			val2=convp[val];
		 }

		 pwm_conv[num]=val2;
		 
		 
			if(val2<=64)
			{
				fl = 5+val2*val2*val2/5;
				fact1=1+fl/1024;
				fact2=-fl*fact1;
			}
			else
			{
				fl = 200000/((val2-64)*(val2-64)+8);
				//fl = 3850-val*30;
				fact1=-fl/1024;
				fact2=fact1*(1024+fl);
			}
			
	 }

//Set the glide time
   void setGlideTime(uint8_t glt)
   {
     glidetime = glt * 10+1;
   }
   
    void setenvA(uint32_t val)
	{
		env.setA(val);
	}

	void setenvD(uint32_t val)
	{
		env.setD(val);
	}

	void setenvS(uint32_t val)
	{
		env.setS(val);
	}

	void setenvR(uint32_t val)
	{
		env.setR(val);
	} 

// Compute the table position in the wavetable from increment values
   void next()
   {

	   for(int i=0; i<NUM_OSC; i++)
	   {
		   if(ulPhaseIncrement[i]<Incrementfin[i]) 
		   {
			 if((Incrementfin[i]-ulPhaseIncrement[i])<=Incrementglide[i])
			 {
			   ulPhaseIncrement[i] = Incrementfin[i];
			 }
			 else ulPhaseIncrement[i] += Incrementglide[i];
		   }
		   if(ulPhaseIncrement[i]>Incrementfin[i]) 
		   {
			 if((ulPhaseIncrement[i]-Incrementfin[i])<=Incrementglide[i])
			 {
			   ulPhaseIncrement[i] = Incrementfin[i];
			 }
			 else ulPhaseIncrement[i] -= Incrementglide[i];
		   }
	   }


		if(play)
		{
			for(int i=0; i<NUM_OSC; i++)
			{
				ulPhaseAccumulator[i] += ulPhaseIncrement[i];   
				if(ulPhaseAccumulator[i] >= SAMPLES_PER_CYCLE-1) 
				{ 
					ulPhaseAccumulator[i] -= SAMPLES_PER_CYCLE-1;
				} 

			}
		}
	  
   }

//Return the sample value from the wavetable 
   int16_t output()
   {
     int16_t ret=0;
	 int voldesc;

	 voldesc=0;
	 int volenv=env.amount();
	 //if(volenv>0) Serial.println(volenv);
	 if(!env.started) play=false;
	 
	 for(int i=0; i<NUM_OSC; i++)
	 {
		 if(pwm_conv[i]==0)
		 {
			 ret+= ((((int32_t)wave[i][(int)ulPhaseAccumulator[i]]* volosc[i])>>7)*volenv)>>7;
			 voldesc+=volosc[i];
		 }
		  if(pwm_conv[i]>64) 
		 {
			 int ind = (fact1-fact2/((1023-ulPhaseAccumulator[i]+fl)))*ulPhaseAccumulator[i];
			 ret+= ((((int32_t)wave[i][ind]* volosc[i])>>7)*volenv)>>7;
		 }
		 if(pwm_conv[i]>0 && pwm_conv[i]<=64)
		 {
			 int ind = (fact1+fact2/(ulPhaseAccumulator[i]+fl))*ulPhaseAccumulator[i];
			 ret+= ((((int32_t)wave[i][ind]* volosc[i])>>7)*volenv)>>7;
		 }
	 }
		 
	 if(voldesc<127) voldesc=127;
     return (ret*127/voldesc);
	 
   }
   
};
