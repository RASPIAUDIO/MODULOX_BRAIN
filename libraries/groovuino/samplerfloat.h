#ifndef samplerfloat_h
#define samplerfloat_h

#include "FFat.h"

#define MAX_SAMPLE_NUM 16
#define NUM_SAMPLES 1300000 

int16_t *sample;

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

class Sampler
{
public:
	float sample_index_inc[MAX_SAMPLE_NUM];
	int sample_index_save[MAX_SAMPLE_NUM];

	int sample_start_index[MAX_SAMPLE_NUM];
	int sample_end_index[MAX_SAMPLE_NUM];
	bool sample_playing[MAX_SAMPLE_NUM];
	float sample_index[MAX_SAMPLE_NUM];

	int sample_volume[MAX_SAMPLE_NUM];
	int sample_length[MAX_SAMPLE_NUM];
	int sampenv[MAX_SAMPLE_NUM];
	
	int param_start_sample[MAX_SAMPLE_NUM];
	int param_end_sample[MAX_SAMPLE_NUM];
	
	int index_glob=0;
	int name_num[MAX_SAMPLE_NUM];
	
	void init()
	{
		sample = (int16_t *) ps_malloc(NUM_SAMPLES * sizeof(int16_t));
		delay(10);
		if(psramInit()){
        Serial.println("\nPSRAM is correctly initialized");
        }else{
        Serial.println("PSRAM not available");
        }
		index_glob=0;
		for (int n = 0; n < MAX_SAMPLE_NUM; n++)
		{
			sample_start_index[n]=NUM_SAMPLES-1;
			sample_end_index[n]=0;
			sample_index[n]=0;
			sample_playing[n]=0;
			sample_index_inc[n]=1.0;
			sampenv[n]=0;
			param_start_sample[n]=0;
			param_end_sample[n]=0;
		}
		sample_playing[0]=1;
	}
	
	void tune(int tu, int num)
	{
	    sample_index_save[num]=tu;
		sample_index_inc[num]=1.0-(((float)tu-64.0)/127.0);
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
	
	void load_file(String fname, int sampnu, int samplist)
	{
	  Serial.println("load_file");
	  Serial.println(fname);
	  File file2 = FFat.open(fname);
	  name_num[sampnu]=samplist;
	  if(!file2){
		Serial.println("Failed to open file for reading");
		return;
	  }

	  sample_start_index[sampnu]=NUM_SAMPLES-1;
	  sample_end_index[sampnu]=0;

	  int i=0;
	  int wav_size=0;
	  int chan_num=0;
	  int smp_rate=0;
	  int bps=0;
	  int find_data=0;
	  int ind_data=10000;
	  int n=0;
	  while(file2.available()){
		char val=file2.read();
		if(i==0) Serial.write(val);
		if(i==1) Serial.write(val);
		if(i==2) Serial.write(val);
		if(i==3) Serial.write(val);
		if(i==22) chan_num+=val;
		if(i==23) chan_num+=val*256;
		if(i==24) smp_rate+=val;
		if(i==25) smp_rate+=val*256;
		if(i==26) smp_rate+=val*65536;
		if(i==27) smp_rate+=val*16777216;
		if(i==34) bps+=val;
		if(i==35) bps+=val*256;
		
		if(i==40) wav_size+=val;
		if(i==41) wav_size+=val*256;
		if(i==42) wav_size+=val*65536;
		if(i==43) 
		{
			wav_size+=val*16777216;
			Serial.println("wave size");
			Serial.println(wav_size);
			if(findMin(sample_start_index, MAX_SAMPLE_NUM)>wav_size) sample_start_index[sampnu]=0;
			else 
			{
				sample_start_index[sampnu]=findMax(sample_end_index, MAX_SAMPLE_NUM);
			}
			index_glob=sample_start_index[sampnu];
			Serial.println(index_glob);
		}
		
		
		if(i<200)
		{
		  if(val==0x64 && find_data==0) find_data=1;
		  if(val==0x61 && find_data==1) find_data=2;
		  if(val==0x74 && find_data==2) find_data=3;
		  if(val==0x61 && find_data==3) {ind_data=i+3;find_data=0;}
		}
		if(i%50==0) file2.flush();
		if(i>=(ind_data+28) && i<wav_size+44)
		{
		  if(chan_num==1 && i%2==0) tempval=val&0xFF;
		  if(chan_num==1 && i%2==1) {sample[n+index_glob]=(((val<<8)&0xFF00)|tempval); n++;}
		}
		i++;
	  }
	  Serial.println();
	  Serial.println(wav_size);
	  Serial.println(chan_num);
	  Serial.println(smp_rate);
	  Serial.println(bps);
	  Serial.println(ind_data);
	  sample_length[sampnu]=n-50;
	  sample_end_index[sampnu]=index_glob+n-50;
	  index_glob+=n;
	  file2.close();
	}
	
	float sample_read()
	{
	  int16_t ret = 0;
	  for (int n = 0; n < MAX_SAMPLE_NUM; n++)
	  {
		if(sample_playing[n] && sampenv[n]<(sample_volume[n])) 
		{
			sampenv[n]++;
		}
		if(!sample_playing[n] && sampenv[n]>0) 
		{
			sampenv[n]--;
		}
		if(sampenv[n]>0) 
		{
		  ret+=(sample[int(sample_index[n])]*(sampenv[n]))>>7;
		  //ret+=sample[sample_index[n]];
		  sample_index[n]+=sample_index_inc[n];
		  if(sample_index[n]>=(sample_end_index[n]-(param_end_sample[n]*sample_length[n]/127)))
		  {
			sample_playing[n]=0;
			sample_index[n]=sample_start_index[n]+(param_start_sample[n]*sample_length[n]/127);
			sampenv[n]=0;
		  }
		  //Serial.println(ret);
		}
	  }
		/*if(sample_playing[1]) 
		{
		  //ret+=(kick01[sample_index[0]]*sample_volume[0])>>7;
		  ret+=sample[sample_index[1]];
		  sample_index[1]++;
		  if(sample_index[1]>=(sample_length[1]+sample_start_index[1]))
		  {
			sample_playing[1]=0;
			sample_index[1]=sample_start_index[1];
		  }
		}*/
		
	  //return (float)ret*0.0000305176;
	  return (float)ret*0.000025;
	}
	
	void sample_stop(int smpnum)
	{
	  Serial.println("stop");
	  Serial.println(smpnum);
	  sample_playing[smpnum]=0;
	  //sample_index[smpnum]=sample_start_index[smpnum]+(param_start_sample[smpnum]*sample_length[smpnum]/127);
	}

	void sample_launch(int smpnum, int vol)
	{
	  Serial.println("launch");
	  sample_playing[smpnum]=1;
	  sample_volume[smpnum]=vol;
	  //sample_index[smpnum]=sample_start_index[smpnum];
		sample_index[smpnum]=sample_start_index[smpnum]+(param_start_sample[smpnum]*sample_length[smpnum]/127);
		sampenv[smpnum]=0;
	}
	
	void sample_launch(int smpnum, int vol, int note)
	{
	   sample_index_inc[smpnum]=convert_pitch[note];
	  Serial.println("launch");
	  sample_playing[smpnum]=1;
	  sample_volume[smpnum]=vol;
	  //sample_index[smpnum]=sample_start_index[smpnum];
		sample_index[smpnum]=sample_start_index[smpnum]+(param_start_sample[smpnum]*sample_length[smpnum]/127);
		sampenv[smpnum]=0;
	}

};
#endif