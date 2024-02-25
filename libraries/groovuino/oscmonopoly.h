#ifndef oscmonopoly_h
#define oscmonopoly_h

#include <arduino.h>

// Change this to change the number of oscillators per voice
#define NUM_OSC 3

// Can adjust the Sample rate
#define SAMPLE_RATE 44100.0 

// Number of samples played in one cycle (a cycle depends on the frequency of the note played)
#define SAMPLES_PER_CYCLE 1024 

#define TICKS_PER_CYCLE (float)((float)SAMPLES_PER_CYCLE/(float)SAMPLE_RATE)

int count2=0;

// This class represents a voice of synth. It can have several oscillators
class OscMonoPoly
{
public:
   bool polyphony;
   int32_t volglb[8];                               // Volume global
   int32_t volglbsave;                           // Volume global temporary save  
   int32_t phase[NUM_OSC];
   int16_t waveform[NUM_OSC];                    // Waveform of each oscillator 
   int32_t volosc[NUM_OSC];                      // Volume of each oscillator
   float fine[NUM_OSC];                           // Fine of each oscillator
   float ulPhaseAccumulator[NUM_OSC][8];          // Position in the reading of each oscillator table 
   float ulPhaseIncrement[NUM_OSC][8] ;  // Reading speed of each oscillaotr table 
   int8_t octave[NUM_OSC];                        // Octave of each oscillator

//GLIDE
   uint8_t noteplaying[8];                           // true : a note is already playing - false : no note is playing
   boolean glideon;                               // true : glide is ON - false : glide is OFF
   boolean glidestart[8];                            // true : glide already started - false : glide is not started
   boolean play[8];                                  
   uint32_t glidetime;                            // glide time in ms
   float Incrementglide[NUM_OSC];              // glide speed
   float Incrementfin[NUM_OSC];                // target frequency

//ARP
	bool arpon;
	uint8_t arpmode;
	uint8_t arprate;
	uint8_t arpstep;
	int8_t arphigh;
	uint8_t arpgate ;
	int note_to_play[64];
	int nb_note_to_play;
	float time_stop;

   Env env[8];

   float fFrequency[8];
	 
   int16_t wave[NUM_OSC][SAMPLES_PER_CYCLE];
   uint8_t pwm_conv[NUM_OSC];
   float fact1, fact2;
   float fl;
   
   struct notepressed{
		bool          on;
		bool          isplaying;
		uint8_t       pitch; 
   };
   typedef struct notepressed Notepressed;
   
   Notepressed notepressed[8];
   int testinc;
   
   int previous_step;
   int current_step;
   
   float convp[64];
// initialize the synth. Here you can put your default values.   
   void init()
   {
     polyphony=false;
     volglbsave = 0;
	 previous_step=0;
	 current_step=0;
	 time_stop=100;
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
			for(int j=0; j<(int)SAMPLES_PER_CYCLE; j++)
			{
				wave[i][j]=waveformTab[j];
			}
			setPWM(i, 0);
		 }

		 for(int i=0; i<8; i++)
		 {
			 for(int j=0; j<NUM_OSC; j++)
			 {
				 ulPhaseAccumulator[j][i] = 0; 
				 ulPhaseIncrement[j][i] = 0;
			 }
			 glidestart[i] = false;
			 noteplaying[i] = 0;
			 play[i] = false;
			 volglb[i] = 64;
			 env[i].init();
		 }
	 
//GLIDE
     glideon = false;
     glidetime = 200; // (en ms)
		
		arpmode=0;
		arpon=true;
		arprate = 1;
	   	arpstep = 1;
	   	arphigh = 0;
	   	arpgate = 100;
	   	note_to_play[64];
	  	nb_note_to_play = 0;
	 
	 fact1=1;
	 fact2=100;
     
   }
   
   void compute_notes()
	 {
		 int ind=0;
		 // UP
		 if(arpmode==0)
		 {
			 for(int j=0; j<arpstep; j++)
			 {
				 int i=0;
				 while(notepressed[i].on)
			   {
					 note_to_play[ind]=notepressed[i].pitch+12*j;
					 i++;
					 ind++;
				 }
			 }
		 }
		 // UP/DOWN
		 if(arpmode==1)
		 {
			 for(int j=0; j<arpstep; j++)
			 {
			   int i=0;
			   while(notepressed[i].on)
				 {
				 	 note_to_play[ind]=notepressed[i].pitch+12*j;
					 i++;
					 ind++;
				 }

			 }
			 for(int j=arpstep-1; j>=0; j--)
			 {
				 int i=0;
			 	 while(i>0)
				 {
					 note_to_play[ind]=notepressed[i].pitch+12*j;
					 i--;
					 ind++;
				 }
			 }
		 }
		// DOWN
		if(arpmode==2)
		{
			for(int j=arpstep-1; j>=0; j--)
			{
				int i=7;
				while(i>=0)
				{
					if(notepressed[i].on)
					{
						note_to_play[ind]=notepressed[i].pitch+12*j;
						ind++;
					}
					i--;
				}
			}
		}
		nb_note_to_play=ind;
	}
   

// Set the note (frequency), and volume 
   bool setNote(uint32_t note, int32_t vol)
   {
// If we are in glide mode and the synth is not playing, we have to compute the glide increment (= glide speed)	
// We do not have to change the frequency value, it's the glide increment which will change it
	 bool ret=true;
	 //Serial.println("SetNote");
	 //Serial.println(note);
	 int note2=128;
	 bool playmono=false;
	 current_step=0;
	 if(!polyphony)
	 {
		 Serial.println("mono");
		 Serial.println(vol);
		 if(vol>0)
		 {
			 for(int i=0; i<8; i++) notepressed[i].isplaying=false;
			 for(int i=0; i<8; i++)
			 {
				 if(!notepressed[i].on)
				 {
					 Serial.print("note num ");
					 Serial.println(i);
					 notepressed[i].on=true;
					 notepressed[i].pitch=note;
					 notepressed[i].isplaying=true;
					 note2=note;
					 playmono=true;
					 i=100;
				 }
			 }
		 }
		 if(vol==0)
		 {
			 //Serial.println("vol 0");
			 for(int i=0; i<8; i++)
			 {
				 //Serial.print("occ ");
				 //Serial.println(i);
				 //Serial.println(notepressed[i].on);
				 //Serial.println(notepressed[i].pitch);
				 if(notepressed[i].on && notepressed[i].pitch==note)
				 {
					 //Serial.println("delete note");
					 //Serial.println(notepressed[i].isplaying);
					 notepressed[i].on=false;
					 
					 //Serial.print("found note : ");
					 //Serial.println(i);
					 if(i>0)
					 {
						 //Serial.println("previous");
						 if(notepressed[i].isplaying)
						 {
							 notepressed[i-1].isplaying=true; 
							 note2=notepressed[i-1].pitch;
							 ret=false;
						 }
					 }
					 if(i==0)
					 {
						 //Serial.println("next");
						 //Serial.println(notepressed[1].on);
						 if(notepressed[i].isplaying)
						 {
							 //Serial.println("is playing");
							 //Serial.println(notepressed[1].on);
							 if(notepressed[1].on) 
							 {
								 note2=notepressed[1].pitch;
								 notepressed[1].isplaying=true; 
								 ret=false;
							 }
							 else note2=129;
						 }
						 else ret=false;
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
		 
		
		 
		 Serial.print("note2 : ");
		 Serial.println(note2);
		 Serial.print("glideon : ");
		 Serial.println(glideon);
		 Serial.print("play[0] : ");
		 Serial.println(play[0]);
		 if(note2<128 && glideon && play[0])
		 {
		   for(int i=0; i<NUM_OSC; i++)
		   {
			 fFrequency[0] = ((pow(2.0,((note2+12*octave[i])+(fine[i]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
			 Incrementfin[i] = fFrequency[0];
			 Incrementglide[i] = abs(Incrementfin[i] - ulPhaseIncrement[i][0])*1000.0/44100.0/glidetime;
			 if(Incrementglide[i]==0) Incrementglide[i] = 1;
			 Serial.println("setnote glide");
			 /*Serial.println(i);
			 Serial.println(NUM_OSC);
			 Serial.println(Incrementfin[i]);
			 Serial.println(ulPhaseIncrement[i][0]);
			 Serial.println(Incrementglide[i]);*/
		   }
		   ret=false;
		 }
		 
		 if(note2==129 ) {
				 env[0].stop();
		 }
		 if(note2<128 && (!glideon || (glideon && !play[0])))
		 {
			 Serial.println("setnote no glide");
			 for(int i=0; i<NUM_OSC; i++)
			 {
				 fFrequency[0] = ((pow(2.0,((note2+12*octave[i])+(fine[i]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
				 ulPhaseIncrement[i][0] = fFrequency[0]; 
				 Incrementfin[i] = fFrequency[0]; 
				 ulPhaseAccumulator[i][0]=phase[i];
			 }
			 // The synth is playing
			 play[0]=true;
			 volglb[0] = vol;
			 env[0].start();		 
		 }
	 }
	 
	 else
	 {
		 //Serial.println("poly");
		 //Serial.println(vol);
		 if(vol>0)
		 {
			 for(int i=0; i<8; i++)
			 {
				 if(!notepressed[i].isplaying)
				 {
					 //Serial.println("play note");
					 //Serial.println(i);
					 notepressed[i].on=true;
					 notepressed[i].pitch=note;
					 notepressed[i].isplaying=true;
					 env[i].start();
					 play[i]=true;					 
					 volglb[i] = vol;
					 for(int j=0; j<NUM_OSC; j++)
					 {
						 fFrequency[i] = ((pow(2.0,((note+12*octave[j])+(fine[j]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
						 ulPhaseIncrement[j][i] = fFrequency[i]; 
						 ulPhaseAccumulator[j][i]=phase[i];
						 //Serial.println(ulPhaseIncrement[j][i]);
					 }
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
					 //Serial.println("stop note");
					 notepressed[i].on=false;
					 notepressed[i].pitch=0;
					 env[i].stop();
				 }
			 }
			 for(int i=0; i<8; i++)
			 {
				 if(notepressed[i].on) ret=false;
			 }
		 }
		 /*Serial.println("tableau");
		 for(int i=0; i<8; i++)
		 {
			Serial.print("note ");
			Serial.println(i);
			Serial.println(notepressed[i].pitch);
			Serial.println(notepressed[i].on);
			Serial.println(notepressed[i].isplaying);
		 }*/
		 
     }
	 if(arpon)
	 {
		 compute_notes();
	 }
	 //Serial.println("end setnote");
	 return ret;

   }
   
   void update_gate(uint8_t _gate)
	{
		arpgate = _gate;
		time_stop = step_time*(float)arpgate/127;
	}

// Stop the playing of the synth
   void stop(uint8_t note)
   {
		 for(int j=0; j<8; j++)
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
		 for(int j=0; j<8; j++)
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
			 wave[num][i]=waveformTab[i+val*1024];
		 }
   }
   
   void SetPolyphony(bool pol)
   {
	   polyphony=pol;
   }

// Set the fine tune of one oscillator
   void setFine(uint8_t num, int val)
   {
     fine[num] = val;
		 for(int j=0; j<8; j++)
		 {
			 fFrequency[j] = ((pow(2.0,((noteplaying[j]+12*octave[num])+(fine[num]/128)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
			 ulPhaseIncrement[num][j] = fFrequency[j]; 
			 if(!polyphony) Incrementfin[num] = fFrequency[j];
		 }
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
		for(int j=0; j<8; j++)
		{
			env[j].setA(val);
		}
	}

	void setenvD(uint32_t val)
	{
		for(int j=0; j<8; j++)
		{
			env[j].setD(val);
		}
	}

	void setenvS(uint32_t val)
	{
		for(int j=0; j<8; j++)
		{
			env[j].setS(val);
		}
	}

	void setenvR(uint32_t val)
	{
		for(int j=0; j<8; j++)
		{
			env[j].setR(val);
		}
	} 

// Compute the table position in the wavetable from increment values
   void next()
   {
	 if(!polyphony)
	 {
	   for(int i=0; i<NUM_OSC; i++)
	   {
		   if(ulPhaseIncrement[i][0]<Incrementfin[i]) 
		   {
			 if((Incrementfin[i]-ulPhaseIncrement[i][0])<Incrementglide[i])
			 {
			   ulPhaseIncrement[i][0] = Incrementfin[i];
			 }
			 else ulPhaseIncrement[i][0] += Incrementglide[i];
		   }
		   if(ulPhaseIncrement[i][0]>Incrementfin[i]) 
		   {
			 if((ulPhaseIncrement[i][0]-Incrementfin[i])<Incrementglide[i])
			 {
			   ulPhaseIncrement[i][0] = Incrementfin[i];
			 }
			 else ulPhaseIncrement[i][0] -= Incrementglide[i];
		   }
	   }
	 }
     for(int j=0; j<8; j++)
	 {
		 if(play[j])
		 {
			 for(int i=0; i<NUM_OSC; i++)
			 {
			   ulPhaseAccumulator[i][j] += ulPhaseIncrement[i][j];   
			   if(ulPhaseAccumulator[i][j] >= SAMPLES_PER_CYCLE) 
			   { 
				 ulPhaseAccumulator[i][j] -= SAMPLES_PER_CYCLE; 
			   } 
			   
			 }
		 }
	 }
	  if(arpon && !polyphony && nb_note_to_play>0)
		 {
			 int tim = millis()-previous_step;
			 if(tim>(step_time*arprate)) 
			 {
				 /*Serial.print("step : ");
				 Serial.println(current_step);
				 Serial.println(note_to_play[current_step]);*/
				 if(arpgate<127)
				 {
					 for(int i=0; i<NUM_OSC; i++)
		             {
						 fFrequency[0] = ((pow(2.0,((note_to_play[current_step]+12*octave[i])+(fine[i]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
						 ulPhaseIncrement[i][0] = fFrequency[0]; 
						 Incrementfin[i] = fFrequency[0]; 
						 ulPhaseAccumulator[i][0]=phase[i];
					 }
					 env[0].start();
				 }
				 else
				 {
					 for(int i=0; i<NUM_OSC; i++)
		             {
						 fFrequency[0] = ((pow(2.0,((note_to_play[current_step]+12*octave[i])+(fine[i]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
						 Incrementfin[i] = fFrequency[0];
						 Incrementglide[i] = abs(Incrementfin[i] - ulPhaseIncrement[i][0])*1000.0/44100.0/glidetime;
						 if(Incrementglide[i]==0) Incrementglide[i] = 1;
					 }
				 }
				 play[0]=true;
				 current_step++;
				 if(current_step>=nb_note_to_play) current_step=0;
				 
				 previous_step=millis();

			 }
			 else
			 {
				 if(tim>time_stop && env[0].activated) 
				 {
					 /*Serial.println("stop");
					 Serial.println(tim);
					 Serial.println(time_stop);
					 Serial.println("--------");*/
					 env[0].stop();
				 }
			 }
		 }
      
   }

//Return the sample value from the wavetable 
   int16_t output()
   {
     int16_t ret=0;
	 int voldesc;
	 int pol;
	 
	 if(polyphony) pol=8;
	 else pol=1;
	 
	 for(int j=0; j<pol; j++)
	 {
		 voldesc=0;
		 int volenv=env[j].amount();
		 //if(volenv>0) Serial.println(volenv);
		 if(!env[j].started && play[j]) {play[j]=false; notepressed[j].isplaying=false;}
		 
		 
		 for(int i=0; i<NUM_OSC; i++)
	     //for(int i=0; i<1; i++)
		 {
			 if(pwm_conv[i]==0)
			 {
				 ret+= ((((int32_t)wave[i][(int)ulPhaseAccumulator[i][j]]* volosc[i])>>7)*volenv)>>7;
				 //ret+= ((int32_t)wave[i][(int)ulPhaseAccumulator[i][j]]* volosc[i])>>7;
				 voldesc+=volosc[i];
			 }
			  if(pwm_conv[i]>64) 
			 {
				 int ind = (fact1-fact2/((1023-ulPhaseAccumulator[i][j]+fl)))*ulPhaseAccumulator[i][j];
				 ret+= ((((int32_t)wave[i][ind]* volosc[i])>>7)*volenv)>>7;
			 }
			 if(pwm_conv[i]>0 && pwm_conv[i]<=64)
			 {
				 int ind = (fact1+fact2/(ulPhaseAccumulator[i][j]+fl))*ulPhaseAccumulator[i][j];
				 ret+= ((((int32_t)wave[i][ind]* volosc[i])>>7)*volenv)>>7;
			 }
		 }
	 }
	 /*count2++;
     if(count2>50)
     {
       count2=0;
	   Serial.print("b ");
       Serial.println((int)ulPhaseAccumulator[0][0]);
     }*/
	 if(voldesc<127) voldesc=127;
     return (ret*127/voldesc);
	 //return ret;
	 //return wave[0][(int)ulPhaseAccumulator[0][0]];
	 
   }
   
};
#endif