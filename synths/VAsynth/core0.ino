int param_displayed = 0;
int enco_focus=0;
int bpm_rect=10;
boolean midi_learn=false;

int passed2=0;
int target=0;
int time_enco=0;

int count5=0;

void update_env(int numos)
{
  oscA[numos].setenvA(param_midi[16]);
  oscA[numos].setenvD(param_midi[17]);
  oscA[numos].setenvS(param_midi[18]);
  oscA[numos].setenvR(param_midi[19]);
}

void update_env_filter()
{
/*  f3.setenvA(param_midi[20]);
  f3.setenvD(param_midi[21]);
  f3.setenvS(param_midi[22]);
  f3.setenvR(param_midi[23]);*/
}

void load_preset()
{
  Serial.println("load preset");
  String aff = String(savenum);
  aff = "/save" + aff + ".txt";
  Serial.println(aff);
  fs::File file = SPIFFS.open(aff, "r");
  Serial.println(file.name());
  for(int i=1; i<128; i++)
  {
    param_midi[i] = file.read();
  }
  file.close();
  Serial.println("file closed");
  init_synth_param();
}

void save_preset()
{
  String aff = String(savenum);
  aff = "/save" + aff + ".txt";
  Serial.println(aff);
  fs::File file = SPIFFS.open(aff, FILE_WRITE);
  Serial.println(file.name());
  for(int i=1; i<128; i++)
  {
    file.write(param_midi[i]);
  }
  file.close();
  Serial.println("file closed");
}

void Core0Task(void *parameter)
{
  
  Serial.println("core 0");
  Serial.println(digitalRead(0));
  //Midi_Setup();
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nSPIFFS initialised.");

  delay(1000);
  disp.init();
  Serial.println("0 disp");
  
  disp.drawBmp("/logo2.bmp", 0, 0);
  
  Serial.println("0 bmp");
  int tBytes = SPIFFS.totalBytes(); 
  int uBytes = SPIFFS.usedBytes();
  Serial.println("0 SPIFFS space");
  Serial.println(tBytes);
  Serial.println(uBytes);
  while(init_running) delay(1);

  Serial.println("0 suite");
  
  disp.clear();
  display_menu();
  display_param();
  r.begin(true);
  lfo[0][current_synth].start();
  lfo[1][current_synth].start();
  lfo[2][current_synth].start();
  detect_button();
  
  Serial.println("0 detect button");

  while (1)
  {
    
    //Serial.println("0 loop");
    int testbut=detect_button();
    //Serial.println(testbut);

    uint8_t result = r.process();
    //Serial.println(result);
    
    //ecart=millis();

    //update_env(0);

    //Midi_Process();
    Sync_Process();
    //if(arp.play) arp.update();

    /*if(MIDI.read()) 
    {
      switch(MIDI.getType())      // Get the type of the message we caught
        {
            case midi::NoteOn:       // If it is a Note On,
                Midi_NoteOn(MIDI.getData1());  // blink the LED a number of times
                                            // correponding to the program number
                                            // (0 to 127, it can last a while..)
                break;
             case midi::NoteOff:       // If it is a Note On,
                Midi_NoteOff(MIDI.getData1());  // blink the LED a number of times
                                            // correponding to the program number
                                            // (0 to 127, it can last a while..)
                break;
            // See the online reference for other message types
            default:
                break;
        }
    }*/
    
    if(bpm_rect<50) bpm_rect++;
    if(bpm_rect==50) {disp.tempo_on(); Serial.println("end tempo"); bpm_rect++;}
    
   if(result) 
   {
      Serial.println("enco");
      int passed=millis()-passed2;
      //Serial.println(passed);
      if(enco_focus!=-1)
      {
        if (result==16)
        {
          target++;
          if(passed<250) target++;
          if(passed<180) target++;
          if(passed<150) target++;
          if(passed<80) target++;
          if(passed<40) target++;
        }
        if (result==32)
        {
          target--;
          if(passed<250) target--;
          if(passed<180) target--;
          if(passed<150) target--;
          if(passed<80) target--;
          if(passed<40) target--;
        }
        //Serial.println(target);
      }
      else
      {
        if (result==16) {change_enco(1);}
        if (result==32) {change_enco(-1);}
      }
     
      passed2=millis();
    }
    else
    {
      if(target!=0)
     {
       int time_enco2=millis()-time_enco;
       if(time_enco2>=1)
       {
         //Serial.println("increment");
         if(target>0) {change_enco(1); target--;}
         if(target<0) {change_enco(-1); target++;}
         //Serial.println(target);
         time_enco=millis();
       }
     }
    }
    
  
    if(testbut==2) 
    {
      Serial.println("but right");
      if(enco_focus==0)
      {
          param_displayed += 1;
          display_menu();
          display_param();
       }
      if(enco_focus==-1)
      {
        int disppar = disp.menu_right();
          if(disppar<200) 
          {
            enco_focus=0;
            param_displayed=disppar;
            disp.clear();
            display_menu();
            display_param();
          }
          else
          {
            disp.clear();
            display_menu();
            disp.menu_hierarchy();
          }
          
          Serial.println(menu_level);
       
      }
 
    }
  
    if(testbut==0) 
    {
      Serial.println("but left");
      if(enco_focus==-1)
      {
          disp.menu_level--;
          disp.clear();
          display_param();
          disp.menu_top(param_displayed);
          disp.display_top();
          disp.menu_hierarchy();     
      }
      if(enco_focus==0)
      { 
          param_displayed -= 1;
          if(param_displayed<0) param_displayed=0;
          display_menu();
          display_param();
      }
    }
  
    if(testbut==1) 
    {
      Serial.println("but mid");
      if(enco_focus!=0)
      {
        enco_focus=0;
        display_menu();
        display_param();
        if(param_displayed==60 && load_bool) load_preset();
        if(param_displayed==61 && save_bool) save_preset();
      }
      else
      {
        //if(enco_focus==0)
        //{
          param_focus_max=disp.list_bottom(param_displayed, param_focus[param_displayed]);
          enco_focus=2;
        //}
        //else if(enco_focus==2)
        //{
        //  enco_focus=0;
        //  display_menu();
        //}
      }
    }
    
    if(testbut==3) 
    {
      if(enco_focus==0)
      {
        //menu_level=0;
        enco_focus=-1;
        disp.clear();
        display_menu();
        display_param();
        disp.menu_hierarchy();      
      }
      else
      {
        enco_focus=0;
        disp.clear();
        display_menu();
      }
    }
    if(testbut==4) 
    {
      if(!midi_learn)
      {
        display_midi_learn();
        midi_learn=true;
      }
      else
      {
        display_menu();
        display_param();
        midi_learn=false;
      }
    }
    /*if(param_displayed == 1 && (env_dest==1||param_focus[3]==1) && display_wave) {display_param(); display_wave=false;}
    if(param_displayed == 2 && (env_dest==1||param_focus[3]==1) && display_wave) {display_param(); display_wave=false;}
    if(param_displayed == 40 && (param_focus[3]==4) && display_wave) {display_param(); display_wave=false;}
    if(param_displayed == 41 && (param_focus[3]==4) && display_wave) {display_param(); display_wave=false;}*/

    count5++;
    if(count5>200)
    {
      count5=0;
      disp.cpu_usage(rec2);
      
    }
    
    delay(1);
  }
}
