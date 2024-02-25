#include <arduino.h>
#include <wavtables.h>

// Change this to change the number of oscillators per voice
#define POLYPHONY 3

// Can adjust the Sample rate
#define SAMPLE_RATE 44100.0 

// Number of samples played in one cycle (a cycle depends on the frequency of the note played)
#define SAMPLES_PER_CYCLE 256 
#define WAVENUM 64

#define SAMPLES_PER_CYCLE_FIXEDPOINT (SAMPLES_PER_CYCLE<<20) 
#define TICKS_PER_CYCLE (float)((float)SAMPLES_PER_CYCLE_FIXEDPOINT/(float)SAMPLE_RATE) 


// This class represents a voice of synth. It can have several oscillators
class OscTab
{
	
public:
   int32_t volglb[POLYPHONY];                               // Volume global
   int32_t volglbsave;                           // Volume global temporary save  
   uint16_t waveform;                    // Waveform of each oscillator 
   int32_t volosc;                      // Volume of each oscillator
   float fine;                           // Fine of each oscillator
   uint32_t ulPhaseAccumulator[POLYPHONY];          // Position in the reading of each oscillator table 
   volatile uint32_t ulPhaseIncrement[POLYPHONY] ;  // Reading speed of each oscillaotr table 
   int8_t octave;                        // Octave of each oscillator

//GLIDE
   uint8_t noteplaying[POLYPHONY];                           // true : a note is already playing - false : no note is playing
   boolean glideon;                               // true : glide is ON - false : glide is OFF
   boolean glidestart[POLYPHONY];                            // true : glide already started - false : glide is not started
   boolean play[POLYPHONY];                                  
   uint32_t glidetime;                            // glide time in ms
   uint32_t Incrementglide;              // glide speed
   uint32_t Incrementfin;                // target frequency
   
   uint8_t table_num;
	 uint32_t table_pos; 
	 uint32_t time_length;
	 bool auto_loop;


   float fFrequency[POLYPHONY];
   
// initialize the synth. Here you can put your default values.   
   void init()
   {
     
     volglbsave = 0;
     
     waveform = 0;
     volosc = 64;
     fine = 0;
	 table_num = 0;
		 table_pos=0;
		 
		 time_length=5;
		 auto_loop=true;
	 

		Incrementglide = 0;
		Incrementfin = 0;

	 for(int i=0; i<POLYPHONY; i++)
	 {
		 ulPhaseAccumulator[i] = 0; 
		 ulPhaseIncrement[i] = 0;
		 glidestart[i] = false;
		 noteplaying[i] = 0;
		 play[i] = false;
		 volglb[i] = 64;
	 }
	 
	 
//GLIDE
     glideon = true;
     
     Incrementfin=0;
     
     glidetime = 1000; // (en ms)
     
   }
   

// Set the note (frequency), and volume 
   void setNote(uint32_t note, int32_t vol)
   {
// If we are in glide mode and the synth is not playing, we have to compute the glide increment (= glide speed)	
// We do not have to change the frequency value, it's the glide increment which will change it
		 
     if(note!=noteplaying[0] && glideon && play[0] && POLYPHONY==1)
     {
 
         fFrequency[0] = ((pow(2.0,(note-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
         Incrementfin = fFrequency[0];
		 Incrementglide = abs((int)Incrementfin - (int)ulPhaseIncrement[0])*1000/44100/glidetime;
		 if(Incrementglide==0) Incrementglide = 1;
     }
//If we are not in glide mode, compute the frequency of the new note
     else
     {
		 int numos = 128;
		 for(int j=0; j<POLYPHONY; j++)
		 {
			 if(!play[j]) numos=j;
		 }
		 if(numos<128)
		 {

			 fFrequency[numos] = ((pow(2.0,(note-69.0)/12.0)) * 440.0)*TICKS_PER_CYCLE; 
			 ulPhaseIncrement[numos] = fFrequency[numos]; 
			 if(POLYPHONY==1) Incrementfin = fFrequency[numos]; 
			 // The synth is playing
			 play[numos] = true;
			 if(vol!=0) 
			 {
				 volglb[numos] = vol;
				 noteplaying[numos] = note;
			 }
			 // If the volume = 0, we consider that the synth is stopped
			 else play[numos] = false;
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


// Set the waveform of one oscillator
   void setWaveform(uint8_t num, uint8_t val)
   {
     waveform = val;
   }


//Set the glide time
   void setGlideTime(uint8_t glt)
   {
     glidetime = glt * 10+1;
   }

// Compute the table position in the wavetable from increment values
   void next()
   {
	 if(POLYPHONY==1)
	 {

		   if(ulPhaseIncrement[0]<Incrementfin) 
		   {
			 if((Incrementfin-ulPhaseIncrement[0])<=Incrementglide)
			 {
			   ulPhaseIncrement[0] = Incrementfin;
			 }
			 else ulPhaseIncrement[0] += Incrementglide;
		   }
		   if(ulPhaseIncrement[0]>Incrementfin) 
		   {
			 if((ulPhaseIncrement[0]-Incrementfin)<=Incrementglide)
			 {
			   ulPhaseIncrement[0] = Incrementfin;
			 }
			 else ulPhaseIncrement[0] -= Incrementglide;
		   }
	   
	 }
     for(int j=0; j<POLYPHONY; j++)
	 {
		 if(play[j])
		 {
			   ulPhaseAccumulator[j] += ulPhaseIncrement[j];   
			   if(ulPhaseAccumulator[j] > SAMPLES_PER_CYCLE_FIXEDPOINT) 
			   { 
				 ulPhaseAccumulator[j] -= SAMPLES_PER_CYCLE_FIXEDPOINT; 
			   } 
		 }
	 }
	 if(auto_loop) table_pos++;
	 if(table_pos>time_length*SAMPLES_PER_CYCLE)
	 {
		 table_pos=0;
		 table_num++;
		 if(table_num>(WAVENUM-1)) table_num=0;
	 }	 
   }
   
   void set_wave(uint8_t val)
	 {
		 table_num=val*WAVENUM/127;
	 }
	 
	 void start_loop()
	 {
		 auto_loop=true;
	 }
	 
	 void stop_loop()
	 {
		 auto_loop=false;
	 }

//Return the sample value from the wavetable 
   int16_t output()
   {
     int16_t ret=0;
	 for(int j=0; j<POLYPHONY; j++)
	 {
		   if(waveform == 0) ret+= (ALPHA_2_[(ulPhaseAccumulator[j]>>20)+table_num*SAMPLES_PER_CYCLE]* volglb[j])>>7;
	 }
     return (ret);
   }
   
};
