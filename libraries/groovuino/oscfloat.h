
#ifndef oscfloat_h
#define oscfloat_h

#include <arduino.h>

#define NUM_OSC 3  // Change this to change the number of oscillators per voice
#define UNI_MAX 8  // Change this to change the max number of oscillators added on unisson mode

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
	uint8_t unisson;
	float volglb[8];                               // Volume global
	float volglbsave;                              // Volume global temporary save  
	int32_t phase[NUM_OSC];
	uint8_t waveform[NUM_OSC];                     // Waveform of each oscillator 
	float volosc[NUM_OSC];                         // Volume of each oscillator
	float fine[NUM_OSC];                           // Fine of each oscillator
	float phase_accu[NUM_OSC][8][UNI_MAX];          // Position in the reading of each oscillator table 
	float phase_inc[NUM_OSC][8][UNI_MAX] ;  // Reading speed of each oscillaotr table 
	float phase_inc_save[NUM_OSC][8][UNI_MAX] ;  // Reading speed of each oscillaotr table 
	uint8_t wavecnt;
	int8_t octave[NUM_OSC];                        // Octave of each oscillator
	float volc[NUM_OSC][UNI_MAX];
	float voldesc;
	int bpm;
	int step_time;

//GLIDE
	uint8_t noteplaying[8];                           // true : a note is already playing - false : no note is playing
	boolean glideon;                               // true : glide is ON - false : glide is OFF
	boolean glidestart[8];                         // true : glide already started - false : glide is not started
	boolean play[8];                               // true : le son joue (même avec le release de l'enveloppe)
    boolean midi_play[8];	                       // true : on a déclenché le son avec une touche - false : on a relaché la touche
	float glidetime;                            // glide time in ms
	float Incrementglide[NUM_OSC][UNI_MAX];              // glide speed
	float Incrementfin[NUM_OSC][UNI_MAX];                // target frequency

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
	float ecartunis;
	
	float unisenv[8];

	Env env[8];

	float fFrequency[8];
	 
	float wave[NUM_OSC][SAMPLES_PER_CYCLE];
	uint8_t pwm_conv[NUM_OSC];
	float fact1[NUM_OSC], fact2[NUM_OSC];
	float fl[NUM_OSC];
   
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
	
	uint8_t pol;
	
	String arpmodes[3]={"Up","Up/Down","Down"};
   
	float convp[64];
	
// initialize the synth. Here you can put your default values.   
	void init()
	{
		
		polyphony=false;
		unisson=1;
		pol=1;
		ecartunis=0.01;
		volglbsave = 0;
		voldesc=0.5;
		previous_step=0;
	    current_step=0;
	    time_stop=100;
		unisenv[0]=1.0;
		unisenv[1]=0.8;
		unisenv[2]=0.8;
		unisenv[3]=0.7;
		unisenv[4]=0.7;
		unisenv[5]=0.6;
		unisenv[6]=0.6;
		unisenv[7]=0.5;
		wavecnt=0;
		bpm = 80;
	    step_time=15000/bpm;
		float convp2[]={0,3,3.8,4.4,4.9,5.2,5.7,6,6.3,6.5,6.7,6.9,7.2,7.4,7.6,7.9,8.1,8.3,8.5,8.7,8.9,9.2,9.4,9.6,9.8,10.1,10.3,10.6,10.8,11.1,11.3,11.6,11.8,12,12.3,12.6,12.8,13,13.3,13.6,13.9,14.2,14.5,14.9,15.2,15.6,15.9,16.4,16.9,17.5,18,18.8,19.3,20.2,20.8,21.6,23.5,25,26,29,32,36,51,64};
		for(int i=0; i<64; i++)	convp[i]=convp2[i];
 	 
		for(int i=0; i<NUM_OSC; i++)
		{
			waveform[i] = 0;
			volosc[i] = 0.5;
			fine[i] = 0;
			phase[i] = 0;
			for(int k=0; k<UNI_MAX; k++)
			{
				Incrementglide[i][k] = 0;
				Incrementfin[i][k] = 0;
				volc[i][k] = 0.5;
			}
			octave[i] = 0;
			pwm_conv[i]=0;
			for(int j=0; j<(int)SAMPLES_PER_CYCLE; j++)
			{
				//wave[i][j]=waveformTab[j];
				wave[i][j]=(float)waveformTab[j]/65536.0;
			}
			setPWM(i, 0);
			fact1[i]=1;
		    fact2[i]=100;
		}
	
		for(int i=0; i<8; i++)
		{
			for(int j=0; j<NUM_OSC; j++)
			{
				for(int k=0; k<UNI_MAX; k++)
				{
					phase_accu[j][i][k] = 0; 
					phase_inc[j][i][k] = 0;
					phase_inc_save[j][i][k] = 0;
				}
			}
			glidestart[i] = false;
			noteplaying[i] = 0;
			play[i] = false;
			midi_play[i] = false;
			volglb[i] = 0.5;
			env[i].init();
		}
		 
	//GLIDE
		glideon = true;
		glidetime = 200.0; // (en ms)
		 
		
	
		arpmode=0;
		arpon=false;
		arprate = 1;
	   	arpstep = 1;
	   	arphigh = 0;
	   	arpgate = 100;
	   	note_to_play[64];
	  	nb_note_to_play = 0;
	}
	
	
	float compute_freq(int notefreq, int numosc)
	{
		return ((pow(2.0,((notefreq+12*octave[numosc])+(fine[numosc]/256)-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE;
	}
	
	void update_polyphony(uint8_t po)
	{
		pol=po; 
		if(po>1) {
			polyphony=true; 
			if(unisson>1) voldesc=0.3;
			else voldesc=0.5;
		}
		else {
			polyphony=false; 
			if(unisson>1) voldesc=0.7;
			else voldesc=1.0;
		}
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
		
		//----------------- MONO --------------------
		if(!polyphony)
		{
			//Serial.println("mono");
			//Serial.println(vol);
			
			//== Note On==
			if(vol>0)
			{
				//On passe la première note dispo à "ON" et "ISPLAYING"
				for(int i=0; i<8; i++) notepressed[i].isplaying=false;
				for(int i=0; i<8; i++)
				{
					if(!notepressed[i].on)
					{
					 //Serial.println(i);
					 //midi_play[0]=true;
					 notepressed[i].on=true;
					 notepressed[i].pitch=note;
					 notepressed[i].isplaying=true;
					 note2=note;
					 playmono=true;
					 i=100;
				    }
			    }
		    }
			//== Note Off==
		    if(vol==0)
		    {
			 //Dans le tableau, on cherche la note qu'on veut arrêter et on la passe à "ON"=false.
			 // Si une note était jouée avant dans le tableau on la passe à "ISPLAYING"
			 // Si on a forcé le "ISPLAYING", on met ret=false
			 // On décalle ensuite les valeurs du tableau qui suivent la note vers la gauche
			 //Serial.println("vol 0");
			 for(int i=0; i<8; i++)
			 {
				 if(notepressed[i].on && notepressed[i].pitch==note)
				 {
					 notepressed[i].on=false;
					 if(i>0)
					 {
						 if(notepressed[i].isplaying)
						 {
							 notepressed[i-1].isplaying=true; 
							 note2=notepressed[i-1].pitch;
							 ret=false;
						 }
					 }
					 if(i==0)
					 {
						 if(notepressed[i].isplaying)
						 {
							 if(notepressed[1].on) 
							 {
								 note2=notepressed[1].pitch;
								 notepressed[1].isplaying=true; 
								 ret=false;
							 }
							 else {
								 note2=129;
							 }
						 }
						 else 
						 {
							 ret=false;
						 }
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
		 
		//== Exécuté tout le temps ==
		 if(note2<128 && glideon && midi_play[0])
		 {
		   for(int i=0; i<NUM_OSC; i++)
		   {
				 fFrequency[0] = compute_freq(note2, i); 
				 
				 
				 for(int k=0; k<UNI_MAX; k++)
				 {
					 Incrementfin[i][k] = fFrequency[0]*(1.0+k*ecartunis);
					 float ftemp=fabs(Incrementfin[i][k] - phase_inc[i][0][k]);
					 Incrementglide[i][k] = ftemp/44.1/glidetime;
					 if(Incrementglide[i][k]==0.0) Incrementglide[i][k] = 1;
				 }
				 
		   }
		   ret=false;
		 }
		 
		 if(note2==129 ) {
				 env[0].stop();
				 midi_play[0]=false;
		 }
		 if(note2<128 && (!glideon || (glideon && !midi_play[0])))
		 {
			 midi_play[0]=true;
			 for(int i=0; i<NUM_OSC; i++)
			 {
				 fFrequency[0] = compute_freq(note2, i); 
				 
				 
				 for(int k=0; k<UNI_MAX; k++)
				 {
					 int fact=k%2;
					 fact=fact*2-1;
					 phase_inc[i][0][k] = fFrequency[0]*pow((1.0+(float)fact*ecartunis),k); 
					 phase_inc_save[i][0][k]=phase_inc[i][0][k];
					 phase_accu[i][0][k] = phase[i];
					 Incrementfin[i][k] =  phase_inc[i][0][k]; 
				 }
			 }
			 // The synth is playing
			 env[0].start();
			 play[0]=true;
			 volglb[0] = (float)vol/127.0;
			 
			 		 
		 }
	 }
	 
	 //----------------- POLYPHONIC -------------------
	 else
	 {
		 if(vol>0)
		 {
			 for(int i=0; i<8; i++)
			 {
				 if(!notepressed[i].isplaying)
				 {
					 Serial.println("play note");
					 Serial.println(i);
					 notepressed[i].on=true;
					 notepressed[i].pitch=note;
					 notepressed[i].isplaying=true;
					 env[i].start();
					 play[i]=true;					 
					 volglb[i] = (float)vol/127.0;
					 for(int j=0; j<NUM_OSC; j++)
					 {
						 fFrequency[i] = compute_freq(note, j); 
						 for(int k=0; k<UNI_MAX; k++)
						 {
							 int fact=k%2;
					         fact=fact*2-1;
							 phase_inc[j][i][k] = fFrequency[i]*pow((1.0+(float)fact*ecartunis),k); 
							 phase_inc_save[j][i][k]=phase_inc[j][i][k];
							 phase_accu[j][i][k] = phase[j];
						 }
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

		 Serial.println("tableau");
		 for(int i=0; i<8; i++)
		 {
			Serial.print("note ");
			Serial.println(i);
			Serial.println(notepressed[i].pitch);
			Serial.println(notepressed[i].on);
			Serial.println(notepressed[i].isplaying);
		 }
		 
     }
	 if(arpon)
	 {
		 compute_notes();
	 }
	 return ret;

   }
   
    void update_gate(uint8_t _gate)
	{
		arpgate = _gate;
		time_stop = step_time*(float)arpgate*0.00787401574;
	}
	
	void update_bpm(uint8_t _bpm)
	{
		bpm=_bpm+50;
		step_time=15000/bpm;
	}

// Stop the playing of the synth
   void stop(uint8_t note)
   {
		 for(int j=0; j<8; j++)
		 {
			 if(note==noteplaying[j]) play[j] = false;
		 }
   }
   
   String arpmodename(uint8_t num)
	{
		return arpmodes[num];
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

// Set the volume of one oscillator
   void setVolOsc(uint8_t num, int32_t vol)
   {
     volosc[num] = vol/127.0;
	 for(int k=0; k<unisson; k++)
	 {
		volc[num][k]=volosc[num]*unisenv[k];
     }
   }
   
   void setPhaseOsc(uint8_t num, uint8_t ph)
   {
		 for(int j=0; j<8; j++)
		 {
			 for(int k=0; k<unisson; k++)
			 {
				 phase_accu[num][j][k]+=(ph*8);
				 if(phase_accu[num][j][k]>1023) phase_accu[num][j][k]-=1024;
			 }
		 }
		 phase[num] = ph*8;
   }

// Set the waveform of one oscillator
   void setWaveform(uint8_t num, uint8_t val)
   {
     waveform[num] = val;
	 for(int i=0; i<SAMPLES_PER_CYCLE; i++)
	 {
		 //Serial.println(waveformTab[i+val*1024]);
		 wave[num][i]=(float)waveformTab[i+val*1024]/65536.0;
	 }
   }
   
   
// Set the fine tune of one oscillator
   void setFine(uint8_t num, int val)
   {
		fine[num] = val;
		for(int j=0; j<8; j++)
		{
			fFrequency[j] = compute_freq(noteplaying[j], num); 
		 
			for(int k=0; k<unisson; k++)
			{
			    int fact=k%2;
			    fact=fact*2-1;
				phase_inc[num][j][k] = fFrequency[j]*pow((1.0+(float)fact*ecartunis),k); 
				phase_inc_save[num][j][k]=phase_inc[num][j][k];
				if(!polyphony) Incrementfin[num][k] = phase_inc[num][j][k];
			}
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
				fl[num] = 5.0+val2*val2*val2*0.2;
				fact1[num]=1+fl[num]*0.0009765625;
				fact2[num]=-fl[num]*fact1[num];
			}
			else
			{
				fl[num] = 200000/((val2-64)*(val2-64)+8);
				//fl = 3850-val*30;
				fact1[num]=-fl[num]/1024;
				fact2[num]=fact1[num]*(1024+fl[num]);
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
	   //for(int i=0; i<1; i++)
	   {
			 for(int k=0; k<unisson; k++)
			 //for(int k=0; k<1; k++)
			 {
				 if(phase_inc[i][0][k]<Incrementfin[i][k]) 
				 {
					 if((Incrementfin[i][k]-phase_inc[i][0][k])<Incrementglide[i][k])
					 {
						 phase_inc[i][0][k] = Incrementfin[i][k];
					 }
					 else phase_inc[i][0][k] += Incrementglide[i][k];
				 }
				 if(phase_inc[i][0][k]>Incrementfin[i][k]) 
				 {
					 if((phase_inc[i][0][k]-Incrementfin[i][k])<Incrementglide[i][k])
					 {
						 phase_inc[i][0][k] = Incrementfin[i][k];
					 }
					 else phase_inc[i][0][k] -= Incrementglide[i][k];
				 }
			 }
	   }
	 }
	 if(unisson>1)
	 {
		 for(int j=0; j<pol; j++)
		 //for(int j=0; j<1; j++)
		 {
			 if(play[j])
			 {
				for(int k=0; k<unisson; k++)
				//for(int k=0; k<1; k++)
				{
					 phase_accu[0][j][k] += phase_inc[0][j][k];   
					 if(phase_accu[0][j][k] >= SAMPLES_PER_CYCLE) 
					 { 
						if(wavecnt>=3)
						{
							wavecnt=0;
						    phase_accu[0][j][k] = 0; 
						}
						else 
						{
							wavecnt++;
						    phase_accu[0][j][k] -= SAMPLES_PER_CYCLE; 
						}
					 } 
				 }
			 }
		 }
	 }
     else
	 {
		 for(int j=0; j<pol; j++)
		 //for(int j=0; j<1; j++)
		 {
			 if(play[j])
			 {
				 for(int i=0; i<NUM_OSC; i++)
				 //for(int i=0; i<1; i++)
				 {

					 phase_accu[i][j][0] += phase_inc[i][j][0];   
					 if(phase_accu[i][j][0] >= SAMPLES_PER_CYCLE) 
					 { 
						 phase_accu[i][j][0] -= SAMPLES_PER_CYCLE; 
					 } 
				 }
			 }
		 }
	 }

	 if(arpon && !polyphony  && nb_note_to_play>0)
		 {
			 int tim = millis()-previous_step;
			 if(tim>(step_time*arprate)) 
			 {
				 if(arpgate<127)
				 {
					 for(int i=0; i<NUM_OSC; i++)
					 {
						 fFrequency[0] = compute_freq(note_to_play[current_step], i); 
						 for(int k=0; k<unisson; k++)
						 {
							 int fact=k%2;
			                 fact=fact*2-1;
							 phase_inc[i][0][k] = fFrequency[0]*pow((1+fact*ecartunis),k); 
							 phase_inc_save[i][0][k]=phase_inc[i][0][k];
							 Incrementfin[i][k] = phase_inc[i][0][k]; 
							 phase_accu[i][0][k]=phase[i];
						 }
					 }
					 env[0].start();
				 }
				 else
				 {
					 for(int i=0; i<NUM_OSC; i++)
		             {
						 fFrequency[0] = compute_freq(note_to_play[current_step], i);
						 for(int k=0; k<unisson; k++)
						 {			
							int fact=k%2;
			                 fact=fact*2-1;
							 Incrementfin[i][k] = fFrequency[0]*pow((1+fact*ecartunis),k);
							 Incrementglide[i][k] = fabs(Incrementfin[i][k] - phase_inc[i][0][k])/44.1/glidetime;
							 if(Incrementglide[i][k]==0) Incrementglide[i][k] = 1;
						 }
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
					 env[0].stop();
				 }
			 }
		 }
   }
   
   void set_pitch_lfo(float val)
   {
	   float va=0.75+val*0.5;
	   if(unisson>1)
	   {

		 for(int j=0; j<pol; j++)
		 {
			 for(int k=0; k<unisson; k++)
			 {
				phase_inc[0][j][k] = phase_inc_save[0][j][k]*va ; 
				 
			 }
		 }
	   }
	   else
	   {
		 for(int i=0; i<NUM_OSC; i++)
		 {
			 for(int j=0; j<pol; j++)
			 {
				phase_inc[i][j][0] = phase_inc_save[i][j][0]*va ; 
			 }
		 }
	  }
   }
   
   void compute_detune(int val)
   {
	   ecartunis=(float)val*0.0001;
	   for(int i=0; i<NUM_OSC; i++)
		 {
			 for(int k=1; k<8; k++)
			 {
				 int fact=k%2;
				 fact=fact*2-1;
				 phase_inc[i][0][k] = fFrequency[0]*pow((1+fact*ecartunis),k); 
				 phase_inc_save[i][0][k]=phase_inc[i][0][k];
				 Incrementfin[i][k] = phase_inc[i][0][k]; 
			 }
		 }
   }
   
   void compute_unisson_vol(int val)
   {
	   for(int k=0; k<UNI_MAX; k++)
	   {
		   int fact=(k+1)/2;
		   unisenv[k]=1.0-(float)val/512.0*fact;
		   if(unisenv[k]<0) unisenv[k]=0;
		   for(int i=0; i<NUM_OSC; i++)
			 {
				volc[i][k]=volosc[i]*unisenv[k];
			 }
	   }
   }
   
   void set_unisson(int val)
   {
	   unisson=val;
	   if(val>1)
	   {
		   if(polyphony) voldesc=0.3;
		   else voldesc=0.5;
		   for(int j=0; j<SAMPLES_PER_CYCLE; j++)
		   {
			  //Serial.println(waveformTab[i+val*1024]);
			  wave[0][j]=0.0;
		   }
		   for(int i=0; i<NUM_OSC; i++)
		   {
			   for(int j=0; j<SAMPLES_PER_CYCLE; j++)
			   {
				  //Serial.println(waveformTab[i+val*1024]);
				  wave[0][j]+=(float)waveformTab[j+waveform[i]*1024]*0.00001525878;
			   }
		   }
	   }
	   else
	   {
		   if(polyphony) voldesc=0.7;
		   else voldesc=1.0;
		   for(int i=0; i<NUM_OSC; i++)
		   {
			   for(int j=0; j<SAMPLES_PER_CYCLE; j++)
			   {
				  //Serial.println(waveformTab[i+val*1024]);
				  wave[i][j]=(float)waveformTab[j+waveform[i]*1024]*0.00001525878;
			   }
		   }
	   }
   }

//Return the sample value from the wavetable 
	float output()
    {
		float ret=0;
		//float voldesc;
		
		for(int j=0; j<pol; j++)
		{
			//voldesc=0.0;
			float volenv=env[j].amount()*volglb[j];
			if(!env[j].started && play[j]) {play[j]=false; notepressed[j].isplaying=false; Serial.print("end env : "); Serial.println(j);}


			if(unisson>1)
			{
				for(int k=0; k<unisson; k++)
				//for(int k=0; k<1; k++)
				{
					if(pwm_conv[0]==0)
					{
						ret+= wave[0][(int)phase_accu[0][j][k]]* volc[0][k]*volenv;
						//ret+= wave[i][(int)phase_accu[i][j][k]]* volosc[i]*volenv*;
						//ret+= wave[i][(int)phase_accu[i][j][k]]* volosc[i];
						//ret+= wave[i][(int)phase_accu[i][j][k]];
					}
					if(pwm_conv[0]>64) 
					{
						int ind = (fact1[0]-fact2[0]/((1023-phase_accu[0][j][k]+fl[0])))*phase_accu[0][j][k];
						ret+= wave[0][ind]* volosc[0]*volenv;
					}
					if(pwm_conv[0]>0 && pwm_conv[0]<=64)
					{
						int ind = (fact1[0]+fact2[0]/(phase_accu[0][j][k]+fl[0]))*phase_accu[0][j][k];
						ret+= wave[0][ind]* volosc[0]*volenv;
					}
					//voldesc+=volosc[i];
				}
				
			}
			else
			{
				for(int i=0; i<NUM_OSC; i++)
				//for(int i=0; i<1; i++)
				{
					if(pwm_conv[i]==0)
					{
						ret+= wave[i][(int)phase_accu[i][j][0]]* volc[i][0]*volenv;
						//ret+= wave[i][(int)phase_accu[i][j][k]]* volosc[i]*volenv*;
						//ret+= wave[i][(int)phase_accu[i][j][k]]* volosc[i];
						//ret+= wave[i][(int)phase_accu[i][j][k]];
					}
					if(pwm_conv[i]>64) 
					{
						int ind = (fact1[i]-fact2[i]/((1023-phase_accu[i][j][0]+fl[i])))*phase_accu[i][j][0];
						ret+= wave[i][ind]* volosc[i]*volenv;
					}
					if(pwm_conv[i]>0 && pwm_conv[i]<=64)
					{
						int ind = (fact1[i]+fact2[i]/(phase_accu[i][j][0]+fl[i]))*phase_accu[i][j][0];
						ret+= wave[i][ind]* volosc[i]*volenv;
					}
					//voldesc+=volosc[i];
				}
			}	
		}

		/*count2++;
		 if(count2>10000)
		 {
		   count2=0;
		   Serial.println("b ");
		   //Serial.println(ret/voldesc);
		   Serial.println(phase_accu[0][0][0]);
		   Serial.println(wave[0][(int)phase_accu[0][0][0]]);
		   Serial.println(ret);
		   Serial.println(voldesc);
		 }*/
		 //voldesc=4;
		/*if(voldesc<1.0) voldesc=1.0;
			return (ret/voldesc);*/
			return (ret*voldesc);
		//return wave[0][(int)phase_accu[0][0][0]];
	}
   
};
#endif