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
    /*
      // input volume
      st += ES8960_Write_Reg(0, 0x13F);
      usleep(10000);
      st += ES8960_Write_Reg(1, 0x13F);
      usleep(10000);
      // INPUTS
      st += ES8960_Write_Reg(32, 0x138);
      usleep(10000);
      st += ES8960_Write_Reg(33, 0x138);
      usleep(10000);
    */
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
  /*
  // input volume
  st += ES8960_Write_Reg(0, 0x13F);
  usleep(10000);  
  st += ES8960_Write_Reg(1, 0x13F);
  usleep(10000);
  // INPUTS
  st += ES8960_Write_Reg(32, 0x138);
  usleep(10000);  
  st += ES8960_Write_Reg(33, 0x138);
  usleep(10000);  
  */
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
