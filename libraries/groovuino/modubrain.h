#ifndef modubrain_h
#define modubrain_h

#include <TFT_eSPI.h> // Hardware-specific library
#include <Wire.h>
#include "driver/i2s.h"
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <tablesfloat.h>
#include <lookuptable.h>
#include <Rotary.h>
#include "SD.h"
#include "FS.h"
#include "FFat.h"
#include "braindisplay.h"

void Synth_Process(int16_t *left, int16_t *right);
void Midi_NoteOn(uint8_t chan, uint8_t note, uint8_t vol);
void Midi_NoteOff(uint8_t chan, uint8_t note);
void HandleShortMsg(uint8_t *data);
void Midi_ControlChange(uint8_t channel, uint8_t data1, uint8_t data2);
void param_action(int num);
void param_action_focus(int num);
void Core0Task(void *parameter);
void Synth_Init();
void display_menu();
void display_param();
void change_enco(int sens);
void but_mid_pressed();
void enco_pressed();
void learn_midi();

static uint16_t WM8960_REG_VAL[56] =
{
  0x0097, 0x0097, 0x0000, 0x0000, 0x0000, 0x0008, 0x0000, 0x000A,
  0x01C0, 0x0000, 0x00FF, 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x007B, 0x0100, 0x0032, 0x0000, 0x00C3, 0x00C3, 0x01C0,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0100, 0x0100, 0x0050, 0x0050, 0x0050, 0x0050, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0040, 0x0000, 0x0000, 0x0050, 0x0050, 0x0000,
  0x0000, 0x0037, 0x004D, 0x0080, 0x0008, 0x0031, 0x0026, 0x00ED
};

//CONFIG BRAIN V2
#define SDA 18
#define SCL 11
#define WM8960_ADDRESS 0x1A
#define I2S_DOUT      17 
#define I2S_BCLK      5
#define I2S_LRC       16
#define I2S_DIN       4
#define I2SN (i2s_port_t)0

#define BUT1 GPIO_NUM_12
#define BUT2 GPIO_NUM_1
#define BUT3 GPIO_NUM_38

#define BUTENCO GPIO_NUM_39
#define BUTLEFT GPIO_NUM_0

#define MAX_POLY_OSC  1 // osc polyphony, always active reduces single voices max poly 
#define MAX_POLY_VOICE  3 // max single voices, can use multiple osc 

//#define SAMPLE_RATE  44100
#define SAMPLE_BUFFER_SIZE 32

#define TFT_DARKBLUE 0x190D

#define TINY 0.000001f;

// Digital I/O used
#define SD_CS         13
#define SPI_MOSI      15
#define SPI_MISO      2
#define SPI_SCK       14

int count = 0;

uint8_t param_midi[128];
uint8_t param_focus[128];
uint8_t midi_cc_val[128];

boolean midi_learn=false;
int value_sync=0;

int index_glob=0;

uint64_t rec1=0;
uint32_t rec2=0;
uint64_t rec3=0;
uint64_t rec4=0;
uint64_t rec5=0;
uint64_t save_rec1=0;

TaskHandle_t Core0TaskHnd;

int screen_width=280;
int screen_height=240;

Rotary r = Rotary(6, 7);

int countz=0;

const i2s_port_t i2s_num = I2S_NUM_0; // i2s port number

Adafruit_USBD_MIDI usb_midi;
// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

bool init_running = true;
int savenum=1;

Bdisplay disp;

String filelist[16];

// MIDI
#define RXD2 21 
#define TXD2 47
#define SYNCRX 48
#define DUMP_SERIAL2_TO_SERIAL

// constant to normalize midi value to 0.0 - 1.0f 
#define NORM127MUL  0.007874f

static int16_t fl_sample[SAMPLE_BUFFER_SIZE];
static int16_t fr_sample[SAMPLE_BUFFER_SIZE];

bool pressed_1=false;
bool pressed_2=false;
bool pressed_3=false;
bool pressed_4=false;
bool pressed_5=false;

int param_displayed = 0;
int enco_focus=0;
int time_enco=0;
int target=0;
int passed2=0;

int param_focus_max;

int num_title;

int detect_button()
{
  int ret=-1;
  if(digitalRead(BUT1) && pressed_1) pressed_1=false;
  if(!digitalRead(BUT1) && !pressed_1)
  {
    ret=0;
    pressed_1 = true;
    Serial.println("1");
  }
  if(digitalRead(BUT2) && pressed_2) pressed_2=false;
  if(!digitalRead(BUT2) && !pressed_2)
  {
    ret=1;
    pressed_2 = true;
    Serial.println("2");
  }
  if(digitalRead(BUT3) && pressed_3) pressed_3=false;
  if(!digitalRead(BUT3) && !pressed_3)
  {
    ret=2;
    pressed_3 = true;
    Serial.println("3");
  }
  if(digitalRead(BUTENCO) && pressed_4) pressed_4=false;
  if(!digitalRead(BUTENCO) && !pressed_4)
  {
    ret=3;
    pressed_4 = true;
    Serial.println("4");
  }
  if(!digitalRead(0) && pressed_5) pressed_5=false;
  if(digitalRead(0) && !pressed_5)
  {
    ret=4;
    pressed_5 = true;
    Serial.println("5");
  }
  return ret;
}

void change_CC(int sens)
{

  param_midi[param_displayed]=sens;  
  if(param_midi[param_displayed]>127) param_midi[param_displayed]=127;
  if(param_midi[param_displayed]<0) param_midi[param_displayed]=0;
  param_action(param_displayed);
}

void init_synth_param()
{
  Serial.println("init_synth_param");
  for(int i=0; i<128; i++)
  {
    Serial.println(i);
    Serial.println(param_midi[i]);
    if(i!=64 && i!=65 && i!=63 && i!=67 && i!=68 && i!=0) param_action(i);
  }
  for(int i=0; i<128; i++)
  {
    Serial.println(i);
    Serial.println(param_focus[i]);
    param_action_focus(i);
  }
}

void load_preset()
{
  Serial.println("load preset");
  String aff = String(savenum);
  aff = "/save" + aff + ".txt";
  Serial.println(aff);
  fs::File file = FFat.open(aff, "r");
  Serial.println(file.name());
  for(int i=1; i<128; i++)
  {
	
    param_midi[i] = file.read();
	Serial.println(param_midi[i]);
  }
  for(int i=1; i<128; i++)
  {
	
    param_focus[i] = file.read();
	Serial.println(param_focus[i]);
  }
  file.close();
  Serial.println("file closed");
  init_synth_param();
  i2s_stop(i2s_num);
  delay(200);
  i2s_start(i2s_num);
}

void save_preset()
{
  String aff = String(savenum);
  aff = "/save" + aff + ".txt";
  Serial.println(aff);
  fs::File file = FFat.open(aff, FILE_WRITE);
  Serial.println(file.name());
  for(int i=1; i<128; i++)
  {
	Serial.println(param_midi[i]);
    file.write(param_midi[i]);
  }
  for(int i=1; i<128; i++)
  {
	Serial.println(param_focus[i]);
    file.write(param_focus[i]);
  }
  file.close();
  Serial.println("file closed");
}

bool i2s_write_sample_32ch2(uint32_t sample)
{
    static size_t bytes_written = 0;
    //Serial.println((sample>>16));
    i2s_write((i2s_port_t)i2s_num, (const char *)&sample, 4, &bytes_written, portMAX_DELAY);

    if (bytes_written > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

String midi_note(int notein)
{
	String noteStr;
	if(notein%12==0) noteStr = "C";
	if(notein%12==1) noteStr = "C#";
	if(notein%12==2) noteStr = "D";
	if(notein%12==3) noteStr = "D#";
	if(notein%12==4) noteStr = "E";
	if(notein%12==5) noteStr = "F";
	if(notein%12==6) noteStr = "F#";
	if(notein%12==7) noteStr = "G";
	if(notein%12==8) noteStr = "G#";
	if(notein%12==9) noteStr = "A";
	if(notein%12==10) noteStr = "Bb";
	if(notein%12==11) noteStr = "B";
	return (noteStr + String((notein/12)-2));
}



// i2s configuration
 

i2s_config_t i2s_config =
{
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX ),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    //.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    //.intr_alloc_flags = 0,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 128,
    .use_apll = 0,
    .tx_desc_auto_clear=true,
    .fixed_mclk=-1
};

i2s_pin_config_t pins =
{
    .bck_io_num = 5,
    .ws_io_num =  16,
    .data_out_num = 17,
    .data_in_num = -1
};

void setup_i2s()
{  
  Serial.printf("1");
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
  Serial.printf("2");
  i2s_set_pin(i2s_num, &pins);
  Serial.printf("3");
  i2s_set_sample_rates(i2s_num, SAMPLE_RATE);
  Serial.printf("4");
  i2s_start(i2s_num);
  Serial.printf("5");
  i2s_set_clk(i2s_num, SAMPLE_RATE, (i2s_bits_per_sample_t)16, (i2s_channel_t)2);
  Serial.printf("6");
  i2s_stop(i2s_num);
  delay(1000);
  i2s_start(i2s_num);
}

void Midi_Setup()
{
    Serial2.begin(31250, SERIAL_8N1, RXD2, TXD2);
    pinMode(SYNCRX,INPUT_PULLDOWN);
}

//-------------------CODEC SETUP
///////////////////////////////////////////////////////////////////////
// Write ES8960 register
///////////////////////////////////////////////////////////////////////
int ES8960_Write_Reg(uint8_t reg, uint16_t dat)
{
  uint8_t res, I2C_Data[2];

  I2C_Data[0] = (reg << 1) | ((uint8_t)((dat >> 8) & 0x0001)); //RegAddr
  I2C_Data[1] = (uint8_t)(dat & 0x00FF);                       //RegValue


//Wire.setClock(10000);
 Wire.beginTransmission(WM8960_ADDRESS); // transmit to device lsb=0 => write
  Wire.write(I2C_Data, 2);                // buffer 1 byte lsb of val1
  res = Wire.endTransmission();           // transmit buffer and then stop

  if (res != 0)
  {
    printf("write error %d", reg);
  }

  return res;
}



void hp_spk(void)
{

  int st;

  do
  {
    st = 0;

    st += ES8960_Write_Reg(15, 0x00);
    usleep(10000);
    //Power
    st += ES8960_Write_Reg(25, 0x1FC);
    usleep(500);
    st += ES8960_Write_Reg(26, 0x1F9);
    usleep(500);
    st += ES8960_Write_Reg(47, 0x03C);
    usleep(10000);
    //Clock PLL
    st += ES8960_Write_Reg(4, 0x001);
    usleep(10000);
    st += ES8960_Write_Reg(52, 0x027);
    usleep(10000);
    st += ES8960_Write_Reg(53, 0x086);
    usleep(10000);
    st += ES8960_Write_Reg(54, 0x0C2);
    usleep(10000);
    st += ES8960_Write_Reg(55, 0x026);
    usleep(10000);
    //ADC/DAC
    st += ES8960_Write_Reg(5, 0x000);
    usleep(10000);
    st += ES8960_Write_Reg(7, 0x002);
    usleep(10000);
    //Noise control
    st += ES8960_Write_Reg(20, 0x0F9);
    usleep(10000);
    //OUT1 volume
    st += ES8960_Write_Reg(2, 0x16F);
    usleep(500);
    st += ES8960_Write_Reg(3, 0x16F);
    usleep(500);
    //SPK volume
    st += ES8960_Write_Reg(40, 0x17F);
    usleep(500);
    st += ES8960_Write_Reg(41, 0x17F);
    usleep(500);
    // OUTPUTS 0 XX11 0111 class D amp
    // XX = 00 off , 11 speaker on R and L
    st += ES8960_Write_Reg(49, 0x0FF);



    usleep(10000);
    st += ES8960_Write_Reg(10, 0x1FF);
    usleep(10000);
    st += ES8960_Write_Reg(11, 0x1FF);
    usleep(10000);

    st += ES8960_Write_Reg(34, 0x100);
    usleep(10000);

    st += ES8960_Write_Reg(37, 0x100);
    usleep(10000);


    if (st == 0) printf("init WS8960 OK....\n");
    else
    {
      //  printf(st);
      printf("init WS8960 failed...\n");
      delay(1000);
    }

  } while (st != 0);



}
//////////////////////////////////////////////////////////////////////////////////////
// DAC ===> HP L&R
//////////////////////////////////////////////////////////////////////////////////////
void dacToHp()
{
  int st;
  do
  {
    st = 0;
    st += ES8960_Write_Reg(15, 0x00);      //RESET
    delay(10);
    //ADC/DAC
    st += ES8960_Write_Reg(5, 0x004);       //DAC att=0dB, ADC pol. not inverted, DAC mute, no de-emphasis
    delay(10);
    st += ES8960_Write_Reg(7, 0x002);       //ADC ch normal, BCLK nrmal, slave mode, DAC ch normal,LRCK not inverted, 16bits, I2S
    delay(10);


    st += ES8960_Write_Reg(25, 0x0C0);      // VMID = 50k VREF enabled
    delay(10);
    st += ES8960_Write_Reg(26, 0x1E0);      // enable DAC L/R LOUT1 ROUT1
    delay(10);
    st += ES8960_Write_Reg(47, 0x00C);      // enable output mixer L/R
    delay(10);
    st += ES8960_Write_Reg(34, 0x100);      // L DAC to L output mixer
    delay(10);
    st += ES8960_Write_Reg(37, 0x100);      // R DAC to R output mixer
    delay(10);
    st += ES8960_Write_Reg(2, 0x179);       // LOUT1 volume
    delay(10);
    st += ES8960_Write_Reg(3, 0x179);       // ROUT1 volume
    delay(10);
    st += ES8960_Write_Reg(5, 0x000);       // unmute DAC
    delay(10);

    if (st == 0) printf("init WS8960 OK....\n");
    else
    {
      printf("init WS8960 failed...\n");
      delay(1000);
    }
  } while (st != 0);
}

//////////////////////////////////////////////////////////////////////////////////////
// DAC ===> SPR L/R and HP L/R
//////////////////////////////////////////////////////////////////////////////////////
void dacToSpkAndHp()
{
  int st;
  do
  {
    st = 0;
    st += ES8960_Write_Reg(15, 0x00);      //RESET
    delay(10);

    //ADC/DAC
    st += ES8960_Write_Reg(5, 0x004);       //DAC att=0dB, ADC pol. not inverted, DAC mute, no de-emphasis
    delay(10);
    st += ES8960_Write_Reg(7, 0x002);       //ADC ch normal, BCLK nrmal, slave mode, DAC ch normal,LRCK not inverted, 16bits, I2S
    delay(10);

    st += ES8960_Write_Reg(25, 0x0C0);      // VMID = 50k VREF enabled
    delay(10);
    st += ES8960_Write_Reg(26, 0x1F8);      // DAC l/R SPK L/R enabled
    delay(10);
    st += ES8960_Write_Reg(47, 0x00C);      // L/R output mixer enabled
    delay(10);
    st += ES8960_Write_Reg(49, 0x0FF);      // spk L/R enabled
    delay(10);
    st += ES8960_Write_Reg(51, 0x0AD);      // DCGAIN = 3.6 dB  ACGAIN = 3.6 dB
    delay(10);
    st += ES8960_Write_Reg(34, 0x100);      // DAC L to left output mixer
    delay(10);
    st += ES8960_Write_Reg(37, 0x100);      // DAC R to right output mixer
    delay(10);
    st += ES8960_Write_Reg(40, 0x168);      // spk L volume
    delay(10);
    st += ES8960_Write_Reg(41, 0x168);      // spk R volume
    st += ES8960_Write_Reg(2, 0x179);       // LOUT1 volume
    delay(10);
    st += ES8960_Write_Reg(3, 0x179);       // ROUT1 volume
    delay(10);

    delay(10);
    st += ES8960_Write_Reg(5, 0x000);       // DAC unmute
    delay(10);

    if (st == 0) printf("init WS8960 OK....\n");
    else
    {
      printf("init WS8960 failed...\n");
      delay(1000);
    }
  } while (st != 0);
}

////////////////////////////////////////////////////////////////////////////////////////
// setVol
//      vol 0=>10
////////////////////////////////////////////////////////////////////////////////////////
void setVol(int vol)
{
  int val;
  if (vol > 10) return;
  if (vol < 0) return;
  val = vol * 5 + 70;
  if (vol == 0)val = 0;

  printf("%x \n", val);
  val = val  | 0x100;

  int st;
  do
  {
    st = 0;

    printf("Vol setup raw %x \n", val);
    st += ES8960_Write_Reg(40, val);
    delay(10);
    st += ES8960_Write_Reg(41, val);
    delay(10);
    st += ES8960_Write_Reg(2, val);
    delay(10);
    st += ES8960_Write_Reg(3, val);
    delay(10);
  } while (st != 0);
}

//------------------CODEC SETUP END

void ES8960_Init2(void)
{


  int st;

  do
  {
    st = 0;
 
   st += ES8960_Write_Reg(15, 0x00);
  usleep(10000);
  //Power
  st += ES8960_Write_Reg(25, 0x1FC);
  usleep(500);  
  st += ES8960_Write_Reg(26, 0x1F9);
  usleep(500);  
  st += ES8960_Write_Reg(47, 0x03C);
  usleep(10000);  
  //Clock PLL
  st += ES8960_Write_Reg(4, 0x001);
  usleep(10000);  
  st += ES8960_Write_Reg(52, 0x027);
  usleep(10000);  
  st += ES8960_Write_Reg(53, 0x086);
  usleep(10000);  
  st += ES8960_Write_Reg(54, 0x0C2);
  usleep(10000);  
  st += ES8960_Write_Reg(55, 0x026);
  usleep(10000);
  //ADC/DAC
  st += ES8960_Write_Reg(5, 0x000);
  usleep(10000);  
  st += ES8960_Write_Reg(7, 0x002);
  usleep(10000);  
  //Noise control
  st += ES8960_Write_Reg(20, 0x0F9);
  usleep(10000);  
  //OUT1 volume
  st += ES8960_Write_Reg(2, 0x16F);
  usleep(500);  
  st += ES8960_Write_Reg(3, 0x16F);
  usleep(500);  
  //SPK volume
  st += ES8960_Write_Reg(40, 0x17F);
  usleep(500);
  st += ES8960_Write_Reg(41, 0x17F);
  usleep(500);
  // OUTPUTS
//  st += ES8960_Write_Reg(49, 0x0F7);
  usleep(10000);  
  st += ES8960_Write_Reg(10, 0x1FF);
  usleep(10000);
  st += ES8960_Write_Reg(11, 0x1FF);
  usleep(10000);

  st += ES8960_Write_Reg(34, 0x100);
  usleep(10000);
  
  st += ES8960_Write_Reg(37, 0x100);
  usleep(10000);  


    if (st == 0) printf("init WS8960 OK....\n");
    else
    {
      //  printf(st);
      printf("init WS8960 failed...\n");
      delay(1000);
    }

  } while (st != 0);



}

//int16_t maxaudio=1000;



bool i2s_write_stereo_samples_buff(int16_t *fl_sample, int16_t *fr_sample)
{
  static union sampleTUNT
  {
      int16_t ch[2];
  } sampleDataU[SAMPLE_BUFFER_SIZE];
  
  //if(fl_sample[0]>0) Serial.println(fl_sample[0]);

  for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
    {
		/*if(fr_sample[n]>maxaudio)
		{
			maxaudio=fr_sample[n];
			Serial.println(maxaudio);
		}*/
        sampleDataU[n].ch[0] = fr_sample[n]; // some bits missing here 
        sampleDataU[n].ch[1] = fl_sample[n];
		

    }
    static size_t bytes_written = 0;
    //Serial.println(fl_sample[0]);
    //i2s_write((i2s_port_t)i2s_num, (const char *)&sampleDataU[0].sample, 4 * buffLen, &bytes_written, portMAX_DELAY);
    //Serial.println(sampleDataU[0].ch[0]);
    i2s_write((i2s_port_t)i2s_num, (const char *)&sampleDataU[0].ch[0], 4 * SAMPLE_BUFFER_SIZE, &bytes_written, portMAX_DELAY);
    
    if (bytes_written > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void Audio_Output(int16_t *left, int16_t *right)
{
    i2s_write_stereo_samples_buff(left, right);
}




void Sync_Process()
{
  int v = digitalRead(SYNCRX);
  if(v!=value_sync)  {
    //Serial.println(v); 
    //if(v==1) current_step++;
    value_sync=v;
  }
}

void USB_Midi_Process()
{
   if(MIDI.read()) 
    {
      switch(MIDI.getType())      // Get the type of the message we caught
        {
            case midi::NoteOn:       // If it is a Note On,
                Midi_NoteOn(MIDI.getChannel(),MIDI.getData1(),MIDI.getData2());  // blink the LED a number of times
                                            // correponding to the program number
                                            // (0 to 127, it can last a while..)
                break;
             case midi::NoteOff:       // If it is a Note On,
                Midi_NoteOff(MIDI.getChannel(),MIDI.getData1());  // blink the LED a number of times
                                            // correponding to the program number
                                            // (0 to 127, it can last a while..)
                break;
			case midi::ControlChange:
				Midi_ControlChange(MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
				break;
            // See the online reference for other message types
            default:
                break;
        }
    }
}

//uint32_t midibuffer=0;
//uint32_t previous_midibuffer=0;

void Midi_Process()
{

    static uint32_t inMsgWd = 0;
    static uint8_t inMsg[3];
    static uint8_t inMsgIndex = 0;

    //Serial.println(digitalRead(RXD2));

    //Choose Serial1 or Serial2 as required
	
	
		

		if (Serial2.available())
		{
			uint8_t incomingByte = Serial2.read();
			//Serial.println("read");

	#ifdef DUMP_SERIAL2_TO_SERIAL
			//Serial.printf("%02x", incomingByte);
	#endif
			// ignore live messages 
			if ((incomingByte & 0xF0) == 0xF0)
			{
				return;
			}

			if (inMsgIndex == 0)
			{
				if ((incomingByte & 0x80) != 0x80)
				{
					inMsgIndex = 1;
				}
			}

			inMsg[inMsgIndex] = incomingByte;
			inMsgIndex += 1;

			if (inMsgIndex >= 3)
			{
				HandleShortMsg(inMsg);
				inMsgIndex = 0;
			}

		  
			inMsgWd = 0;
		}
		if(MIDI.read()) 
		{
		  switch(MIDI.getType())      // Get the type of the message we caught
			{
				case midi::NoteOn:       // If it is a Note On,
					Midi_NoteOn(MIDI.getChannel(),MIDI.getData1(),MIDI.getData2());  // blink the LED a number of times
												// correponding to the program number
												// (0 to 127, it can last a while..)
					break;
				 case midi::NoteOff:       // If it is a Note On,
					Midi_NoteOff(MIDI.getChannel(),MIDI.getData1());  // blink the LED a number of times
												// correponding to the program number
												// (0 to 127, it can last a while..)
					break;
				case midi::ControlChange:
					//midibuffer=millis();
					//if((midibuffer-previous_midibuffer)>10)
					//{
						//previous_midibuffer=midibuffer;
						Midi_ControlChange(MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
					//}
					break;
				// See the online reference for other message types
				default:
					break;
			}
		}

}



void modubrainProcess()
{
  uint32_t rec1=micros();
  for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
  {
      Synth_Process(&fl_sample[i], &fr_sample[i]);
  }
  rec2=micros()-rec1;
  countz++;
  if(countz>1000) {countz=0;}
  Audio_Output(fl_sample, fr_sample);
}

void modubrainInit()
{
  xTaskCreatePinnedToCore(Core0Task, "CoreTask0", 40000, NULL, 999, &Core0TaskHnd, 0);

  waveformTab = (int16_t *) ps_malloc(WAVEFORM_NUMBER * WAVEFORM_SIZE * sizeof(int16_t));

  pinMode(BUT1, INPUT_PULLDOWN);
  pinMode(BUT2, INPUT_PULLDOWN);
  pinMode(BUT3, INPUT_PULLDOWN);
  pinMode(BUTENCO, INPUT_PULLDOWN);
  
  for(int i=0; i<128; i++)
  {
    midi_cc_val[i]=0;
  }
  
  Serial.println("1 Starting I2C codec comm");
  //I2c init for codec

  Wire.begin(SDA, SCL);
  Serial.println(digitalRead(0));
  delay(1000);

  gpio_reset_pin((gpio_num_t)SD_CS);  
  gpio_reset_pin((gpio_num_t)SPI_MOSI);  
  gpio_reset_pin((gpio_num_t)SPI_MISO);  
  gpio_reset_pin((gpio_num_t)SPI_SCK);  

   
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  Serial.println("0 SD");
  delay(1000);
  
   int  volume = 10;

  //ES8960_Init2(); //headphone OK, 
  //dacToSpkAndHp();
  hp_spk();
  Serial.println("1 SPK");
  delay(1000);
  setVol(10);
  Serial.println("1 vol");
  Serial.println(digitalRead(0));

  setup_i2s();
  pinMode(0, INPUT_PULLUP);
  Serial.println("1 I2S");
  
  createSineTable();
  createSawTable();
  createSqTable();

  Synth_Init();
  Serial.println("1 synth init");

  delay(500);
  
  Serial.println("1 search");
  
  //Midi_Setup();
  Serial.println("midi setup");
  //MIDI.begin(MIDI_CHANNEL_OMNI);

  buildTables();
  Serial.println("build tables");
  
  // wait until device mounted
  int i=0;
  while( !TinyUSBDevice.mounted() && i<30  ) {delay(100);Serial.println("0 try");i++;}
  Serial.println("1 mounted");

  init_running=false;
}

void core0_init()
{
  if (!FFat.begin()) {
    Serial.println("FFat initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nFFat initialised.");

  delay(1000);
  disp.init();
  Serial.println("0 disp");
  
  disp.drawBmp("/logo2.bmp", 0, 0);
  
  Serial.println("0 bmp");
  //int tBytes = FFat.totalBytes(); 
  //int uBytes = FFat.usedBytes();
  //Serial.println("0 FFat space");
  //Serial.println(tBytes);
  //Serial.println(uBytes);
  while(init_running) delay(1);

  Serial.println("0 suite");
  
  disp.clear();
  display_menu();
  display_param();
  r.begin(true);
  detect_button();
  
  Serial.println("0 detect button");
  
  num_title = disp.find_title_num();
  Serial.println(num_title);
}

void enco_turned()
{
   uint8_t result = r.process();
   if(result) 
   {
      //Serial.println("enco");
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
}

void button_pressed()
{
	int testbut=detect_button();
	if(testbut==2) 
    {
      Serial.println("but right");
      if(enco_focus==0)
      {
          param_displayed += 1;
		  Serial.println(param_displayed);
		  if(param_displayed>=num_title) param_displayed=num_title-1;
		  Serial.println(param_displayed);
		  display_param();
          display_menu();
          
       }
      if(enco_focus==-1)
      {
        int disppar = disp.menu_right();
          if(disppar<200) 
          {
            enco_focus=0;
            param_displayed=disppar;
            disp.clear();
			display_param();
            display_menu();
            
          }
          else
          {
            disp.clear();
            display_menu();
            disp.menu_hierarchy();
          }
          
       
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
		  display_param();
          display_menu();
          
      }
    }
  
    if(testbut==1) 
    {
      Serial.println("but mid");
      if(enco_focus!=0)
      {
		but_mid_pressed();
        enco_focus=0;
		display_param();
        display_menu();
        
      }
      else
      {
        //if(enco_focus==0)
        //{
          param_focus_max=disp.list_bottom(param_displayed, param_focus[param_displayed]);
          if(param_focus_max>0) enco_focus=2;
		  if(param_focus_max==-1) enco_focus=3;
		  if(param_focus_max==-2) {but_mid_pressed(); disp.display_bottom();}
		  if(param_focus_max==-3) {but_mid_pressed();}
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
		display_param();
        display_menu();
        
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
	  Serial.println("but4");
	  enco_pressed();
      
    }
}

void learn_midi()
{
	Serial.println("learn midi");
	if(!midi_learn)
	{
		disp.midi_learn();
		disp.display_window();
		midi_learn=true;
		i2s_stop(i2s_num);
		delay(200);
		i2s_start(i2s_num);
	}
	else
	{
		display_menu();
		display_param();
		midi_learn=false;
		i2s_stop(i2s_num);
		delay(200);
		i2s_start(i2s_num);
	}
}

#endif