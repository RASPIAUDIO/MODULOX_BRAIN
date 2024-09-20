
#ifndef oschammond_h
#define oschammond_h

#include <arduino.h>
#include <envfloat.h>
#include "FFat.h"

int16_t *waveformTab;

#define WAVEFORM_NUMBER 4
#define WAVEFORM_SIZE 1024


#define NUM_OSC 2  // Change this to change the number of oscillators per voice

// Can adjust the Sample rate
#define SAMPLE_RATE 44100.0 

// Number of samples played in one cycle (a cycle depends on the frequency of the note played)
#define SAMPLES_PER_CYCLE 1024 

#define TICKS_PER_CYCLE (float)((float)SAMPLES_PER_CYCLE/(float)SAMPLE_RATE)

int count2=0;

void wavef_init()
{
  for(int fn=0; fn<4; fn++)
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
        if(chan_num==1 && i%2==1) {waveformTab[n+fn*1024]|=((int)val<<8)&0xFF00; n++;}
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
	float volglb;                               // Volume global
	float volglbsave;                              // Volume global temporary save  
	int32_t phase[NUM_OSC];
	uint8_t waveform[NUM_OSC];                     // Waveform of each oscillator 
	float volosc[9];                         // Volume of each oscillator
	float phase_accu[NUM_OSC][8];          // Position in the reading of each oscillator table 
	float phase_inc[NUM_OSC][8] ;  // Reading speed of each oscillaotr table 
	float phase_inc_save[NUM_OSC][8] ;  // Reading speed of each oscillaotr table 
	uint8_t wavecnt;
	int8_t octave[NUM_OSC];                        // Octave of each oscillator
	float voldesc;
	float volaccent;
	float accent;

//GLIDE
	uint8_t noteplaying[8];                           // true : a note is already playing - false : no note is playing
	boolean play[8];                               // true : le son joue (même avec le release de l'enveloppe)
    boolean midi_play[8];	                       // true : on a déclenché le son avec une touche - false : on a relaché la touche
	float Incrementfin[NUM_OSC];                // target frequency

	Env env[8];
	Env env_accent[8];

	float fFrequency[8];
	 
	float wave[SAMPLES_PER_CYCLE];
	float waves_mix1[SAMPLES_PER_CYCLE];
	float waves_mix2[SAMPLES_PER_CYCLE];
	float wave_accent[SAMPLES_PER_CYCLE];
   
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
	bool wave_used;
	
// initialize the synth. Here you can put your default values.   
	void init()
	{
		Serial.println("hammond init");
		waveformTab = (int16_t *) ps_malloc(WAVEFORM_NUMBER * WAVEFORM_SIZE * sizeof(int16_t));
		Serial.println("waveformTab OK");
		wavef_init();
		pol=4;
		volglbsave = 0;
		voldesc=0.5;
		wavecnt=0;
		wave_used=0;
		volaccent=1.0;
		
		Serial.println("var OK");
 	 
		for(int i=0; i<NUM_OSC; i++)
		{
			waveform[i] = 0;
			phase[i] = 0;
			Incrementfin[i] = 0;
			octave[i] = 0;
		}
		
		volosc[0] = 0;
		volosc[1] = 0.5;
		volosc[2] = 0;
		volosc[3] = 0;
		volosc[4] = 0;
		volosc[5] = 0;
		volosc[6] = 0;
		volosc[7] = 0;
		volosc[8] = 0;
		setWaveform(0);
		for(int j=0; j<(int)SAMPLES_PER_CYCLE; j++)
		{
			wave_accent[j]=wave[(j*6)%1024];
		}
		
		Serial.println("samples OK");
		
		volglb = 0.5;
	
		for(int i=0; i<8; i++)
		{
			for(int j=0; j<NUM_OSC; j++)
			{

					phase_accu[j][i] = 0; 
					phase_inc[j][i] = 0;
					phase_inc_save[j][i] = 0;
			}
			noteplaying[i] = 0;
			play[i] = false;
			midi_play[i] = false;
			
			env[i].init();
			env_accent[i].init();
			env_accent[i].setA(5);
			env_accent[i].setS(0);
			env_accent[i].setD(15);	
			env_accent[i].setR(0);
		}
		Serial.println("env values");
		Serial.println(env_accent[0].envA*10000);
		Serial.println(env_accent[0].envD*10000);
		Serial.println(env_accent[0].envS*10000);
	}
	
	
	float compute_freq(int notefreq, int numosc)
	{
		return ((pow(2.0,((notefreq+12*octave[numosc])-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE;
	}
	
	void update_polyphony(uint8_t po)
	{
		pol=po; 
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

	 //----------------- POLYPHONIC -------------------

		 if(vol>0)
		 {
			 volglb = (float)vol/127.0;
			 for(int i=0; i<8; i++)
			 {
				 if(!play[i])
				 {
					 Serial.println("play note");
					 Serial.println(i);
					 notepressed[i].on=true;
					 notepressed[i].pitch=note;
					 notepressed[i].isplaying=true;
					 env[i].start();
					 env_accent[i].start();
					 play[i]=true;					 
					 
					 for(int j=0; j<NUM_OSC; j++)
					 {
						 fFrequency[i] = compute_freq(note, j); 

							 phase_inc[j][i] = fFrequency[i]; 
							 phase_inc_save[j][i]=phase_inc[j][i];
							 phase_accu[j][i] = phase[j];
						
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
					 env_accent[i].stop();
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
		 
	 return ret;

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
     volosc[num] = vol/127.0;
	 if(wave_used) 
	 {
		 for(int j=0; j<SAMPLES_PER_CYCLE; j++)
		 {
			 //Serial.println(waveformTab[i+val*1024]);
			waves_mix1[j]=wave[j]*volosc[0]+wave[(j*2)%1024]*volosc[1]+wave[(j*3)%1024]*volosc[2]+wave[(j*4)%1024]*volosc[3]+wave[(j*6)%1024]*volosc[4]+wave[(j*8)%1024]*volosc[5]+wave[(j*10)%1024]*volosc[6]+wave[(j*12)%1024]*volosc[7]+wave[(j*16)%1024]*volosc[8];
		 }
		 wave_used=false;
	 }
	 else
	 {
		 for(int j=0; j<SAMPLES_PER_CYCLE; j++)
		 {
			 //Serial.println(waveformTab[i+val*1024]);
			waves_mix2[j]=wave[j]*volosc[0]+wave[(j*2)%1024]*volosc[1]+wave[(j*3)%1024]*volosc[2]+wave[(j*4)%1024]*volosc[3]+wave[(j*6)%1024]*volosc[4]+wave[(j*8)%1024]*volosc[5]+wave[(j*10)%1024]*volosc[6]+wave[(j*12)%1024]*volosc[7]+wave[(j*16)%1024]*volosc[8];
		 }
		 wave_used=true;
	 }
   }
   
   void setPhaseOsc(uint8_t num, uint8_t ph)
   {
		 for(int j=0; j<8; j++)
		 {

				 phase_accu[num][j]+=(ph*8);
				 if(phase_accu[num][j]>1023) phase_accu[num][j]-=1024;

		 }
		 phase[num] = ph*8;
   }

// Set the waveform of one oscillator
   void setWaveform(uint8_t val)
   {
	 if(wave_used) 
	 {
		 for(int j=0; j<SAMPLES_PER_CYCLE; j++)
		 {
			 //Serial.println(waveformTab[i+val*1024]);
			 wave[j]=(float)waveformTab[j+val*1024]/65536.0;
			 waves_mix1[j]=wave[j]*volosc[0]+wave[(j*2)%1024]*volosc[1]+wave[(j*3)%1024]*volosc[2]+wave[(j*4)%1024]*volosc[3]+wave[(j*6)%1024]*volosc[4]+wave[(j*8)%1024]*volosc[5]+wave[(j*10)%1024]*volosc[6]+wave[(j*12)%1024]*volosc[7]+wave[(j*16)%1024]*volosc[8];

		 }
		 wave_used=false;
	 }
	 else
	 {
		 for(int j=0; j<SAMPLES_PER_CYCLE; j++)
		 {
			 //Serial.println(waveformTab[i+val*1024]);
			wave[j]=(float)waveformTab[j+val*1024]/65536.0;
			waves_mix2[j]=wave[j]*volosc[0]+wave[(j*2)%1024]*volosc[1]+wave[(j*3)%1024]*volosc[2]+wave[(j*4)%1024]*volosc[3]+wave[(j*6)%1024]*volosc[4]+wave[(j*8)%1024]*volosc[5]+wave[(j*10)%1024]*volosc[6]+wave[(j*12)%1024]*volosc[7]+wave[(j*16)%1024]*volosc[8];
		 }
		 wave_used=true;
	 }
   }
	 
 
    void setenvA(uint32_t val)
	{
		for(int j=0; j<8; j++)
		{
			env[j].setA(val);
		}
	}
	
	void setenvAacc(uint32_t val)
	{
		for(int j=0; j<8; j++)
		{
			env_accent[j].setA(val);
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
	
	void setvolacc(uint32_t val)
	{
		volaccent=(float)val/127.0;
	}

// Compute the table position in the wavetable from increment values
   void next()
   {

		 for(int j=0; j<pol; j++)
		 {
			 if(play[j])
			 {
				 for(int i=0; i<NUM_OSC; i++)
				 {

					 phase_accu[i][j] += phase_inc[i][j];   
					 if(phase_accu[i][j] >= SAMPLES_PER_CYCLE) 
					 { 
						 phase_accu[i][j] -= SAMPLES_PER_CYCLE; 
					 } 
				 }
			 }
		 }

		 //Serial.println(phase_accu[0][0][0]);
   }
   
   void set_pitch_lfo(float val)
   {
	   float va=0.75+val*0.5;

		 for(int i=0; i<NUM_OSC; i++)
		 {
			 for(int j=0; j<pol; j++)
			 {
				phase_inc[i][j] = phase_inc_save[i][j]*va ; 
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
			
			float volenv=env[j].amount()*volglb;
			float volacc=env_accent[j].amount()*volglb*volaccent;
			//if(j==0) Serial.println(volenv);
			if(!env[j].started && play[j]) {play[j]=false; notepressed[j].isplaying=false; }

    		if(wave_used) ret+=waves_mix2[(int)phase_accu[0][j]]*volenv;
			else ret+=waves_mix1[(int)phase_accu[0][j]]*volenv;
			//if(j==0 && env_accent[j].phaseD) Serial.println(volacc);
			ret+=wave_accent[(int)phase_accu[0][j]]*volacc;
			//ret+=wave[(int)phase_accu[0][j]]*volenv;
	
		}

		return (ret*voldesc);
	}
   
};
#endif