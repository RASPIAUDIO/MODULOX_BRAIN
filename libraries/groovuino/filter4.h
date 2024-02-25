#include <cmath>
#include <stdint.h>

#define MOOG_E         2.71828182845904523536028747135266250
#define MOOG_LOG2E     1.44269504088896340735992468100189214
#define MOOG_LOG10E    0.434294481903251827651128918916605082
#define MOOG_LN2       0.693147180559945309417232121458176568
#define MOOG_LN10      2.30258509299404568401799145468436421
#define MOOG_PI        3.14159265358979323846264338327950288
#define MOOG_PI_2      1.57079632679489661923132169163975144
#define MOOG_PI_4      0.785398163397448309615660845819875721
#define MOOG_1_PI      0.318309886183790671537767526745028724
#define MOOG_2_PI      0.636619772367581343075535053490057448
#define MOOG_2_SQRTPI  1.12837916709551257389615890312154517
#define MOOG_SQRT2     1.41421356237309504880168872420969808
#define MOOG_SQRT1_2   0.707106781186547524400844362104849039
#define MOOG_INV_PI_2  0.159154943091895

#define NO_COPY(C) C(const C &) = delete; C & operator = (const C &) = delete
#define NO_MOVE(C) NO_COPY(C); C(C &&) = delete; C & operator = (const C &&) = delete

#define SNAP_TO_ZERO(n)    if (! (n < -1.0e-8 || n > 1.0e-8)) n = 0;

// Linear interpolation, used to crossfade a gain table
inline float moog_lerp(float amount, float a, float b)
{
	return (1.0f - amount) * a + amount * b;
}

inline float moog_min(float a, float b)
{
	a = b - a;
	a += fabs(a);
	a *= 0.5f;
	a = b - a;
	return a;
}

// Clamp without branching
// If input - _limit < 0, then it really substracts, and the 0.5 to make it half the 2 inputs.
// If > 0 then they just cancel, and keeps input normal.
// The easiest way to understand it is check what happends on both cases.
inline float moog_saturate(float input)
{
	float x1 = fabs(input + 0.95f);
	float x2 = fabs(input - 0.95f);
	return 0.5f * (x1 - x2);
}

// Imitate the (tanh) clipping function of a transistor pair.
// to 4th order, tanh is x - x*x*x/3; this cubic's
// plateaus are at +/- 1 so clip to 1 and evaluate the cubic.
// This is pretty coarse - for instance if you clip a sinusoid this way you
// can sometimes hear the discontinuity in 4th derivative at the clip point
inline float clip(float value, float saturation, float saturationinverse)
{
	float v2 = (value * saturationinverse > 1 ? 1 :
				(value * saturationinverse < -1 ? -1:
				 value * saturationinverse));
	return (saturation * (v2 - (1./3.) * v2 * v2 * v2));
}

#define HZ_TO_RAD(f) (MOOG_PI_2 * f)
#define RAD_TO_HZ(omega) (MOOG_INV_PI_2 * omega)

	template<typename T>
	inline int ctz(T x)
	{
		int p, b;
		for (p = 0, b = 1; !(b & x); b <<= 1, ++p)
			;
		return p;
	}

inline double fast_tanh(double x) 
{
	double x2 = x * x;
	return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

class BiQuadBase
{
public:
	
	BiQuadBase()
	{
		bCoef = {{0.0f, 0.0f, 0.0f}};
		aCoef = {{0.0f, 0.0f}};
		w = {{0.0f, 0.0f}};
	}
	
	~BiQuadBase()
	{

	}
	
	// DF-II impl
	void Process(float * samples, const uint32_t n)
	{
		float out = 0;
		for (int s = 0; s < n; ++s)
		{
			out = bCoef[0] * samples[s] + w[0];
			w[0] = bCoef[1] * samples[s] - aCoef[0] * out + w[1];
			w[1] = bCoef[2] * samples[s] - aCoef[1] * out;
			samples[s] = out;
		}
	}

	float Tick(float s)
	{
		float out = bCoef[0] * s + w[0];
		w[0] = bCoef[1] * s - aCoef[0] * out + w[1];
		w[1] = bCoef[2] * s - aCoef[1] * out;
		return out;
	}

	void SetBiquadCoefs(std::array<float, 3> b, std::array<float, 2> a)
	{
		bCoef = b;
		aCoef = a;
	}
	
protected:
	std::array<float, 3> bCoef; // b0, b1, b2
	std::array<float, 2> aCoef; // a1, a2
	std::array<float, 2> w; // delays
};

class RBJFilter : public BiQuadBase
{
public:
	
	enum FilterType
	{
		LOWPASS,
		HIGHPASS,
		BANDPASS,
		ALLPASS,
		NOTCH,
		PEAK,
		LOW_SHELF,
		HIGH_SHELF
	};
	
	RBJFilter(FilterType type = FilterType::LOWPASS, float cutoff = 1, float sampleRate = 44100) : sampleRate(sampleRate), t(type)
	{
		Q = 1;
		A = 1;

		a = {{0.0f, 0.0f, 0.0f}};
		b = {{0.0f, 0.0f, 0.0f}};

		SetCutoff(cutoff);
	}
	
	~RBJFilter()
	{
		
	}
	
	void UpdateCoefficients()
	{
		cosOmega = cos(omega);
		sinOmega = sin(omega);
		
		switch (t)
		{
			case LOWPASS:
			{
				alpha = sinOmega / (2.0 * Q);
				b[0] = (1 - cosOmega) / 2;
				b[1] = 1 - cosOmega;
				b[2] = b[0];
				a[0] = 1 + alpha;
				a[1] = -2 * cosOmega;
				a[2] = 1 - alpha;
			} break;
				
			case HIGHPASS:
			{
				alpha = sinOmega / (2.0 * Q);
				b[0] = (1 + cosOmega) / 2;
				b[1] = -(1 + cosOmega);
				b[2] = b[0];
				a[0] = 1 + alpha;
				a[1] = -2 * cosOmega;
				a[2] = 1 - alpha;
			} break;
				
			case BANDPASS:
			{
				alpha = sinOmega * sinhf(logf(2.0) / 2.0 * Q * omega/sinOmega);
				b[0] = sinOmega / 2;
				b[1] = 0;
				b[2] = -b[0];
				a[0] = 1 + alpha;
				a[1] = -2 * cosOmega;
				a[2] = 1 - alpha;
			} break;
				
			case ALLPASS:
			{
				alpha = sinOmega / (2.0 * Q);
				b[0] = 1 - alpha;
				b[1] = -2 * cosOmega;
				b[2] = 1 + alpha;
				a[0] = b[2];
				a[1] = b[1];
				a[2] = b[0];
			} break;
				
			case NOTCH:
			{
				alpha = sinOmega * sinhf(logf(2.0) / 2.0 * Q * omega/sinOmega);
				b[0] = 1;
				b[1] = -2 * cosOmega;
				b[2] = 1;
				a[0] = 1 + alpha;
				a[1] = b[1];
				a[2] = 1 - alpha;
			} break;
				
			case PEAK:
			{
				alpha = sinOmega * sinhf(logf(2.0) / 2.0 * Q * omega/sinOmega);
				b[0] = 1 + (alpha * A);
				b[1] = -2 * cosOmega;
				b[2] = 1 - (alpha * A);
				a[0] = 1 + (alpha / A);
				a[1] = b[1];
				a[2] = 1 - (alpha / A);
			} break;
				
			case LOW_SHELF:
			{
				alpha = sinOmega / 2.0 * sqrt( (A + 1.0 / A) * (1.0 / Q - 1.0) + 2.0);
				b[0] = A * ((A + 1) - ((A - 1) * cosOmega) + (2 * sqrtf(A) * alpha));
				b[1] = 2 * A * ((A - 1) - ((A + 1) * cosOmega));
				b[2] = A * ((A + 1) - ((A - 1) * cosOmega) - (2 * sqrtf(A) * alpha));
				a[0] = ((A + 1) + ((A - 1) * cosOmega) + (2 * sqrtf(A) * alpha));
				a[1] = -2 * ((A - 1) + ((A + 1) * cosOmega));
				a[2] = ((A + 1) + ((A - 1) * cosOmega) - (2 * sqrtf(A) * alpha));
			} break;
				
			case HIGH_SHELF:
			{
				alpha = sinOmega / 2.0 * sqrt( (A + 1.0 / A) * (1.0 / Q - 1.0) + 2.0);
				b[0] = A * ((A + 1) + ((A - 1) * cosOmega) + (2 * sqrtf(A) * alpha));
				b[1] = -2 * A * ((A - 1) + ((A + 1) * cosOmega));
				b[2] = A * ((A + 1) + ((A - 1) * cosOmega) - (2 * sqrtf(A) * alpha));
				a[0] = ((A + 1) - ((A - 1) * cosOmega) + (2 * sqrtf(A) * alpha));
				a[1] = 2 * ((A - 1) - ((A + 1) * cosOmega));
				a[2] = ((A + 1) - ((A - 1) * cosOmega) - (2 * sqrtf(A) * alpha));
			} break;
		}
		
		// Normalize filter coefficients
		float factor = 1.0f / a[0];
		
		std::array<float, 2> aNorm;
		std::array<float, 3> bNorm;

		aNorm[0] = a[1] * factor;
		aNorm[1] = a[2] * factor;

		bNorm[0] = b[0] * factor;
		bNorm[1] = b[1] * factor;
		bNorm[2] = b[2] * factor;
		
		SetBiquadCoefs(bNorm, aNorm);
	}
	
	// In Hertz, 0 to Nyquist
	void SetCutoff(float c)
	{
		omega = HZ_TO_RAD(c) / sampleRate;
		UpdateCoefficients();
	}
	
	float GetCutoff()
	{
		return omega;
	}
	
	// Arbitrary, from 0.01f to ~20
	void SetQValue(float q)
	{
		Q = q;
		UpdateCoefficients();
	}
	
	float GetQValue()
	{
		return Q;
	}

	void SetType(FilterType newType)
	{
		t = newType;
		UpdateCoefficients();
	}
	
	FilterType GetType()
	{
		return t;
	}
	
private:

	float sampleRate;
	
	float omega;
	float cosOmega;
	float sinOmega;
	
	float Q;
	float alpha;
	float A;

	std::array<float, 3> a;
	std::array<float, 3> b;
	
	FilterType t;
};

// +/-0.05dB above 9.2Hz @ 44,100Hz
class PinkingFilter
{
	double b0, b1, b2, b3, b4, b5, b6;
public:
	PinkingFilter() : b0(0), b1(0), b2(0), b3(0), b4(0), b5(0), b6(0) {}
	float process(const float s)
	{
		b0 = 0.99886 * b0 + s * 0.0555179;
		b1 = 0.99332 * b1 + s * 0.0750759;
		b2 = 0.96900 * b2 + s * 0.1538520;
		b3 = 0.86650 * b3 + s * 0.3104856;
		b4 = 0.55000 * b4 + s * 0.5329522;
		b5 = -0.7616 * b5 - s * 0.0168980;
		const double pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + (s * 0.5362)) * 0.11;
		b6 = s * 0.115926;
		return pink;
	}
};

class BrowningFilter
{
float l;
public:
	BrowningFilter() : l(0) {}
	float process(const float s)
	{
		float brown = (l + (0.02f * s)) / 1.02f;
		l = brown;
		return brown * 3.5f; // compensate for gain
	}
};

class LadderFilterBase
{
public:
	
	LadderFilterBase(float sampleRate) : sampleRate(sampleRate) {}
	virtual ~LadderFilterBase() {}
	
	virtual void Process(float * samples, uint32_t n) = 0;
	virtual void SetResonance(float r) = 0;
	virtual void SetCutoff(float c) = 0;
	
	float GetResonance() { return resonance; }
	float GetCutoff() { return cutoff; }
	
protected:
	
	float cutoff;
	float resonance;
	float sampleRate;
};

class HuovilainenMoog : public LadderFilterBase
{
public:
	
	HuovilainenMoog(float sampleRate) : LadderFilterBase(sampleRate), thermal(0.000025)
	{
		memset(stage, 0, sizeof(stage));
		memset(delay, 0, sizeof(delay));
		memset(stageTanh, 0, sizeof(stageTanh));
		SetCutoff(1000.0f);
		SetResonance(0.10f);
	}
	
	virtual ~HuovilainenMoog()
	{
		
	}
	
	virtual void Process(float * samples, uint32_t n) override
	{
		for (int s = 0; s < n; ++s)
		{
			// Oversample
			for (int j = 0; j < 2; j++) 
			{
				float input = samples[s] - resQuad * delay[5];
				delay[0] = stage[0] = delay[0] + tune * (tanh(input * thermal) - stageTanh[0]);
				for (int k = 1; k < 4; k++) 
				{
					input = stage[k-1];
					stage[k] = delay[k] + tune * ((stageTanh[k-1] = tanh(input * thermal)) - (k != 3 ? stageTanh[k] : tanh(delay[k] * thermal)));
					delay[k] = stage[k];
				}
				// 0.5 sample delay for phase compensation
				delay[5] = (stage[3] + delay[4]) * 0.5;
				delay[4] = stage[3];
			}
			samples[s] = delay[5];
		}

	}
	
	virtual void SetResonance(float r) override
	{
		resonance = r;
		resQuad = 4.0 * resonance * acr;
	}
	
	virtual void SetCutoff(float c) override
	{
		cutoff = c;

		double fc =  cutoff / sampleRate;
		double f  =  fc * 0.5; // oversampled 
		double fc2 = fc * fc;
		double fc3 = fc * fc * fc;

		double fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * fc + 0.9988;
		acr = -3.9364 * fc2 + 1.8409 * fc + 0.9968;

		tune = (1.0 - exp(-((2 * MOOG_PI) * f * fcr))) / thermal; 

		SetResonance(resonance);
	}
	
private:
	
	double stage[4];
	double stageTanh[3];
	double delay[6];

	double thermal;
	double tune;
	double acr;
	double resQuad;
	
}; 

class MusicDSPMoog : public LadderFilterBase
{
	
public:
	
	MusicDSPMoog(float sampleRate) : LadderFilterBase(sampleRate)
	{
		memset(stage, 0, sizeof(stage));
		memset(delay, 0, sizeof(delay));
		SetCutoff(1000.0f);
		SetResonance(0.10f);
	}
	
	virtual ~MusicDSPMoog()
	{

	}
	
	virtual void Process(float * samples, uint32_t n) override
	{
		for (int s = 0; s < n; ++s)
		{
			float x = samples[s] - resonance * stage[3];

			// Four cascaded one-pole filters (bilinear transform)
			stage[0] = x * p + delay[0]  * p - k * stage[0];
			stage[1] = stage[0] * p + delay[1] * p - k * stage[1];
			stage[2] = stage[1] * p + delay[2] * p - k * stage[2];
			stage[3] = stage[2] * p + delay[3] * p - k * stage[3];
		
			// Clipping band-limited sigmoid
			stage[3] -= (stage[3] * stage[3] * stage[3]) / 6.0;
			
			delay[0] = x;
			delay[1] = stage[0];
			delay[2] = stage[1];
			delay[3] = stage[2];

			samples[s] = stage[3];
		}
	}
	
	virtual void SetResonance(float r) override
	{
		resonance = r * (t2 + 6.0 * t1) / (t2 - 6.0 * t1);
	}
	
	virtual void SetCutoff(float c) override
	{
		cutoff = 2.0 * c / sampleRate;

		p = cutoff * (1.8 - 0.8 * cutoff);
		k = 2.0 * sin(cutoff * MOOG_PI * 0.5) - 1.0;
		t1 = (1.0 - p) * 1.386249;
		t2 = 12.0 + t1 * t1;

		SetResonance(resonance);
	}
	
private:
	
	double stage[4];
	double delay[4];

	double p;
	double k;
	double t1;
	double t2;

};