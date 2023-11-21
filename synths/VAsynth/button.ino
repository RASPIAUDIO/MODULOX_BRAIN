bool pressed_1=false;
bool pressed_2=false;
bool pressed_3=false;
bool pressed_4=false;
bool pressed_5=false;

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
