String dest_list[]={"None","Pitch","Vol","PWM1","PWM2","PWM3","VolOsc1","VolOsc2","VolOsc3","Pitch1","Pitch2","Pitch3", "Cutoff","Reso","UniDetune","UniVol"};
int filt_mode=1;
int filt_pente=30;
int filt_pente2=30;


void but_record()
{

    
}

void but_mid_pressed()
{
  Serial.println("but mid pressed");
  Serial.println(param_displayed);
  
  if(param_displayed==65) 
  {
    disp.draw_warning("loading...");
    disp.display_wave();
    delay(500);
    load_preset();
    load_window(26);
    display_window(65);
  }
  if(param_displayed==66) 
  {
    disp.draw_warning("saving...");
    disp.display_wave();
    delay(500);
    save_preset();
    load_window(26);
    display_window(66);
  }
  if(param_displayed!=65 && param_displayed!=66 && enco_focus==0) learn_midi();
}

void enco_pressed()
{
  
}

void enco_released()
{
  //codec.enableSpeakers();
 
}

float volg=1.0;
float envamount_prev[2][2]={{0,0},{0,0}};
float lfo1amount_prev[3][2]={{0,0},{0,0},{0,0}};


void test_matrix(int desti, float amoun)
{
  if(desti==1) {
    oscA[current_synth].set_pitch_lfo(amoun);
  }
  if(desti==2) 
  {
    volg=amoun;
  }
  if(desti==3) 
  {
    oscA[current_synth].setPWM(0, (int)(1.0+126.0*amoun));
  }
  if(desti==4) 
  {
    oscA[current_synth].setPWM(1, (int)(1+126.0*amoun));
  }
  if(desti==5) 
  {
    oscA[current_synth].setPWM(2, (int)(1+126.0*amoun));
  }
  if(desti==6) 
  {
    oscA[current_synth].setVolOsc(0, (int)(127.0*amoun));
  }
  if(desti==7) 
  {
    oscA[current_synth].setVolOsc(1, (int)(127.0*amoun));
  }
  if(desti==8) 
  {
    oscA[current_synth].setVolOsc(2, (int)(127.0*amoun));
  }
  if(desti==9) 
  {
    oscA[current_synth].set_pitch_lfo(0, amoun);
  }
  if(desti==10) 
  {
    oscA[current_synth].set_pitch_lfo(1, amoun);
  }
  if(desti==11) 
  {
    oscA[current_synth].set_pitch_lfo(2, amoun);
  }
  if(desti==12) 
  {
    Filter[current_synth].SetCutoff( (int)(127.0*amoun),true); 
  }
  if(desti==13) 
  {
    Filter[current_synth].SetResonance( (int)(127.0*amoun),true);
  }
  if(desti==14) 
  {
    oscA[current_synth].compute_detune((int)(127.0*amoun));
  }
  if(desti==15) 
  {
    oscA[current_synth].compute_unisson_vol((int)(127.0*amoun));
  }
}

void change_matrix(int desti)
{
  if(desti==1) {
    volg=1.0;
  }
  if(desti==2) 
  {
    oscA[current_synth].set_pitch_lfo(0.5);
    oscA[current_synth].setPWM(0, param_midi[1]);
  }
  if(desti==3) 
  {
    oscA[current_synth].setPWM(1, param_midi[6]);
    volg=1.0;
  }
  if(desti==4) 
  {
    oscA[current_synth].setPWM(0, param_midi[1]);
    oscA[current_synth].setPWM(2, param_midi[11]);
  }
  if(desti==5) 
  {
    oscA[current_synth].setVolOsc(0, param_midi[4]);
    oscA[current_synth].setPWM(1, param_midi[6]);
  }
  if(desti==6) 
  {
    oscA[current_synth].setPWM(2, param_midi[11]);
    oscA[current_synth].setVolOsc(1, param_midi[9]);
  }
  if(desti==7) 
  {
    oscA[current_synth].setVolOsc(0, param_midi[4]);
    oscA[current_synth].setVolOsc(2, param_midi[14]);
  }
  if(desti==8) 
  {
    oscA[current_synth].setVolOsc(1, param_midi[9]);
    oscA[current_synth].set_pitch_lfo(0, 0.5);
  }
  if(desti==9) 
  {
    oscA[current_synth].setVolOsc(2, param_midi[14]);
    oscA[current_synth].set_pitch_lfo(1, 0.5);
  }
  if(desti==10) 
  {
    oscA[current_synth].set_pitch_lfo(0, 0.5);
    oscA[current_synth].set_pitch_lfo(2, 0.5);
  }
  if(desti==11) 
  {
    oscA[current_synth].set_pitch_lfo(1, 0.5);
    Filter[current_synth].SetCutoff( param_midi[45],true);
  }
  if(desti==12) 
  {
    oscA[current_synth].set_pitch_lfo(2, 0.5);
    Filter[current_synth].SetResonance( param_midi[46],true);
  }
  if(desti==13) 
  {
    Filter[current_synth].SetCutoff( param_midi[45],true);
    oscA[current_synth].compute_detune(param_midi[59]);
  }
  if(desti==14) 
  {
    Filter[current_synth].SetResonance( param_midi[46],true);
    oscA[current_synth].compute_unisson_vol(param_midi[60]);
  }
  if(desti==15) 
  {
    oscA[current_synth].compute_detune(param_midi[59]);
  }
}

void load_window(int screen_n, bool init_s)
{
  Serial.print("load_window : ");
  Serial.println(screen_n);
  if(screen_n>=0 && screen_n<6)
  {
    int ind=screen_n/2;
    int tes=screen_n%2;
    if(tes==0)
    {
      if(init_s) param_displayed=ind*5;
      disp.controller_val[0]=param_midi[ind*5];
      disp.controller_val[1]=param_midi[ind*5+1];
      //disp.draw_waveform_osc(oscA[0].wave[0], oscA[0].fact1[0], oscA[0].fact2[0], oscA[0].fl[0], oscA[0].pwm_conv[0]);
      disp.draw_waveform(oscA[0].wave[ind], 1024);
    }
    if(tes==1)
    {
      if(init_s) param_displayed=ind*5+2;
      disp.controller_val[0]=param_midi[ind*5+2];
      disp.controller_val[1]=param_midi[ind*5+3];
      disp.controller_val[2]=param_midi[ind*5+4];
      disp.init_window();
      String aff = String(param_midi[ind*5+3]*100/127);
      aff = aff+" %";
      disp.load_param(1, aff);

      aff = String((int)param_midi[ind*5+2]-64);
      disp.load_param(0, aff);

      aff = String(param_midi[ind*5+4]*100/127);
      aff = aff+" %";
      disp.load_param(2, aff);
    }
  }
  if(screen_n==6) 
  {
    if(init_s) param_displayed=15;
    disp.controller_val[0]=param_midi[15];
    disp.controller_val[1]=param_midi[16];
    disp.controller_val[2]=param_midi[17];
    disp.controller_val[3]=param_midi[18];
    disp.init_window();
    disp.draw_adsr(param_midi[15], param_midi[16], param_midi[17], param_midi[18], 0);
  }
  if(screen_n==7) 
  {
    if(init_s) param_displayed=19;
    disp.controller_val[0]=param_midi[19];
    disp.init_window();
    disp.draw_adsr(param_midi[20], param_midi[21], param_midi[22], param_midi[23], 0);
  }
  if(screen_n==8) 
  {
    if(init_s) param_displayed=20;
    disp.controller_val[0]=param_midi[20];
    disp.controller_val[1]=param_midi[21];
    disp.controller_val[2]=param_midi[22];
    disp.controller_val[3]=param_midi[23];
    disp.init_window();
    disp.draw_adsr(param_midi[20], param_midi[21], param_midi[22], param_midi[23], 0);
  }
  if(screen_n==9) 
  {
    if(init_s) param_displayed=24;
    disp.controller_val[0]=param_midi[24];
    disp.init_window();
    disp.draw_adsr(param_midi[25], param_midi[26], param_midi[27], param_midi[28], 0);
  }
  if(screen_n==10) 
  {
    if(init_s) param_displayed=25;
    disp.controller_val[0]=param_midi[25];
    disp.controller_val[1]=param_midi[26];
    disp.controller_val[2]=param_midi[27];
    disp.controller_val[3]=param_midi[28];
    disp.init_window();
    disp.draw_adsr(param_midi[25], param_midi[26], param_midi[27], param_midi[28], 0);
  }
  if(screen_n==11)
  {
    if(init_s) param_displayed=29;
    disp.controller_val[0]=param_midi[29];
    disp.controller_val[1]=param_midi[30];
    disp.init_window();
    disp.draw_waveform_lfo(lfo[0][current_synth].wave, lfo[0][current_synth].lfovol, lfo[0][current_synth].maincutoff);
  }
  if(screen_n==12) 
  {
    if(init_s) param_displayed=31;
    disp.controller_val[0]=param_midi[31];
    disp.controller_val[1]=param_midi[32];
    disp.controller_val[2]=param_midi[33];
    disp.init_window();
    disp.draw_waveform_lfo(lfo[0][current_synth].wave, lfo[0][current_synth].lfovol, lfo[0][current_synth].maincutoff);
  }
  if(screen_n==13)
  {
    if(init_s) param_displayed=34;
    disp.controller_val[0]=param_midi[34];
    disp.controller_val[1]=param_midi[35];
    disp.init_window();
    disp.draw_waveform_lfo(lfo[1][current_synth].wave, lfo[1][current_synth].lfovol, lfo[1][current_synth].maincutoff);
  }
  if(screen_n==14) 
  {
    if(init_s) param_displayed=36;
    disp.controller_val[0]=param_midi[36];
    disp.controller_val[1]=param_midi[37];
    disp.controller_val[2]=param_midi[38];
    disp.init_window();
    disp.draw_waveform_lfo(lfo[1][current_synth].wave, lfo[1][current_synth].lfovol, lfo[1][current_synth].maincutoff);
  }
  if(screen_n==15)
  {
    if(init_s) param_displayed=39;
    disp.controller_val[0]=param_midi[39];
    disp.controller_val[1]=param_midi[40];
    disp.init_window();
    disp.draw_waveform_lfo(lfo[2][current_synth].wave, lfo[2][current_synth].lfovol, lfo[2][current_synth].maincutoff);
  }
  if(screen_n==16) 
  {
    if(init_s) param_displayed=41;
    disp.controller_val[0]=param_midi[41];
    disp.controller_val[1]=param_midi[42];
    disp.controller_val[2]=param_midi[43];
    disp.init_window();
   disp.draw_waveform_lfo(lfo[2][current_synth].wave, lfo[2][current_synth].lfovol, lfo[2][current_synth].maincutoff);
  }
  if(screen_n==17)
  {
    if(init_s) param_displayed=44;
    disp.controller_val[0]=param_midi[44];
    disp.controller_val[1]=param_midi[45];
    disp.controller_val[2]=param_midi[46];
    disp.init_window();
    disp.draw_filter(Filter[current_synth].GetCutoff(), param_midi[46], filt_mode, filt_pente, filt_pente2);
  }
  if(screen_n==18)
  {
    if(init_s) param_displayed=47;
    disp.controller_val[0]=param_midi[47];
    disp.controller_val[1]=param_midi[48];
    disp.controller_val[2]=param_midi[49];

    disp.init_window();
    String aff = String(param_midi[47]*1000/64*param_midi[8]/127);
    aff = aff+" ms";
    disp.load_param(0, aff);

    aff = String(param_midi[48]*100/127);
    aff = aff+" %";
    disp.load_param(1, aff);

    aff = String(param_midi[49]*100/127);
    aff = aff+" %";
    disp.load_param(2, aff);
  }
  if(screen_n==19)
  {
    if(init_s) param_displayed=50;
    disp.controller_val[0]=param_midi[50];
    disp.controller_val[1]=param_midi[51];

    disp.init_window();

    String aff = String(param_midi[51]*100/127);
    aff = aff+" %";
    disp.load_param(1, aff);
    
  }
  if(screen_n==20)
  {
    if(init_s) param_displayed=52;
    disp.controller_val[0]=param_midi[52];
    disp.controller_val[1]=param_midi[53];
    disp.controller_val[2]=param_midi[54];
    
    String aff = String(param_midi[54]+50);
    disp.load_param(2, aff);

    disp.init_window();
  }
  if(screen_n==21)
  {
    if(init_s) param_displayed=55;
    disp.controller_val[0]=param_midi[55];
    disp.controller_val[1]=param_midi[56];
    disp.controller_val[2]=param_midi[57];

    disp.init_window();

    String aff = String(param_midi[55]%16);
    disp.load_param(0, aff);

    aff = String(param_midi[56]*100/127);
    aff = aff+" %";
    disp.load_param(1, aff);

    aff = String(param_midi[57]);
    disp.load_param(2, aff);
  }
  if(screen_n==22)
  {
    if(init_s) param_displayed=58;
    disp.controller_val[0]=param_midi[58];
    disp.controller_val[1]=param_midi[59];
    disp.controller_val[2]=param_midi[60];

    disp.init_window();

    String aff = String(param_midi[59]*100/127);
    aff = aff+" %";
    disp.load_param(1, aff);

    aff = String(param_midi[60]*100/127);
    aff = aff+" %";
    disp.load_param(2, aff);
  }
  if(screen_n==23)
  {
    if(init_s) param_displayed=61;
    disp.controller_val[0]=param_midi[61];
    disp.controller_val[1]=param_midi[62];
    disp.init_window();
  }
  if(screen_n==24)
  {
    if(init_s) param_displayed=63;
    disp.controller_val[0]=param_midi[63];
    disp.controller_val[1]=param_midi[64];
    disp.init_window();
  }
  if(screen_n==25)
  {
    if(init_s) param_displayed=65;
    disp.controller_val[0]=param_midi[65];
    disp.controller_val[1]=param_midi[66];
    disp.init_window();
  }
  if(screen_n==26)
  {
    if(init_s) param_displayed=67;
    disp.controller_val[0]=param_midi[67];
    disp.init_window();
  }
}

void param_action(int num, bool stop_engines)
{
  Serial.print("param_action : ");
  Serial.println(num);
  if(num>=0 && num<15)
  {
    int ind=num/5;
    int tes=num%5;
    if(tes == 0) oscA[0].setWaveform(ind, param_midi[num]);
    if(tes == 1) oscA[0].setPWM(ind, param_midi[num]*100/127);
    if(tes == 2) oscA[0].setFine(ind, ((int)param_midi[num]-64)*256 + param_midi[num+1]);
    if(tes == 3) oscA[0].setFine(ind, ((int)param_midi[num-1]-64)*256 + param_midi[num]);
    if(tes == 4) oscA[0].setVolOsc(ind, param_midi[num]);
  }
  if(num==15) oscA[0].setenvA(param_midi[num]);
  if(num==16) oscA[0].setenvD(param_midi[num]);
  if(num==17) oscA[0].setenvS(param_midi[num]);
  if(num==18) oscA[0].setenvR(param_midi[num]);
  if(num==19) {env[1][current_synth].dest=param_midi[num]; change_matrix(param_midi[num]);}
  if(num==20) env[1][current_synth].setA(param_midi[num]);
  if(num==21) env[1][current_synth].setD(param_midi[num]);
  if(num==22) env[1][current_synth].setS(param_midi[num]);
  if(num==23) env[1][current_synth].setR(param_midi[num]);
  if(num==24) {env[2][current_synth].dest=param_midi[num]; change_matrix(param_midi[num]);}
  if(num==25) env[2][current_synth].setA(param_midi[num]);
  if(num==26) env[2][current_synth].setD(param_midi[num]);
  if(num==27) env[2][current_synth].setS(param_midi[num]);
  if(num==28) env[2][current_synth].setR(param_midi[num]);
  if(num>=29 && num<=43)
  {
    int ind=(num-29)/5;
    int tes=(num-29)%5;
    if(tes == 0) {lfo[ind][current_synth].dest=param_midi[num]; change_matrix(param_midi[num]);}
    if(tes == 1) lfo[ind][current_synth].setWaveform(param_midi[num]%3);
    if(tes == 2) lfo[ind][current_synth].setfreq((float)param_midi[num]*(float)param_midi[num]/127);
    if(tes == 3) lfo[ind][current_synth].setvol(param_midi[num]);
    if(tes == 4) lfo[ind][current_synth].setmaincutoff(param_midi[num]);

  }
  if(num==44) {
    Filter[current_synth].SetMode(param_midi[num]);
    if(param_midi[num]==0) {filt_mode=0;}
    if(param_midi[num]==1) {filt_mode=1;  filt_pente=40;}
    if(param_midi[num]==2) {filt_mode=1;  filt_pente=30;}
    if(param_midi[num]==3) {filt_mode=1;  filt_pente=20;}
    if(param_midi[num]==4) {filt_mode=1;  filt_pente=10;}
    if(param_midi[num]==5) {filt_mode=2;  filt_pente=40;}
    if(param_midi[num]==6) {filt_mode=2;  filt_pente=30;}
    if(param_midi[num]==7) {filt_mode=2;  filt_pente=20;}
    if(param_midi[num]==8) {filt_mode=2;  filt_pente=10;}
    if(param_midi[num]==9) {filt_mode=3;  filt_pente=20; filt_pente2=20;}
    if(param_midi[num]==10) {filt_mode=3;  filt_pente=30; filt_pente2=10;}
    if(param_midi[num]==11) {filt_mode=3;  filt_pente=10; filt_pente2=30;}
    if(param_midi[num]==12) {filt_mode=3;  filt_pente=20; filt_pente2=10;}
    if(param_midi[num]==13) {filt_mode=3;  filt_pente=10; filt_pente2=20;}
    if(param_midi[num]==14) {filt_mode=3;  filt_pente=10; filt_pente2=10;}
  }
  if(num==45) Filter[current_synth].SetCutoff((float)param_midi[num],true);
  if(num==46) Filter[current_synth].SetResonance((float)param_midi[num],true);
  if(num==47) set_time(param_midi[num]);
  if(num==48) set_feedback(param_midi[num]);
  if(num==49) delay_mix=(float)param_midi[num]/127.0;
  if(num==50) oscA[0].update_polyphony((param_midi[num]%8)+1);
  if(num==51) oscA[0].setGlideTime(param_midi[num]);
  if(num==52) {
    arpon=param_midi[num]%2; 
    if(!arpon) arp.stop(&oscA[0]);
    else arp.start();
  }
  if(num==53) {
    arp.stop(&oscA[0]);
    arp.update_mode(param_midi[num]); 
    arp.start();
  }
  if(num==54) {
    //arp.stop(&oscA[0]);
    arp.update_bpm(param_midi[num]);
    //arp.start();
  }
  if(num==55) {
    arp.stop(&oscA[0]);
    arp.update_rate(param_midi[num]%16);
    arp.start();
  }
  if(num==56) {
    //arp.stop(&oscA[0]);
    arp.update_gate(param_midi[num]);
    //arp.start();
  }
  if(num==57) {
    arp.stop(&oscA[0]);
    arp.update_step(param_midi[num]%5);
    arp.start();
  }
  if(num==58) oscA[0].unisson = (param_midi[num]%8)+1;
  if(num==59) oscA[0].compute_detune(param_midi[num]);
  if(num==60) oscA[0].compute_unisson_vol(param_midi[num]);
  if(num==61) disto_on[current_synth]=param_midi[num]%2;
  if(num==62) disto[current_synth].set_gain(param_midi[num]);
  if(num==63) tempo_source=param_midi[num]%3;
  if(num==64) changlob=(param_midi[num]%16);
  //if(num==63) {if(param_midi[num]%2==0) ES8960_Init2();
  //              if(param_midi[num]%2==1) hp_spk();}
  /*if(num==65) multi_mode=param_midi[num]%2;
  if(num==66) {savenum=param_midi[num]%8;
                load_preset();
                current_synth=0;
                init_synth_param();}
  if(num==67) {savenum=param_midi[num]%8;
                load_preset();
                current_synth=0;
                init_synth_param();}
  if(num==68) chansynth1=(param_midi[num]%8)+1;
  if(num==69) chansynth2=(param_midi[num]%8)+1;*/
  if(num==65) savenum=param_midi[num]%16;
  if(num==66) savenum=param_midi[num]%16;
  if(num==67) {volglobal=(float)param_midi[num]*80.0/127.0-74.0; codec.setHeadphoneVolumeDB(volglobal);}
}

void param_action_focus(int num)
{
  
}

void display_window(int num)
{
  Serial.print("display window : ");
  Serial.println(num);
  disp.display_val_controller(num, param_midi[num]);
  if(num>=0 && num<15)
  {
    int ind=num/5;
    int tes=num%5;
    if(tes == 0 || tes==1) {
      disp.draw_waveform(&oscA[0].wave[ind][0], 1024);
      disp.display_wave();
    }
  }
  if(param_displayed >= 15 && param_displayed <= 18) 
  {
    disp.draw_adsr(param_midi[15], param_midi[16], param_midi[17], param_midi[18], param_displayed-15);
    disp.display_wave();
  }
  if(param_displayed >= 20 && param_displayed <= 23) 
  {
    disp.draw_adsr(param_midi[20], param_midi[21], param_midi[22], param_midi[23], param_displayed-20);
    disp.display_wave();
  }
  if(param_displayed >= 25 && param_displayed <= 28) 
  {
    disp.draw_adsr(param_midi[25], param_midi[26], param_midi[27], param_midi[28], param_displayed-25);
    disp.display_wave();
  }
  if(num>=29 && num<=43)
  {
    int ind=(num-29)/5;
    int tes=(num-29)%5;
    disp.draw_waveform_lfo(lfo[ind][current_synth].wave, lfo[ind][current_synth].lfovol, lfo[ind][current_synth].maincutoff);
    disp.display_wave();

  }
  if(param_displayed == 44) 
  {
    disp.draw_filter(Filter[current_synth].GetCutoff(), param_midi[param_displayed+2], filt_mode, filt_pente, filt_pente2); 
    disp.display_wave();
  }
  if(param_displayed == 45) 
  {
    disp.draw_filter(Filter[current_synth].GetCutoff(), param_midi[param_displayed+1], filt_mode, filt_pente, filt_pente2); 
    disp.display_wave();
  }
  if(param_displayed == 46) 
  {
    disp.draw_filter(Filter[current_synth].GetCutoff(), param_midi[param_displayed], filt_mode, filt_pente, filt_pente2);
    disp.display_wave();
  }
  if(num == 47) {
    String aff = String(param_midi[param_displayed]*1000/64*param_midi[param_displayed]/127);
		aff = aff+" ms";
    disp.draw_param(0, aff);
  }
  /*if(num == 48) {
    
    disp.display_val_controller(num, param_midi[num]);
  }
  if(num == 49) {
    
    disp.display_val_controller(num, param_midi[num]);
  }
  if(num == 51) {
    String aff = String(param_midi[param_displayed]*100/127);
		aff = aff+" %";
    disp.draw_param(1, aff);
  }
  if(num == 54) {
    String aff = String(param_midi[param_displayed]+50);
    disp.draw_param(2, aff);
  }
  if(num == 55) {
    String aff = String(1+param_midi[param_displayed]%16);
    disp.draw_param(0, aff);
  }
  if(num == 56) {
    String aff = String(param_midi[param_displayed]*100/127);
		aff = aff+" %";
    disp.draw_param(1, aff);
  }
  if(num == 57) {
    String aff = String((param_midi[param_displayed]%5)-2);
    disp.draw_param(2, aff);
  }
  if(num == 59) {
    String aff = String(param_midi[param_displayed]*100/127);
		aff = aff+" %";
    disp.draw_param(1, aff);
  }
  if(num == 60) {
    String aff = String(param_midi[param_displayed]*100/127);
		aff = aff+" %";
    disp.draw_param(2, aff);
  }
  if(num == 62) {
    String aff = String(param_midi[param_displayed]*100/127);
		aff = aff+" %";
    disp.draw_param(1, aff);
  }*/
}


inline void Midi_NoteOn(uint8_t ch, uint8_t note, uint8_t vol)
{
  Serial.println("Midi_NoteOn");
  Serial.println(current_synth);
  Serial.println(note);
  Serial.println(vol);
  if(param_midi[64]==ch || param_midi[64]==0) 
  {
    if(arpon)
    {
      if(vol>0) arp.add_note(note);
      else arp.delete_note(note, &oscA[0]);
    }
    else 
    {
      if(oscA[current_synth].setNote(note, vol)) { 
        //f3.start(); 
        if(env[0][current_synth].dest>0) env[0][current_synth].start(); 
        if(env[1][current_synth].dest>0) env[1][current_synth].start();
        lfo[0][current_synth].start();
        lfo[1][current_synth].start();
        lfo[2][current_synth].start();
        Serial.print("dest : ");
        Serial.println(lfo[0][current_synth].dest);
      }
    }
  }
}

inline void Midi_NoteOff(uint8_t ch, uint8_t note)
{
  Serial.println("Midi_NoteOff");
  Serial.println(current_synth);
  Serial.println(note);
  if(param_midi[64]==ch || param_midi[64]==0) 
  {
    if(arpon)
    {
      arp.delete_note(note, &oscA[0]);
    }
    else
    {
      oscA[current_synth].setNote(note, 0);
      if(env[0][current_synth].dest>0) env[0][current_synth].stop(); 
      if(env[1][current_synth].dest>0) env[1][current_synth].stop();
    }
  }
  
}

uint32_t previous_time = micros();

inline void Midi_ControlChange(uint8_t ch, uint8_t note, uint8_t val)
{
  Serial.println("Midi_CC");
  if(midi_learn)
    {
      midi_cc_val[param_displayed]=note;
      num_from_MIDI=note;
    }
    else
    {
      for(int i=0; i<128; i++)
      {
        if(note==midi_cc_val[i])  
        {    
          change_CC(i, val);
          param_action(i);
          num_from_MIDI=i;
          val_from_MIDI=val;
          
        }
      }
    }
}

void Synth_Init()
{
  for(int i=0; i<MAX_SYNTH; i++)
  {
    oscA[i].init();
    oscA[i].glideon = true;
    disto[i].init();
    disto_on[i]=false;
    Filter[i].Init((float)SAMPLE_RATE);
    Filter[i].SetMode(TeeBeeFilter::LP_24);
    Filter[i].SetResonance( 5,true);
    filter_on[i]=true;
    lfo[0][i].init();
    //lfo[0][i].setfreq(param_lfo_freq[0]);
    lfo[1][i].init();
    //lfo[1][i].setfreq(param_lfo_freq[1]);
    lfo[2][i].init();
    //lfo[2][i].setfreq(param_lfo_freq[2]);
    delay_init();
    env[0][i].init();
    env[1][i].init();
    highpass1.setMode(OnePoleFilter::HIGHPASS);
    highpass1.setCutoff(24.167f);
  }

}




inline void Synth_Process(int16_t *left, int16_t *right)
{
  float out_l, out_r;
    out_l = 0;
    out_r = 0;
    float glbvol=1.0;

    

    if(multi_mode) // multi mode
    {
      float outvoice[2];
      outvoice[0]=0;
      outvoice[1]=0;
      for(int cur=0; cur<2; cur++)
      {
        for(int i=0; i<2; i++)
        {
          if(env[i][cur].dest>0)
          {
            float envamount=env[i][cur].amount();
            if(envamount<envamount_prev[i][cur]-0.01 || envamount>envamount_prev[i][cur]+0.01 )
            {
              test_matrix(env[i][cur].dest, envamount);
              display_wave=true;
              envamount_prev[i][cur]=envamount;
            }
          }
        }
    
        for(int i=0; i<3; i++)
        {
          if(lfo[i][cur].dest>0)
          {
            float lfo1amount=lfo[i][cur].output();
            if(lfo1amount<0) lfo1amount=0;
            if(lfo1amount>=1.0) lfo1amount=1.0;
            if(lfo1amount>lfo1amount_prev[i][cur]+0.01 || lfo1amount<lfo1amount_prev[i][cur]-0.01)
            {
              test_matrix(lfo[i][current_synth].dest, lfo1amount);
              display_wave=true;
              lfo1amount_prev[i][cur]=lfo1amount;
              //rec1=micros()-save_rec1;
            }
          }
        }
        
        /* counter required to optimize processing */
        //count += 1;
    
        oscA[cur].next();
      
        out_l = oscA[cur].output();
    
        if(disto_on[cur]) out_l = disto[cur].out(out_l);
        else out_l=fast_tanh(out_l);
    
        if(filter_on[cur]) out_l = Filter[cur].Process(out_l); 
        
        //if(delay_on) out_l+=delay_output(out_l)*(float)param_midi[49]/127.0;
        out_l+=delay_output(out_l)*(float)param_midi[49]/127.0;

        outvoice[cur]=out_l;
      }
      out_l=outvoice[0]+outvoice[1];
    }
    else
    {
      for(int i=0; i<2; i++)
      {
        if(env[i][current_synth].dest>0)
        {
          float envamount=env[i][current_synth].amount();
          if(envamount<envamount_prev[i][current_synth]-0.01 || envamount>envamount_prev[i][current_synth]+0.01 )
          {
            test_matrix(env[i][current_synth].dest, envamount);            
            display_wave=true;
            envamount_prev[i][current_synth]=envamount;
          }
        }
      }
  
      for(int i=0; i<3; i++)
      {
        if(lfo[i][current_synth].dest>0)
        {
          float lfo1amount=lfo[i][current_synth].output();
          if(lfo1amount<0) lfo1amount=0;
          if(lfo1amount>=1.0) lfo1amount=1.0;
          if(lfo1amount>lfo1amount_prev[i][current_synth]+0.01 || lfo1amount<lfo1amount_prev[i][current_synth]-0.01)
          {
            test_matrix(lfo[i][current_synth].dest, lfo1amount);
            display_wave=true;
            lfo1amount_prev[i][current_synth]=lfo1amount;
            //rec1=micros()-save_rec1;
          }
        }
      }
      
      
      
  
      /* counter required to optimize processing */
      //count += 1;
  
      oscA[current_synth].next();
   
      out_l = oscA[current_synth].output();

      
  
      if(disto_on[current_synth]) out_l = disto[current_synth].out(out_l);
  
      if(filter_on[current_synth]) out_l = Filter[current_synth].Process(out_l); 

      //if(delay_on) out_l+=delay_output(out_l)*delay_mix;
      //out_l+=delay_output(out_l)*delay_mix;
      

      out_l*=volg;

      //out_l = highpass1.getSample(out_l);
    }
  
    
    out_l=fast_tanh(out_l);
    if(out_l>=0.95) Serial.println(out_l);
    
    int16_t finalout=16383.0*out_l;
    finalout=finalout*(1.0-delay_mix) + delay_output(finalout)*delay_mix;
    //out_r=out_l;
 
    *left = finalout;
    *right = finalout;

}

