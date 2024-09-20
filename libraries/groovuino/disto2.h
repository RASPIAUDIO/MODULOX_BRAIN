#ifndef disto2_h
#define disto2_h

#include <math.h>
//#include <setup.h>
//#include <lookuptable.h>

class Disto2
{

public:

	//float gain;
	int type;
	float drive_;
    float pre_gain_;
    float post_gain_;
    float compens_;

	void init()
	{
		//gain=2.0;
		set_gain(1);
		type=0;
	}

	float out(float x)
	{
		/*if(type==0) x=fast_tanh(gain * x);
		if(type==6) x=tanh(gain * x);
		if(type==3) x=mod(gain*x+1.0,2.0)-1;
		if(type==4) x=abs(mod(2*gain*x+2.0,4.0)-2.0)-1.0;
		if(type==1) x=fast_sin(gain*x);
		if(type==2) 
		{
			if (x > 0)
			   x = 1 - exp(-gain*x);
			else
			   x = -1 + exp(gain*x);
		}
		if(type==5) 
		{
			x=x*x*gain;
		}

		if(x>2.0) x = 2.0;
		if(x<-2.0) x = -2.0;
		return (x);*/
		
		float pre = (float)(pre_gain_ * x * 2.0f);
    
        return (float)(fast_tanh(pre) * post_gain_) ;
		//return x;
	}
	
	void set_gain(int gin)
	{
		//gain = gin/12.0+1.0;
		
		float drive = 0.125f + ((float)gin/127.0) * (0.875f);
		//drive  = fclamp(drive, 0.f, 1.f);
		drive_ = 1.999f * (float)drive;

		const float drive_2    = (float)drive_ * drive_;
		const float pre_gain_a = (float)drive_ * 0.5f;
		const float pre_gain_b = (float)drive_2 * drive_2 * drive_ * 24.0f;
		pre_gain_              = (float)pre_gain_a + (float)(pre_gain_b - pre_gain_a) * drive_2;

		const float drive_squashed = drive_ * (2.0f - drive_);
		post_gain_ =  0.5f * one_div( fast_tanh((float)(0.33f + drive_squashed * (float)(pre_gain_ - 0.33f))));
	}
	
	void set_type(int ty)
	{
		type=ty;
	}
	
	float linearFold(float value, float drive) {
      float adjust = value * drive * 0.25f + 0.75f;
      float range = mod(adjust,1);
      return abs(range * -4.0f + 2.0f) - 1.0f;
    }

    float sinFold(float value, float drive) {
      float adjust = value * drive * -0.25f + 0.5f;
      float range = mod(adjust,1);
      return sin(range);
    }


    float softClip(float value, float drive) {
      return tanh(value * drive);
    }

    float hardClip(float value, float drive) {
      return clamp(value * drive, -1.0f, 1.0f);
    }

    float bitCrush(float value, float drive) {
      return round(value / drive) * drive;
    }

	
	
private:
	float mod (float n, float d) {
	  n = fmod(n, d);
	  if (n<0) n += d;
	  return n;
	}
	/*float tan(float f)
	{
		const float a = 3.260e-01f * 31;
		const float b = 1.823e-01f * 306;
		float f2 = f * f;
		return f * (3.14159 + f2 * (a + b * f2));
	}*/
	float tanh(float value) {
		  float abs_value = fabsf(value);
		  float square = value * value;

		  float num = value * (2.45550750702956f + 2.45550750702956f * abs_value +
									square * (0.893229853513558f + 0.821226666969744f * abs_value));
		  float den = 2.44506634652299f + (2.44506634652299f + square) *
						   fabsf(value + 0.814642734961073f * value * abs_value);
		  return num / den;
    }
	
	
	float mod(float value, float* divisor) {
      return modff(value, divisor);
    }


	float clamp(float value, float min,float max) {
		  return fmin(max, fmax(value, min));
	}

};

#endif
