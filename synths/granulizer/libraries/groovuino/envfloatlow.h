#ifndef envfloatlow_h
#define envfloatlow_h

#include <arduino.h>

class EnvLow
{
  
public:


  float envA;
  float envD;
  float envS;
  float envR;

  float accu;

  boolean phaseA;
  boolean phaseD;
  boolean phaseS;
  boolean phaseR;
  
  boolean started;
  boolean activated;
  
  int dest;

  void init()
  {
	dest=0;
    envA = 0.000025;
    envD = 0.000025;
    envS = 0.25;
    envR = 0.000025;
    accu = 0;
    phaseA = false;
    phaseD = false;
    phaseS = false;
    phaseR = false;
	started=false;
	activated=false;
  }
  
  void start()
  {
	//Serial.println("env start");
    phaseA = true;
    phaseD = false;
    phaseS = false;
    phaseR = false;
	started=true;
	activated=true;
  }
  
  
  void stop()
  {
	//Serial.println("env stop");
    phaseA = false;
    phaseD = false;
    phaseS = false;
    phaseR = true;
	activated=false;
  }
  
  void setA(float val)
  {
    envA = 0.025/(val*35+1);
  }
  
  void setD(float val)
  {
    envD = 0.1*(0.25-envS)/(val*35+1);
  }
  
  void setS(float val)
  {
    envS = val/512.0;
	envD = 0.1*(0.25-envS)/(val*35+1);
	envR = envS*0.1/(val*35+1);
  }
  
  void setR(float val)
  {
    envR = envS*0.1/(val*35+1);
  }  
  
  float amount()
  {
// ATTACK
    if(phaseA)
    {
      if(accu >= 0.25)
      {
        phaseA = false;
        phaseD = true;
      }
      else
      {
        accu += envA;
      }
    }

// SUSTAIN
// We do nothing
  
// DECAY
    if(phaseD)
    {

      accu = accu - envD;
    
      if(accu<=envS)
      {
        phaseD = false;
        phaseS = true;
      }
    
    }   
 
  
// RELEASE
    if(phaseR)
    {
      if(accu>=envR)
      {
        accu -= envR;
      }
      else 
      {
        phaseR = false;
        accu = 0;
		started=false;
      }
    }
	
    return (accu);
  }
};
#endif