#ifndef compressorf_h
#define compressorf_h

#define  buf_len 100


class Compressor
{

public:

	/// main compressor parameters. Adjust these to your needs. ////
	int attack_f;  // attack period (how soon the compressor will start attenuating loud signals) given in measurement frame
						// units (see window_ms). Default setting corresponds to 50ms. Max buf_len / 2. Min 4.
	int release_f; // release period (how soon the compressor will soften attenuation after signals have become more silent),
						// given in measurement frame units. Default setting corresponds to 200ms; Max buf_len.
						// Does not have an effect if <= attack_f
	float threshold; // minimum signal amplitude before the compressor will kick in. Each unit corresponds to roughly 5mV
						// peak-to-peak.
	float ratio;  // dampening applied to signals exceeding the threshold. n corresponds to limiting the signal to a level of
						// threshold level plus 1/3 of the level in excess of the threshold (if possible: see duty_min, below)
						// 1(min) = no attenuation; 20(max), essentially limit to threshold, aggressively
	float max_transition_rate; // although the moving averages for attack and release will result in smooth transitions
						// of the compression rate in most regular cases sudden signal spikes can result in abrupt transitions, introducing
						// additional artefacts. This limits the maximum speed of the transition to +/- 11% of current value.

	//// Some further constants that you will probably not have to tweak ////
	int window_ms;  // milliseconds per measurement window. A narrow window will allow finer control over attack and release,
						// but it will also cripple detection of low frequency amplitudes. Probably you don't want to change this.
	int duty_min;  // ceiling value for attenuation (lower values = more attenuation, 0 = off, 255 = no attenuation)
						// beyond a certain value further attenuation is just too coarse grained for good results. Ideally, this
						// value is never reached, but might be for aggressive dampening ratio and low thresholds.
	int duty_warn;  // See above. At attenuation beyond this (i.e. smaller numbers), warning LED will flash.
						// Reaching this point on occasion is quite benign. Reaching this point much of the time means too strong
						// signal, too low threshold setting, or too aggressive inv_ratio.
	int signal_warn;  // A warning LED will flash for signals exceeding this amplitude (5mv per unit, peak-to-peak) as
						// it is probably (almost) too much for the circuit too handle (default value corresponds to about +-750mV
						// in order to stay below the .8V typical 2N7000 body diode forward voltage, as well as below
						// the 1.7V signal swing (centered at 3.3V) that the Arduino can handle).

	//// working variables ////
	float cmin; // minimum amplitude found in current measurement window
	float cmax;    // maximum amplitude found in current measurement window
	float buf[buf_len];         // ring buffer for moving averages / sums
	int pos;              // current buffer position
	float attack_mova;      // moving average (actually sum) of amplitudes over past attack period
	float release_mova;     // moving average (actually sum) of amplitudes over past release period
	int32_t now;          // start time of current loop
	int32_t last;         // time of last loop
	float duty;           // current PWM duty cycle for attenuator switch(es) (0: hard off, 255: no attenuation)
	byte display_hold;
	float invratio;  // inverse of ratio. Saves some floating point divisions
	
	int ind_buffer;


	void init()
	{
		attack_f = 10;  // attack period (how soon the compressor will start attenuating loud signals) given in measurement frame
						// units (see window_ms). Default setting corresponds to 50ms. Max buf_len / 2. Min 4.
		release_f = 40; // release period (how soon the compressor will soften attenuation after signals have become more silent),
							// given in measurement frame units. Default setting corresponds to 200ms; Max buf_len.
							// Does not have an effect if <= attack_f
		threshold = 0.1; // minimum signal amplitude before the compressor will kick in. Each unit corresponds to roughly 5mV
							// peak-to-peak.
		ratio = 3.0;  // dampening applied to signals exceeding the threshold. n corresponds to limiting the signal to a level of
							// threshold level plus 1/3 of the level in excess of the threshold (if possible: see duty_min, below)
							// 1(min) = no attenuation; 20(max), essentially limit to threshold, aggressively
		max_transition_rate = 1.11; // although the moving averages for attack and release will result in smooth transitions
							// of the compression rate in most regular cases sudden signal spikes can result in abrupt transitions, introducing
							// additional artefacts. This limits the maximum speed of the transition to +/- 11% of current value.

		//// Some further constants that you will probably not have to tweak ////
		window_ms = 5;  // milliseconds per measurement window. A narrow window will allow finer control over attack and release,
							// but it will also cripple detection of low frequency amplitudes. Probably you don't want to change this.
		duty_min = 10;  // ceiling value for attenuation (lower values = more attenuation, 0 = off, 255 = no attenuation)
							// beyond a certain value further attenuation is just too coarse grained for good results. Ideally, this
							// value is never reached, but might be for aggressive dampening ratio and low thresholds.
		duty_warn = 2 * duty_min;  // See above. At attenuation beyond this (i.e. smaller numbers), warning LED will flash.
							// Reaching this point on occasion is quite benign. Reaching this point much of the time means too strong
							// signal, too low threshold setting, or too aggressive inv_ratio.
		signal_warn = 300;  // A warning LED will flash for signals exceeding this amplitude (5mv per unit, peak-to-peak) as
							// it is probably (almost) too much for the circuit too handle (default value corresponds to about +-750mV
							// in order to stay below the .8V typical 2N7000 body diode forward voltage, as well as below
							// the 1.7V signal swing (centered at 3.3V) that the Arduino can handle).

		//// working variables ////
		cmin = 1.0; // minimum amplitude found in current measurement window
		cmax = 0.0;    // maximum amplitude found in current measurement window
		buf[buf_len];         // ring buffer for moving averages / sums
		pos = 0;              // current buffer position
		attack_mova = 0;      // moving average (actually sum) of amplitudes over past attack period
		release_mova = 0;     // moving average (actually sum) of amplitudes over past release period
		now = 0;          // start time of current loop
		last = 0;         // time of last loop
		duty = 1.0;           // current PWM duty cycle for attenuator switch(es) (0: hard off, 255: no attenuation)
		display_hold = 0;
		invratio = 1.0 / ratio;  // inverse of ratio. Saves some floating point divisions
		for (int i = 0; i < buf_len; ++i) {  // clear buffer
			buf[i] = 0;
		}
		ind_buffer=0;
	}

	float out(float input)
	{
		if (input < cmin) cmin = input;
        if (input > cmax) cmax = input;
		float ret=input;	
		
		ind_buffer++;
		if(ind_buffer>220)
		{
		  ind_buffer=0;
			 // get amplitude in current meausrement window, and set up next window
		  if (++pos >= buf_len) pos = 0;
		  float val = cmax - cmin;
		  if (val < 0) val = 0;
		  cmax = 0;
		  cmin = 1.0;

		  // update the two moving averages (sums)
		  int old_pos = pos - attack_f;
		  if (old_pos < 0) old_pos += buf_len;
		  attack_mova += val - buf[old_pos];
		  old_pos = pos - release_f;
		  if (old_pos < 0) old_pos += buf_len;
		  release_mova += val - buf[old_pos];

		  // store new value in ring buffer
		  buf[pos] = val;

		  // calculate new attenuation settings
		  // first caculate based on attack period
		  float attack_threshold = threshold * attack_f;
		  float attack_duty = 1.0;
		  if (attack_mova > attack_threshold) {
			const float target_level = attack_threshold * pow (attack_mova / attack_threshold, invratio);
		// Instead of the logrithmic volume calculation above, the faster linear one below seems too yield
		// acceptable results, too. Hoever, the Arduino is fast enough, so we do the "real" thing.
		//   const float target_level = (attack_mova - attack_threshold) / ratio + attack_threshold;
			attack_duty = target_level / attack_mova;
		  }
		  // if the new duty setting is _below_ the current, based on attack period, check release window to see, if
		  // the time has come to release attenuation, yet:
		  if (attack_duty < duty) duty = max (attack_duty, duty / max_transition_rate);
		  else {
			float release_duty = 1.0;
			float release_threshold = threshold * release_f;
			if (release_mova > release_threshold) {
			  float target_level = release_threshold * pow (release_mova / release_threshold, invratio);
			  release_duty = target_level / release_mova;
			} else {
			  release_duty = 1.0;
			}
			if (release_duty >= duty) duty = min (release_duty, duty * max_transition_rate);
		  }

		  
		}		
		return duty*input; // enable the new duty cycle 
	}
	
	void set_ratio(int rat)
	{
		if(rat<1) rat=1;
		ratio=rat;
		invratio = 1.0 / ratio;
	}
	
	void set_type(int ty)
	{
		
	}
	
	void set_attack(int at)
	{
		Serial.println("attack");
		attack_f=at;
	}
	
	void set_release(int re)
	{
		Serial.println("release");
		release_f=re;
	}
	
	/*float findMax(float array[], int size) {
		float max = array[0];
		for(int i = 1; i < size; i++) {
			if(array[i] > max) {
				max = array[i];
			}
		}
		//Serial.println(max);
		return max;
	}*/

};

#endif
