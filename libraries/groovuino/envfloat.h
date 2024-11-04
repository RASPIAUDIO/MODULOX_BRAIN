#ifndef envfloat_h
#define envfloat_h

#include <arduino.h>

class Env
{
  
public:


  float envA;
  float envD;
  float envS;
  float envR;
  
  //float valA;
  float valD;
  //float valS;
  float valR;

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
    /*envA = 0.0001;
    envD = 0.0001;
    envS = 1.0;
    envR = 0.0001;*/
	setA(10);
	setD(10);
	setS(100);
	setA(20);
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
	Serial.println("env start");
    phaseA = true;
    phaseD = false;
    phaseS = false;
    phaseR = false;
	started=true;
	activated=true;
  }
  
  
  void stop()
  {
	Serial.println("env stop");
    phaseA = false;
    phaseD = false;
    phaseS = false;
    phaseR = true;
	activated=false;
  }
  
  void setA(float val)
  {
	  Serial.println("setA");
	  Serial.println(val);
    envA = 0.1/(val*35+1);
	Serial.println(envA);
//	valA=val;
  }
  
  void setD(float val)
  {
    envD = 0.1*(1.0-envS)/(val*35+1);
	valD=val;
  }
  
  void setS(float val)
  {
    envS = val/127.0;
	envD = 0.1*(1.0-envS)/(valD*35+1);
	envR = envS*0.1/(valR*35+1);
//	valS=val;
  }
  
  void setR(float val)
  {
    envR = envS*0.1/(val*35+1);
	valR=val;
  }  
  
  float amount()
  {
// ATTACK
    if(phaseA)
    {
      if(accu >= 1.0)
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