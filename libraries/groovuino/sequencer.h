struct step{
	bool          on;
	uint8_t       pitch; 
  uint8_t       vol; 
  uint8_t       length;	
};
typedef struct step Step;

struct  instru{
	bool on;
	uint8_t num;
  Step        step[64]; 
  int time_stop;
  bool playing;
};
typedef struct instru Instru;

struct  pattern{
  Instru        instru[16];                
  int         length;                
};
typedef struct pattern Pattern;

Pattern pat[8];


int current_pattern = 0;
int current_step = 0;
uint32_t previous_step = 0; 
int prev_step = 0;
bool seq_playing = false;

void change_bpm(int b)
{
	bpm=b;
	step_time=15000/bpm;
}

void change_stepTime(int b)
{
	step_time=b;
}

// OSC.H
void sound_play(uint8_t inst_num, uint8_t pitch, uint8_t vol)
{
	//Serial.print("inst : ");
	//Serial.println(inst_num);
	if(inst_num==0)
	{
		oscA[0].setNote(pitch, vol); 
		//f3.start(); 
		env[current_synth].start(); 
		
		//lfo[0].start();
	}
	/*if(inst_num>7)
	{
		sample_launch(inst_num-8, vol);
	}*/
}

void sound_stop(uint8_t inst_num, uint8_t pit)
{
	//Serial.println("stop");
	
	if(inst_num==0)
	{
		oscA[0].setNote(pit, 0); 
		//f3.stop(); 
		env[current_synth].stop();
		//lfo[0].stop();
	}
	/*if(inst_num>7)
	{
		sample_stop(inst_num-8);
	}*/
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

void seq_init()
{
  for(int p=0; p<8; p++)
  {
    for(int i=0; i<16; i++)
    {
      pat[p].instru[i].on =true;
	  pat[p].instru[i].playing = false;
      pat[p].instru[i].num =i;
      pat[p].instru[i].time_stop=1000;
      for(int s=0; s<64; s++)
      {
        pat[p].instru[i].step[s].on=false;
		pat[p].instru[i].step[s].pitch=30;
        pat[p].instru[i].step[s].vol=127;
        pat[p].instru[i].step[s].length=127;
      }
    }
  }
  

  for(int i=0; i<32; i++)
  {
    pat[0].instru[10].step[i*2].on=true;
  }
  for(int i=0; i<8; i++)
  {
    pat[0].instru[8].step[i*8].on=true;
  }
  for(int i=0; i<8; i++)
  {
    pat[0].instru[9].step[i*8+4].on=true;
  }
}

bool seq_play()
{
	bool ret=false;
	if(seq_playing)
	{
		int tim = millis()-previous_step;
		if(tim>step_time) 
		{
			if(current_step%4==0) ret=true;
			//Serial.println(current_step);
			//Serial.print("play : ");
			//Serial.println(pat[current_pattern].instru[0].playing);
			prev_step = current_step;
			current_step++;
			if(current_step>63) current_step=0;
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
				if(pat[current_pattern].instru[i].playing && !pat[current_pattern].instru[i].step[current_step].on)
				{
					sound_stop(pat[current_pattern].instru[i].num, pat[current_pattern].instru[i].step[prev_step].pitch);
					pat[current_pattern].instru[i].playing    = false;
				}
				if(pat[current_pattern].instru[i].on && pat[current_pattern].instru[i].step[current_step].on && play_n)
				{
					sound_play(pat[current_pattern].instru[i].num, pat[current_pattern].instru[i].step[current_step].pitch, pat[current_pattern].instru[i].step[current_step].vol);
					pat[current_pattern].instru[i].time_stop  = step_time*pat[current_pattern].instru[i].step[current_step].length/127;
					//pat[current_pattern].instru[i].pitch_stop = pat[current_pattern].instru[i].step[current_step].pitch;
					pat[current_pattern].instru[i].playing    = true;
				}
			}
			previous_step=millis();
			
		}
		else
		{
			for(int i=0; i<16; i++)
			{
				if(tim>pat[current_pattern].instru[i].time_stop && pat[current_pattern].instru[i].playing) 
				{
					sound_stop(pat[current_pattern].instru[i].num, pat[current_pattern].instru[i].step[current_step].pitch);
					pat[current_pattern].instru[i].playing=false;
				}
			}
		}
	}
	return ret;
}

void seq_stop()
{

	for(int i=0; i<16; i++)
	{
		if(pat[current_pattern].instru[i].playing) 
		{
			sound_stop(pat[current_pattern].instru[i].num, pat[current_pattern].instru[i].step[current_step].pitch);
			pat[current_pattern].instru[i].playing=false;
		}
	}
	seq_playing=false;
}

void seq_play_sync()
{
	int tim = millis()-previous_step;
	if(prev_step != current_step)
	{
		if(current_step>63) current_step=0;
		
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
			if(pat[current_pattern].instru[i].playing && !pat[current_pattern].instru[i].step[current_step].on)
			{
				sound_stop(pat[current_pattern].instru[i].num, pat[current_pattern].instru[i].step[prev_step].pitch);
				pat[current_pattern].instru[i].playing    = false;
			}
			if(pat[current_pattern].instru[i].on && pat[current_pattern].instru[i].step[current_step].on && play_n)
			{
				sound_play(pat[current_pattern].instru[i].num, pat[current_pattern].instru[i].step[current_step].pitch, pat[current_pattern].instru[i].step[current_step].vol);
				pat[current_pattern].instru[i].time_stop  = step_time*pat[current_pattern].instru[i].step[current_step].length/127;
				//pat[current_pattern].instru[i].pitch_stop = pat[current_pattern].instru[i].step[current_step].pitch;
				pat[current_pattern].instru[i].playing    = true;
			}
		}
		change_stepTime(millis()-previous_step);
		previous_step=millis();
		prev_step = current_step;
	}
	else
	{
		for(int i=0; i<16; i++)
		{
			if(tim>pat[current_pattern].instru[i].time_stop && pat[current_pattern].instru[i].playing) 
			{
				sound_stop(pat[current_pattern].instru[i].num, pat[current_pattern].instru[i].step[current_step].pitch);
				pat[current_pattern].instru[i].playing=false;
			}
		}
	}
}