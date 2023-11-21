#ifndef lookuptable_h
#define lookuptable_h

#define TABLE_BIT  		        10UL				// bits per index of lookup tables for waveforms, exp(), sin(), cos() etc. 10 bit means 2^10 = 1024 samples
#define TABLE_SIZE            (1<<TABLE_BIT)        // samples used for lookup tables (it works pretty well down to 32 samples due to linear approximation, so listen and free some memory at your choice)
#define TABLE_MASK  	        (TABLE_SIZE-1)        // strip MSB's and remain within our desired range of TABLE_SIZE
#define CICLE_INDEX(i)        (((int32_t)(i)) & TABLE_MASK ) // this way we can operate with periodic functions or waveforms without phase-reset ("if's" are time-consuming)

const float DIV_TABLE_SIZE =  1.0f / (float)TABLE_SIZE;

const float TWOPI = PI*2.0f;
const float MIDI_NORM = 1.0f/127.0f;
const float DIV_PI = 1.0f/PI;
const float DIV_TWOPI = 1.0f/TWOPI;

#define SAMPLE_RATE  44100

const float DIV_SAMPLE_RATE = 1.0f / (float)SAMPLE_RATE;
const float DIV_2SAMPLE_RATE = 0.5f / (float)SAMPLE_RATE;

#define TANH_LOOKUP_MAX 5.0f        // maximum X argument value for tanh(X) lookup table, tanh(X)~=1 if X>4 
const float TANH_LOOKUP_COEF = (float)TABLE_SIZE / TANH_LOOKUP_MAX;

// lookuptables
static float midi_pitches[128];
static float midi_phase_steps[128];
static float midi_tbl_steps[128];
static float exp_square_tbl[TABLE_SIZE+1];
//static float square_tbl[TABLE_SIZE+1];
//static float saw_tbl[TABLE_SIZE+1];
static float exp_tbl[TABLE_SIZE+1];
static float knob_tbl[TABLE_SIZE+1]; // exp-like curve
static float tanh_tbl[TABLE_SIZE+1];
static float sin_tbl[TABLE_SIZE+1];
static uint32_t last_reset = 0;

inline float bilinearLookup(float (&table)[16][16], float x, float y) {
  static float kmap = 0.1181f; // map from 0-127 to 0-14.99
  int32_t i,j;
  float fi,fj;
  float v1,v2,v3,v4;
  float res1,res2,res3;
  x *= kmap;
  y *= kmap;
  i = (int32_t)x;
  j = (int32_t)y;
  fi = (float)x - i;
  fj = (float)y - j;
  v1 = table[i][j];
  v2 = table[i+1][j];
  v3 = table[i][j+1];
  v4 = table[i+1][j+1];  
  res1 = (float)fi * (float)(v2-v1) + v1;
  res2 = (float)fi * (float)(v4-v3) + v3;
  res3 = (float)fj * (float)(res2-res1) + res1;
  return res3;
}

inline float lookupTable(float (&table)[TABLE_SIZE+1], float index ) { // lookup value in a table by float index, using linear interpolation
  static float v1, v2, res;
  static int32_t i;
  static float f;
 // if (index >= TABLE_SIZE) return table[TABLE_SIZE];
  i = (int32_t)index;
  f = (float)index - i;
  v1 = (table)[i];
  v2 = (table)[i+1];
  res = (float)f * (float)(v2-v1) + v1;
 // DEBF("i %0.6f mantissa %0.6f v1 %0.6f v2 %0.6f \r\n" , index , f , v1, v2  );
  return res;
}

float noteToFreq(int note) {
    return (440.0f / 32.0f) * pow(2, ((float)(note - 9) / 12.0));
}

float expSaw_fill(uint16_t i) { // this one contains a piece of exp(-x) normalized to fit into [-1.0 .. 1.0] , "saw", "square" are generated basing on this table
  float x = (float)i * 2048.0f * (float)DIV_TABLE_SIZE;
  float res = exp((float)(-x)*0.002057613168f) * 2.03f - 1.03f;
  return res;
}

float knob_fill(uint16_t i) { // f(x) = (exp(k*x)-1)/b, 0 <= x <= 1, 0 <= f(x) <= 1, x mapped to [0 .. TABLE_SIZE]
  float x = (float)i * (float)DIV_TABLE_SIZE;
  float res = ( expf((float)(x * 2.71f))-1.0f) * 0.071279495455219f;
  return res;
}

float tanh_fill(uint16_t i) {
  float res = tanh( (float)i * TANH_LOOKUP_MAX * (float)DIV_TABLE_SIZE); // 0.0 -- 5.0 argument
  return res;
}

float sin_fill(uint16_t i) {
  float res = sinf( (float)i * TWOPI * (float)DIV_TABLE_SIZE ); // 0.0 -- 2*pi argument
  return res;
}

float expSquare_fill(uint16_t i) { // requires exp() table
  uint16_t j = i + 0.5f*TABLE_SIZE;
  if (j>=TABLE_SIZE) j = j - TABLE_SIZE;
  float res = 0.685f * (exp_tbl[i]-exp_tbl[j]);
  return res;
}

float freqToPhaseInc(float freq, uint16_t sampleSize, uint16_t sampleRate) {
  return freq * (float)sampleSize / (float)sampleRate;
}

void buildTables() {
  for (int i = 0 ; i<128; ++i) {
    midi_pitches[i] = noteToFreq(i);
    midi_phase_steps[i] = noteToFreq(i) * PI * 2.0f * (float)DIV_SAMPLE_RATE;
    midi_tbl_steps[i] = noteToFreq(i) * (float)TABLE_SIZE * (float)DIV_SAMPLE_RATE;
  }

  for (int i = 0; i <= TABLE_SIZE; i++) {
    exp_tbl[i] = expSaw_fill(i);
  }

  for (int i = 0; i <= TABLE_SIZE; i++) {
    exp_square_tbl[i] = expSquare_fill(i);
    tanh_tbl[i] = tanh_fill(i); 
    knob_tbl[i] = knob_fill(i);
	  sin_tbl[i]  = sin_fill(i);
  //  saw_tbl[i] = 1.0f - 2.0f * (float)i * (float)DIV_TABLE_SIZE;
  //  square_tbl[i] = (i>TABLE_SIZE/2) ? 1.0f : -1.0f; 
  }
 /* 
  DEBF("Control sin(-10)=%0.6f fast_sin(-10)=%0.6f\r\n", sinf(-10.0f), fast_sin(-10.0f));
  DEBF("Control sin(10+2*pi)=%0.6f fast_sin(10+2*pi)=%0.6f\r\n", sinf(10.0f + TWOPI), fast_sin(10.0f+TWOPI));
  DEBF("Control cos(-6)=%0.6f fast_cos(-6)=%0.6f\r\n", cosf(-6.0f), fast_cos(-6.0f));
  while(1){;}
  */
}

inline float fclamp(float in, float min, float max){
    return fmin(fmax(in, min), max);
}

inline float fast_tanh(float x){
    float sign = 1.0f;
    if (x<0) {
      x = -x;
      sign = -1.0f;
    }
   
    if (x>=4.95f) {
      return sign; // tanh(x) ~= 1, when |x| > 4
    }

  //  if (x<=0.4f) return float(x*sign) * 0.9498724f; // smooth region borders; tanh(x) ~= x, when |x| < 0.4 
    return  sign * lookupTable(tanh_tbl, (x*TANH_LOOKUP_COEF)); // lookup table contains tanh(x), 0 <= x <= 5
  // float poly = (2.12f-2.88f*x+4.0f*x*x);
   // return sign * x * (poly * one_div(poly * x + 1.0f)); // very good approximation found here https://www.musicdsp.org/en/latest/Other/178-reasonably-accurate-fastish-tanh-approximation.html
                                                    // but it uses float division which is not that fast on esp32
}

inline float fast_sin(const float x) {
  const float argument = ((x * DIV_TWOPI) * TABLE_SIZE);
  const float res = lookupTable(sin_tbl, CICLE_INDEX(argument)+((float)argument-(int32_t)argument));
  return res;
}

inline float fast_cos(const float x) {  
  const float argument = ((x * DIV_TWOPI + 0.25f) * TABLE_SIZE);
  const float res = lookupTable(sin_tbl, CICLE_INDEX(argument)+((float)argument-(int32_t)argument));
  return res;
}

inline void fast_sincos(const float x, float* sinRes, float* cosRes){
	*sinRes = fast_sin(x);
	*cosRes = fast_cos(x);
}


// reciprocal asm injection for xtensa LX6 FPU
static __attribute__((always_inline)) inline float one_div(float a) {
    float result;
    asm volatile (
        "wfr f1, %1"          "\n\t"
        "recip0.s f0, f1"     "\n\t"
        "const.s f2, 1"       "\n\t"
        "msub.s f2, f1, f0"   "\n\t"
        "maddn.s f0, f0, f2"  "\n\t"
        "const.s f2, 1"       "\n\t"
        "msub.s f2, f1, f0"   "\n\t"
        "maddn.s f0, f0, f2"  "\n\t"
        "rfr %0, f0"          "\n\t"
        : "=r" (result)
        : "r" (a)
        : "f0","f1","f2"
    );
    return result;
}

inline float dB2amp(float dB){
  return expf(dB * 0.11512925464970228420089957273422f);
  //return pow(10.0, (0.05*dB)); // naive, inefficient version
}

inline float amp2dB(float amp)
{
  return 8.6858896380650365530225783783321f * logf(amp);
  //return 20*log10(amp); // naive version
}

inline float linToLin(float in, float inMin, float inMax, float outMin, float outMax)
{
  // map input to the range 0.0...1.0:
  float tmp = (in-inMin) * one_div(inMax-inMin);

  // map the tmp-value to the range outMin...outMax:
  tmp *= (outMax-outMin);
  tmp += outMin;

  return tmp;
}

inline float linToExp(float in, float inMin, float inMax, float outMin, float outMax)
{
  // map input to the range 0.0...1.0:
  float tmp = (in-inMin) * one_div(inMax-inMin);

  // map the tmp-value exponentially to the range outMin...outMax:
  //tmp = outMin * exp( tmp*(log(outMax)-log(outMin)) );
  return outMin * expf( tmp*(logf(outMax * one_div(outMin))) );
}



inline float expToLin(float in, float inMin, float inMax, float outMin, float outMax)
{
  float tmp = logf(in * one_div(inMin)) * one_div( logf(inMax * one_div(inMin)));
  return outMin + tmp * (outMax-outMin);
}

inline float knobMap(float in, float outMin, float outMax) {
  return outMin + lookupTable(knob_tbl, (int)(in * TABLE_SIZE)) * (outMax - outMin);
}

#endif