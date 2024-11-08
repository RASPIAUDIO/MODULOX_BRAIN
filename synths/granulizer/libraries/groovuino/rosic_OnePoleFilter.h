#ifndef rosic_OnePoleFilter_h
#define rosic_OnePoleFilter_h

#include <setup.h>
#include <lookuptable.h>
/**
  This is an implementation of a simple one-pole filter unit.
*/

class OnePoleFilter
{
  public:

    /** This is an enumeration of the available filter modes. */
    enum modes
    {
      BYPASS = 0,
      LOWPASS,
      HIGHPASS,
      LOWSHELV,
      HIGHSHELV,
      ALLPASS
    };
    // \todo (maybe): let the user choose between LP/HP versions obtained via bilinear trafo and
    // impulse invariant trafo

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. */
    OnePoleFilter()
	{
	  shelvingGain = 1.0f;
	  setSampleRate((float)SAMPLE_RATE);  // sampleRate = 44100 Hz by default
	  setMode      (0);        // bypass by default
	  setCutoff    (100.0f);  // cutoff = 100 Hz by default
	  reset();                 // reset memorized samples to zero
	}

    //---------------------------------------------------------------------------------------------
    // parameter settings:

    /** Sets the sample-rate. */
    void setSampleRate(float newSampleRate)
	{
	  if( newSampleRate > 0.0f )
		sampleRate = newSampleRate;
	  sampleRateRec = 1.0f / sampleRate;

	  calcCoeffs();
	  return;
	}

    /** Chooses the filter mode. See the enumeration for available modes. */
    void setMode(int newMode)
	{
	  mode = newMode; // 0:bypass, 1:Low Pass, 2:High Pass
	  calcCoeffs();
	}

    /** Sets the cutoff-frequency for this filter. */
    void setCutoff(float newCutoff)
	{
	  if( (newCutoff>0.0f) && (newCutoff<=20000.0f) )
		cutoff = newCutoff;
	  else
		cutoff = 20000.0f;

	  calcCoeffs();
	  return;
	}

    /** This will set the time constant 'tau' for the case, when lowpass mode is chosen. This is
      the time, it takes for the impulse response to die away to 1/e = 0.368... or equivalently, the
      time it takes for the step response to raise to 1-1/e = 0.632... */
    void setLowpassTimeConstant(float newTimeConstant) {
      setCutoff(1.0 / (2 * PI * newTimeConstant));
    }

    /** Sets the gain factor for the shelving modes (this is not in decibels). */
    void setShelvingGain(float newGain)
	{
	  if( newGain > 0.0f )
	  {
		shelvingGain = newGain;
		calcCoeffs();
	  }
	  else 
	  {
	  }
	}

    /** Sets the gain for the shelving modes in decibels. */
    void setShelvingGainInDecibels(float newGain)
	{
	  setShelvingGain(exp(newGain * 0.11512925464970228420089957273422f));
	}

    /** Sets the filter coefficients manually. */
    void setCoefficients(float newB0, float newB1, float newA1)
	{
	  b0 = newB0;
	  b1 = newB1;
	  a1 = newA1;
	}

    /** Sets up the internal state variables for both channels. */
    void setInternalState(float newX1, float newY1)
	{
	  x1 = newX1;
	  y1 = newY1;
	}

    //---------------------------------------------------------------------------------------------
    // inquiry

    /** Returns the cutoff-frequency. */
    float getCutoff() const {
      return cutoff;
    }

    //---------------------------------------------------------------------------------------------
    // audio processing:

    /** Calculates a single filtered output-sample. */
    inline float getSample(float in);

    //---------------------------------------------------------------------------------------------
    // others:
	
	void calcCoeffs()
	{
	  switch(mode)
	  {
	  case LOWPASS: 
		{
		  // formula from dspguide:
		  float x = exp( -2.0f * PI * cutoff * sampleRateRec); 
		  b0 = 1.0f-x;
		  b1 = 0.0f;
		  a1 = x;
		}
		break;
	  case HIGHPASS:  
		{
		  // formula from dspguide:
		  float x = exp( -2.0f * PI * cutoff * sampleRateRec);
		  b0 =  0.5f*(1.0f+x);
		  b1 = -0.5f*(1.0f+x);
		  a1 = x;
		}
		break;
	  case LOWSHELV:
		{
		  // formula from DAFX:
		  float c = 0.5f*(shelvingGain-1.0f);
		  float t = tan(PI*cutoff*sampleRateRec);
		  float a;
		  if( shelvingGain >= 1.0f )
			a = (t-1.0f)/(t+1.0f);
		  else
			a = (t-shelvingGain)/(t+shelvingGain);

		  b0 = 1.0f + c + c*a;
		  b1 = c + c*a + a;
		  a1 = -a;
		}
		break;
	  case HIGHSHELV:
		{
		  // formula from DAFX:
		  float c = 0.5f*(shelvingGain-1.0f);
		  float t = tan(PI*cutoff*sampleRateRec);
		  float a;
		  if( shelvingGain >= 1.0f )
			a = (t-1.0f)/(t+1.0f);
		  else
			a = (shelvingGain*t-1.0f)/(shelvingGain*t+1.0f);

		  b0 = 1.0f + c - c*a;
		  b1 = a + c*a - c;
		  a1 = -a;
		}
		break;

	  case ALLPASS:  
		{
		  // formula from DAFX:
		  float t = tan(PI*cutoff*sampleRateRec);
		  float x = (t-1.0f) / (t+1.0f);

		  b0 = x;
		  b1 = 1.0f;
		  a1 = -x;
		}
		break;

	  default: // bypass
		{
		  b0 = 1.0f;
		  b1 = 0.0f;
		  a1 = 0.0f;
		}break;
	  }
	}

    /** Resets the internal buffers (for the \f$ x[n-1], y[n-1] \f$-samples) to zero. */
    void reset()
	{
	  x1 = 0.0f;
	  y1 = 0.0f;
	}

    //=============================================================================================

  protected:

    // buffering:
    float x1, y1;

    // filter coefficients:
    float b0; // feedforward coeffs
    float b1;
    float a1; // feedback coeff

    // filter parameters:
    float cutoff;
    float shelvingGain;
    int    mode;

    float sampleRate;
    float sampleRateRec;  // reciprocal of the sampleRate

    // internal functions:

};

//-----------------------------------------------------------------------------------------------
// inlined functions:

inline float OnePoleFilter::getSample(float in)
{
  // calculate the output sample:
  y1 = (float)b0 * in + b1 * x1 + a1 * y1 + (float)1.1e-38;

  // update the buffer variables:
  x1 = in;

  return y1;
}
#endif
