class Disto
{

public:

	int gain_in;
	int gain_out;
	int type;

	void init()
	{
		gain_in=5;
		gain_out=64;
		type=0;
	}

	int16_t out(int16_t x)
	{
		x *= gain_in;
		if(type==0) x=SoftClip(x);
		if(type==1) x=SoftClip2(x);
		if(type==2) x=HardClip(x);
		return (x*gain_out)>>7;
	}
	
	void set_gain(int gin, int gout)
	{
		gain_in = gin/2;
		gain_out = gout/2+32;
	}
	
	void set_type(int ty)
	{
		type=ty;
	}


private:
	int16_t SoftLimit(int16_t x)
	{
		return x * (55296 + ((x * x)>>12)) / (55296 + 9.f * ((x * x)>>12));
	}

	int16_t SoftClip(int16_t x)
	{
		if(x < -6144)
			return -2048;
		else if(x > 6144)
			return 2048;
		else
			return SoftLimit(x);
	}

	int16_t SoftClip2(int16_t in)      // 1/X DISTORTION FROM FV-1
	{
		int16_t out = in;
		if ( in > 2048)
		{
			out = 4096 - 2048*2048 / in;
		}
		if ( in < -2048)
		{
			out = -4096 - 2048*2048 / in;
		}
		return out / 2;
	}

	int16_t HardClip(int16_t x)
	{
		if(x < -2048)
			return -2048;
		else if(x > 2048)
			return 2048;
		else
			return x;
	}
};
