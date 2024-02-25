#ifndef lfofloat_h
#define lfofloat_h

#define FX_SHIFT 8
#define SHIFTED_1 256

class Filter3
{

public:
	Env env;
	float envam_prev;
	float freq;
	float freq_save;
	void init() {

		g_ = tan(0.1);
		r_ = 2.0;
		h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
		state_1_ = state_2_ = 0.0f;
		mode=0;
		envam_prev=0;
		freq=0.5;
		env.init();
	}
	
	void stop()
	{
		env.stop();
	}
	
	void start()
	{
		env.start();
	}
	
	void setenvA(uint32_t val)
	{
		env.setA(val);
	}

	void setenvD(uint32_t val)
	{
		env.setD(val);
	}

	void setenvS(uint32_t val)
	{
		env.setS(val);
	}

	void setenvR(uint32_t val)
	{
		env.setR(val);
	} 
	
	inline void set_fw(float f) {
		g_ = tan(f);
		h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
	  }
	
	inline void set_f(float fin) {
		freq_save = fin;
		float f = (fin*fin/127+1.0)/512.0;
		g_ = tan(f);
		h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
		freq=f;
	  }

	inline void set_f_q(float f, float reso) {
		g_ = tan(f);
		r_ = 1.0f / reso;
		h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
		freq=f;
	  }
	  
	inline void set_f_q(uint8_t fin, uint8_t rin) {
		float f = ((float)fin*(float)fin/127+1.0)/512.0;
		float reso = ((float)rin+1.0)/2.0;
		g_ = tan(f);
		r_ = 1.0f / reso;
		h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
		freq=f;
	  }
	  
	inline void set_q(float rin) {
		float reso = rin/20.0+0.5;
		r_ = 1.0f / reso;
	  }
	  
	inline void set_mode(int mo) {
		mode=mo;
	}

	
	inline float process(float in) 
	{
		float hp, bp, lp;
		float envam = env.amount();
		if(envam!=envam_prev){
			set_fw(envam*freq);
			envam_prev=envam;
		}
		hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
		bp = g_ * hp + state_1_;
		state_1_ = g_ * hp + bp;
		lp = g_ * bp + state_2_;
		state_2_ = g_ * bp + lp;
		
		if (mode == 1) {
		  return lp;
		} else if (mode == 2) {
		  return bp;
		} else if (mode == 3) {
		  return bp * r_;
		} else if (mode == 4) {
		  return hp;
		}
	}


private:
	float g_;
	float r_;
	float h_;

	float state_1_;
	float state_2_;
	
	int mode;
	
	float tan(float f)
	{
		const float a = 3.260e-01f * 31;
		const float b = 1.823e-01f * 306;
		float f2 = f * f;
		return f * (3.14159 + f2 * (a + b * f2));
	}
};
#endif