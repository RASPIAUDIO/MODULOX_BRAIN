#include "arduino.h"
#include "sprites.h"
#include <TFT_eSPI.h>       // Hardware-specific library
#include "levels.h"

#define BUT1 GPIO_NUM_12
#define BUT2 GPIO_NUM_1
#define BUT3 GPIO_NUM_38
#define BUTENCO GPIO_NUM_39
#define BUTLEFT GPIO_NUM_0

#define STAND_RIGHT 0
#define STAND_LEFT 1
#define STAND_UP 2
#define STAND_DOWN 3
#define WALK_RIGHT 4
#define WALK_LEFT 5
#define WALK_UP 6
#define WALK_DOWN 7

bool pressed_1=false;
bool pressed_2=false;
bool pressed_3=false;
bool pressed_4=false;
bool pressed_5=false;

TFT_eSPI tft = TFT_eSPI();  // Invoke custom library
TFT_eSprite buffer1 = TFT_eSprite(&tft);
TFT_eSprite buffer2 = TFT_eSprite(&tft);
TFT_eSprite player = TFT_eSprite(&tft);
TFT_eSprite player2 = TFT_eSprite(&tft);
TFT_eSprite ballsprite = TFT_eSprite(&tft);
TFT_eSprite cornersL = TFT_eSprite(&tft);
TFT_eSprite cornersR = TFT_eSprite(&tft);
TFT_eSprite bonu = TFT_eSprite(&tft);
TFT_eSprite tir = TFT_eSprite(&tft);
TFT_eSprite ennemy1 = TFT_eSprite(&tft);
TFT_eSprite ennemy2 = TFT_eSprite(&tft);
TFT_eSprite ennemy3 = TFT_eSprite(&tft);
TFT_eSprite explosion = TFT_eSprite(&tft);



int level_num=0;

uint32_t tim;
uint32_t timg;
int count=0;
uint32_t frame=0;

bool buf1use=false;
bool buf2use=false;
int bufCdisp=1;

TaskHandle_t Core0TaskHnd;


int player_posy=100;

int ship_posx=40;

const int left_pos=30;

int tile_begin_x=0;
int tile_begin_y=0;

int player_status =0;
int player_frame=1;

int anim_count=0;

float speed=2;
float ball_posx[3] = {22+ship_posx,0,0};
float ball_posy[3] = {221,0,0};
boolean is_glued[3]={true,false,false};
float ball_speedx[3]={1,1,1};
float ball_speedy[3]={-1,-1,-1};
int ball_number=1;

int max_ennemy = 3;
int num_ennemy=0;


float ennemy_posx[]={0,0,0};
float ennemy_posy[]={0,0,0};

float ennemy_speedx[]={0.5,0.5,0.5};
float ennemy_speedy[]={1,1,1};

float bonus_posx=0;
float bonus_posy=0;

int bonus_type=0;
// 0 : slow
// 1 : glue
// 2 : laser
// 3 : large
// 4 : 3 balls
// 5 : exit
// 6 : extra life

int ship_select=1;

int animate_ship_shooter=9;
int ship_status=0;
int lives=5;

int shootx[3]={0,0,0};
int shooty[3]={0,0,0};

bool end_level=false;

int opened_trape=0;
bool opening_trap=false;

int tempo_trappe=0;
int tempo_loose=0;

int ship_add=1;

bool explode=false;
int explode_x=0;
int explode_y=0;
int explode_frame=0;

void reinit()
{
  ship_status=0;
  speed=2;
  ball_number=1;
  ball_posx[0] = 22+ship_posx;
  ball_posy[0] = 221;
  is_glued[0]=true;
  ball_speedx[0]=1;
  ball_speedy[0]=-1;
}

void load_level()
{
  for(int i=0; i<11; i++)
  {
    for(int j=0; j<28; j++)
    {
      level_data[j][i]=levels_data[level_num][j][i];
    }
  }
}

void reinit_game()
{
  lives=5;
  
  
  speed=2;
  ball_number=1;
  ball_posx[0] = 22+ship_posx;
  ball_posy[0] = 221;
  is_glued[0]=true;
  ball_speedx[0]=1;
  ball_speedy[0]=-1;
  level_num=0;
  load_level();
}

void setup(void) {
  pinMode(BUT1, INPUT_PULLDOWN);
  pinMode(BUT2, INPUT_PULLDOWN);
  pinMode(BUT3, INPUT_PULLDOWN);
  pinMode(BUTENCO, INPUT_PULLDOWN);
  
  Serial.begin(115200);
  Serial.println("yo1");
  delay(1000);
  Serial.println("yo2");
  delay(1000);
  Serial.println("yo3");
  delay(1000);
  buffer1.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  buffer1.createSprite(280, 240);  // 8bpp requires 64 * 30 = 1920 bytes
  buffer1.fillSprite(TFT_BLACK); // Fill the Sprite with black
  buffer2.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  buffer2.createSprite(280, 240);  // 8bpp requires 64 * 30 = 1920 bytes
  buffer2.fillSprite(TFT_BLACK); // Fill the Sprite with black
  player.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  player.createSprite(48, 8);  // 8bpp requires 64 * 30 = 1920 bytes
  player.fillSprite(0x0007); // Fill the Sprite with black
  player2.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  player2.createSprite(48, 8);  // 8bpp requires 64 * 30 = 1920 bytes
  player2.fillSprite(0x0007); // Fill the Sprite with black 
  ballsprite.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  ballsprite.createSprite(5, 4);  // 8bpp requires 64 * 30 = 1920 bytes
  ballsprite.fillSprite(TFT_BLACK); // Fill the Sprite with black
  cornersL.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  cornersL.createSprite(12, 16);  // 8bpp requires 64 * 30 = 1920 bytes
  cornersL.fillSprite(TFT_BLACK); // Fill the Sprite with black
  cornersR.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  cornersR.createSprite(12, 16);  // 8bpp requires 64 * 30 = 1920 bytes
  cornersR.fillSprite(TFT_BLACK); // Fill the Sprite with black
  bonu.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  bonu.createSprite(16, 8);  // 8bpp requires 64 * 30 = 1920 bytes
  bonu.fillSprite(TFT_BLACK); // Fill the Sprite with black
  tir.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  tir.createSprite(3, 7);  // 8bpp requires 64 * 30 = 1920 bytes
  tir.fillSprite(TFT_BLACK); // Fill the Sprite with black
  ennemy1.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  ennemy1.createSprite(16, 16);  // 8bpp requires 64 * 30 = 1920 bytes
  ennemy1.fillSprite(TFT_BLACK); // Fill the Sprite with black
  ennemy2.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  ennemy2.createSprite(16, 16);  // 8bpp requires 64 * 30 = 1920 bytes
  ennemy2.fillSprite(TFT_BLACK); // Fill the Sprite with black
  ennemy3.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  ennemy3.createSprite(16, 16);  // 8bpp requires 64 * 30 = 1920 bytes
  ennemy3.fillSprite(TFT_BLACK); // Fill the Sprite with black
  explosion.setColorDepth(16);      // Create an 8bpp Sprite of 60x30 pixels
  explosion.createSprite(16, 16);  // 8bpp requires 64 * 30 = 1920 bytes
  explosion.fillSprite(TFT_BLACK); // Fill the Sprite with black


  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  player.pushImage(0, 0, 32, 8, ship[1]);
  player2.pushImage(0, 0, 32, 8, ship[1]);
  if(!end_level) ballsprite.pushImage(0, 0, 5, 4, ball);
  cornersL.pushImage(0, 0, 12, 16, corner[0]);
  cornersR.pushImage(0, 0, 12, 16, corner[1]);
  bonu.pushImage(0, 0, 16, 8, bonus[0]);
  tir.pushImage(0, 0, 3, 7, tirs);
  ennemy1.pushImage(0, 0, 16, 16, ennemies[0]);
  ennemy2.pushImage(0, 0, 16, 16, ennemies[0]);
  ennemy3.pushImage(0, 0, 16, 16, ennemies[0]);
  //player.pushToSprite(&buffer1, 100, 100,0x0404);
  xTaskCreatePinnedToCore(Core0Task, "CoreTask0", 40000, NULL, 999, &Core0TaskHnd, 0);
  
}

int detect_button()
{
  int ret=-1;
  if(!digitalRead(BUT1) && pressed_1) pressed_1=false;
  if(digitalRead(BUT1) && !pressed_1)
  {
    ret=0;
    pressed_1 = true;
    Serial.println("1");
  }
  if(!digitalRead(BUT2) && !pressed_2) pressed_2=true;
  if(digitalRead(BUT2) && pressed_2)
  {
    ret=1;
    pressed_2 = false;
    //Serial.println("2");
  }
  if(!digitalRead(BUT3) && pressed_3) 
  {
    //Serial.println("rel 3");
    ret=6;
    pressed_3=false;
  }
  if(digitalRead(BUT3) && !pressed_3)
  {
    ret=2;
    pressed_3 = true;
    //Serial.println("3");
  }
  if(!digitalRead(BUTENCO) && !pressed_4) pressed_4=true;
  if(digitalRead(BUTENCO) && pressed_4)
  {
    ret=3;
    pressed_4 = false;
    //Serial.println("4");
  }
  if(!digitalRead(0) && pressed_5) pressed_5=false;
  if(digitalRead(0) && !pressed_5)
  {
    ret=4;
    pressed_5 = true;
    //Serial.println("5");
  }
  return ret;
}

void Core0Task(void *parameter)
{
  while(1)
  {
    if(bufCdisp==2)
    {
      buf1use=true;
      for(int i=0; i<6; i++)
      {
        for(int j=0; j<8; j++)
        {
          buffer1.pushImage(left_pos+i*32, j*32, 32, 32, back[4]);
          
        }
      }
      for(int i=0; i<6; i++)
      {
        buffer1.pushImage(left_pos,210-40*i,8,40,pipes[0]);
        buffer1.pushImage(left_pos+184,210-40*i,8,40,pipes[0]);
      }
      if(end_level) {
        buffer1.pushImage(left_pos,200,8,40,pipes[(anim_count%3)+1]);
        buffer1.pushImage(left_pos+184,200,8,40,pipes[(anim_count%3)+1]);
      }

      cornersL.pushToSprite(&buffer1, left_pos, 0, 0x0007);
      cornersR.pushToSprite(&buffer1, left_pos+180, 0, 0x0007);

      /*for(int i=0; i<5; i++)
      {
        buffer1.pushImage(left_pos+12+i*32,0,32,8,trappe[0]);
      }*/
      buffer1.pushImage(left_pos+12,0,16,8,pipesolo);
      buffer1.pushImage(left_pos+28,0,32,8,trappe[opened_trape]);
      for(int i=0; i<5; i++)
      {
        buffer1.pushImage(left_pos+60+16*i,0,16,8,pipesolo);
      }
      buffer1.pushImage(left_pos+172,0,16,8,pipesolo);
      buffer1.pushImage(left_pos+140,0,32,8,trappe[0]);
      
      if(ennemy_posy[0]>0) ennemy1.pushToSprite(&buffer1, ennemy_posx[0], ennemy_posy[0], 0x1ce7);
      if(ennemy_posy[1]>0) ennemy2.pushToSprite(&buffer1, ennemy_posx[1], ennemy_posy[1], 0x1ce7);
      if(ennemy_posy[2]>0) ennemy3.pushToSprite(&buffer1, ennemy_posx[2], ennemy_posy[2], 0x1ce7);
      if(explode) explosion.pushToSprite(&buffer1, explode_x,explode_y,0x1ce7);

      for(int j = 0; j<28; j++)
      {
        for(int i = 0; i<11; i++)
        {
          if(level_data[j][i]>0) {
            buffer1.pushImage(i*16+left_pos+9,j*8+10,16,8,bricks[level_data[j][i]-1]);
          }
        }
      }
      for(int i=0; i<lives; i++)
      {
        buffer1.pushImage(left_pos+4+i*16,235,15,5,life);
      }
      if(!end_level) for(int i=0; i<ball_number; i++) ballsprite.pushToSprite(&buffer1, ball_posx[i], ball_posy[i], 0x0007);
      player.pushToSprite(&buffer1, ship_posx, 225,0x0007);
      if(bonus_posy>0) bonu.pushToSprite(&buffer1, bonus_posx, bonus_posy,0x1ce7);
      for(int i=0; i<3; i++)
      {
        if(shooty[i]>0) {
          tir.pushToSprite(&buffer1, shootx[i], shooty[i],0x0007);
          tir.pushToSprite(&buffer1, shootx[i]+10, shooty[i],0x0007);
        }
      }
      
      
      //Serial.println("draw1");
      delay(30);
      buf1use=false;
    }
    if(bufCdisp==1)
    {
      buf2use=true;
      for(int i=0; i<6; i++)
      {
        for(int j=0; j<8; j++)
        {
          buffer2.pushImage(left_pos+i*32, j*32, 32, 32, back[4]);
          
        }
      }
      for(int i=0; i<6; i++)
      {
        buffer2.pushImage(left_pos,210-40*i,8,40,pipes[0]);
        buffer2.pushImage(left_pos+184,210-40*i,8,40,pipes[0]);
      }
      if(end_level) {
        buffer2.pushImage(left_pos,200,8,40,pipes[(anim_count%3)+1]);
        buffer2.pushImage(left_pos+184,200,8,40,pipes[(anim_count%3)+1]);
      }
      cornersL.pushToSprite(&buffer2, left_pos, 0, 0x0007);
      cornersR.pushToSprite(&buffer2, left_pos+180, 0, 0x0007);

      buffer2.pushImage(left_pos+12,0,16,8,pipesolo);
      buffer2.pushImage(left_pos+28,0,32,8,trappe[opened_trape]);
      for(int i=0; i<5; i++)
      {
        buffer2.pushImage(left_pos+60+16*i,0,16,8,pipesolo);
      }
      buffer2.pushImage(left_pos+172,0,16,8,pipesolo);
      buffer2.pushImage(left_pos+140,0,32,8,trappe[0]);

      if(ennemy_posy[0]>0) ennemy1.pushToSprite(&buffer2, ennemy_posx[0], ennemy_posy[0], 0x1ce7);
      if(ennemy_posy[1]>0) ennemy2.pushToSprite(&buffer2, ennemy_posx[1], ennemy_posy[1], 0x1ce7);
      if(ennemy_posy[2]>0) ennemy3.pushToSprite(&buffer2, ennemy_posx[2], ennemy_posy[2], 0x1ce7);
      if(explode) explosion.pushToSprite(&buffer2, explode_x,explode_y,0x1ce7);
      
      for(int j = 0; j<28; j++)
      {
        for(int i = 0; i<11; i++)
        {
          if(level_data[j][i]>0) {
            buffer2.pushImage(i*16+left_pos+9,j*8+10,16,8,bricks[level_data[j][i]-1]);
          }
        }
      }
      for(int i=0; i<lives; i++)
      {
        buffer2.pushImage(left_pos+4+i*16,235,15,5,life);
      }
      if(!end_level) for(int i=0; i<ball_number; i++) ballsprite.pushToSprite(&buffer2, ball_posx[i], ball_posy[i], 0x0007);
      player2.pushToSprite(&buffer2, ship_posx, 225,0x0007);
      if(bonus_posy>0) bonu.pushToSprite(&buffer2, bonus_posx, bonus_posy,0x1ce7);
      //Serial.println("draw2");
      for(int i=0; i<3; i++)
      {
        if(shooty[i]>0) {
          tir.pushToSprite(&buffer2, shootx[i], shooty[i],0x0007);
          tir.pushToSprite(&buffer2, shootx[i]+10, shooty[i],0x0007);
        }
      }
      delay(30);
      buf2use=false;
    }
  }
}

int tot=0;

void but()
{
  int det=detect_button();
}

void action()
{
  anim_count++;
  if(ship_status==3) player_frame=3 + (anim_count%5)*5;
  else player_frame=ship_add + (anim_count%5)*5;
  
  if(ship_status==2) 
  {
    player.pushImage(0, 0, 48, 8, bigship[anim_count%5]);
    player2.pushImage(0, 0, 48, 8, bigship[anim_count%5]);
  }
  else
  {
    player.pushImage(0, 0, 48, 8, fond);
    player2.pushImage(0, 0, 48, 8, fond);
    player.pushImage(8, 0, 32, 8, ship[player_frame]);
    player2.pushImage(8, 0, 32, 8, ship[player_frame]);
  }

  ennemy1.pushImage(0, 0, 16, 16, ennemies[anim_count%8]);
  ennemy2.pushImage(0, 0, 16, 16, ennemies[anim_count%8]);
  ennemy3.pushImage(0, 0, 16, 16, ennemies[anim_count%8]);
  
  
  if(bonus_posy>0) bonu.pushImage(0, 0, 16, 8, bonus[bonus_type*8+(anim_count%8)]);
  //Serial.println(player_frame);

  if(opened_trape>0 && opened_trape<5 && !opening_trap) opened_trape--;
  if(opened_trape>0 && opened_trape<5 && opening_trap) opened_trape++;
  if(opened_trape>=5) {
    if(tempo_trappe==0)
    {
      ennemy_posx[num_ennemy]=left_pos+30;
      ennemy_posy[num_ennemy]=8;
      num_ennemy++;
    }
    
    if(tempo_trappe<4) tempo_trappe++;
    else {
      tempo_trappe=0;
      opening_trap=false; 
      opened_trape--;
    }
    
  }
  //Serial.println(opened_trape);

  //if(opened_trape>0 && opening_trap) opened_trape++;

  if(num_ennemy<max_ennemy && opened_trape==0 && !opening_trap) 
  {
    opened_trape=1;
    opening_trap=true;
  }

  if(!end_level)
  {
    end_level=true;
    for(int i=0; i<20; i++)
    {
      for(int j=0; j<11; j++)
      {
        if(level_data[i][j]>0)
        {
          i=30;
          j=20;
          end_level=false;
        }
      }
    }
  }
  if((anim_count%25)==0) {
    /*Serial.println("test");
    Serial.println(ship_status);
    Serial.println(speed);*/
    for(int i=0; i<ball_number; i++)
    {
      speed*=1.1;
      /*ball_speedx[i]*=1.1; 
      ball_speedy[i]*1.1;*/
    }   
  } 
}

void loop() {
  // put your main code here, to run repeatedly:
  //tim=millis();
//  Serial.print("*");
//  Serial.println(bufCdisp);
  
  bool shipstopped=false;
  if(!digitalRead(BUT1))
    {
      ship_posx+=3;
      if(ship_posx>left_pos+148 && ship_status!=2) {ship_posx=left_pos+148; shipstopped=true;}
      if(ship_posx>left_pos+140 && ship_status==2) {ship_posx=left_pos+140; shipstopped=true;}
      for(int i=0; i<ball_number; i++)
      {
        if(is_glued[i] && !shipstopped) ball_posx[i]+=3;
      }
      if(shipstopped && end_level) 
      {
        level_num++;
        load_level();
        end_level=false;
        reinit();
      }
    }

    if (!digitalRead(BUT3))
    {
      ship_posx-=3;
      if(ship_posx<left_pos && ship_status!=2) {ship_posx=left_pos; shipstopped=true;}
      if(ship_posx<left_pos+8 && ship_status==2) {ship_posx=left_pos; shipstopped=true;}
      for(int i=0; i<ball_number; i++)
      {
        if(is_glued[i] && ship_posx!=28 && !shipstopped) ball_posx[i]-=3;
      }
    }
    for(int i=0; i<3; i++)
    {
      if(shooty[i]>0) {
        tir.pushToSprite(&buffer2, shootx[i], shooty[i],0x0007);
        tir.pushToSprite(&buffer2, shootx[i]+10, shooty[i],0x0007);
        shooty[i]-=4;
      }
    }
    
    int brick_findx;
    int brick_findy;
    int ball_numero=0;
    for(int i=0; i<3; i++)
    {
      brick_findx=int((shootx[i]-left_pos-9)/16);
      if(brick_findx>11) brick_findx=11;
      if(brick_findx<0) brick_findx=0;
      int brick_findy=int((shooty[i]-10)/8);
      if(brick_findy>28) brick_findy=28;
      if(brick_findy<0) brick_findy=0;
     
      if(level_data[brick_findy][brick_findx]>0) {
        if(level_data[brick_findy][brick_findx]>8 && level_data[brick_findy][brick_findx]!=11) {
          if(level_data[brick_findy][brick_findx]<11) level_data[brick_findy][brick_findx]++;
          //is_animated[brick_findy][brick_findx]=1;
        }
        else level_data[brick_findy][brick_findx]=0;
        if(random(10)>3 && bonus_posy==0 && level_data[brick_findy][brick_findx]<9) {
        //if(bonus_posy==0) {
          bonus_posx=shootx[i]-8;
          bonus_posy=shooty[i];
          bonus_type=(int)random(7.99);
          bonu.pushImage(0, 0, 16, 8, bonus[bonus_type*8]);
          
        }
        shooty[i]=0;
      }
    }
    for(int i=0; i<ball_number; i++)
    {
      brick_findx=int((ball_posx[i]-left_pos-9)/16);
      if(brick_findx>11) brick_findx=11;
      if(brick_findx<0) brick_findx=0;
      int brick_findy=int((ball_posy[i]-10)/8);
      if(brick_findy>28) brick_findy=28;
      if(brick_findy<0) brick_findy=0;
     
      if(level_data[brick_findy][brick_findx]>0) {
        if(level_data[brick_findy][brick_findx]>8 && level_data[brick_findy][brick_findx]!=11) {
          if(level_data[brick_findy][brick_findx]<11) level_data[brick_findy][brick_findx]++;
          //is_animated[brick_findy][brick_findx]=1;
        }
        else level_data[brick_findy][brick_findx]=0;
        if(random(10)>4 && bonus_posy==0 && level_data[brick_findy][brick_findx]<9) {
        //if(bonus_posy==0) {
          bonus_posx=ball_posx[i]-8;
          bonus_posy=ball_posy[i];
          bonus_type=(int)random(7.95);
          bonu.pushImage(0, 0, 16, 8, bonus[bonus_type*8]);
          
        }
        ball_speedy[i]=-ball_speedy[i];
      }
    }

    if(bonus_posy>0) bonus_posy+=3;
    
    if(bonus_type==2 && bonus_posy>224 && bonus_posx>ship_posx-16 && bonus_posx<=46+ship_posx) {ship_status=3; bonus_posy=0;}
    if(bonus_type==0 && bonus_posy>224 && bonus_posx>ship_posx-16 && bonus_posx<=46+ship_posx) {
        speed*=0.8; 
        bonus_posy=0;    
    }
    if(bonus_type==1 && bonus_posy>224 && bonus_posx>ship_posx-16 && bonus_posx<=46+ship_posx) {ship_status=1; bonus_posy=0;}
    if(bonus_type==3 && bonus_posy>224 && bonus_posx>ship_posx-16 && bonus_posx<=46+ship_posx) {ship_status=2; bonus_posy=0;}
    if(bonus_type==4 && bonus_posy>224 && bonus_posx>ship_posx-16 && bonus_posx<=46+ship_posx) {
      ship_status=0;
      ball_number=3; 
      bonus_posy=0;
      ball_posx[1]=ball_posx[0];
      ball_posy[1]=ball_posy[0];
      ball_speedx[1]=ball_speedx[0]+1.5;
      ball_speedy[1]=ball_speedy[0]-1.5;
      ball_posx[2]=ball_posx[0];
      ball_posy[2]=ball_posy[0];
      ball_speedx[2]=ball_speedx[0]-1.5;
      ball_speedy[2]=ball_speedy[0]+1.5;
    }
    if(bonus_type==5 && bonus_posy>224 && bonus_posx>ship_posx-16 && bonus_posx<=46+ship_posx) {end_level=true; bonus_posy=0;}
    if(bonus_type==6 && bonus_posy>224 && bonus_posx>ship_posx-16 && bonus_posx<=46+ship_posx) {lives++; bonus_posy=0;}
    if(bonus_posy>230) bonus_posy=0;
    
    if(detect_button()==1) 
    {
      for(int i=0; i<ball_number; i++)
      {
        if(is_glued[i]) 
        {
          ball_speedy[i]=-1;
          ball_speedx[i]=1;
          is_glued[i]=false;
        }
      }
      if(ship_status==3) 
      {
        for(int i=0; i<3; i++)
        {
          if(shooty[i]<=0)
          {
            shootx[i]=ship_posx+10;
            shooty[i]=217;
            i=10;
          }       
        }
      }
    }
    for(int i=0; i<num_ennemy; i++)
    {
      ennemy_posx[i]+=ennemy_speedx[i];
      ennemy_posy[i]+=ennemy_speedy[i];
      if(ennemy_posx[i]>left_pos+166) ennemy_speedx[i]=-ennemy_speedx[i];
      if(ennemy_posy[i]<8) {ennemy_speedy[i]=-ennemy_speedy[i]; ennemy_posy[i]=9;}
      if(ennemy_posx[i]<left_pos+8) ennemy_speedx[i]=-ennemy_speedx[i];
      if(ennemy_posy[i]>160) ennemy_speedy[i]=-ennemy_speedy[i];

      if(ennemy_speedx[i]<0) brick_findx=int((ennemy_posx[i]-left_pos-9)/16);
      else brick_findx=int((ennemy_posx[i]-left_pos+8)/16);
      if(brick_findx>11) brick_findx=11;
      if(brick_findx<0) brick_findx=0;
      int brick_findy;
      if(ennemy_speedy[i]<0) brick_findy=int((ennemy_posy[i]-10)/8);
      else brick_findy=int((ennemy_posy[i]+6)/8);
      if(brick_findy>28) brick_findy=28;
      if(brick_findy<0) brick_findy=0;
     
      if(level_data[brick_findy][brick_findx]>0) {
        ennemy_speedy[i]=-ennemy_speedy[i];
      }
      for(int j=0; j<ball_number; j++)
      {
        if(ball_posx[j]>ennemy_posx[i]-4 && ball_posx[j]<ennemy_posx[i]+16 && ball_posy[j]>ennemy_posy[i]-4 && ball_posy[j]<ennemy_posy[i]+16)
        {
          explode=true;
          explode_x=ennemy_posx[i];
          explode_y=ennemy_posy[i];
          if(i==1 || i==0)
          {
            ennemy_posx[i]=ennemy_posx[2];
            ennemy_posy[i]=ennemy_posy[2];
          }
          ennemy_posy[2]=0;
          num_ennemy--;
          ball_speedy[j]=-ball_speedy[j];
        }
      }
      for(int j=0; j<3; j++)
      {
        if(shootx[j]>ennemy_posx[i]-4 && shootx[j]<ennemy_posx[i]+16 && shooty[j]>ennemy_posy[i]-4 && shooty[j]<ennemy_posy[i]+16)
        {
          explode=true;
          explode_x=ennemy_posx[i];
          explode_y=ennemy_posy[i];
          if(i==1 || i==0)
          {
            ennemy_posx[i]=ennemy_posx[2];
            ennemy_posy[i]=ennemy_posy[2];
          }
          ennemy_posy[2]=0;
          num_ennemy--;
          ball_speedy[j]=-ball_speedy[j];
        }
      }
    }

    if(explode) {
      explosion.pushImage(0,0,16,16,ennemies[explode_frame+64]);
      explode_frame++;
      if(explode_frame>7) {explode=false; explode_frame=0;}
    }

    if(ball_number==0) 
    {
      if((frame%5)==0)
      {
        if(tempo_loose<10) {tempo_loose++; ship_add=4;}
        else
        {
          lives--; 
          tempo_loose=0;
          ship_add=1;
          ship_status=0;
          if(lives>0) reinit();
          else reinit_game();
        }
      }
    }
    
    if(!end_level)
    {
      for(int i=0; i<ball_number; i++)
      {
        if(!is_glued[i])
        {
          ball_posx[i]+=ball_speedx[i]*speed;
          ball_posy[i]+=ball_speedy[i]*speed;
          if(ball_posx[i]>left_pos+176) ball_speedx[i]=-ball_speedx[i];
          if(ball_posy[i]<8) ball_speedy[i]=-ball_speedy[i];
          if(ball_posx[i]<left_pos+8) ball_speedx[i]=-ball_speedx[i];
          if(ship_status==1 && ball_posy[i]>221 && ball_posx[i]>5+ship_posx && ball_posx[i]<=46+ship_posx) {
            Serial.println("yo");
            Serial.println(is_glued[i]);
            is_glued[i]=true; 
            ball_posy[i]=221; 
            ball_speedy[i]=-ball_speedy[i];
          }
          else
          {
            if(ship_status==2)
            {
              if(ball_posy[i]>221 && ball_posx[i]>ship_posx-5 && ball_posx[i]<=8+ship_posx) {ball_speedy[i]=-ball_speedy[i]; ball_speedx[i]-=0.4;}
              if(ball_posy[i]>221 && ball_posx[i]>8+ship_posx && ball_posx[i]<=23+ship_posx) {ball_speedy[i]=-ball_speedy[i]; ball_speedx[i]-=0.2;}
              if(ball_posy[i]>221 && ball_posx[i]>23+ship_posx && ball_posx[i]<=29+ship_posx) {ball_speedy[i]=-ball_speedy[i]; }
              if(ball_posy[i]>221 && ball_posx[i]>29+ship_posx && ball_posx[i]<=34+ship_posx) {ball_speedy[i]=-ball_speedy[i]; ball_speedx[i]+=0.2;}
              if(ball_posy[i]>221 && ball_posx[i]>34+ship_posx && ball_posx[i]<=46+ship_posx) {ball_speedy[i]=-ball_speedy[i]; ball_speedx[i]+=0.4;}
            }
            else
            {
              if(ball_posy[i]>221 && ball_posx[i]>3+ship_posx && ball_posx[i]<=8+ship_posx) {ball_speedy[i]=-ball_speedy[i]; ball_speedx[i]-=0.4;}
              if(ball_posy[i]>221 && ball_posx[i]>8+ship_posx && ball_posx[i]<=16+ship_posx) {ball_speedy[i]=-ball_speedy[i]; ball_speedx[i]-=0.2;}
              if(ball_posy[i]>221 && ball_posx[i]>16+ship_posx && ball_posx[i]<=24+ship_posx) {ball_speedy[i]=-ball_speedy[i]; }
              if(ball_posy[i]>221 && ball_posx[i]>24+ship_posx && ball_posx[i]<=32+ship_posx) {ball_speedy[i]=-ball_speedy[i]; ball_speedx[i]+=0.2;}
              if(ball_posy[i]>221 && ball_posx[i]>32+ship_posx && ball_posx[i]<=40+ship_posx) {ball_speedy[i]=-ball_speedy[i]; ball_speedx[i]+=0.4;}
            }
            
          }

          
          
          if(ball_posy[i]>226) {
            if(ball_number==1) 
            {
              ball_number=0;
              anim_count=0;    
            }
            if(ball_number==2) 
            {
              ball_number=1;
              if(i==0)
              {
                ball_posx[i]=ball_posx[1]-ball_speedx[1];
                ball_posy[i]=ball_posy[1]-ball_speedy[1];
                ball_speedx[i]=ball_speedx[1];
                ball_speedy[i]=ball_speedy[1];
              }
            }
            if(ball_number==3) 
            {
              ball_number=2;
              if(i==1 ||i==0)
              {
                ball_posx[i]=ball_posx[2]-ball_speedx[2];
                ball_posy[i]=ball_posy[2]-ball_speedy[2];
                ball_speedx[i]=ball_speedx[2];
                ball_speedy[i]=ball_speedy[2];
              }
            }         
          }
        }
      }
    }

    

  if(bufCdisp==1) {
    
    //Serial.println("wait buffer1");
    while(buf1use){but();}
    //Serial.println("start buffer1");
    buffer1.pushSprite(0, 0); 
    bufCdisp=2;
  }
  else {
    //Serial.println("wait buffer2");
    while(buf2use){but();}
    //Serial.println("start buffer2");
    buffer2.pushSprite(0, 0); 
    bufCdisp=1;
  }
  
  //top.pushSprite(28, 0, TFT_GREEN);
  //timg=millis()-tim;
  //count++;
  //tot+=timg;
  
  /*if(count>50)
  {
    count=0;
    Serial.println(tot/50);
    tot=0;
  }*/
  if((frame%10)==0) action();
  frame++;
}
