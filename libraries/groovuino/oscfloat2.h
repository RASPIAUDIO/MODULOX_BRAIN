
#ifndef oscfloat_h
#define oscfloat_h

#include <arduino.h>
#include <envfloat.h>
#include "FFat.h"

int16_t *waveformTab;

#define WAVEFORM_NUMBER 128
#define WAVEFORM_SIZE 1024

#ifndef NUM_OSC
#define NUM_OSC 3  // Change this to change the number of oscillators per voice
#endif
#ifndef UNI_MAX
#define UNI_MAX 8  // Change this to change the max number of oscillators added on unisson mode
#endif

// Can adjust the Sample rate
#define SAMPLE_RATE 44100.0 

// Number of samples played in one cycle (a cycle depends on the frequency of the note played)
#define SAMPLES_PER_CYCLE 1024 

#define TICKS_PER_CYCLE (float)((float)SAMPLES_PER_CYCLE/(float)SAMPLE_RATE)

int count2=0;

void wavef_init()
{
  for(int fn=0; fn<44; fn++)
  {
    String nu = "/wavef" + String(fn+1) + ".wav";
    fs::File file = FFat.open(nu, "r");
    Serial.println(nu);

    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }
    
    int i=0;
    int wav_size=0;
    int chan_num=0;
    int smp_rate=0;
    int bps=0;
    int find_data=0;
    int ind_data=10000;
    int n=0;
    
    while(file.available()){
      char val=file.read();
      if(i==0) Serial.write(val);
      if(i==1) Serial.write(val);
      if(i==2) Serial.write(val);
      if(i==3) Serial.write(val);
      if(i==4) wav_size+=val;
      if(i==5) wav_size+=val*256;
      if(i==22) chan_num+=val;
      if(i==23) chan_num+=val*256;
      if(i==24) smp_rate+=val;
      if(i==25) smp_rate+=val*256;
      if(i==26) smp_rate+=val*65536;
      if(i==27) smp_rate+=val*16777216;
      if(i==34) bps+=val;
      if(i==35) bps+=val*256;
      if(i<200)
      {
        if(val==0x64 && find_data==0) find_data=1;
        if(val==0x61 && find_data==1) find_data=2;
        if(val==0x74 && find_data==2) find_data=3;
        if(val==0x61 && find_data==3) {ind_data=i+5;find_data=0;}
      }
      if(i>=(ind_data))
      {
        if(chan_num==1 && i%2==0) waveformTab[n+fn*1024]=val&0xFF;
        if(chan_num==1 && i%2==1) {waveformTab[n+fn*1024]|=((int)val<<8)&0xFF00; waveformTab[n+fn*1024]=waveformTab[n+fn*1024]; n++;}
        if(fn<=1 && i%2==1) {Serial.print((n-1)+fn*1024); Serial.print(" : "); Serial.println((int)waveformTab[(n-1)+fn*1024]);}
      }
      i++;
    }
    Serial.println("waveformat");
    Serial.println(n);
    Serial.println(wav_size);
    Serial.println(chan_num);
    Serial.println(smp_rate);
    Serial.println(bps);
    Serial.println(ind_data);
    
    file.close();
  }
}

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

//GLIDE
	//uint8_t noteplaying[8];                           // true : a note is already playing - false : no note is playing
	boolean glideon;                               // true : glide is ON - false : glide is OFF
	boolean glidestart[8];                         // true : glide already started - false : glide is not started
	boolean play[8];                               // true : le son joue (même avec le release de l'enveloppe)
    boolean midi_play[8];	                       // true : on a déclenché le son avec une touche - false : on a relaché la touche
	float glidetime;                            // glide time in ms
	float Incrementglide[NUM_OSC][UNI_MAX];              // glide speed
	float Incrementfin[NUM_OSC][UNI_MAX];                // target frequency

	float ecartunis;
	
	float unisenv[8];

	Env env[8];

	float fFrequency[8];
	 
	
	float wave_buf[2][NUM_OSC][SAMPLES_PER_CYCLE];
	float* wave[NUM_OSC];  
	uint8_t active_bank[NUM_OSC]; 
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
   
	float convp[64];
	
// initialize the synth. Here you can put your default values.   
	void init()
	{
		waveformTab = (int16_t *) ps_malloc(WAVEFORM_NUMBER * WAVEFORM_SIZE * sizeof(int16_t));
		wavef_init();
		polyphony=false;
		unisson=1;
		pol=1;
		ecartunis=0.01;
		volglbsave = 0;
		voldesc=0.5;
		unisenv[0]=1.0;
		unisenv[1]=0.8;
		unisenv[2]=0.8;
		unisenv[3]=0.7;
		unisenv[4]=0.7;
		unisenv[5]=0.6;
		unisenv[6]=0.6;
		unisenv[7]=0.5;
		wavecnt=0;
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
				float sample=(float)waveformTab[j]/32768.0;
				wave_buf[0][i][j] = sample;
                wave_buf[1][i][j] = sample;
			}
			setPWM(i, 0);
			fact1[i]=1;
		    fact2[i]=100;
			wave[i]  = wave_buf[0][i];
            active_bank[i]  = 0;
            pwm_conv[i]     = 0.0f;
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
			//noteplaying[i] = 0;
			play[i] = false;
			midi_play[i] = false;
			volglb[i] = 0.5;
			env[i].init();
		}
		 
	//GLIDE
		glideon = true;
		glidetime = 200.0; // (en ms)

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
		Serial.println("SetNote");
		Serial.println(note);
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
		 Serial.println("play note");
		 if(vol>0)
		 {
			 int found_note=-1;
			 Serial.println(pol);
			 Serial.println(note);
			 for(int i=0; i<pol; i++)
			 {
				 Serial.println("test 1");
				 Serial.println(i);
				 Serial.println(notepressed[i].isplaying);
				 Serial.println(notepressed[i].pitch);
				 if(notepressed[i].isplaying && notepressed[i].pitch==note) {found_note=i; i=100;}
				 
			 }
			 Serial.println(found_note);
			 if(found_note<0)
			 {
				 for(int i=0; i<pol; i++) 
				 {
					 Serial.println("test 2");
					 Serial.println(i);
					 Serial.println(notepressed[i].isplaying);
					 if(!notepressed[i].isplaying) {found_note=i; i=100;}
				 }
					 
			 }
			 Serial.println(found_note);
			 if(found_note>=0)
			 {
				 Serial.println("play note");
				 Serial.println(found_note);
				 notepressed[found_note].on=true;
				 notepressed[found_note].pitch=note;
				 notepressed[found_note].isplaying=true;
				 env[found_note].start();
				 play[found_note]=true;					 
				 volglb[found_note] = (float)vol/127.0;
				 for(int j=0; j<NUM_OSC; j++)
				 {
					 fFrequency[found_note] = compute_freq(note, j); 
					 for(int k=0; k<UNI_MAX; k++)
					 {
						 int fact=k%2;
						 fact=fact*2-1;
						 phase_inc[j][found_note][k] = fFrequency[found_note]*pow((1.0+(float)fact*ecartunis),k); 
						 phase_inc_save[j][found_note][k]=phase_inc[j][found_note][k];
						 phase_accu[j][found_note][k] = phase[j];
					 }
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
					 //notepressed[i].pitch=0;
					 env[i].stop();
				 }
			 }
			 for(int i=0; i<8; i++)
			 {
				 if(notepressed[i].on) ret=false;
			 }
		 }

		 Serial.println("tableau");
		 for(int i=0; i<pol; i++)
		 {
			Serial.print("note ");
			Serial.println(i);
			Serial.println(notepressed[i].pitch);
			Serial.println(notepressed[i].on);
			Serial.println(notepressed[i].isplaying);
		 }
		 
     }

	 return ret;

   }
   
 
	


// Stop the playing of the synth
   void stop(uint8_t note)
   {
		 for(int j=0; j<8; j++)
		 {
			 if(note==notepressed[j].pitch) play[j] = false;
		 }
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
		 wave[num][i]=(float)waveformTab[i+val*1024]/32768.0;
	 }
   }
   
   
// Set the fine tune of one oscillator
   void setFine(uint8_t num, int val)
   {
	   Serial.println("SetFine");
		fine[num] = val;
		for(int j=0; j<8; j++)
		{
			if(notepressed[j].isplaying) 
			{
				fFrequency[j] = compute_freq(notepressed[j].pitch, num); 
		 
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
    }
	
	void rebuildPwmTable(uint8_t num, float val2, float* dest)
	{
		for (uint16_t k = 0; k < SAMPLES_PER_CYCLE; ++k) {
			uint16_t idx;

			if (val2 == 0.0f) {                  /* 50 % */
				idx = k+waveform[num]*SAMPLES_PER_CYCLE;
			} else if (val2 > 64.0f) {
				idx = (uint16_t)((fact1[num] - fact2[num] / (1023 - k + fl[num])) * k)+waveform[num]*SAMPLES_PER_CYCLE;
			} else {                             /* 0 < val ≤ 64 */
				idx = (uint16_t)((fact1[num] + fact2[num] / (k + fl[num])) * k)+waveform[num]*SAMPLES_PER_CYCLE;
			}
			//idx &= (SAMPLES_PER_CYCLE - 1);      /* wrap mod 1024 */
			dest[k] = (float)waveformTab[idx] * (1.0f / 32768.0f);   /* ré-échantillonnage */
		}
	}
	 
	void setPWM(uint8_t num, uint8_t val)
	 {
		 Serial.println("SetPWM");
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
			
			/* ── sélection du buffer caché et génération de la nouvelle forme ────── */
			uint8_t new_bank = active_bank[num] ^ 1;         /* 0⇄1 */
			float*  dest     = wave_buf[new_bank][num];
			rebuildPwmTable(num, val2, dest);
			
			wave[num] = dest;         /* nouveau front-buffer */
			active_bank[num] = new_bank;
			Serial.println(wave[num][512]);
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
		 //Serial.println(phase_accu[0][0][0]);
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
   
   void set_pitch_lfo(uint8_t num, float val)
   {
	   float va=0.75+val*0.5;
	 for(int j=0; j<pol; j++)
	 {
		phase_inc[num][j][0] = phase_inc_save[num][j][0]*va ; 
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
	   unisenv[0]=1.0;
	   for(int i=0; i<NUM_OSC; i++)
		 {
			volc[i][0]=volosc[i];
		 }
	   for(int k=1; k<UNI_MAX; k++)
	   {
		   unisenv[k]=(float)val*(float)k/127.0/(((float)k+1.0))+1.0/((float)k+1.0);
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
			//Serial.println(volenv);
			if(!env[j].started && play[j]) {play[j]=false; notepressed[j].isplaying=false; Serial.print("end env : "); Serial.println(j);}


			if(unisson>1)
			{
				for(int k=0; k<unisson; k++)
				//for(int k=0; k<1; k++)
				{
					
						ret+= wave[0][(int)phase_accu[0][j][k]]* volc[0][k]*volenv;
					
					//voldesc+=volosc[i];
				}
				
			}
			else
			{
				for(int i=0; i<NUM_OSC; i++)
				//for(int i=0; i<1; i++)
				{
					
					ret+= wave[i][(int)phase_accu[i][j][0]]* volosc[i]*volenv;
					
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
			//Serial.println(ret*voldesc);
			return (ret*voldesc);
		//return wave[0][(int)phase_accu[0][0][0]];
	}
   
};
#endif