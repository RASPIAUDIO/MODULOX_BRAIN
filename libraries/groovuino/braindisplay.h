#ifndef braindisplay_h
#define braindisplay_h

#define BIGCHAR 4
#define SMALLCHAR 2

#define SCREEN_WIDTH 280
#define SCREEN_HEIGHT 240

#include <TFT_eSPI.h>
#include "FS.h"
#include "SPIFFS.h"

int peak_filt[] ={0,1,1,1,1,2,2,2,2,3,3,3,4,4,5,5,6,7,8,10,12,14,17,20,24,27,29,31,32,32,33,33,34,34,34,35,35,35,35,34,34,34,33,33,33,32,32,31,31,30,30,29,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};

class Bdisplay
{
  
public:
	
	
	
	uint8_t menuminmax[16][2];
	uint8_t paramdrawnum[128];
	String list_text[10];
	String list_previous;
	uint8_t list_size=0;
	uint8_t line_selected[4];
	uint8_t line_selected_max[4];
	uint8_t menu_level;
	
	TFT_eSPI tft = TFT_eSPI();
	TFT_eSprite spr = TFT_eSprite(&tft);
	TFT_eSprite sprt = TFT_eSprite(&tft);
	
	int cur[20];
	
	Bdisplay() {
		spr.setColorDepth(8);      // Create an 8bpp Sprite of 60x30 pixels
        spr.createSprite(280, 160);  // 8bpp requires 64 * 30 = 1920 bytes
        //spr.setPivot(32, 55);      // Set pivot relative to top left corner of Sprite
        spr.fillSprite(TFT_BLACK); // Fill the Sprite with black
		
		sprt.setColorDepth(8);      // Create an 8bpp Sprite of 60x30 pixels
        sprt.createSprite(280, 40);  // 8bpp requires 64 * 30 = 1920 bytes
        //spr.setPivot(32, 55);      // Set pivot relative to top left corner of Sprite
        sprt.fillSprite(TFT_BLACK); // Fill the Sprite with black
	}
	
	void init()
	{
		tft.init();
		tft.setRotation(3);
		for(int i=0; i<10; i++)
		  {
			list_text[i]="";
		  }
		menu_level=0;
		compute_filter_curve(10);
	}
	
	void display_top()
	{
		sprt.pushSprite(0, 0);
	}
	
	void display_bottom()
	{
		sprt.pushSprite(0, 200);
	}
	
	void display_window()
	{
		spr.pushSprite(0, 40);
	}
	
	void clear()
	{
		spr.fillSprite(TFT_BLACK);
		sprt.fillSprite(TFT_BLACK);
	}
	
	void menu_top(int titlenum, int bcol=0x5ACB)
	{
		sprt.setTextColor(TFT_WHITE);
		sprt.fillRect(0, 0, SCREEN_WIDTH, 40,  bcol);
		sprt.drawRect(0, 0, SCREEN_WIDTH, 40, 0x3186);
		fs::File file = SPIFFS.open("/menutitle.txt", "r");
		for(int i=0; i<2*titlenum; i++) 
		{
			//file.findUntil("","\r\n");
			String titleSt = file.readStringUntil('\r\n');
		}
		String titleSt = file.readStringUntil('\r\n');
		Serial.println(titleSt);
		file.close();
		draw_title(titleSt);
	}
	
	void menu_bottom(int selnum, int menuselect, int bcol=0x5ACB)
	{
		Serial.println("menu_bottom");
		String titleSt;
		
		sprt.fillRect(0, 0, SCREEN_WIDTH, 40,  bcol);
		sprt.drawFastVLine(SCREEN_WIDTH/3, 0, 40, 0x3186);
		sprt.drawFastVLine(SCREEN_WIDTH*2/3, 0, 40, 0x3186);
		sprt.drawString("<", SCREEN_WIDTH/6, 10, 2);
		sprt.drawString(">", SCREEN_WIDTH*5/6, 10, 2);
		
		fs::File file = SPIFFS.open("/menutitle.txt", "r");
		Serial.println("file opened");
		for(int i=0; i<2*selnum; i++) 
		{
			titleSt = file.readStringUntil('\r\n');
		}
		titleSt = file.readStringUntil('\r\n');
		Serial.println(titleSt);
		String menu=file.readStringUntil('\r\n');
		Serial.println(menu);

		char Buf[20];
        menu.toCharArray(Buf, 20);

		if(Buf[0]=='n' && Buf[1]=='o')
		{
			Serial.println("no select");
		}
		else
		{
			file = SPIFFS.open("/menulist.txt", "r");
			Serial.println("file opened");
			char Buf2[20];
			bool test=true;
			while(test)
			{
				titleSt = file.readStringUntil('\r\n');
				Serial.println(titleSt);
				titleSt.toCharArray(Buf2, 20);
				if(Buf2[0]=='/')
				{
					if(Buf[0]==Buf2[1] && Buf[1]==Buf2[2]) test=false;
				}
			}
			Serial.println(menuselect);
			for(int i=0; i<menuselect; i++) 
			{
				titleSt = file.readStringUntil('\r\n');
			}
			titleSt = file.readStringUntil('\r\n');
			Serial.println(titleSt);
			draw_bottom_menu(titleSt);
		}
		file.close();
	}
	
	int list_bottom(int selnum, int listselnum, int bcol=0x5ACB)
	{
		int ret=0;
		Serial.println("list_bottom");
		String titleSt;
		
		fs::File file = SPIFFS.open("/menutitle.txt", "r");
		Serial.println("file opened");
		for(int i=0; i<2*selnum; i++) 
		{
			titleSt = file.readStringUntil('\r\n');
		}
		titleSt = file.readStringUntil('\r\n');
		String menu=file.readStringUntil('\r\n');

		char Buf[20];
        menu.toCharArray(Buf, 20);

		if(Buf[0]=='n' && Buf[1]=='o')
		{
			Serial.println("no select");
		}
		else
		{
			file = SPIFFS.open("/menulist.txt", "r");
			Serial.println("file opened");
			char Buf2[20];
			bool test=true;
			while(test)
			{
				titleSt = file.readStringUntil('\r\n');
				Serial.println(titleSt);
				titleSt.toCharArray(Buf2, 20);
				if(Buf2[0]=='/')
				{
					if(Buf[0]==Buf2[1] && Buf[1]==Buf2[2]) test=false;
				}
			}
			int i=0;
			while(Buf[0]!='/')
			{
				Serial.println(i);
				list_text[i]=file.readStringUntil('\r\n');
				Serial.println(list_text[i]);
				list_text[i].toCharArray(Buf, 20);
				i++;
			}
			list_size=i-1;
			ret=list_size-1;
			display_list(listselnum,90,100,100);
		}
		file.close();
		return ret;
	}
	
	void change_menu_select(int _sens)
	{
		line_selected[menu_level]+=_sens;
		line_selected[menu_level+1]=0;
		if(line_selected[menu_level]>line_selected_max[menu_level]) line_selected[menu_level]=line_selected_max[menu_level];
		if(line_selected[menu_level]<0) line_selected[menu_level]=0;
		if(menu_level==0) display_list(line_selected[0],0,10,80);
		if(menu_level==1) display_list(line_selected[1],80,10,65);
		if(menu_level==2) display_list(line_selected[2],145,10,65);
		if(menu_level==3) display_list(line_selected[3],210,10,75);
	}
	
	int search_menutitle(String stringsearch)
	{
		int ret=200;
		Serial.println("search_menutitle");
		Serial.println(stringsearch);
		fs::File file = SPIFFS.open("/menutitle.txt", "r");
		char Buf2[20];
		for(int j=0; j<20; j++) Buf2[j]=' ';
		stringsearch.toCharArray(Buf2, 20);	
		Serial.println(Buf2[0]);
		Serial.println(Buf2[1]);
		Serial.println(Buf2[2]);
		Serial.println(Buf2[3]);
		Serial.println(Buf2[4]);
		Serial.println(Buf2[5]);
		Serial.println(Buf2[6]);
		Serial.println(Buf2[7]);
		Serial.println(Buf2[8]);
		Serial.println(Buf2[9]);
		Serial.println(Buf2[10]);
		String titleSt;
		int i=0;
		char Buf[20];
		while(Buf[0]!='#' && ret==200)
		{
			titleSt=file.readStringUntil('\r\n');
			int lastIndex = titleSt.length() - 1;
            titleSt.remove(lastIndex);
			Serial.println(titleSt);
			for(int j=0; j<20; j++) Buf[j]=' ';
			titleSt.toCharArray(Buf, 20);
			ret=i/2;
			for(int j=0; j<titleSt.length(); j++)
			{
				if(Buf[j]!=Buf2[j]) ret=200;
			}
			i++;
		}
		file.close();
		return ret;
	}
	
	int menu_right()
	{
		int ret=200;
		char Buf[20];
		list_text[line_selected[menu_level]].toCharArray(Buf, 20);
		if(Buf[0]=='@')	
		{
			String search = list_previous+" "+list_text[line_selected[menu_level]].substring(1);
			//String search = list_previous+"aaa";
			Serial.println(search);
			ret=search_menutitle(search);
		}
		else
		{
			list_previous = list_text[line_selected[menu_level]];
			menu_level++;
			line_selected[menu_level]=0;
			menu_hierarchy();
		}
		return ret;
	}
	
	void menu_hierarchy()
	{
		fs::File file = SPIFFS.open("/menuorg.txt", "r");
		
		Serial.println("hierarchy");
		Serial.println(menu_level);
		
		int niv=0;
		int i=0;
		char Buf[20];
		String titleSt;
		
		if(menu_level>=0)
		{
			while(Buf[0]!='#')
			{
				list_text[i]=file.readStringUntil('\r\n');
				//mystring[length-1] = '\0';
				int lastIndex = list_text[i].length() - 1;
                list_text[i].remove(lastIndex);
				list_text[i].toCharArray(Buf, 20);
				list_text[i]=list_text[i].substring(1);
				if(Buf[0]=='-')	i++;
			}
			line_selected_max[0]=i-1;
			list_size=i;
			display_list(line_selected[0],0,10,80);
			
		}
		file.close();
		file = SPIFFS.open("/menuorg.txt", "r");
		if(menu_level>=1)
		{
			i=0;
			while(i<(line_selected[0]+1))
			{
				titleSt=file.readStringUntil('\r\n');
				titleSt.toCharArray(Buf, 20);
				if(Buf[0]=='-')	i++;
			}
			i=0;
			Buf[0]='a';
			while(Buf[0]!='-' && Buf[0]!='#')
			{
				Serial.println(i);
				list_text[i]=file.readStringUntil('\r\n');
				Serial.println(list_text[i]);
				int lastIndex = list_text[i].length() - 1;
                list_text[i].remove(lastIndex);
				list_text[i].toCharArray(Buf, 20);
				list_text[i]=list_text[i].substring(2);
				if(Buf[0]=='/')	i++;
			}
			Serial.println(i);
			line_selected_max[1]=i-1;
			list_size=i;
			display_list(line_selected[1],80,10,65);		
		}
		file.close();
		file = SPIFFS.open("/menuorg.txt", "r");
		if(menu_level>=2)
		{
			i=0;
			while(i<(line_selected[0]+1))
			{
				titleSt=file.readStringUntil('\r\n');
				titleSt.toCharArray(Buf, 20);
				if(Buf[0]=='-')	i++;
			}
			i=0;
			Buf[0]='a';
			while(i<(line_selected[1]+1))
			{
				titleSt=file.readStringUntil('\r\n');
				titleSt.toCharArray(Buf, 20);
				if(Buf[0]=='/')	i++;
			}
			i=0;
			Buf[0]='a';
			while(Buf[0]!='/' && Buf[0]!='-' && Buf[0]!='#')
			{
				Serial.println(i);
				list_text[i]=file.readStringUntil('\r\n');
				Serial.println(list_text[i]);
				int lastIndex = list_text[i].length() - 1;
                list_text[i].remove(lastIndex);
				list_text[i].toCharArray(Buf, 20);
				list_text[i]=list_text[i].substring(3);
				if(Buf[0]=='*')	i++;
			}
			Serial.println(i);
			line_selected_max[2]=i-1;
			list_size=i;
			display_list(line_selected[2],145,10,65);	
		}
		file.close();
		file = SPIFFS.open("/menuorg.txt", "r");
		if(menu_level>=3)
		{
			i=0;
			while(i<(line_selected[0]+1))
			{
				titleSt=file.readStringUntil('\r\n');
				titleSt.toCharArray(Buf, 20);
				if(Buf[0]=='-')	i++;
			}
			i=0;
			Buf[0]='a';
			while(i<(line_selected[1]+1))
			{
				titleSt=file.readStringUntil('\r\n');
				titleSt.toCharArray(Buf, 20);
				if(Buf[0]=='/')	i++;
			}
			i=0;
			Buf[0]='a';
			while(i<(line_selected[2]+1))
			{
				titleSt=file.readStringUntil('\r\n');
				titleSt.toCharArray(Buf, 20);
				if(Buf[0]=='*')	i++;
			}
			i=0;
			Buf[0]='a';
			while(Buf[0]!='*' && Buf[0]!='/' && Buf[0]!='-' && Buf[0]!='#')
			{
				Serial.println(i);
				list_text[i]=file.readStringUntil('\r\n');
				Serial.println(list_text[i]);
				int lastIndex = list_text[i].length() - 1;
                list_text[i].remove(lastIndex);
				list_text[i].toCharArray(Buf, 20);
				list_text[i]=list_text[i].substring(4);
				if(Buf[0]=='+')	i++;
			}
			Serial.println(i);
			line_selected_max[3]=i-1;
			list_size=i;
			display_list(line_selected[3],210,10,75);	
		}
		/*niv=0;
		while(niv!=line_selected[1])
		{
			Serial.println(i);
			list_text[i]=file.readStringUntil('\r\n');
			Serial.println(list_text[i]);
			list_text[i].toCharArray(Buf, 20);
			if(Buf[0]=='/')	niv++;
		}
		if(Buf[2]=='@') display_list(line_selected[0],0,10,80);
		*/
		file.close();
	}
	
	void cpu_usage(int cpu)
	{
		String aff = String(cpu/6) + " %";
        tft.fillRect(SCREEN_WIDTH/2+100, 0, 40, 40,  0x5ACF);
        tft.drawString(aff, SCREEN_WIDTH/2+100, 10, 2);
	}
	
	void tempo_on()
	{
		tft.fillRect(250, 10, 20, 20, 0x5ACB);
	}
	
	void tempo_sync()
	{
		tft.fillRect(250, 10, 20, 20, TFT_BLUE);
	}
	
	void draw_centered(int height, String txt, int size)
	{
		if(size==BIGCHAR) spr.drawString(txt, SCREEN_WIDTH/2-txt.length()*8, height, size);
		if(size==SMALLCHAR) spr.drawString(txt, SCREEN_WIDTH/2-txt.length()*4, height, size);
	}
	
	void draw_title(String txt)
	{
		sprt.drawString(txt, SCREEN_WIDTH/2-txt.length()*7, 10, BIGCHAR);
	}
	
	void draw_bottom_menu(String txt)
	{
		sprt.drawString(txt, SCREEN_WIDTH/2-txt.length()*4, 10, SMALLCHAR);
	}
	
	void draw_string_center(String txt)
	{
		draw_centered(90, txt, BIGCHAR);
	}
	
	void draw_string_number_center(String txt, int num)
	{
		String aff = String(num);
		aff = txt + aff;
		draw_centered(90, aff, BIGCHAR);
	}
	
	void draw_number_string_center(int num, String txt)
	{
		String aff = String(num);
		aff = aff+txt;
		draw_centered(90, aff, BIGCHAR);
	}
	
	void draw_number_string_low(int num, String txt)
	{
		String aff = String(num);
		aff = aff+txt;
		draw_centered(140, aff, SMALLCHAR);
	}
	
	void midi_learn()
	{
	  spr.fillRect(60, 20, 160, 120, TFT_BLUE);
	  spr.drawString("MIDI LEARN", SCREEN_WIDTH/2-105, 110, 4);
	}

	
	void draw_adsr(int valA, int valD, int valS, int valR, int part)
	{
	  int color;
  
	  if(part==0) color=TFT_BLUE;
	  else color=0xF808;
	  spr.drawLine(10, 150, 10+valA/2, 20, color);
	  if(part==1) color=TFT_BLUE;
	  else color=0xF808;
	  spr.drawLine(10+valA/2, 20, 10+valA/2+valD/2, 147-valS, color);
	  if(part==2) color=TFT_BLUE;
	  else color=0xF808;
	  spr.drawLine(10+valA/2+valD/2, 147-valS, 220, 147-valS, color);
	  if(part==3) color=TFT_BLUE;
	  else color=0xF808;
	  spr.drawLine(220, 147-valS, 220+valR/2 , 150, color);

	  spr.drawCircle(10, 150, 2, TFT_WHITE);
	  spr.drawCircle(10+valA/2, 20, 2, TFT_WHITE);
	  spr.drawCircle(10+valA/2+valD/2, 147-valS, 2, TFT_WHITE);
	  spr.drawCircle(220, 147-valS, 2, TFT_WHITE);
	  spr.drawCircle(220+valR/2, 150, 2, TFT_WHITE);
	}
	
	void draw_waveform_lfo(float wave[], float vol, float cutoff)
	{
		Serial.println("draw lfo");
		int borne=1024/5;
		Serial.println(borne);
		Serial.println(wave[100]);
      for(int i=0; i<borne-1; i++)
      {
        int point1=160 - (wave[i*5]*vol)*80.0-cutoff*80;
        int point2=160 - (wave[i*5+3]*vol)*80.0-cutoff*80;
        if(point1<80) point1=80;
        if(point2<80) point2=80;
        if(point1>160) point1=160;
        if(point2>160) point2=160;
        if(i==0) point1=120;
        if(i==199) point2=120;
        int dist = point2-point1;
 
        spr.drawFastVLine(40+i,point1-40 ,160-point1, TFT_DARKGREEN);

        if(dist>0) {
          spr.drawFastVLine(40+i,point1-40 ,dist, TFT_GREEN);   
        }
        if(dist<0) {
          spr.drawFastVLine(40+i,point2-40 ,-dist, TFT_GREEN);
        }
        if(dist==0) spr.drawFastVLine(40+i,point1-40 ,1, TFT_GREEN);
      }
      //tft.drawFastHLine(40, 120,200, TFT_GREEN);
      spr.drawFastVLine(40, 40 , 80, TFT_DARKGREY);
      spr.drawFastVLine(140, 40 , 80, TFT_DARKGREY);
      spr.drawFastVLine(240, 40 , 80, TFT_DARKGREY);
	}
	
	void draw_waveform(float *wavefo)
	{
		int borne=SAMPLES_PER_CYCLE/5;
		Serial.println("draw_waveform");
		Serial.println(borne);
      for(int i=0; i<borne-1; i++)
      {
        //int point1=32 - sine[i<<4]*28;
        //int point2=32 - sine[(i+1)<<4]*28;
        int point1=120 - *(wavefo+(i*5))*200;
        int point2=120 - *(wavefo+((i+1)*5))*200;
        if(i==0) point1=120;
        if(i==borne-2) point2=120;
        int dist = point2-point1;
        /*if(dist>0) tft.drawFastVLine(point1, 32+i,dist, TFT_WHITE);
        if(dist<0) tft.drawFastVLine(point2, 32+i,-dist, TFT_WHITE);
        if(dist==0) tft.drawFastVLine(point1, 32+i,1, TFT_WHITE);*/
        if(point1<120)
        {
          spr.drawFastVLine(40+i,point1-40 ,120-point1, TFT_DARKGREEN);
        }
        else
        {
          spr.drawFastVLine(40+i,80 ,point1-120, TFT_DARKGREEN);
        }
        if(dist>0) {
          spr.drawFastVLine(40+i,point1-40 ,dist, TFT_GREEN);   
        }
        if(dist<0) {
          spr.drawFastVLine(40+i,point2-40 ,-dist, TFT_GREEN);
          //tft.drawFastVLine(40+i,point1 ,110-point1, 0x00B0);
        }
        if(dist==0) spr.drawFastVLine(40+i,point1-40 ,1, TFT_GREEN);
      }
      //tft.drawFastHLine(40, 120,200, TFT_GREEN);
      spr.drawFastVLine(40, 40 , 80, TFT_DARKGREY);
      spr.drawFastVLine(borne/2+39, 40 , 80, TFT_DARKGREY);
      spr.drawFastVLine(borne+40, 40 , 80, TFT_DARKGREY);
	}
	
	void draw_waveform_osc(OscMonoPoly os, int num)
    {
      int borne=SAMPLES_PER_CYCLE/5;
      //Serial.println("draw_waveform_osc");
      //Serial.println(os.pwm_conv[num]);
      if(os.pwm_conv[num]!=0)
      {
        //Serial.println(((os.fact1+os.fact2/(300+os.fl))*300));
		  for(int i=0; i<borne-1; i++)
		  {
			int i1,i2;
			if(os.pwm_conv[num]>64) 
			 {
			   i1 = (os.fact1[num]-os.fact2[num]/(SAMPLES_PER_CYCLE-i*5+os.fl[num]))*i*5;
			   i2= (os.fact1[num]-os.fact2[num]/(SAMPLES_PER_CYCLE-i*5-5 +os.fl[num]))*(i*5+5);
			 }
			 else 
			 {
			   i1 = (os.fact1[num]+os.fact2[num]/(i*5+os.fl[num]))*i*5;
			   i2 = (os.fact1[num]+os.fact2[num]/(i*5+5+os.fl[num]))*(i*5+5);
			 }
			//int point1=32 - sine[i<<4]*28;
			//int point2=32 - sine[(i+1)<<4]*28;
			int point1=120 - os.wave[num][i1]*200;
			int point2=120 - os.wave[num][i2]*200;
			if(i==0) point1=120;
			if(i==borne-2) point2=120;
			int dist = point2-point1;
			/*if(dist>0) tft.drawFastVLine(point1, 32+i,dist, TFT_WHITE);
			if(dist<0) tft.drawFastVLine(point2, 32+i,-dist, TFT_WHITE);
			if(dist==0) tft.drawFastVLine(point1, 32+i,1, TFT_WHITE);*/
			if(point1<120)
			{
			  spr.drawFastVLine(40+i,point1-40 ,120-point1, TFT_DARKGREEN);
			}
			else
			{
			  spr.drawFastVLine(40+i,80 ,point1-120, TFT_DARKGREEN);
			}
			if(dist>0) {
			  spr.drawFastVLine(40+i,point1-40 ,dist, TFT_GREEN);   
			}
			if(dist<0) {
			  spr.drawFastVLine(40+i,point2-40 ,-dist, TFT_GREEN);
			  //tft.drawFastVLine(40+i,point1 ,120-point1, 0x00B0);
			}
			if(dist==0) spr.drawFastVLine(40+i,point1-40 ,1, TFT_GREEN);
			
			if(point1>=120 && dist>=0) {
			  spr.drawFastVLine(40+i,point2+dist-39 ,180-point2-dist-1, TFT_BLACK);   
			}
			if(point1>=120 && dist<0) {
			  spr.drawFastVLine(40+i,point1+dist-39 ,180-point1-dist-1, TFT_BLACK);  
			}
			if(point1<=120 && dist>=0) {
			  spr.drawFastVLine(40+i,10 ,point1-50, TFT_BLACK);
			}
			if(point1<=120 && dist<0) {
			  spr.drawFastVLine(40+i,10 ,point2-50, TFT_BLACK);
			}

			if(point1>=120 && point2>=120) {
			  spr.drawFastVLine(40+i,30 ,50, TFT_BLACK);
			}
			if(point1<=120 && point2<=120) {
			  spr.drawFastVLine(40+i,81 ,50, TFT_BLACK);
			}
		  }
		  //tft.drawFastHLine(40, 120,200, TFT_GREEN);
		  spr.drawFastVLine(40, 40 , 80, TFT_DARKGREY);
		  spr.drawFastVLine(39+borne/2, 40 , 80, TFT_DARKGREY);
		  spr.drawFastVLine(40+borne, 40 , 80, TFT_DARKGREY);
		}
		else draw_waveform(os.wave[num]);
	}
	
	void compute_filter_curve(float res)
	{
	  float q2 = 1+exp((127.0-res)/12.0)/400.0;
	  float curve = 100/(q2*q2);
	  //if(param_filter_mode[0]==1) for(int i=0; i<20; i++) cur[i] = 99+40*((i*2+curve)*(i*2+curve)-curve*curve)/((40+curve)*(40+curve)-curve*curve);
	  //if(param_filter_mode[0]==4) for(int i=0; i<20; i++) cur[i] = 99+40*(((20-i)*2+curve)*((20-i)*2+curve)-curve*curve)/((40+curve)*(40+curve)-curve*curve);
	  for(int i=0; i<20; i++) cur[i] = 99+40*((i*2+curve)*(i*2+curve)-curve*curve)/((40+curve)*(40+curve)-curve*curve);
	}
	
	void draw_filter(float freq, float res)
	{
		for(int i=1; i<10; i++)
		{
		  spr.drawFastVLine(log10(100*i)*80-120,40,100,TFT_DARKGREY);
		  spr.drawFastVLine(log10(1000*i)*80-120,40,100,TFT_DARKGREY);
		}
		spr.drawFastVLine(log10(10000)*80-120,40,100,TFT_DARKGREY);
		spr.drawFastVLine(log10(20000)*80-120,40,100,TFT_DARKGREY);
		spr.drawString("100 Hz", 30, 140, SMALLCHAR);
		spr.drawString("1000", 100, 140, SMALLCHAR);
		spr.drawString("10000", 180, 140, SMALLCHAR);
		float freqcal = log10(freq)*80-160; 
	  //if(param_filter_mode[0]==1)
	  //{
		spr.drawFastHLine(40, 60,freqcal, TFT_GREEN);
		spr.drawFastHLine(40, 59,freqcal, TFT_GREEN);
		spr.drawFastHLine(40, 61,freqcal, TFT_GREEN);
		float q2 = 1+exp((127.0-res)/12.0)/400.0;
		float curve = 100/(q2*q2);
		//spr.drawFastVLine(freqcal+40,54-(float)peak_filt[0]/q2 ,13, TFT_BLACK);
		for(int i=0; i<40; i++) {spr.drawFastVLine(freqcal+40+i,59-(float)peak_filt[i*2]/q2 ,3, TFT_GREEN);}
		for(int i=0; i<20; i++) {spr.drawFastVLine(freqcal+i+80,cur[i]-40,3, TFT_GREEN);}
		spr.drawFastVLine(freqcal+20,90 ,13, TFT_BLACK);
	  //}
	  /*if(param_filter_mode[0]==2)
	  {
		for(int i=0; i<40; i++) spr.drawFastVLine(40+f3.freq_save+i,(i-20)*(i-20)/10+60 ,3, TFT_GREEN);
	  }
	  if(param_filter_mode[0]==3)
	  {
		for(int i=0; i<40; i++) spr.drawFastVLine(40+f3.freq_save+i,(i-20)*(i-20)/10+60 ,3, TFT_GREEN);
	  }
	  if(param_filter_mode[0]==4)
	  {    
		float q2 = 1+exp((127.0-(float)param_filter_res[0])/12.0)/400.0;
		float curve = 100/(q2*q2);
		//for(int i=0; i<20; i++) tft.drawFastVLine(40+i+param_filter_freq[0],99+40*(((20-i)*2+curve)*((20-i)*2+curve)-curve*curve)/((40+curve)*(40+curve)-curve*curve),3, TFT_GREEN);
		for(int i=0; i<20; i++) spr.drawFastVLine(40+i+f3.freq_save,cur[i]-40,3, TFT_GREEN);
		for(int i=0; i<40; i++) spr.drawFastVLine(60+i+f3.freq_save,59-(float)peak_filt[80-i*2]/q2 ,3, TFT_GREEN);
		
		spr.drawFastHLine(100+f3.freq_save, 60,140-f3.freq_save, TFT_GREEN);
		spr.drawFastHLine(100+f3.freq_save, 59,140-f3.freq_save, TFT_GREEN);
		spr.drawFastHLine(100+f3.freq_save, 61,140-f3.freq_save, TFT_GREEN); 
	  }
	  spr.drawFastVLine(40, 40 , 60, TFT_DARKGREY);
	  spr.drawFastHLine(40, 100 , 200, TFT_DARKGREY);*/
	}
	
	void display_list(int ls, int x, int y, int larg)
	{
		Serial.println("display_list");
	  int max_line = 8;
	  //uint8_t a = detect_button();
	  for(int i=0; i<list_size && i<max_line; i++)
	  {
		uint16_t line_color = 0xF800;
		if(i==ls) line_color = 0x0A80;
		tft.fillRect(x, y+i*20, larg, 20, line_color);
		tft.drawRect(x, y+i*20, larg, 20, TFT_WHITE);
		tft.setTextColor(TFT_WHITE);
		tft.drawString(list_text[i], x+5, y+2+i*20, 2);
		//tft.drawNumber(a, 50, 32+i*20,2);
	  }
	  if(list_size>max_line)
	  {
		tft.fillTriangle(x+larg/2-10,y+20*max_line+2, x+larg/2+10,y+20*max_line+2, x+larg/2,y+20*max_line+12,TFT_RED);
		tft.drawTriangle(x+larg/2-10,y+20*max_line+2, x+larg/2+10,y+20*max_line+2, x+larg/2,y+20*max_line+12,TFT_WHITE);
	  }
	}
	
	void drawBmp(const char *filename, int16_t x, int16_t y) {

	  if ((x >= tft.width()) || (y >= tft.height())) return;

	  fs::File bmpFS;

	  // Open requested file on SD card
	  bmpFS = SPIFFS.open(filename, "r");

	  if (!bmpFS)
	  {
		Serial.print("File not found");
		return;
	  }

	  uint32_t seekOffset;
	  uint16_t w, h, row, col;
	  uint8_t  r, g, b;

	  uint32_t startTime = millis();

	  uint16_t fir = read16(bmpFS);
	  Serial.println(fir&0x000F);
	  Serial.println((fir&0x00F0)>>4);
	  Serial.println((fir&0x0F00)>>8);
	  Serial.println((fir&0xF000)>>12);

	  //if (fir == 0x4D42)
	  //{
		Serial.println("OK");
		read32(bmpFS);
		read32(bmpFS);
		seekOffset = read32(bmpFS);
		read32(bmpFS);
		w = read32(bmpFS);
		h = read32(bmpFS);

		if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
		{
		  y += h - 1;

		  bool oldSwapBytes = tft.getSwapBytes();
		  tft.setSwapBytes(true);
		  bmpFS.seek(seekOffset);

		  uint16_t padding = (4 - ((w * 3) & 3)) & 3;
		  uint8_t lineBuffer[w * 3 + padding];

		  for (row = 0; row < h; row++) {
			
			bmpFS.read(lineBuffer, sizeof(lineBuffer));
			uint8_t*  bptr = lineBuffer;
			uint16_t* tptr = (uint16_t*)lineBuffer;
			// Convert 24 to 16 bit colours
			for (uint16_t col = 0; col < w; col++)
			{
			  b = *bptr++;
			  g = *bptr++;
			  r = *bptr++;
			  *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
			}

			// Push the pixel row to screen, pushImage will crop the line if needed
			// y is decremented as the BMP image is drawn bottom up
			tft.pushImage(x, y--, w, 1, (uint16_t*)lineBuffer);
		  }
		  tft.setSwapBytes(oldSwapBytes);
		  Serial.print("Loaded in "); Serial.print(millis() - startTime);
		  Serial.println(" ms");
		//}
		//else Serial.println("BMP format not recognized.");
	  }
	  bmpFS.close();
	}

	// These read 16- and 32-bit types from the SD card file.
	// BMP data is stored little-endian, Arduino is little-endian too.
	// May need to reverse subscript order if porting elsewhere.

	uint16_t read16(fs::File &f) {
	  uint16_t result;
	  ((uint8_t *)&result)[0] = f.read(); // LSB
	  ((uint8_t *)&result)[1] = f.read(); // MSB
	  return result;
	}

	uint32_t read32(fs::File &f) {
	  uint32_t result;
	  ((uint8_t *)&result)[0] = f.read(); // LSB
	  ((uint8_t *)&result)[1] = f.read();
	  ((uint8_t *)&result)[2] = f.read();
	  ((uint8_t *)&result)[3] = f.read(); // MSB
	  return result;
	}
};

#endif