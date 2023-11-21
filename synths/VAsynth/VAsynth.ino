//INIT passe ok avec Xtal, en retirant commande   st += ES8960_Write_Reg(49, 0x0F7);
//  --> mais pas de son avec DOUT 4 seulemet son failbe avec 17




#include <TFT_eSPI.h> // Hardware-specific library
#include <Wire.h>
#include "driver/i2s.h"
#include <envfloat.h>
int16_t *waveformTab;
#include <oscfloat.h>
#include <tablesfloat.h>
#include <lfofloat.h>
#include <Rotary.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
//#include <reverb.h>
#include "SD.h"
#include "FS.h"
#include "SPIFFS.h"
#include <lookuptable.h>
#include <disto2.h>
#define TINY 0.000001f;
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"

#include "braindisplay.h"



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

#define MAX_POLY_OSC  1 /* osc polyphony, always active reduces single voices max poly */
#define MAX_POLY_VOICE  3 /* max single voices, can use multiple osc */

//#define SAMPLE_RATE  44100
#define SAMPLE_BUFFER_SIZE 32
#define DELAY_SAMPLES 80000 

#define TFT_DARKBLUE 0x190D

// Digital I/O used
#define SD_CS         13
#define SPI_MOSI      15
#define SPI_MISO      2
#define SPI_SCK       14

int count = 0;

int env_dest=0;
bool delay_on=false;
bool save_bool=false;
bool load_bool=false;
int current_instru = 0;
uint8_t current_bar = 0;
uint8_t step_selected = 0;
uint8_t tempo_source = 0;   // 0: interne
                            // 1: sync in
uint8_t audio_out = 0;      // 0: HP
                            // 1: Headphone

uint8_t param_midi[128];
uint8_t param_focus[128];
int param_focus_max;
uint8_t multi_mode;
uint8_t changlob=0;
uint8_t chansynth1=0;
uint8_t chansynth2=0;
uint64_t rec1=0;
uint32_t rec2=0;
uint64_t rec3=0;
uint64_t rec4=0;
uint64_t rec5=0;
uint64_t save_rec1=0;

#define MAX_SYNTH       2
int current_synth = 0;

OscMonoPoly oscA[MAX_SYNTH];
Lfo lfo[3][MAX_SYNTH];
Disto2 disto[MAX_SYNTH];
TeeBeeFilter      Filter[MAX_SYNTH];
Env env[MAX_SYNTH];
OnePoleFilter     highpass1;

float* samp_rec;
int pos;
float feedback;
float deltime;

bool disto_on = false;
bool display_wave=false;
bool display_par=true;

TaskHandle_t Core0TaskHnd;

uint8_t midi_cc_val[128];

int index_glob=0;


int line_selected[4] = {0,0,0,0};
int menu_level=0;
int lbox_number=4;

uint8_t prev_value=0;

int screen_width=280;
int screen_height=240;

Rotary r = Rotary(6, 7);

int max_val=0;
int countz=0;

const i2s_port_t i2s_num = I2S_NUM_0; // i2s port number

Adafruit_USBD_MIDI usb_midi;
// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

bool init_running = true;
int savenum=1;

uint8_t poly=1;

Bdisplay disp;

#define WAVEFORM_NUMBER 128
#define WAVEFORM_SIZE 1024

String filelist[16];

void setup() {

  // power enable
  //     gpio_reset_pin(PW);
  //     gpio_set_direction(PW, GPIO_MODE_OUTPUT);
  //           gpio_set_level(PW, 1);

  Serial.begin(115200);
  Serial.println("I2S simple tone");
  
  delay(1000);
  xTaskCreatePinnedToCore(Core0Task, "CoreTask0", 40000, NULL, 999, &Core0TaskHnd, 0);

  waveformTab = (int16_t *) ps_malloc(WAVEFORM_NUMBER * WAVEFORM_SIZE * sizeof(int16_t));

  pinMode(BUT1, INPUT_PULLDOWN);
  pinMode(BUT2, INPUT_PULLDOWN);
  pinMode(BUT3, INPUT_PULLDOWN);
  pinMode(BUTENCO, INPUT_PULLDOWN);
  

  Serial.println(digitalRead(0));
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

  wavef_init();
  delay(500);

  Synth_Init();
  Serial.println("1 synth init");

  delay(500);
  
  Serial.println("1 search");

  Midi_Setup();
  MIDI.begin(MIDI_CHANNEL_OMNI);

  buildTables();

  // wait until device mounted
  int i=0;
  while( !TinyUSBDevice.mounted() && i<30  ) {delay(100);Serial.println("0 try");i++;}
  Serial.println("1 mounted");

  init_running=false;

  
}

/*static int16_t fl_sample[SAMPLE_BUFFER_SIZE];
static int16_t fr_sample[SAMPLE_BUFFER_SIZE];

static int16_t lr_sample[SAMPLE_BUFFER_SIZE*2];*/

static int16_t fl_sample[SAMPLE_BUFFER_SIZE];
static int16_t fr_sample[SAMPLE_BUFFER_SIZE];

bool i2s_write_stereo_samples_buff(int16_t *fl_sample, int16_t *fr_sample)
{
/*  static union sampleTUNT
  {
      uint32_t sample;
      int16_t ch[2];
  } sampleDataU[SAMPLE_BUFFER_SIZE];*/
  static union sampleTUNT
  {
      int16_t ch[2];
  } sampleDataU[SAMPLE_BUFFER_SIZE];

  for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
    {
        /*
         * using RIGHT_LEFT format
         */
        sampleDataU[n].ch[0] = fr_sample[n]; /* some bits missing here */
        sampleDataU[n].ch[1] = fl_sample[n];

    }
    static size_t bytes_written = 0;
    //Serial.println((sample>>16));
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

void loop() {
  uint32_t rec1=micros();
  Midi_Process();
  if(MIDI.read()) 
    {
      switch(MIDI.getType())      // Get the type of the message we caught
        {
            case midi::NoteOn:       // If it is a Note On,
                Midi_NoteOn(MIDI.getData1(),MIDI.getData2());  // blink the LED a number of times
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
    }
  for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
  {
      Synth_Process(&fl_sample[i], &fr_sample[i]);
      //if(fl_sample[i]>max_val) max_val=fl_sample[i];
  }
  rec2=micros()-rec1;
  countz++;
  if(countz>1000) {countz=0;}

  Audio_Output(fl_sample, fr_sample);
}
