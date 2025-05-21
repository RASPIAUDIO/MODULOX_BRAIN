class Arp
{
  public:
	struct step{
		bool          on;
		uint8_t       pitch; 
		uint8_t       vol; 
		uint8_t       length;	
	};
	typedef struct step Step;
	
	int note_pressed[8]={-1,-1,-1,-1,-1,-1,-1,-1};
	int note_to_play[64];
	int nb_note_to_play = 0;
	
	uint8_t arpmode = 0;      // 0 : UP
														// 1 : UP/DOWN
	uint8_t arprate = 1;
	uint8_t arpstep = 1;
	int8_t arphigh = 0;
	uint8_t arpgate = 100;

	int8_t transpo = 0;
	
	bool play=true;
	bool note_playing=false;
	
	uint32_t previous_step = 0; 
	
	float time_stop=100;
	int current_step=0;
	int step_time=300;
		
	void add_note(int note)
	{
		Serial.println("add note");
		for(int i=0; i<8; i++)
		{
			if(note_pressed[i]<0 || note_pressed[i]==note) 
			{
				note_pressed[i]=note;
				i=9;
			}
		}
		compute_notes();
		for(int i=0; i<nb_note_to_play; i++)
		{
			Serial.print(i);
			Serial.print(" ; ");
			Serial.println(note_to_play[i]);
		}
		
	}
	
	
	void delete_note(int note)
	{
		Serial.println("delete note");
		for(int i=0; i<8; i++)
		{
			if(note_pressed[i]==note) 
			{
				note_pressed[7]=-1;
				for(int j=i; j<7; j++)
				{
					note_pressed[j]=note_pressed[j+1];
				}
				
			}
		}
		compute_notes();
		for(int i=0; i<nb_note_to_play; i++)
		{
			Serial.print(i);
			Serial.print(" ; ");
			Serial.println(note_to_play[i]);
		}
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
				while(note_pressed[i]>=0)
				{
					note_to_play[ind]=note_pressed[i]+12*j;
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
				while(note_pressed[i]>=0)
				{
					note_to_play[ind]=note_pressed[i]+12*j;
					i++;
					ind++;
				}
				while(i>0)
				{
					note_to_play[ind]=note_pressed[i]+12*j;
					i--;
					ind++;
				}
			}
		}
		// DOWN
		if(arpmode==2)
		{
			for(int j=arpstep; j>=0; j--)
			{
				int i=7;
				while(i>=0)
				{
					if(note_pressed[i]>=0)
					{
						note_to_play[ind]=note_pressed[i]+12*j;
						ind++;
					}
					i--;
				}
			}
		}
		nb_note_to_play=ind;
	}
	
	void start()
	{
		current_step=0;
		play=true;
		time_stop = step_time*(float)arpgate/127;
	}
	
	void stop(OscMonoPoly* osc)
	{
		play=false;
		osc->setNote(note_to_play[current_step], 0); 
	}
	
	void update_gate(uint8_t _gate)
	{
		arpgate = _gate;
		time_stop = step_time*(float)arpgate/127;
	}
	
	void update_rate(uint8_t _rate)
	{
		arprate = _rate;
	}
	
	void update_mode(uint8_t _mode)
	{
		arpmode = _mode;
		compute_notes();
	}
	
	void update_step(uint8_t _step)
	{
		arpstep = _step;
		compute_notes();
	}
	
	void update_bpm(uint8_t _bpm)
	{
		step_time=15000/(_bpm+50);
	}

  void update(OscMonoPoly* osc)
	{
		int tim = millis()-previous_step;
		if(tim>(step_time*arprate) && nb_note_to_play>0) 
		{
			note_playing=true;
			current_step++;
			if(current_step>=nb_note_to_play) current_step=0;
			Serial.println(current_step);
			Serial.println(note_to_play[current_step]);
			osc->setNote(note_to_play[current_step], 127); 
			previous_step=millis();
			
		}
		else
		{
			if(tim>time_stop && note_playing) 
			{
				Serial.println("stop");
				osc->setNote(note_to_play[current_step], 0); 
				note_playing=false;
			}
		}
	}



	uint8_t cal_nb_notes()
	{
		uint8_t ret = 0;
		for(int i=0; i<8; i++)
		{
			if(note_pressed[i]>0) ret++;
		}
		return ret;
	}
};