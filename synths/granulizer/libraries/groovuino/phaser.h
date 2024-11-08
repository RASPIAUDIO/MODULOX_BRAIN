#ifndef phaser_h
#define phaser_h

float *delayBuffer0;
float *delayBuffer1;
float *delayBuffer2;
float *delayBuffer3;

class AllpassFilter
{
	public:

	//float delayBuffer[44100];
	size_t writeIndex;
	float rate;
	float feedback;
	int buf_leng;

	void init(int num)
	{
		if(num==0) delayBuffer0 = (float *) ps_malloc(1000 * sizeof(float));
		if(num==1) delayBuffer1 = (float *) ps_malloc(1000 * sizeof(float));
		if(num==2) delayBuffer2 = (float *) ps_malloc(1000 * sizeof(float));
		if(num==3) delayBuffer3 = (float *) ps_malloc(1000 * sizeof(float));
		for(int i=0; i++; i<1000) 
			{
				if(num==0) delayBuffer0[i]=0;
				if(num==1) delayBuffer1[i]=0;
				if(num==2) delayBuffer2[i]=0;
				if(num==3) delayBuffer3[i]=0;
			}
		rate=20;
		feedback=0.0;
		writeIndex=0;
		buf_leng=1000/rate;
	}

	float ProcessSample(float input, int num)
	{
		
		float delayedSample;
		if(num==0) delayedSample = delayBuffer0[writeIndex];
		if(num==1) delayedSample = delayBuffer1[writeIndex];
		if(num==2) delayedSample = delayBuffer2[writeIndex];
		if(num==3) delayedSample = delayBuffer3[writeIndex];
		
		float output = -input + delayedSample;
		//float output = (input+ delayedSample)/2.0;
		
		if(num==0) delayBuffer0[writeIndex] = input + output * feedback;
		if(num==1) delayBuffer1[writeIndex] = input + output * feedback;
		if(num==2) delayBuffer2[writeIndex] = input + output * feedback;
		if(num==3) delayBuffer3[writeIndex] = input + output * feedback;
		

		writeIndex++;
		if(writeIndex>=buf_leng) writeIndex=0;
		
		if(writeIndex==0) Serial.println(output);

		return output;
	}

	void SetRate(float newRate, int num)
	{
		/*if(newRate<rate)
		{
			for(int i=1000/rate; i++; i<1000/newRate) 
			{
				if(num==0) delayBuffer0[i]=0;
				if(num==1) delayBuffer1[i]=0;
				if(num==2) delayBuffer2[i]=0;
				if(num==3) delayBuffer3[i]=0;
			}
		}*/
		rate = newRate;
		// Adjust the delay time based on the new rate
		buf_leng=1000/rate;

	}
};

class Phaser
{
public:
	int numStages;
	float rate;
	float depth;
	float feedback;
	float feedbackBuffer;
	AllpassFilter allpassFilters[10];

	void init(){
		numStages=1;
		for (int i = 0; i < numStages; ++i)
		{
			allpassFilters[i].init(i);
			//delay(1);
			//allpassFilters[i].SetRate(i+10);
		}
	}

	float ProcessSample(float input)
	{
		float output = input;

		for (int i = 0; i < numStages; ++i)
		{
			output = allpassFilters[i].ProcessSample(output, i);
		}
		//output = allpassFilters[0].ProcessSample(output);

		// Feedback loop
		//feedbackBuffer = output * feedback + input * (1 - feedback);

		return output;
	}
	
	void SetRate(float newRate)
	{
		rate = newRate;
		// Recalculate filter coefficients based on the new rate
		for (int i = 0; i < numStages; ++i)
		{
			allpassFilters[i].SetRate(rate, i);
		}
	}
	
	void SetFeedback(float newFdbck)
	{
		feedback = newFdbck;
		// Recalculate filter coefficients based on the new rate
		for (int i = 0; i < numStages; ++i)
		{
			allpassFilters[i].feedback=feedback;
		}
	}

	private:
};


#endif