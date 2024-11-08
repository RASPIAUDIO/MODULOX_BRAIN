#include <arduino.h>

class Env
{
  
public:


  uint32_t envA;
  uint32_t envD;
  uint32_t envS;
  uint32_t envR;

  uint32_t accu;

  boolean phaseA;
  boolean phaseD;
  boolean phaseS;
  boolean phaseR;
  
  boolean started;
  boolean activated;

  void init()
  {
    envA = 10000;
    envD = 30;
    envS = 200000;
    envR = 30;
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
  
  void setA(uint32_t val)
  {
    envA = 100000/(val*100+1);
  }
  
  void setD(uint32_t val)
  {
    envD = (524288-envS)*50/((val+1)*44100);
  }
  
  void setS(uint32_t val)
  {
    envS = val<<12;
  }
  
  void setR(uint32_t val)
  {
    envR = envS*200/((val+1)*44100);
  }  
  
  uint32_t amount()
  {
// ATTACK
    if(phaseA)
    {
      if(accu >= 524288)
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
	
    return (accu>>12);
  }
};
