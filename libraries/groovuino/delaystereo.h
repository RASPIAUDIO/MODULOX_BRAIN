#include <Arduino.h>

int16_t *samp_rec_l;
int16_t *samp_rec_r;
int pos_l;
int pos_r;
float feedback;
float deltime_l;
float deltime_r;
bool ping_pong = true;


void delay_init()
{
  pos_l = 0;
  pos_r = 0;
  feedback = 0.4f;
  deltime_l = 0.5f;
  deltime_r = 0.6f;
  ping_pong = true;
  samp_rec_l = (int16_t *)ps_malloc(DELAY_SAMPLES * sizeof(int16_t));
  samp_rec_r = (int16_t *)ps_malloc(DELAY_SAMPLES * sizeof(int16_t));
  for (int i = 0; i < DELAY_SAMPLES; i++)
  {
    samp_rec_l[i] = 0;
    samp_rec_r[i] = 0;
  }
}

void set_feedback(int feed)
{
  feedback = feed / 127.0f;
}

void set_time(int ti)
{
  Serial.print("set_time : ");
  Serial.println(ti);
  float t = ti / 127.0f;
  deltime_l = t/1.5;
  deltime_r = t;
}

// Set delay times independently
void set_time_lr(int ti_l, int ti_r)
{
  deltime_l = ti_l / 127.0f;
  deltime_r = ti_r / 127.0f;
}

void set_time_l(int ti_l)
{
  deltime_l = ti_l / 127.0f;
}

void set_time_r(int ti_r)
{
  deltime_r = ti_r / 127.0f;
}

void set_pingpong(bool state)
{
  ping_pong = state;
}

// Process a stereo sample. in_l and in_r are the dry inputs. The
// processed delayed samples are stored in out_l and out_r.
inline void delay_output(int16_t in_l, int16_t in_r,
                         int16_t *out_l, int16_t *out_r)
{
  int16_t ret_l = samp_rec_l[pos_l];
  int16_t ret_r = samp_rec_r[pos_r];

  if (ping_pong)
  {
    samp_rec_l[pos_l] = in_r + samp_rec_l[pos_l] * feedback;
    samp_rec_r[pos_r] = in_l + samp_rec_r[pos_r] * feedback;
  }
  else
  {
    samp_rec_l[pos_l] = in_l + samp_rec_l[pos_l] * feedback;
    samp_rec_r[pos_r] = in_r + samp_rec_r[pos_r] * feedback;
  }

  pos_l++;
  pos_r++;
  if (pos_l >= DELAY_SAMPLES * deltime_l * deltime_l) pos_l = 0;
  if (pos_r >= DELAY_SAMPLES * deltime_r * deltime_r) pos_r = 0;

  *out_l = ret_l;
  *out_r = ret_r;
}