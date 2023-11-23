// Declaration of boolean variables to track the state of each button
bool pressed_1=false;
bool pressed_2=false;
bool pressed_3=false;
bool pressed_4=false;
bool pressed_5=false;

// Function to detect which button is pressed
int detect_button()
{
  // Default value indicating no button is pressed
  int ret=-1;
  // Check if button 1 is released
  if(digitalRead(BUT1) && pressed_1) pressed_1=false;
  // Check if button 1 is pressed
  if(!digitalRead(BUT1) && !pressed_1)
  {
    ret=0;
    pressed_1 = true;
    Serial.println("1");
  }
  // Check if button 2 is released
  if(digitalRead(BUT2) && pressed_2) pressed_2=false;
  // Check if button 2 is pressed
  if (!digitalRead(BUT2) && !pressed_2)
  {
    ret = 1;
    pressed_2 = true;
    Serial.println("2");
  }

  // Check if button 3 is released
  if (digitalRead(BUT3) && pressed_3) pressed_3 = false;
  // Check if button 3 is pressed
  if (!digitalRead(BUT3) && !pressed_3)
  {
    ret = 2;
    pressed_3 = true;
    Serial.println("3");
  }

  // Check if button 4 (BUTENCO) is released
  if (digitalRead(BUTENCO) && pressed_4) pressed_4 = false;
  // Check if button 4 (BUTENCO) is pressed
  if (!digitalRead(BUTENCO) && !pressed_4)
  {
    ret = 3;
    pressed_4 = true;
    Serial.println("4");
  }

  // Check if button 5 (presumed connected to pin 0) is released
  if (!digitalRead(0) && pressed_5) pressed_5 = false;
  // Check if button 5 (presumed connected to pin 0) is pressed
  if (digitalRead(0) && !pressed_5)
  {
    ret = 4;
    pressed_5 = true;
    Serial.println("5");
  }

  // Return the index of the pressed button, -1 if none
  return ret;
}
