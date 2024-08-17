#ifndef rosic_TeeBeeFilter_h
#define rosic_TeeBeeFilter_h

#define ONE_OVER_SQRT2 0.70710678118654752440084436210485f

#include <setup.h>
#include <lookuptable.h>
#include "rosic_OnePoleFilter.h"

/**
  This class is a filter that aims to emulate the filter in the Roland TB 303. It's a variation of
  the Moog ladder filter which includes a highpass in the feedback path that reduces the resonance
  on low cutoff frequencies. Moreover, it has a highpass and an allpass filter in the input path to
  pre-shape the input signal (important for the sonic character of internal and subsequent
  nonlinearities).

  ...18 vs. 24 dB? blah?
*/

class TeeBeeFilter
{

  public:

    /** Enumeration of the available filter modes. */
    enum modes
    {
      FLAT = 0,
      LP_6,
	  BP_12_12,
	  BP_6_6,
	  HP_6,
      LP_24,
      HP_24,
	  LP_12,
      LP_18,
	  HP_12,
      HP_18,
	  BP_6_18,
      BP_18_6,
      BP_6_12,
      BP_12_6,
	  TB_303,      // ala mystran & kunn (page 40 in the kvr-thread)
      NUM_MODES
    };

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    TeeBeeFilter()
	{
	  cutoff              =  1000.0f;
	  drive               =     0.1f;
	  driveFactor         =     1.0f;
	  resonanceRaw        =     0.0f;
	  resonanceSkewed     =     0.0f;
	  g                   =     1.0f;
	  sampleRate          = SAMPLE_RATE;
	  twoPiOverSampleRate = 2.0*PI/sampleRate;

	  feedbackHighpass.setMode(OnePoleFilter::HIGHPASS);
	  feedbackHighpass.setCutoff(100.0f);

	 // SetMode(LP_18);
	  SetMode(TB_303);
	  calculateCoefficientsExact();
	  Init();
	}

    /** Destructor. */
    //~TeeBeeFilter();

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** Sets the sample-rate for this filter. */
    void SetSampleRate(float newSampleRate)
	{
	  if( newSampleRate > 0.0 )
		sampleRate = newSampleRate;
	  twoPiOverSampleRate = 2.0*PI/sampleRate;
	  highLimit = sampleRate / 4.18f;
	  feedbackHighpass.setSampleRate(newSampleRate);
	  calculateCoefficientsExact();
	}

    /** Sets the cutoff frequency for this filter - the actual coefficient calculation may be
      supressed by passing 'false' as second parameter, in this case, it should be triggered
      manually later by calling calculateCoefficients. */
    inline void SetCutoff(float newCutoff, bool updateCoefficients)
	{
	  //newCutoff=(newCutoff*80.0f)+200.0f;
	  //newCutoff=pow(10.0, ((newCutoff+5.0)/25.0f))+200.0f;
	  //newCutoff=midi_pitches[(int)newCutoff]/2+40.0f;
	  newCutoff= 100.0f  * pow(2, (newCutoff / 12.0));
	  if ( newCutoff != cutoff )
	  {
		if ( newCutoff < 200.0f ) // an absolute floor for the cutoff frequency - tweakable
		  cutoff = 200.0f;
		else if ( newCutoff > highLimit ) // seems to be stable with the current settings, higher values may lead to nan, +inf or -inf during processing
		  cutoff = highLimit;
		else
		  cutoff = newCutoff;

		if ( updateCoefficients == true )
		  calculateCoefficientsApprox4();
	  }
	}

    /** Sets the resonance in percent where 100% is self oscillation. */
    inline void SetResonance(float newResonance, bool updateCoefficients)
	{
	  resonanceRaw    =  newResonance/127.0f;
	//  compens = 1.8f * (resonanceRaw + 0.25f) * one_div((resonanceRaw + 0.25f) * 0.75f + 0.113f); // gain compensation; one_div(x) = 1/x
	  resonanceSkewed = (1.0f - exp(-3.0f * resonanceRaw)) / (1.0f - exp(-3.0f));
	  if ( updateCoefficients == true )
		calculateCoefficientsApprox4();
	}

    /** Sets the input drive in decibels. */
    void SetDrive(float newDrive)
	{
	  drive       = newDrive + 0.01f;

	}

    /** Sets the mode of the filter, @see: modes */
    void SetMode(int newMode)
	{
	  Serial.println("SetMode");
	  if( newMode >= 0 && newMode < NUM_MODES )
	  {
		mode = newMode;
		switch(mode)
		{
		case FLAT:      c0 =  1.0f; c1 =  0.0f; c2 =  0.0f; c3 =  0.0f; c4 =  0.0f;  break;
		case LP_6:      c0 =  0.0f; c1 =  1.0f; c2 =  0.0f; c3 =  0.0f; c4 =  0.0f;  break;
		case LP_12:     c0 =  0.0f; c1 =  0.0f; c2 =  1.0f; c3 =  0.0f; c4 =  0.0f;  break;
		case LP_18:     c0 =  0.0f; c1 =  0.0f; c2 =  0.0f; c3 =  1.0f; c4 =  0.0f;  break;
		case LP_24:     c0 =  0.0f; c1 =  0.0f; c2 =  0.0f; c3 =  0.0f; c4 =  1.0f;  break;
		case HP_6:      c0 =  1.0f; c1 = -1.0f; c2 =  0.0f; c3 =  0.0f; c4 =  0.0f;  break;
		case HP_12:     c0 =  1.0f; c1 = -2.0f; c2 =  1.0f; c3 =  0.0f; c4 =  0.0f;  break;
		case HP_18:     c0 =  1.0f; c1 = -3.0f; c2 =  3.0f; c3 = -1.0f; c4 =  0.0f;  break;
		case HP_24:     c0 =  1.0f; c1 = -4.0f; c2 =  6.0f; c3 = -4.0f; c4 =  1.0f;  break;
		case BP_12_12:  c0 =  0.0f; c1 =  0.0f; c2 =  1.0f; c3 = -2.0f; c4 =  1.0f;  break;
		case BP_6_18:   c0 =  0.0f; c1 =  0.0f; c2 =  0.0f; c3 =  1.0f; c4 = -1.0f;  break;
		case BP_18_6:   c0 =  0.0f; c1 =  1.0f; c2 = -3.0f; c3 =  3.0f; c4 = -1.0f;  break;
		case BP_6_12:   c0 =  0.0f; c1 =  0.0f; c2 =  1.0f; c3 = -1.0f; c4 =  0.0f;  break;
		case BP_12_6:   c0 =  0.0f; c1 =  1.0f; c2 = -2.0f; c3 =  1.0f; c4 =  0.0f;  break;
		case BP_6_6:    c0 =  0.0f; c1 =  1.0f; c2 = -1.0f; c3 =  0.0f; c4 =  0.0f;  break;
		default:        c0 =  1.0f; c1 =  0.0f; c2 =  0.0f; c3 =  0.0f; c4 =  0.0f;  // flat
		}
	  }
	  calculateCoefficientsApprox4();
	  Serial.println(b0);
	  Serial.println(a1);
	  Serial.println(k);
	  if(mode==TB_303) {Serial.println("TB303"); calculateCoefficientsExact();}
	}

    /** Sets the cutoff frequency for the highpass filter in the feedback path. */
    void SetFeedbackHighpassCutoff(float newCutoff) {
      feedbackHighpass.setCutoff(newCutoff);
    }

    //---------------------------------------------------------------------------------------------
    // inquiry:

    /** Returns the cutoff frequency of this filter. */
    float GetCutoff() const {
      return cutoff;
    }

    /** Returns the resonance parameter of this filter. */
    float GetResonance() const {
      return 100.0 * resonanceRaw;
    }

    /** Returns the drive parameter in decibels. */
    float GetDrive() const {
      return drive;
    }

    /** Returns the slected filter mode. */
    int GetMode() const {
      return mode;
    }

    /** Returns the cutoff frequency for the highpass filter in the feedback path. */
    float GetFeedbackHighpassCutoff() const {
      return feedbackHighpass.getCutoff();
    }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates one output sample at a time. */
    inline float Process(float in)
	{
	  static float y0, ret_val;
	  if ( mode == TB_303 )
	  {
	 //   y0 =  shape(in * drive) - feedbackHighpass.getSample(k * y4);
		
	  y0 = fclamp(0.08f * in - feedbackHighpass.getSample(k * y4), -10e4, 10e4);
		//y0  = in - k*shape(y4);
		//y0  = in-k*y4;
		y1 += 2.0f * b0 * (y0 - y1 + y2);
		y2 += b0 * (y1 - 2.0f * y2 + y3);
		y3 += b0 * (y2 - 2.0f * y3 + y4);
		y4 += b0 * (y3 - 2.0f * y4);
		return 2.0f * g * y4;
	  }

	  // apply drive and feedback to obtain the filter's input signal:
	  //float y0 = inputFilter.getSample(0.125*driveFactor*in) - feedbackHighpass.getSample(k*y4);
	  // y0 = 0.125*driveFactor*in - feedbackHighpass.getSample(k*y4);  
	  // y0 = fclamp(  (0.08f * in - feedbackHighpass.getSample(k * y4)), -1e8, 1e8);
	  y0 = fclamp(0.08f * in - feedbackHighpass.getSample(k * y4), -10e4, 10e4);

	  // DEBF("%0.3f\r\n", y0);
	  /*
		// cascade of four 1st order sections with nonlinearities:
		y1 = shape(b0*y0 - a1*y1);
		y2 = shape(b0*y1 - a1*y2);
		y3 = shape(b0*y2 - a1*y3);
		y4 = shape(b0*y3 - a1*y4);
	  */

	  // cascade of four 1st order sections with only 1 nonlinearity:
	  /*
		y1 =       b0*y0 - a1*y1;
		y2 =       b0*y1 - a1*y2;
		y3 =       b0*y2 - a1*y3;
		y4 = shape(b0*y3 - a1*y4);
	  */
	  /*
	  y1 = fclamp(y0 + a1 * (y0 - y1), -1e8, 1e8);  // fclamp on 4 stages gives additional 200us per 64samp buffer (todo : optimize fclamp() to avoid fmax() and fmin())
	  y2 = fclamp(y1 + a1 * (y1 - y2), -1e8, 1e8);
	  y3 = fclamp(y2 + a1 * (y2 - y3), -1e8, 1e8);
	  y4 = fclamp(y3 + a1 * (y3 - y4), -1e8, 1e8); // \todo: performance test both versions of the ladder
	*/
	  y1 = y0 + a1 * (y0 - y1);
	  y2 = y1 + a1 * (y1 - y2);
	  y3 = y2 + a1 * (y2 - y3);
	  y4 = y3 + a1 * (y3 - y4); 
	  
	  ret_val = (20.0f * (c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3 + c4 * y4 )) ;
	  // bias = 0.0005f * ret_val + 0.9995f * bias ;
	  return fast_tanh((ret_val - bias) );// * compens ;
	}

    //---------------------------------------------------------------------------------------------
    // others:


    /** Causes the filter to re-calculate the coeffiecients via the exact formulas. */
    inline void calculateCoefficientsExact()
	{
	  // calculate intermediate variables:
	  float wc = twoPiOverSampleRate * cutoff;
	  float s, c;
	  //sinCos(wc, &s, &c);             // c = cos(wc); s = sin(wc);
	  fast_sincos(wc, &s, &c);             // c = cos(wc); s = sin(wc);
	  float t  = tan(0.25f * (wc - PI));
	  float r  = resonanceSkewed;

	  // calculate filter a1-coefficient tuned such the resonance frequency is just right:
	  float a1_fullRes = t * one_div(s - c * t);

	  // calculate filter a1-coefficient as if there were no resonance:
	  float x        = exp(-wc);
	  float a1_noRes = -x;

	  // use a weighted sum between the resonance-tuned and no-resonance coefficient:
	  a1 = r * a1_fullRes + (1.0f - r) * a1_noRes;
	  // calculate the b0-coefficient from the condition that each stage should be a leaky
	  // integrator:
	  b0 = 1.0 + a1;

	  // calculate feedback factor by dividing the resonance parameter by the magnitude at the
	  // resonant frequency:
	  float gsq = b0 * b0 * one_div(1.0f + a1 * a1 + 2.0f * a1 * c);
	  k          = r * one_div(gsq * gsq);

	  if ( mode == TB_303 )
		k *= (4.25f);
	}

    /** Causes the filter to re-calculate the coeffiecients using an approximation that is valid
      for normalized radian cutoff frequencies up to pi/4. */
    inline void calculateCoefficientsApprox4()
	{
	  // calculate intermediate variables:
	  float wc  = twoPiOverSampleRate * cutoff;
	  float wc2 = wc * wc;
	  float r   = resonanceSkewed;
	  float tmp;

	  // compute the filter coefficient via a 12th order polynomial approximation (polynomial
	  // evaluation is done with a Horner-rule alike scheme with nested quadratic factors in the hope
	  // for potentially better parallelization compared to Horner's rule as is):
	  const float pa12 = -1.341281325101042e-02;
	  const float pa11 =  8.168739417977708e-02;
	  const float pa10 = -2.365036766021623e-01;
	  const float pa09 =  4.439739664918068e-01;
	  const float pa08 = -6.297350825423579e-01;
	  const float pa07 =  7.529691648678890e-01;
	  const float pa06 = -8.249882473764324e-01;
	  const float pa05 =  8.736418933533319e-01;
	  const float pa04 = -9.164580250284832e-01;
	  const float pa03 =  9.583192455599817e-01;
	  const float pa02 = -9.999994950291231e-01;
	  const float pa01 =  9.999999927726119e-01;
	  const float pa00 = -9.999999999857464e-01;
	  tmp  = wc2 * pa12 + pa11 * wc + pa10;
	  tmp  = wc2 * tmp  + pa09 * wc + pa08;
	  tmp  = wc2 * tmp  + pa07 * wc + pa06;
	  tmp  = wc2 * tmp  + pa05 * wc + pa04;
	  tmp  = wc2 * tmp  + pa03 * wc + pa02;
	  a1   = wc2 * tmp  + pa01 * wc + pa00;
	  b0   = 1.0f + a1;

	  // compute the scale factor for the resonance parameter (the factor to obtain k from r) via an
	  // 8th order polynomial approximation:
	  const float pr8 = -4.554677015609929e-05;
	  const float pr7 = -2.022131730719448e-05;
	  const float pr6 =  2.784706718370008e-03;
	  const float pr5 =  2.079921151733780e-03;
	  const float pr4 = -8.333236384240325e-02;
	  const float pr3 = -1.666668203490468e-01;
	  const float pr2 =  1.000000012124230e+00;
	  const float pr1 =  3.999999999650040e+00;
	  const float pr0 =  4.000000000000113e+00;
	  tmp  = wc2 * pr8 + pr7 * wc + pr6;
	  tmp  = wc2 * tmp + pr5 * wc + pr4;
	  tmp  = wc2 * tmp + pr3 * wc + pr2;
	  tmp  = wc2 * tmp + pr1 * wc + pr0; // this is now the scale factor
	  k    = r * tmp;
	  g    = 1.0f;

	  if ( mode == TB_303 )
	  {
		float fx = wc * ONE_OVER_SQRT2 * DIV_TWOPI;
		b0 = (0.00045522346f + 6.1922189f * fx) * one_div(1.0f + 12.358354f * fx + 4.4156345f * (fx * fx));
		k  = fx * (fx * (fx * (fx * (fx * (fx + 7198.6997f) - 5837.7917f) - 476.47308f) + 614.95611f) + 213.87126f) + 16.998792f;
		g  = k * 0.05882352f; // 17 reciprocal
		g  = (g - 1.0f) * r + 1.0f;                     // r is 0 to 1.0
		g  = (g * (1.0f + r));
		k  = k * r;                                   // k is ready now
	  }
	}

    /** Calculates sine and cosine of x - this is more efficient than calling sin(x) and
      cos(x) seperately. */
    inline void sinCos(float x, float* sinResult, float* cosResult)
	{
	#ifdef __GNUC__  // \todo assembly-version causes compiler errors on gcc
	  *sinResult = sin(x);
	  *cosResult = cos(x);
	#else
	  float s, c;     // do we need these intermediate variables?
	  __asm fld x
	  __asm fsincos
	  __asm fstp c
	  __asm fstp s
	  *sinResult = s;
	  *cosResult = c;
	#endif
	}

    /** Implements the waveshaping nonlinearity between the stages. */
    inline float shape(float x)
	  {
		// return tanhApprox(x); // \todo: find some more suitable nonlinearity here
		//return x; // test

	   // const float r6 = 1.0/6.0;
	  //  x = clip<float>(x, -SQRT2, SQRT2);
	  //  return x - r6*x*x*x;

		//return clip(x, -1.0, 1.0);
		return fast_tanh(x);
	  }

    /** Implements limitting of a given value */
    inline float fclamp(float val, float minval, float maxval) {
      return fmin(fmax(val, minval), maxval) ;
    };

    /** Resets the internal state variables. */
    void Init() {
	  feedbackHighpass.reset();
	  y1 = 0.0f;
	  y2 = 0.0f;
	  y3 = 0.0f;
	  y4 = 0.0f;
	  SetDrive(0.11f);
	  SetResonance(0.5f, false);
	  SetCutoff(1000.0f, true);
	}

    void Init(float sr)
	{
	  feedbackHighpass.reset();
	  SetSampleRate(sr);
	  y1 = 0.0f;
	  y2 = 0.0f;
	  y3 = 0.0f;
	  y4 = 0.0f;
	  SetDrive(0.11f);
	  SetResonance(0.5f, false);
	  SetCutoff(1000.0f, true);
	}
    //=============================================================================================

  protected:

    float b0, a1;              // coefficients for the first order sections
    float y1, y2, y3, y4;      // output signals of the 4 filter stages
    float c0, c1, c2, c3, c4;  // coefficients for combining various ouput stages
    float k;                   // feedback factor in the loop
    float g;                   // output gain
    float driveFactor;         // filter drive as raw factor
    float cutoff;              // cutoff frequency
    float drive;               // filter drive normalized
    float resonanceRaw;        // resonance parameter (normalized to 0...1)
    float resonanceSkewed;     // mapped resonance parameter to make it behave more musical
    float sampleRate;          // the sample rate in Hz
    float twoPiOverSampleRate; // 2*PI/sampleRate
    float highLimit;           // highest cutoff freq allowing stable behaviour
    float compens;             // drive gain correction
    float bias;                 // reso bias compensation
    int    mode;                // the selected filter-mode

    OnePoleFilter feedbackHighpass;

};


#endif
