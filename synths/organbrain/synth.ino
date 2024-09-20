void Synth_Init()
{
  oscA.init();
  oscA.update_polyphony(5);
  filter.Init((float)SAMPLE_RATE);
    filter.SetMode(TeeBeeFilter::LP_24);
    filter.SetResonance( 5,true);
    lfo.init();
    lfo_phase.init();
    lfo.setmaincutoff(64);
    lfo_phase.setmaincutoff(64);
    lfo.setvol(8);
    lfo_phase.setvol(8);
    lfo_phase.phase_accu=256;
    lfo.start();
    lfo_phase.start();
    delay_init();
    disto.init();
    disto.set_gain(30);

}

float lfo1amount_prev = 0;

inline void Synth_Process(int16_t *left, int16_t *right)
{

  /* gerenate a noise signal */
  //float noise_signal = ((random(1024) / 512.0f) - 1.0f) * soundNoiseLevel;

  /*
     generator simulation, rotate all wheels
  */
  float out_l, out_r;

  out_l = 0;
  out_r = 0;

  float lfo1amount = lfo.output();
  float lfo2amount = lfo_phase.output();
  if (lfo1amount < 0) lfo1amount = 0;
  if (lfo1amount >= 1.0) lfo1amount = 1.0;
  if (lfo2amount < 0) lfo2amount = 0;
  if (lfo2amount >= 1.0) lfo2amount = 1.0;
  oscA.set_pitch_lfo(lfo1amount);
  if(lfo2amount>lfo1amount_prev+0.01 || lfo2amount<lfo1amount_prev-0.01)
  {
    lfo1amount_prev=lfo2amount;
  }


  oscA.next();

  out_l = oscA.output();
  if(disto_on) out_l = disto.out(out_l);
  out_l=fast_tanh(out_l);
  if(delay_on) out_l+=delay_output(out_l)*delay_mix;
  out_l=fast_tanh(out_l);
  
  out_r=out_l;

  *left = 16383.0 * out_l;
  *right = *left;

}
