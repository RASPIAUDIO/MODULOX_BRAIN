struct stepsamp{
	bool          on;
	uint8_t       pitch; 
  uint8_t       vol; 
  uint8_t       length;	
};
typedef struct stepsamp Stepsamp;

struct  instrusamp{
	bool on;
	uint8_t num;
  Stepsamp        stepsamp[64]; 
  int time_stop;
  bool playing;
};
typedef struct instrusamp Instrusamp;

struct  patternsamp{
  Instrusamp        instrusamp[16];                
  int         length;                
};
typedef struct patternsamp Patternsamp;

Patternsamp patsamp[8];

int bpmsamp = 80;
int step_timesamp=15000/bpmsamp;
int current_patternsamp = 0;
int current_stepsamp = 0;
uint32_t previous_stepsamp = 0; 
int prev_stepsamp = 0;
bool seq_playingsamp = false;

void change_bpmsamp(int b)
{
	bpmsamp=b;
	step_timesamp=15000/bpmsamp;
}

void change_stepTimesamp(int b)
{
	step_timesamp=b;
}

// OSC.H
void sound_playsamp(uint8_t inst_num, uint8_t pitch, uint8_t vol)
{
	Serial.print("pitch : ");
	Serial.println(pitch);
	sample_launch(inst_num, 127); 
}

void sound_stopsamp(uint8_t inst_num, uint8_t pit)
{
	Serial.println("stop");
	sample_stop(inst_num);
}

// OSCORGAN.H
/*void sound_play(uint8_t inst_num, uint8_t pitch, uint8_t vol)
{
	Serial.print("pitch : ");
	Serial.println(pitch);
	if(inst_num==0)
	{
		oscA.setNote(pitch, vol); 
		f3.start(); 
		env.start(); 
		//lfo[0].start();
	}
}

void sound_stop(uint8_t inst_num, uint8_t pit)
{
	Serial.println("stop");
	if(inst_num==0)
	{
		oscA.setNote(pit, 0); 
		f3.stop(); 
		env.stop();
		//lfo[0].stop();
	}
}*/

void seq_initsamp()
{
  for(int p=0; p<8; p++)
  {
    for(int i=0; i<16; i++)
    {
      patsamp[p].instrusamp[i].on =true;
	  patsamp[p].instrusamp[i].playing = false;
      patsamp[p].instrusamp[i].num =i;
      patsamp[p].instrusamp[i].time_stop=1000;
      for(int s=0; s<64; s++)
      {
        patsamp[p].instrusamp[i].stepsamp[s].on=false;
		patsamp[p].instrusamp[i].stepsamp[s].pitch=30;
        patsamp[p].instrusamp[i].stepsamp[s].vol=127;
        patsamp[p].instrusamp[i].stepsamp[s].length=127;
      }
    }
  }

  patsamp[0].instrusamp[0].stepsamp[0].on=true;
  patsamp[0].instrusamp[0].stepsamp[8].on=true;
  patsamp[0].instrusamp[0].stepsamp[16].on=true;
  patsamp[0].instrusamp[0].stepsamp[24].on=true;
}

void seq_playsamp()
{
	if(seq_playing)
	{
		int tim = millis()-previous_stepsamp;
		if(tim>step_timesamp) 
		{
			//Serial.println(current_step);
			Serial.print("play : ");
			Serial.println(patsamp[current_patternsamp].instrusamp[0].playing);
			prev_stepsamp = current_stepsamp;
			current_stepsamp++;
			if(current_stepsamp>63) current_stepsamp=0;
			for(int i=0; i<1; i++)
			{
				bool play_n = true;
				/*if(pat[current_pattern].instru[i].playing && pat[current_pattern].instru[i].step[current_step].on)
				{
					if(pat[current_pattern].instru[i].step[current_step].pitch!=pat[current_pattern].instru[i].step[prev_step].pitch) 
					{
						sound_stop(pat[current_pattern].instru[i].num, pat[current_pattern].instru[i].step[prev_step].pitch);
						pat[current_pattern].instru[i].playing    = false;
					}
					// Si la note précédente est identique et en train de jouer, on ne rejoue pas la nouvelle note
					else play_n=false;
				}*/
				if(patsamp[current_patternsamp].instrusamp[i].playing && !patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].on)
				{
					sound_stopsamp(patsamp[current_patternsamp].instrusamp[i].num, patsamp[current_patternsamp].instrusamp[i].stepsamp[prev_stepsamp].pitch);
					patsamp[current_patternsamp].instrusamp[i].playing    = false;
				}
				if(patsamp[current_patternsamp].instrusamp[i].on && patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].on && play_n)
				{
					sound_playsamp(patsamp[current_patternsamp].instrusamp[i].num, patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].pitch, patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].vol);
					patsamp[current_patternsamp].instrusamp[i].time_stop  = step_timesamp*patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].length/127;
					//pat[current_pattern].instru[i].pitch_stop = pat[current_pattern].instru[i].step[current_step].pitch;
					patsamp[current_patternsamp].instrusamp[i].playing    = true;
				}
			}
			previous_stepsamp=millis();
			
		}
		else
		{
			for(int i=0; i<1; i++)
			{
				if(tim>patsamp[current_patternsamp].instrusamp[i].time_stop && patsamp[current_patternsamp].instrusamp[i].playing) 
				{
					sound_stopsamp(patsamp[current_patternsamp].instrusamp[i].num, patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].pitch);
					patsamp[current_patternsamp].instrusamp[i].playing=false;
				}
			}
		}
	}
}

void seq_play_syncsamp()
{
	int tim = millis()-previous_stepsamp;
	if(prev_stepsamp != current_stepsamp)
	{
		if(current_stepsamp>63) current_stepsamp=0;
		
		for(int i=0; i<16; i++)
		{
			bool play_n = true;
			/*if(pat[current_pattern].instru[i].playing && pat[current_pattern].instru[i].step[current_step].on)
			{
				if(pat[current_pattern].instru[i].step[current_step].pitch!=pat[current_pattern].instru[i].step[prev_step].pitch) 
				{
					sound_stop(pat[current_pattern].instru[i].num, pat[current_pattern].instru[i].step[prev_step].pitch);
					pat[current_pattern].instru[i].playing    = false;
				}
				// Si la note précédente est identique et en train de jouer, on ne rejoue pas la nouvelle note
				else play_n=false;
			}*/
			if(patsamp[current_patternsamp].instrusamp[i].playing && !patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].on)
			{
				sound_stopsamp(patsamp[current_patternsamp].instrusamp[i].num, patsamp[current_patternsamp].instrusamp[i].stepsamp[prev_stepsamp].pitch);
				patsamp[current_patternsamp].instrusamp[i].playing    = false;
			}
			if(patsamp[current_patternsamp].instrusamp[i].on && patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].on && play_n)
			{
				sound_playsamp(patsamp[current_patternsamp].instrusamp[i].num, patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].pitch, patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].vol);
				patsamp[current_patternsamp].instrusamp[i].time_stop  = step_timesamp*patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].length/127;
				//pat[current_pattern].instru[i].pitch_stop = pat[current_pattern].instru[i].step[current_step].pitch;
				patsamp[current_patternsamp].instrusamp[i].playing    = true;
			}
		}
		change_stepTimesamp(millis()-previous_stepsamp);
		previous_stepsamp=millis();
		prev_stepsamp = current_stepsamp;
	}
	else
	{
		for(int i=0; i<16; i++)
		{
			if(tim>patsamp[current_patternsamp].instrusamp[i].time_stop && patsamp[current_patternsamp].instrusamp[i].playing) 
			{
				sound_stopsamp(patsamp[current_patternsamp].instrusamp[i].num, patsamp[current_patternsamp].instrusamp[i].stepsamp[current_stepsamp].pitch);
				patsamp[current_patternsamp].instrusamp[i].playing=false;
			}
		}
	}
}