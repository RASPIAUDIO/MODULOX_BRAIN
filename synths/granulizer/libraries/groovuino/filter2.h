#define FX_SHIFT 8
#define SHIFTED_1 256

class Filter2
{

public:

	void init() {

		g_ = tan(0.5);
		gi_ = 1.0 / (1.0 + g_);
		state_ = 0.0f;
		mode=1;
	}

	void setCutoffFreq(float f)
	{
		g_ = tan(f);
		gi_ = 1.0 / (1.0 + g_);
	}

	
	inline float process(float in) 
	{
		float lp;
		lp = (g_ * in + state_) * gi_;
		state_ = g_ * (in - lp) + lp;

		if (mode == 0) {
			return lp;
		} 
		else if (mode == 1) {
			return in - lp;
		} 
	}


private:
	float g_;
    float gi_;
    float state_;
	int mode;
	
	float tan(float f)
	{
		const float a = 3.260e-01f * 31;
		const float b = 1.823e-01f * 306;
		float f2 = f * f;
		return f * (3.14159 + f2 * (a + b * f2));
	}
};
