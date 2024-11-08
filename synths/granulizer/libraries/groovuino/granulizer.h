#ifndef granulizer_h
#define granulizer_h

#include <FS.h>
#include <FFat.h>
#include <envfloat.h>

#define MAX_SAMPLE_NUM 2
#define NUM_SAMPLES 3000000 

#define MAX_GRAIN_NUM 4

#define POLYPHONY 4

int16_t *sample;

typedef struct {
	  float start;
	  float position;
	  float amplitude;
	  float env;
	  bool active;
	} Grain;
	
	Grain grains[MAX_GRAIN_NUM][POLYPHONY];

float convert_pitch[]=
{
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
0.04960448175,
0.05255414342,
0.05567939394,
0.05898996731,
0.0625,
0.0662141045,
0.07015168475,
0.07432288486,
0.07874264562,
0.08342433542,
0.08838556577,
0.09364123579,
0.09920943371,
0.1051088146,
0.1113581955,
0.1179799346,
0.125,
0.1324290468,
0.1403024291,
0.1486457697,
0.1574852912,
0.1668500008,
0.1767696387,
0.1872807959,
0.1984188674,
0.2102155181,
0.2227187606,
0.2359598692,
0.25,
0.2648547424,
0.2806048582,
0.2972957618,
0.3149658431,
0.3336946818,
0.3535452488,
0.3745615919,
0.3968302116,
0.420439481,
0.4454375213,
0.4719303783,
0.5,
0.5297094848,
0.5612097163,
0.5945915237,
0.629950644,
0.6673893636,
0.7070666126,
0.7491231837,
0.7936604231,
0.8408451836,
0.8908750426,
0.9438181982,
1,
1.05941897,
1.122479622,
1.189183047,
1.259901288,
1.334778727,
1.414133225,
1.498353615,
1.587441226,
1.681825486,
1.781750085,
1.887806638,
2,
2.11905244,
2.244959245,
2.378095887,
2.519499278,
2.669897959,
2.828648649,
2.996278271,
3.174400971,
3.36311054,
3.56350017,
3.775613276,
4,
4.237246964,
4.48991849,
4.757272727,
5.038998556,
5.339795918,
5.657297297,
5.994272623,
6.350728155,
6.72622108,
7.124574541,
7.551226551,
8,
8.474493927,
8.975986278,
9.514545455,
10.08285164,
10.67959184,
11.31459459,
11.98854525,
12.70145631,
13.45244216,
14.25885559,
15.1024531,
16,
16.96272285,
17.95197256,
19.02909091,
20.16570328,
21.35918367,
22.60475162,
23.94965675,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1};

class Granulizer
{
public:
	int index_glob=0;
	
	int sample_start_index[MAX_SAMPLE_NUM];
	int sample_end_index[MAX_SAMPLE_NUM];
	
	int param_start_index[MAX_SAMPLE_NUM];
	int param_end_index[MAX_SAMPLE_NUM];
	
	int sample_volume[MAX_SAMPLE_NUM];
	int sample_length[MAX_SAMPLE_NUM];
	int sampenv[MAX_SAMPLE_NUM];

	int grain_start=0;
	float grain_env=0.5; //(0=square env, 1=triangle env)
	//float grain_index[10];
		
	int grain_density=2;
	int grain_size=2000;
	float grain_rate[POLYPHONY];
	
	float grain_env_rel;
	float grain_env_dec;
	float grain_env_inc;

	float sample_pos[POLYPHONY];
	float sample_rate=1;
	float wait_for_grain[POLYPHONY];

	float time_new_grain;
	
	bool is_playing[POLYPHONY];
	
	int trace=0;
	
	int random_start_grain=0;
	
	Env env[POLYPHONY];
	
	struct notepressed{
		bool          on;
		bool          isplaying;
		uint8_t       pitch; 
	};
	typedef struct notepressed Notepressed;
   
	Notepressed notepressed[POLYPHONY];

  
	
	void init()
	{
		Serial.printf("Free PSRAM before allocation: %d\n", ESP.getFreePsram());
		sample = (int16_t *) ps_malloc(NUM_SAMPLES * sizeof(int16_t));
		if (sample == NULL) {
			Serial.println("Failed to allocate memory for samples");
			return;
		}
		delay(10);
		if(psramInit()){
			Serial.println("PSRAM is correctly initialized");
        }	
		else
		{
			Serial.println("PSRAM not available");
        }
		Serial.printf("Free PSRAM after allocation: %d\n", ESP.getFreePsram());
		init_grain();
		index_glob=0;
		
		for(int i=0; i<POLYPHONY; i++) 
		{
			sample_pos[i]=0;
			grain_rate[i]=1;
			is_playing[i]=false;
			env[i].init();
		}
	}
	
	void env_change(uint8_t val)
	{
		grain_env=(float)val/127.0;
		init_grain();
	}
	
	void size_change(uint8_t val)
	{
		grain_size=(int)val*(int)val+50;
		init_grain();
	}
	
	void density_change(uint8_t val)
	{
		grain_density=(int)val;
		init_grain();
	}
	
	void rate_change(uint8_t val)
	{
		//grain_rate=(float)val/64.0;
		//init_grain();
	}
	
	void sample_rate_change(uint8_t val)
	{
		sample_rate=(float)(val+20)*(float)(val+20)/7056.0;
	}
	
	void noteOn(uint8_t pitch)
	{
		for(int i=0; i<POLYPHONY; i++)
		{
			if(!notepressed[i].isplaying)
			{
				notepressed[i].pitch=pitch;
				notepressed[i].isplaying=true;
				grain_rate[i]=convert_pitch[pitch];
				env[i].start();
				i=100;
			}
		}		
		//init_grain();
	}
	
	void noteOff(uint8_t pitch)
	{
		for(int i=0; i<POLYPHONY; i++)
		{
			if(notepressed[i].isplaying && notepressed[i].pitch==pitch)
			{
				notepressed[i].pitch=0;
				env[i].stop();
			}
		}
	}
	
	void setenvA(uint32_t val)
	{
		for(int i=0; i<POLYPHONY; i++) env[i].setA(val);
	}

	void setenvD(uint32_t val)
	{
		for(int i=0; i<POLYPHONY; i++) env[i].setD(val);
		
	}

	void setenvS(uint32_t val)
	{
		for(int i=0; i<POLYPHONY; i++) env[i].setS(val);
	}

	void setenvR(uint32_t val)
	{
		for(int i=0; i<POLYPHONY; i++) env[i].setR(val);
	}
	
	int findMin(int array[], int size) {
		Serial.println("findmin");
		int min = array[0];
		for(int i = 1; i < size; i++) {
			if(array[i] < min) {
				min = array[i];
			}
		}
		Serial.println(min);
		return min;
	}
	
	int findMax(int array[], int size) {
		Serial.println("findmax");
		int max = array[0];
		for(int i = 1; i < size; i++) {
			if(array[i] > max) {
				max = array[i];
			}
		}
		Serial.println(max);
		return max;
	}
	
	int16_t tempval=0;
	
	bool load_file(String fname, int sampnu)
	{
	  Serial.println("load_file");
	  Serial.println(fname);
	  fs::File file2 = FFat.open(fname, "r");
	  if(!file2){
		Serial.println("Failed to open file for reading");
		return false;
	  }

	  sample_start_index[sampnu]=NUM_SAMPLES-1;
	  sample_end_index[sampnu]=0;

	  int i=0;
	  int wav_size=0;
	  int chan_num=0;
	  int smp_rate=0;
	  int bps=0;
	  int find_data=0;
	  int n=0;
	  int cksize=0;
	  int wFormatTag=0;
	  int blcksize=0;
	  int wBitsPerSample=0;
	  int octets=0;
	  char previous_data[] = {
		  0, 0, 0, 0
	  };
	  int fmt_begin=100000;
	  int ind_data=100000;
	  
	  int indtemp=0;
	  
	  while(file2.available()){
		char val=file2.read();
		previous_data[3]=previous_data[2];
		previous_data[2]=previous_data[1];
		previous_data[1]=previous_data[0];
		previous_data[0]=val;
		
		if(i<4) Serial.println(val);
		
		/*if ((i==0 && val!=0x52)||(i==1 && val!=0x49)||(i==2 && val!=0x46)||(i==3 && val!=0x46)) {
		  Serial.println("no RIFF wave file");
		  file2.close();
		}*/
		
		if (i==3 && val!=0x46) {
		  Serial.println("no RIFF wave file");
		  file2.close();
		}
		
		if (previous_data[0]==0x20 && previous_data[1]==0x74 && previous_data[2]==0x6d && previous_data[3]==0x66) fmt_begin = i+1;

		if (i==fmt_begin)    cksize+=val;
		if (i==fmt_begin+1)  cksize+=val*256;
		if (i==fmt_begin+2)  cksize+=val*65536;
		if (i==fmt_begin+3)  cksize+=val*16777216;
	 
		if (i==fmt_begin+4)  wFormatTag+=val;
		if (i==fmt_begin+5)  wFormatTag+=val*256;
	 
		if (i==fmt_begin+6)  chan_num+=val;
		if (i==fmt_begin+7)  chan_num+=val*256;
	 
		if (i==fmt_begin+8)  smp_rate+=val;
		if (i==fmt_begin+9)  smp_rate+=val*256;
		if (i==fmt_begin+10) smp_rate+=val*65536;
		if (i==fmt_begin+11) smp_rate+=val*16777216;
	 
		if (i==fmt_begin+12) bps+=val;
		if (i==fmt_begin+13) bps+=val*256;  
		if (i==fmt_begin+14) bps+=val*65536;  
		if (i==fmt_begin+15) bps+=val*16777216;  
	 
		if (i==fmt_begin+16) blcksize+=val;
		if (i==fmt_begin+17) blcksize+=val*256;
	 
		if (i==fmt_begin+18) wBitsPerSample+=val;
		if (i==fmt_begin+19) wBitsPerSample+=val*256;	
		
		if (i==fmt_begin+19)
		{
		  if (wFormatTag!=1) {
			Serial.println("not PCM file");
		  }
		  Serial.print("wFormatTag : ");
		  Serial.println(wFormatTag);
		  Serial.print("chan_num : ");
		  Serial.println(chan_num);
		  Serial.print("smp_rate : ");
		  Serial.println(smp_rate);
		  Serial.print("bps : ");
		  Serial.println(bps);
		  octets=wBitsPerSample>>3;
		  Serial.print("octets : ");
		  Serial.println(octets);
		  Serial.print("blcksize : ");
		  Serial.println(blcksize);
		  Serial.print("wBitsPerSample : ");
		  Serial.println(wBitsPerSample);
		}
		
		if (previous_data[0]==0x61 && previous_data[1]==0x74 && previous_data[2]==0x61 && previous_data[3]==0x64 && i<ind_data) {
		  ind_data = i+1;
		  Serial.print("ind_data : ");
		  Serial.println(ind_data);
		}
		//if(i%50==0) file2.flush();
		if(i==ind_data) wav_size+=val;
		if(i==ind_data+1) wav_size+=val*256;
		if(i==ind_data+2) wav_size+=val*65536;
		if(i==ind_data+3) 
		{
			wav_size+=val*16777216;
			Serial.print("wave size : ");
			Serial.println(wav_size);
			if(findMin(sample_start_index, MAX_SAMPLE_NUM)>wav_size) sample_start_index[sampnu]=0;
			else 
			{
				sample_start_index[sampnu]=findMax(sample_end_index, MAX_SAMPLE_NUM);
			}
			index_glob=sample_start_index[sampnu];
			Serial.println(index_glob);
			if((wav_size+index_glob)>4000000) return false; 
		}
		
		
		if(i>=ind_data+4 && i<=(wav_size+ind_data+4) && octets==2)
		{
		  if(chan_num==1 && (i-ind_data-4)%2==0) tempval=val&0xFF;
		  if(chan_num==1 && (i-ind_data-4)%2==1) {
			  sample[n+index_glob]=(((val<<8)&0xFF00)|tempval); 
			  n++;
		  }
		  if(chan_num==2 && (i-ind_data-4)%4==0) tempval=val&0xFF;
		  if(chan_num==2 && (i-ind_data-4)%4==1) {sample[n+index_glob]=(((val<<8)&0xFF00)|tempval); n++;}
		}
		if(i>=ind_data+4 && i<=(wav_size+ind_data+4) && octets==3)
		{
		  if(chan_num==1 && (i-ind_data-4)%3==1) tempval=val&0xFF;
		  if(chan_num==1 && (i-ind_data-4)%3==2) {sample[n+index_glob]=(((val<<8)&0xFF00)|tempval); n++;}
		  if(chan_num==2 && (i-ind_data-4)%6==1) tempval=val&0xFF;
		  if(chan_num==2 && (i-ind_data-4)%6==2) {sample[n+index_glob]=(((val<<8)&0xFF00)|tempval); n++;}
		}
		if(i>=ind_data+4 && i<=(wav_size+ind_data+4) && octets==4)
		{
		  if(chan_num==1 && (i-ind_data-4)%4==2) tempval=val&0xFF;
		  if(chan_num==1 && (i-ind_data-4)%4==3) {sample[n+index_glob]=(((val<<8)&0xFF00)|tempval); n++;}
		  if(chan_num==2 && (i-ind_data-4)%8==2) tempval=val&0xFF;
		  if(chan_num==2 && (i-ind_data-4)%8==3) {sample[n+index_glob]=(((val<<8)&0xFF00)|tempval); n++;}
		}
		indtemp++;
		if(indtemp>=512)
		{
			indtemp=0;
			delay(1);
		}
		i++;
		
	  }
	  Serial.println();
	  Serial.println(wav_size);
	  Serial.println(chan_num);
	  Serial.println(smp_rate);
	  Serial.println(bps);
	  Serial.println(ind_data);
	  sample_length[sampnu]=n;
	  sample_end_index[sampnu]=index_glob+n;
	  param_end_index[sampnu]=index_glob+n;
	  param_start_index[sampnu]=sample_start_index[sampnu];
	  index_glob+=n;
	  file2.close();
	  return true;
	}
	
	void change_start(int val)
	{
		param_start_index[0]=sample_start_index[0]+sample_length[0]*val/127;
	}
	
	void change_end(int val)
	{
		param_end_index[0]=sample_start_index[0]+sample_length[0]*val/127;
	}

  
  void init_grain()
  {
	Serial.println("init grain");
    grain_env_rel=(float)grain_size*grain_env*0.5;
    grain_env_dec=(float)grain_size-(float)grain_size*grain_env*0.5;
	if(grain_env_rel==0) grain_env_inc=1.0;
    else grain_env_inc=1.0/grain_env_rel;
	
    time_new_grain = SAMPLE_RATE/grain_density;
	Serial.println(grain_env_rel);
	Serial.println(grain_env_dec);
	Serial.println(grain_env_inc);
	Serial.println(time_new_grain);
  }

  int16_t grain_read(int num, int pol)
	{
	  trace++;
	  int16_t ret = 0;
	  if(grains[num][pol].position>=grain_size) grains[num][pol].env=0;
	  if(grains[num][pol].position==0) grains[num][pol].env=0;
	  //grains[num].env=0.1;
	  if(grain_env==0) ret=sample[(int)(grains[num][pol].position+grains[num][pol].start)];
	  else ret=sample[(int)(grains[num][pol].position+grains[num][pol].start)]*grains[num][pol].env;
	  //ret=sample[(int)(grains[num].position+grains[num].start)];
	  //if(trace<50) {Serial.println(grains[num].env); Serial.println(sample[(int)(grains[num].position+grains[num].start)]); Serial.println(ret); Serial.println((int)(grains[num].position+grains[num].start));}
	  grains[num][pol].position+=grain_rate[pol];  
	  if(grains[num][pol].position>grain_size) grains[num][pol].active=false;
  	  if(grains[num][pol].position<=grain_env_rel) grains[num][pol].env += grain_env_inc;
      if(grains[num][pol].position>=grain_env_dec) grains[num][pol].env -= grain_env_inc;
	  if(grains[num][pol].env>1.0) grains[num][pol].env=1.0;
	  if(grains[num][pol].env<0) grains[num][pol].env=0;
	  
	  
	  
	  //Serial.println(ret);
	
	  //return (float)ret*0.0000305176;
	  return ret;
	}
	
	void change_random_start_grain(int val)
	{
		random_start_grain = val*val+10;
	}
	
	
	void launch_grain(int num, int pol)
	{
		grains[num][pol].active=true;
		grains[num][pol].position=0;
		grains[num][pol].start=sample_pos[pol]+random(random_start_grain)-random_start_grain/2;
		if(grains[num][pol].start<param_start_index[0]) grains[num][pol].start=param_start_index[0];
		if(grains[num][pol].start>param_end_index[0]-grain_size) grains[num][pol].start=param_end_index[0]-grain_size;	
	}
	
	float process()
	{
		int16_t ret = 0;
		for(int j=0; j<POLYPHONY; j++)
		{
			if(!env[j].started && notepressed[j].isplaying) {notepressed[j].isplaying=false; Serial.print("end env : "); Serial.println(j);}			
			
			if(notepressed[j].isplaying)
			{
			  for(int i=0; i<MAX_GRAIN_NUM; i++)
			  {
				  if(grains[i][j].active) ret+=grain_read(i,j);	
				  //if(sample_pos	==	sample_start_index[0]) {Serial.println(grains[i].active); Serial.println(ret);}
			  }
			  ret*=env[j].amount();
			  //if(grains[0].active) ret=grain_read(0);
			  sample_pos[j]+=sample_rate;
			  if(sample_pos[j]>=(param_end_index[0]-grain_size)) {sample_pos[j]=param_start_index[0]; wait_for_grain[j]=time_new_grain; grains[0][j].active=false; grains[1][j].active=false;}
			  wait_for_grain[j]+=1;
			  if(wait_for_grain[j]>time_new_grain)
			  {
				  //Serial.print("new grain ");
				  trace=0;
				  wait_for_grain[j]=0;
				  for(int i=0; i<MAX_GRAIN_NUM; i++)
				  {
					  if(!grains[i][j].active) 
					  {
						  //Serial.println(i);
						  launch_grain(i,j);
						  break;
					  }
				  }
			  }
			}
		}
		return (ret);
	}
	
	/*void stop(int smpnum)
	{
	  Serial.println("stop");
	  Serial.println(smpnum);
	  is_playing=false;
		for(int i=0; i<MAX_GRAIN_NUM; i++)
		{
			grains[i].active=false;			  
		}
	}*/

	void launch(int smpnum, int vol)
	{
	    Serial.println("launch");
	    for(int i=0; i<POLYPHONY; i++) 
		{
			sample_pos[i]=sample_start_index[smpnum];
			wait_for_grain[i]=0; 
			is_playing[i]=true;
			grains[0][i].position=0;
			grains[0][i].start=sample_pos[i];
			grains[0][i].active=true;
		}			
	}
};
#endif
