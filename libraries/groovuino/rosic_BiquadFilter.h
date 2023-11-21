#ifndef rosic_BiquadFilter_h
#define rosic_BiquadFilter_h

#include <setup.h>
#include <lookuptable.h>

/**
This is an implementation of a simple one-pole filter unit.
*/

class BiquadFilter
{

public:

/** Enumeration of the available filter modes. */
enum modes
{
  BYPASS = 0,
  LOWPASS6,
  LOWPASS12,
  HIGHPASS6,
  HIGHPASS12,
  BANDPASS,
  BANDREJECT,
  PEAK,
  LOW_SHELF,
  //HIGH_SHELF,
  //ALLPASS,

  NUM_FILTER_MODES
};

//---------------------------------------------------------------------------------------------
// construction/destruction:

/** Constructor. */
BiquadFilter()
{
  frequency  = 1000.0f;
  gain       = 0.0f;
  bandwidth  = 2.0f*asinh(1.0f/sqrt(2.0f))/log(2.0f);
  sampleRate = SAMPLE_RATE;
  mode       = BYPASS;
  calcCoeffs();
  reset();    
} 

//---------------------------------------------------------------------------------------------
// parameter settings:

/** Sets the sample-rate (in Hz) at which the filter runs. */
void setSampleRate(float newSampleRate)
{
  if( newSampleRate > 0.0f )
    sampleRate = newSampleRate;
  calcCoeffs();
}

/** Sets the filter mode as one of the values in enum modes. */
void setMode(int newMode)
{
  mode = newMode; // 0:bypass, 1:Low Pass, 2:High Pass
  calcCoeffs();
}

/** Sets the center frequency in Hz. */
void setFrequency(float newFrequency)
{
  frequency = newFrequency;
  calcCoeffs();
}

/** Sets the boost/cut gain in dB. */
void setGain(float newGain)
{
  gain = newGain;
  calcCoeffs();
}

/** Sets the bandwidth in octaves. */
void setBandwidth(float newBandwidth)
{
  bandwidth = newBandwidth;
  calcCoeffs();
}

//---------------------------------------------------------------------------------------------
// inquiry

/** Sets the filter mode as one of the values in enum modes. */
int getMode() const { return mode; }

/** Returns the center frequency in Hz. */
float getFrequency() const { return frequency; }

/** Returns the boost/cut gain in dB. */
float getGain() const { return gain; }

/** Returns the bandwidth in octaves. */
float getBandwidth() const { return bandwidth; }

//---------------------------------------------------------------------------------------------
// audio processing:

/** Calculates a single filtered output-sample. */
inline float getSample(float in);

//---------------------------------------------------------------------------------------------
// others:

/** Resets the internal buffers (for the \f$ x[n-1], y[n-1] \f$-samples) to zero. */
void reset()
{
  x1 = 0.0f;
  x2 = 0.0f;
  y1 = 0.0f;
  y2 = 0.0f;
}

//=============================================================================================

protected:

// internal functions:
void calcCoeffs()
{
  float w = 2.0f*PI*frequency/sampleRate;
  float s, c;
  switch(mode)
  {
  case LOWPASS6: 
    {
      // formula from dspguide:
      float x = exp(-w);
      a1 = x;
      a2 = 0.0f;
      b0 = 1.0f-x;
      b1 = 0.0f;
      b2 = 0.0f;
    }
    break;
  case LOWPASS12: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      fast_sincos(w, &s, &c);
      float q     = dB2amp(gain);
      float alpha = s/(2.0f*q);
      float scale = 1.0f/(1.0f+alpha);
      a1 = 2.0f*c       * scale;
      a2 = (alpha-1.0f) * scale;
      b1 = (1.0f-c)     * scale;
      b0 = 0.5f*b1;
      b2 = b0;
    }
    break;
  case HIGHPASS6: 
    {
      // formula from dspguide:
      float x = exp(-w);
      a1 = x;
      a2 = 0.0f;
      b0 = 0.5f*(1.0f+x);
      b1 = -b0;
      b2 = 0.0f;
    }
    break;
  case HIGHPASS12: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      fast_sincos(w, &s, &c);
      float q     = dB2amp(gain);
      float alpha = s/(2.0f*q);
      float scale = 1.0f/(1.0f+alpha);
      a1 = 2.0f*c       * scale;
      a2 = (alpha-1.0f) * scale;
      b1 = -(1.0f+c)    * scale;
      b0 = -0.5f*b1;
      b2 = b0;
    }
    break;
  case BANDPASS: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:      
      fast_sincos(w, &s, &c);
      float alpha = s * sinh( 0.5f*log(2.0f) * bandwidth * w / s );
      float scale = 1.0f/(1.0f+alpha);
      a1 = 2.0f*c       * scale;
      a2 = (alpha-1.0f) * scale;
      b1 = 0.0f;
      b0 = 0.5f*s       * scale;
      b2 = -b0;
    }
    break;
  case BANDREJECT: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      fast_sincos(w, &s, &c);
      float alpha = s * sinh( 0.5f*log(2.0f) * bandwidth * w / s );
      float scale = 1.0f/(1.0f+alpha);
      a1 = 2.0f*c       * scale;
      a2 = (alpha-1.0f) * scale;
      b0 = 1.0f         * scale;
      b1 = -2.0f*c      * scale;
      b2 = 1.0f         * scale;
    }
    break;
  case PEAK: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      fast_sincos(w, &s, &c);
      float alpha = s * sinh( 0.5f*log(2.0f) * bandwidth * w / s );
      float A     = dB2amp(gain);
      float scale = 1.0f/(1.0f+alpha/A);
      a1 = 2.0f*c             * scale;
      a2 = ((alpha/A) - 1.0) * scale;
      b0 = (1.0f+alpha*A)     * scale;
      b1 = -2.0f*c            * scale;
      b2 = (1.0f-alpha*A)     * scale;
    }
    break;
  case LOW_SHELF: 
    {
      // formula from Robert Bristow Johnson's biquad cookbook:
      fast_sincos(w, &s, &c);
      float A     = dB2amp(0.5f*gain);
      float q     = 1.0f / (2.0f*sinh( 0.5f*log(2.0f) * bandwidth ));
      float beta  = sqrt(A) / q;
      float scale = 1.0f / ( (A+1.0f) + (A-1.0f)*c + beta*s);
      a1 = 2.0f *     ( (A-1.0f) + (A+1.0f)*c          ) * scale;
      a2 = -         ( (A+1.0f) + (A-1.0f)*c - beta*s ) * scale;
      b0 =       A * ( (A+1.0f) - (A-1.0f)*c + beta*s ) * scale;
      b1 = 2.0f * A * ( (A-1.0f) - (A+1.0f)*c          ) * scale;
      b2 =       A * ( (A+1.0f) - (A-1.0f)*c - beta*s ) * scale;
    }
    break;




    // \todo: implement shelving and allpass modes

  default: // bypass
    {
      b0 = 1.0f;
      b1 = 0.0f;
      b2 = 0.0f;
      a1 = 0.0f;
      a2 = 0.0f;
    }break;
  }
}

float b0, b1, b2, a1, a2;
float x1, x2, y1, y2;

float frequency, gain, bandwidth;
float sampleRate;
int    mode;

};

//-----------------------------------------------------------------------------------------------
// inlined functions:

inline float BiquadFilter::getSample(float in)
{
// calculate the output sample:
float y = b0*in + b1*x1 + b2*x2 + a1*y1 + a2*y2 + TINY;

// update the buffer variables:
x2 = x1;
x1 = in;
y2 = y1;
y1 = y;

return y;
}


#endif // rosic_BiquadFilter_h
