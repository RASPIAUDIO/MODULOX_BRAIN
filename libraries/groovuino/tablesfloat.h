#ifndef tablesfloat_h
#define tablesfloat_h

#define WAVE_SAMPLES 1024 
float nSineTable[WAVE_SAMPLES]; 
float nSawTable[WAVE_SAMPLES]; 
float nSqTable[WAVE_SAMPLES];


void createSineTable() 
{ 
  for(uint32_t nIndex = 0;nIndex  < WAVE_SAMPLES;nIndex++) 
  { 
    nSineTable[nIndex] = sin(((2.0*PI)/WAVE_SAMPLES)*nIndex);
  } 
} 

void createSawTable() 
{ 
  for(uint32_t nIndex = 0;nIndex  < WAVE_SAMPLES;nIndex++) 
  { 
    nSawTable[nIndex] = nIndex*(2.0/WAVE_SAMPLES)-1.0; 
  } 
} 

void createSqTable() 
{ 
  for(uint32_t nIndex = 0; nIndex  < WAVE_SAMPLES; nIndex++) 
  { 
    if(nIndex<(WAVE_SAMPLES/2)) nSqTable[nIndex] = -1.0;
    if(nIndex>=(WAVE_SAMPLES/2)) nSqTable[nIndex] = 1.0; 
  } 
} 

#endif