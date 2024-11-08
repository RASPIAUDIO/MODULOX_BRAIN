#ifndef modubrain_h
#define modubrain_h

#include <TFT_eSPI.h> // Hardware-specific library
#include <Wire.h>
#include "driver/i2s.h"
#include <FS.h>
#include <FFat.h>
#include "USB.h"
#include "USBMSC.h"
#include "USBMIDI.h"
#include "esp_partition.h"
#include <tablesfloat.h>
#include <lookuptable.h>
#include <Rotary.h>
#include "braindisplay.h"
#include <SparkFun_WM8960_Arduino_Library.h> 

void Synth_Process(int16_t *left, int16_t *right);
void Midi_NoteOn(uint8_t chan, uint8_t note, uint8_t vol);
void Midi_NoteOff(uint8_t chan, uint8_t note);
void HandleShortMsg(uint8_t *data);
void Midi_ControlChange(uint8_t channel, uint8_t data1, uint8_t data2);
void param_action(int num);
void param_action_focus(int num);
void taskAudio(void *parameter);
void Synth_Init();
void display_param();
void but_mid_pressed();
void enco_pressed();
void enco_released();
void learn_midi();

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

// Digital I/O used
/*#define SD_CS         13
#define SPI_MOSI      15
#define SPI_MISO      2
#define SPI_SCK       14*/

// MIDI
#define RXD2 21 
#define TXD2 47
#define SYNCRX 48
#define DUMP_SERIAL2_TO_SERIAL

// constant to normalize midi value to 0.0 - 1.0f 
#define NORM127MUL  0.007874f
#define SAMPLE_RATE  44100

USBMSC MSC;
EspClass _flash;
USBMIDI midi; // MIDI
const esp_partition_t *fatfs_partition;

#define FAT_U8(v)          ((v) & 0xFF)
#define FAT_U16(v)         FAT_U8(v), FAT_U8((v) >> 8)
#define FAT_U32(v)         FAT_U8(v), FAT_U8((v) >> 8), FAT_U8((v) >> 16), FAT_U8((v) >> 24)
#define FAT_MS2B(s, ms)    FAT_U8(((((s) & 0x1) * 1000) + (ms)) / 10)
#define FAT_HMS2B(h, m, s) FAT_U8(((s) >> 1) | (((m) & 0x7) << 5)), FAT_U8((((m) >> 3) & 0x7) | ((h) << 3))
#define FAT_YMD2B(y, m, d) FAT_U8(((d) & 0x1F) | (((m) & 0x7) << 5)), FAT_U8((((m) >> 3) & 0x1) | ((((y) - 1980) & 0x7F) << 1))
#define FAT_TBL2B(l, h)    FAT_U8(l), FAT_U8(((l >> 8) & 0xF) | ((h << 4) & 0xF0)), FAT_U8(h >> 4)

#define README_CONTENTS \
  "This is tinyusb's MassStorage Class demo.\r\n\r\nIf you find any bugs or get any questions, feel free to file an\r\nissue at github.com/hathach/tinyusb"

static uint32_t DISK_SECTOR_COUNT;   // 8KB is the smallest size that windows allow to mount
static const uint16_t DISK_SECTOR_SIZE = 4096;      // Should be 512
static const uint16_t DISC_SECTORS_PER_TABLE = 1;  //each table sector can fit 170KB (340 sectors)

void taskAudio(void *parameter);
void taskOther(void *parameter);

const i2s_port_t i2s_num = I2S_NUM_0; // i2s port number

bool pressed_1=false;
bool pressed_2=false;
bool pressed_3=false;
bool pressed_4=false;
bool pressed_5=false;

int param_displayed = 0;
int param_CC=0;
int enco_focus=0;
int time_enco=0;
int target=0;
int passed2=0;

bool filefound=true;

int param_focus_max;

int num_title;

int count = 0;

uint8_t param_midi[128];
uint8_t param_focus[128];
uint8_t midi_cc_val[128];

boolean midi_learn=false;

int value_sync=0;

int index_glob=0;

Rotary r = Rotary(6, 7);
Bdisplay disp;
String filelist[16];

bool init_running = true;
bool midi_changed=false;
int savenum=1;

WM8960 codec;

void display_menu()
{
  Serial.println("display_menu");
  if(enco_focus>0) disp.menu_top(param_displayed, 0x5ACF); 
  else disp.menu_top(param_displayed);
  disp.display_top();
  Serial.println("background");
  disp.menu_bottom(param_displayed,param_focus[param_displayed]);
  disp.display_bottom();
}

void change_enco(int sens)
{
  Serial.println("change_enco");
  if(enco_focus==-1)
  {
    disp.change_menu_select(sens);
  }
  if(enco_focus==1)
  {
    disp.line_selected[disp.menu_level]+=sens;
    display_menu();
    disp.menu_hierarchy();
  }
  if(enco_focus>1)
  {
    param_focus[param_displayed]+=sens;
    if(param_focus[param_displayed]<0) param_focus[param_displayed]=0;
    if(param_focus[param_displayed]>param_focus_max) param_focus[param_displayed]=param_focus_max;
    param_action_focus(param_displayed);
    //display_menu();
    disp.display_list(param_focus[param_displayed],90,100,100);
  }
  if(enco_focus==0)
  {
    Serial.println(param_midi[param_displayed]);
    if(sens<0) 
    {
       if(param_midi[param_displayed]>=-sens) param_midi[param_displayed]+=sens;
    }
    else
    {
      if((127-param_midi[param_displayed])>=sens) param_midi[param_displayed]+=sens;
    }
    Serial.println("param " + String(param_displayed) + " changed : " + String(param_midi[param_displayed]));
    param_action(param_displayed);
    display_param();
  }
    
  //if((param_displayed != 2 || env_dest!=1)&&display_par) display_param();
  Serial.println("end change_enco");
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  uint32_t address = (lba * DISK_SECTOR_SIZE) + offset;

  if (address + bufsize > fatfs_partition->size) {
    Serial.println("Écriture au-delà de la taille de la partition");
    return -1;
  }
  
  Serial.println("Write");
  Serial.println(lba);
  Serial.println(offset);
  Serial.println(bufsize);

  /*esp_err_t err = esp_partition_write(fatfs_partition, address, buffer, bufsize);
  if (err != ESP_OK) {
    Serial.printf("Erreur d'écriture dans la partition: %s\n", esp_err_to_name(err));
    return -1;
  }*/
  // Erase block before writing as to not leave any garbage
  _flash.partitionEraseRange(fatfs_partition, address, bufsize);

  // Write data to flash memory in blocks from buffer
  _flash.partitionWrite(fatfs_partition, address, (uint32_t*)buffer, bufsize);
  return bufsize;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  uint32_t address = (lba * DISK_SECTOR_SIZE) + offset;

  if (address + bufsize > fatfs_partition->size) {
    Serial.println("Lecture au-delà de la taille de la partition");
    return -1;
  }
  Serial.println("Read");
  Serial.println(lba);
  Serial.println(offset);
  Serial.println(bufsize);

  //esp_err_t err = esp_partition_read(fatfs_partition, address, buffer, bufsize);
  _flash.partitionRead(fatfs_partition, address, (uint32_t*)buffer, bufsize);
  /*if (err != ESP_OK) {
    Serial.printf("Erreur de lecture de la partition: %s\n", esp_err_to_name(err));
    return -1;
  }*/
  return bufsize;
}

// Fonctions pour monter et démonter FFat
void mountFFat() {
  if (!FFat.begin(false, "/ffat", 5, "ffat")) {
    Serial.println("Échec du montage de la partition FATFS.");
  } else {
    Serial.println("Partition FATFS montée.");
  }
}

void unmountFFat() {
  FFat.end();
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  Serial.printf("MSC START/STOP: power: %u, start: %u, eject: %u\n", power_condition, start, load_eject);
  return true;
}

static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == ARDUINO_USB_EVENTS) {
    arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
    switch (event_id) {
      case ARDUINO_USB_STARTED_EVENT: Serial.println("USB PLUGGED"); unmountFFat(); break;
      case ARDUINO_USB_STOPPED_EVENT: Serial.println("USB UNPLUGGED"); mountFFat(); break;
      case ARDUINO_USB_SUSPEND_EVENT: Serial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en); break;
      case ARDUINO_USB_RESUME_EVENT:  Serial.println("USB RESUMED"); break;

      default: break;
    }
  }
}




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
  if(!digitalRead(0) && pressed_5) 
  {
	  ret=5;
	  pressed_5=false;
  }
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
  Serial.println("change_CC");
  Serial.println(param_CC);
  param_midi[param_CC]=sens;  
  if(param_midi[param_CC]>127) param_midi[param_CC]=127;
  if(param_midi[param_CC]<0) param_midi[param_CC]=0;
  
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



i2s_config_t i2s_config =
{
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX ),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    //.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_MSB),
    //.intr_alloc_flags = 0,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = 0,
    .tx_desc_auto_clear=false,
    .fixed_mclk=0
};

i2s_pin_config_t pins =
{
	.mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = 5,
    .ws_io_num =  16,
    .data_out_num = 17,
    .data_in_num = 4
};

void setup_i2s()
{  
  Serial.printf("1");
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
  Serial.printf("2");
  i2s_set_pin(i2s_num, &pins);
  Serial.printf("3");
  //i2s_set_sample_rates(i2s_num, SAMPLE_RATE);
  Serial.printf("4");
  i2s_start(i2s_num);
  Serial.printf("5");
  //i2s_set_clk(i2s_num, SAMPLE_RATE, (i2s_bits_per_sample_t)16, (i2s_channel_t)2);
  Serial.printf("6");
  i2s_stop(i2s_num);
  delay(1000);
  i2s_start(i2s_num);
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

/*bool i2s_write_stereo_samples_buff(int16_t *fl_sample, int16_t *fr_sample)
{
  static union sampleTUNT
  {
      int16_t ch[2];
  } sampleDataU[SAMPLE_BUFFER_SIZE];
  
  //if(fl_sample[0]>0) Serial.println(fl_sample[0]);

  for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
    {
		
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
}*/

/*void modubrainProcess()
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
}*/

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

void Midi_Setup()
{
    Serial2.begin(31250, SERIAL_8N1, RXD2, TXD2);
    pinMode(SYNCRX,INPUT_PULLDOWN);
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
	midiEventPacket_t midi_packet_in = {0, 0, 0, 0};

    if (midi.readPacket(&midi_packet_in)) {
		uint8_t cable_num = MIDI_EP_HEADER_CN_GET(midi_packet_in.header);
		midi_code_index_number_t code_index_num = MIDI_EP_HEADER_CIN_GET(midi_packet_in.header);
		
		switch (code_index_num) {
			case MIDI_CIN_MISC:        Serial.println("This a Miscellaneous event"); break;
			case MIDI_CIN_CABLE_EVENT: Serial.println("This a Cable event"); break;
			case MIDI_CIN_SYSCOM_2BYTE:  // 2 byte system common message e.g MTC, SongSelect
			case MIDI_CIN_SYSCOM_3BYTE:  // 3 byte system common message e.g SPP
			  Serial.println("This a System Common (SysCom) event");
			  break;
			case MIDI_CIN_SYSEX_START:      // SysEx starts or continue
			case MIDI_CIN_SYSEX_END_1BYTE:  // SysEx ends with 1 data, or 1 byte system common message
			case MIDI_CIN_SYSEX_END_2BYTE:  // SysEx ends with 2 data
			case MIDI_CIN_SYSEX_END_3BYTE:  // SysEx ends with 3 data
			  Serial.println("This a system exclusive (SysEx) event");
			  break;
			case MIDI_CIN_NOTE_ON:       
				Serial.printf("This a Note-On event of Note %d with a Velocity of %d\n", midi_packet_in.byte2, midi_packet_in.byte3);
				Midi_NoteOn(cable_num,midi_packet_in.byte2,midi_packet_in.byte3);				
				break;
			case MIDI_CIN_NOTE_OFF:      
				Serial.printf("This a Note-Off event of Note %d with a Velocity of %d\n", midi_packet_in.byte2, midi_packet_in.byte3); 
				Midi_NoteOff(cable_num,midi_packet_in.byte2	);
				break;
			case MIDI_CIN_POLY_KEYPRESS: Serial.printf("This a Poly Aftertouch event for Note %d and Value %d\n", midi_packet_in.byte2, midi_packet_in.byte3); break;
			case MIDI_CIN_CONTROL_CHANGE:
			  Serial.printf(
				"This a Control Change/Continuous Controller (CC) event of Controller %d "
				"with a Value of %d\n",
				midi_packet_in.byte2, midi_packet_in.byte3
			  );
			  Midi_ControlChange(cable_num, midi_packet_in.byte2, midi_packet_in.byte3);
			  break;
			case MIDI_CIN_PROGRAM_CHANGE:   Serial.printf("This a Program Change event with a Value of %d\n", midi_packet_in.byte2); break;
			case MIDI_CIN_CHANNEL_PRESSURE: Serial.printf("This a Channel Pressure event with a Value of %d\n", midi_packet_in.byte2); break;
			case MIDI_CIN_PITCH_BEND_CHANGE:
			  Serial.printf("This a Pitch Bend Change event with a Value of %d\n", ((uint16_t)midi_packet_in.byte2) << 7 | midi_packet_in.byte3);
			  break;
			case MIDI_CIN_1BYTE_DATA: Serial.printf("This an embedded Serial MIDI event byte with Value %X\n", midi_packet_in.byte1); break;
		}
	}
}

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

			//Serial.printf("%02x", incomingByte);
			//Serial.println("");
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
 res=1;
 while(res>0)
 {
  Wire.beginTransmission(WM8960_ADDRESS); // transmit to device lsb=0 => write
  Wire.write(I2C_Data, 2);                // buffer 1 byte lsb of val1
  res = Wire.endTransmission();           // transmit buffer and then stop
  delay(10);
 }

  return res;
}



void hp_spk(void)
{
	int st=0;

  /*int st;

  do
  {
    st = 0;*/
	
	while(codec.begin() == false)
	{
		delay(1);
	}
/*	codec.reset();
	
	codec.enableVREF();
	codec.enableAINL();
	codec.enableAdcLeft();
	codec.enableVMID();
	codec.setVMID(WM8960_VMIDSEL_2X5KOHM);
	codec.disableLB2LO();
	codec.enableLD2LO();
	codec.disableLoopBack();
	codec.enableOUT3MIX(); // Provides VMID as buffer for headphone ground
	
	
	codec.enableDacLeft();
	codec.disableDacMute();
	codec.enableOUT3MIX();
	codec.enableRightHeadphone();
	codec.enableLeftHeadphone();
	codec.enableRightSpeaker();
	codec.enableLeftSpeaker();
	
	codec.enableLMIC();
	
	// Enable output mixers
	codec.enableLOMIX();
	codec.enableROMIX();
	
	codec.connectLMN1();
	
	codec.disableLINMUTE();
	codec.setLINVOLDB(0.00);
	
	codec.setLMICBOOST(WM8960_MIC_BOOST_GAIN_0DB);
	codec.connectLMIC2B();
	
	codec.enablePLL();
	codec.setCLKSEL(WM8960_CLKSEL_PLL);
	codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2);
	codec.setBCLKDIV(4);
	codec.setDCLKDIV(WM8960_DCLKDIV_16);
    //codec.setPLLPRESCALE(false);
	codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
	codec.setPLLN(7);
	codec.setSMD(WM8960_PLL_MODE_FRACTIONAL);
    codec.setPLLK(0x86, 0xC2, 0x26);
	codec.setWL(WM8960_WL_16BIT);
	
	codec.enablePeripheralMode();

    codec.disableDacMute();
	codec.disableDac6dbAttentuation();
	
	codec.enableLeftSpeaker();
	codec.enableRightSpeaker();
	
	codec.setDacLeftDigitalVolume(100);
	codec.setDacRightDigitalVolume(100);
	
	codec.setHeadphoneVolumeDB(0.00);*/
	
	
	Serial.println("Device is connected properly.");
	usleep(10000);

	codec.reset();
	
	codec.enableVREF();
	codec.enableVMID();
    //Power
	codec.enableLMIC();
	codec.enableRMIC();
	
	codec.connectLMN1();
    codec.connectRMN1();
	
	// Disable mutes on PGA inputs (aka INTPUT1)
	codec.disableLINMUTE();
	codec.disableRINMUTE();
	
	// Set pga volumes
	codec.setLINVOLDB(0.00); // Valid options are -17.25dB to +30dB (0.75dB steps)
	codec.setRINVOLDB(0.00); // Valid options are -17.25dB to +30dB (0.75dB steps)

	// Set input boosts to get inputs 1 to the boost mixers
	codec.setLMICBOOST(WM8960_MIC_BOOST_GAIN_29DB);
	codec.setRMICBOOST(WM8960_MIC_BOOST_GAIN_29DB);

	// Connect from MIC inputs (aka pga output) to boost mixers
	codec.connectLMIC2B();
	codec.connectRMIC2B();
	
	codec.enableAINL();
	codec.enableAINR();
	
	codec.disableLB2LO();
	codec.disableRB2RO();
	
	codec.enableLD2LO();
	codec.enableRD2RO();
	
	// For this loopback example, we are going to keep these as low as they go
	codec.setLB2LOVOL(WM8960_OUTPUT_MIXER_GAIN_NEG_21DB); 
	codec.setRB2ROVOL(WM8960_OUTPUT_MIXER_GAIN_NEG_21DB);
	
	codec.enableLOMIX();
	codec.enableROMIX();
	
	
	
	
	/*st+= !codec.setCLKSEL(true);
	st+= !codec.setPLLPRESCALE(false);
	st+= !codec.setPLLN(7);
	st+= !codec.setSMD(true);
	st+= !codec.setPLLK(0x86, 0xC2, 0x26);
	st+= !codec.disableDacMute();
	st+= !codec.disableDac6dbAttentuation();
 	st+= !codec.setWL(0);*/
	
	codec.enablePLL();
	codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
	codec.setSMD(WM8960_PLL_MODE_FRACTIONAL);
	codec.setCLKSEL(WM8960_CLKSEL_PLL);
	codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2);
	codec.setBCLKDIV(4);
	codec.setDCLKDIV(WM8960_DCLKDIV_16);
	codec.setPLLN(7);
	codec.setPLLK(0x86, 0xC2, 0x26); // PLLK=86C226h
	//codec.setADCDIV(0); // Default is 000 (what we need for 44.1KHz)
	//codec.setDACDIV(0); // Default is 000 (what we need for 44.1KHz)
	codec.setWL(WM8960_WL_16BIT);
    codec.setFormat(1);
	codec.enablePeripheralMode();
	
	codec.enableMicBias();
	codec.enableAdcLeft();
	codec.enableAdcRight();
	codec.enableDacLeft();
	codec.enableDacRight();
	codec.disableDacMute();

	codec.disableLoopBack();
	codec.disableDacMute();
	codec.enableHeadphones();
	codec.enableOUT3MIX();
	//st+= !codec.enableRightHeadphone();
	//st+= !codec.enableLeftHeadphone();
	
	//codec.enableRightSpeaker();
	//codec.enableLeftSpeaker();
	
	
	
    //st += ES8960_Write_Reg(20, 0x0F9);
	//st+= !codec.setHeadphoneVolume(100);
	codec.setHeadphoneVolumeDB(0.00);

	/*st+= !codec.setSpeakerVolume(100);
    usleep(500);
    // OUTPUTS 0 XX11 0111 class D amp
    // XX = 00 off , 11 speaker on R and L
    st += ES8960_Write_Reg(49, 0x0FF);
	//st+= !codec.enableLeftSpeaker();
	//st+= !codec.enableRightSpeaker();*/
	

	Serial.print("st : ");
    Serial.println(st);
	
	/*st += ES8960_Write_Reg(15, 0x00);
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
    usleep(10000);*/
	
	Serial.print("st : ");
    Serial.println(st);


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

void load_preset()
{
  Serial.println("load preset");
  String aff = String(savenum);
  aff = "/save" + aff + ".txt";
  Serial.println(aff);
  fs::File file = FFat.open(aff, "r");
  Serial.println(file.name());
  if (!file) {
	  filefound=false;
  }
  else
  {
	  filefound=true;
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

void modubrainInit()
{

  
  
  fatfs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "ffat");
  if (fatfs_partition == NULL) {
    Serial.println("Partition FATFS introuvable !");
    return;
  }
  DISK_SECTOR_COUNT = fatfs_partition->size / DISK_SECTOR_SIZE;

  // Initialiser FFat
  if (!FFat.begin(true, "/ffat", 5 , "ffat")) { // true pour formater si nécessaire
    Serial.println("Échec du montage de la partition FATFS.");
    return;
  }
  Serial.println("Partition FATFS initialisée.");

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

  /*gpio_reset_pin((gpio_num_t)SD_CS);  
  gpio_reset_pin((gpio_num_t)SPI_MOSI);  
  gpio_reset_pin((gpio_num_t)SPI_MISO);  
  gpio_reset_pin((gpio_num_t)SPI_SCK);  */

   
  //SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  Serial.println("0 SD");
  delay(1000);
  
  int  volume = 10;
  
  USB.onEvent(usbEventCallback);
  MSC.vendorID("ESP32");       //max 8 chars
  MSC.productID("USB_MSC");    //max 16 chars
  MSC.productRevision("1.0");  //max 4 chars
  MSC.onStartStop(onStartStop);
  MSC.onRead(onRead);
  MSC.onWrite(onWrite);

  MSC.mediaPresent(true);
  MSC.isWritable(true);  // true if writable, false if read-only

  MSC.begin(DISK_SECTOR_COUNT, DISK_SECTOR_SIZE);
  Serial.println("MSC OK");

  Wire.begin(SDA, SCL);
  Serial.println("wire OK");
  delay(500);

  midi.begin();
  Serial.println("MIDI OK");
  
  USB.begin();
  Serial.println("USB OK");

  hp_spk();
  Serial.println("I2C OK");

  //setVol(3);
  Serial.println("volume OK");
  delay(500);

  setup_i2s();
  Serial.println("I2S OK");
  pinMode(0, INPUT_PULLUP);

  //ES8960_Init2(); //headphone OK, 
  //dacToSpkAndHp();
   
  createSineTable();
  createSawTable();
  createSqTable();

  Synth_Init();
  Serial.println("1 synth init");

  delay(500);
  
  Serial.println("1 search");
  
  Midi_Setup();
  Serial.println("midi setup");
  //MIDI.begin(MIDI_CHANNEL_OMNI);

  buildTables();
  Serial.println("build tables");

  init_running=false;
  
  xTaskCreatePinnedToCore(taskAudio, "TaskAudio", 4096, NULL, 1, NULL, 1); // Cœur 0
  xTaskCreatePinnedToCore(taskOther, "TaskOther", 4096, NULL, 1, NULL, 0);   // Cœur 1
}

void core0_init()
{
  /*if (!FFat.begin()) {
    Serial.println("FFat initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nFFat initialised.");*/

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
    if(testbut==5) 
    {
	  Serial.println("but4");
	  enco_pressed();
      
    }
	if(testbut==4) 
    {
	  Serial.println("but5");
	  enco_released();
      
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