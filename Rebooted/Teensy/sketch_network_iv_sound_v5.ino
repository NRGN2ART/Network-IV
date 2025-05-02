
/* Network IV - emulation - sound processor - reasonator demo */
/* Sketch for the Teensy 4.0 https://www.pjrc.com/store/teensy40.html */
/* Andrew Seawright */

#include <math.h>
#include <ctype.h>

int debug = 0;

/***********/
/*** DAC ***/
/***********/

/* PT8211 DAC - https://www.pjrc.com/store/pt8211_kit.html */
/* https://www.pjrc.com/store/pt8211.pdf */

#define PIN_DAC_BCK 21
#define PIN_DAC_DIN  7
#define PIN_DAC_WS  20
#define BCK_DELAY 50

/* holds left and right current audio samples - 16 bit signed (two's complement) */
short audio_L;
short audio_R;

#define DAC_MAX 32767
#define DAC_MIN -32767

#define DAC_CLIP(s) ((short) (s <= (double) DAC_MAX) ? (s >= (double) DAC_MIN ? s : DAC_MIN  ) : DAC_MAX)

/***********/
/* Buttons */
/***********/

/* Button matrix column sense lines - Teensy left side */
#define BUTTON_IN_0 0
#define BUTTON_IN_1 1
#define BUTTON_IN_2 2
#define BUTTON_IN_3 3
#define BUTTON_IN_4 4
#define BUTTON_IN_5 5
#define BUTTON_IN_6 6
/* skip 7 (7 is DAC DIN)*/
#define BUTTON_IN_7 8 

/* Button matrix scan row select (open drain) - Teensy right side  */
#define BUTTON_OUT_0 23
#define BUTTON_OUT_1 22
/* skip 21 DAC BCK */
/* skip 20 DAC WS */
#define BUTTON_OUT_2 19
#define BUTTON_OUT_3 18
#define BUTTON_OUT_4 17
#define BUTTON_OUT_5 16
#define BUTTON_OUT_6 15
#define BUTTON_OUT_7 14

/* read row*/
int buttons_in[8] = {  
  BUTTON_IN_0,
  BUTTON_IN_1,
  BUTTON_IN_2,
  BUTTON_IN_3,
  BUTTON_IN_4,
  BUTTON_IN_5,
  BUTTON_IN_6,
  BUTTON_IN_7,
};

/*select row*/
/* onecold */
int buttons_out[8] = {  
  BUTTON_OUT_0,
  BUTTON_OUT_1,
  BUTTON_OUT_2,
  BUTTON_OUT_3,
  BUTTON_OUT_4,
  BUTTON_OUT_5,
  BUTTON_OUT_6,
  BUTTON_OUT_7,
};

int button_state[8] = { 0,0,0,0,0,0,0,0};
int button_index = 0;

/****************/
/* time related */
/****************/

#define DAC_SAMPLE_INTERVAL 50 /*microseconds */
double t = 0.0; /* current time in seconds */
double time_incr = (((double)(DAC_SAMPLE_INTERVAL))/1000000.0);
unsigned long long num_samples = 0;
unsigned long long loop_count = 0;

/* bit mask array from MSB to LSB as index from 0 to 15 */
int bit_mask[16] = {0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100, 0x0080, 0x0040, 0x0020, 0x0010, 0x0008, 0x0004, 0x0002, 0x0001};

/* This routine sends the current audio values audio_L and audio_R to the DAC */
/* called by the interval timer at the specified output rate */
/* also updates current time "t" */

void
output_audio_sample()
{
  int i;
  short L = audio_L;
  short R = audio_R;

  /* Output R channel sample */
  for (i=0; i<16; ++i) {
    digitalWrite(PIN_DAC_BCK, 0);
    digitalWrite(PIN_DAC_DIN, R & bit_mask[i] ? 1 : 0);
    delayNanoseconds(BCK_DELAY);
    digitalWrite(PIN_DAC_BCK, 1);
    delayNanoseconds(BCK_DELAY);
  }
  digitalWrite(PIN_DAC_WS, 1); /* Word select change indicates last bit and channel switch */
  /* Output L channel sample */
  for (i=0; i<16; ++i) {
    digitalWrite(PIN_DAC_BCK, 0);
    digitalWrite(PIN_DAC_DIN, L & bit_mask[i] ? 1 : 0);
    delayNanoseconds(BCK_DELAY);
    digitalWrite(PIN_DAC_BCK, 1);
    delayNanoseconds(BCK_DELAY);
  }
  digitalWrite(PIN_DAC_WS, 0); /* Word select change indicates last bit and channel switch */
  t += time_incr;
  ++num_samples;
}

IntervalTimer myTimer;

/* serial command processing buffer */

#define COMMAND_BUFFER_SIZE 1024
char command_buffer[COMMAND_BUFFER_SIZE];
int command_buffer_index = 0;

/********************/
/* sound parameters */
/********************/


double A = 12000.0; /* ouput level to fit to DAC range on conversion to 16 bits */

#define TWOPI (2.0*M_PI)

#define SINE_WAVE_TABLE_SIZE 1024

double sine_wave_table[SINE_WAVE_TABLE_SIZE];

#define CLIP_V 0.5
#define CLIP(s,V) ((s>V)?(V):((s<-V)?(-V):(s)))


#define NUM_RESONATORS 32
#define RESON_THRESH 4.0
#define RESON_COMPUTE_THRESH 8
#define RESON_COMPUTE_BINS 8

typedef struct resonator_data_s {
  int enabled;
  double trigger_t;
  double f1;
  double f2;
  double f3;
  double T_decay;
  int clip;
  double signal;
  double amp;
  double phase;
} resonator_data_t;

#define PITCH_C4       261.63
#define PITCH_C4_SHARP 277.18
#define PITCH_D4       293.66
#define PITCH_D4_SHARP 311.13
#define PITCH_E4       329.63
#define PITCH_F4       349.23
#define PITCH_F4_SHARP 369.99
#define PITCH_G4       392.00
#define PITCH_G4_SHARP 415.30
#define PITCH_A4       440.00
#define PITCH_A4_SHARP 466.16
#define PITCH_B4       493.88


#define INTERVAL1 (4.0/3.0)
#define INTERVAL2 (16.0/9.0)

resonator_data_t resonator_data[NUM_RESONATORS] = {

 /* bank 1*/
 { 0, 0.0, PITCH_B4/4.0, 0.0, 0.0, 0.10, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_C4_SHARP/2.0, 0.0, 0.0, 0.18, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_C4/2.0, 0.0, 0.0, 0.20, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_D4_SHARP/2.0, 0.0, 0.0, 0.09, 0, 0.0, 0.0, 0.0 },

 { 0, 0.0, PITCH_D4/2.0, 0.0, 0.0, 0.11, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_F4/2.0, 0.0, 0.0, 0.17, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_E4/2.0, 0.0, 0.0, 0.14, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_G4/2.0, 0.0, 0.0, 0.095, 0, 0.0, 0.0, 0.0 },

 { 0, 0.0, PITCH_F4_SHARP/2.0, 0.0, 0.0, 0.17, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_A4/4.0, 0.0, 0.0, 0.09, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_G4_SHARP/4.0, 0.0, 0.0, 0.19, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_D4_SHARP/2.0, 0.0, 0.0, 0.11, 0, 0.0, 0.0, 0.0 },

 { 0, 0.0, PITCH_D4/4.0, 0.0, 0.0, 0.11, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_F4/2.0, 0.0, 0.0, 0.18, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_E4/2.0, 0.0, 0.0, 0.163, 0, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_C4/2.0, 0.0, 0.0, 0.09, 0, 0.0, 0.0, 0.0 },

 /* bank 2*/
 { 0, 0.0, PITCH_F4/4.0, 0.0, 0.0, 0.17, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_G4/4.0, 0.0, 0.0, 0.11, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_A4_SHARP/4.0, 0.0, 0.0, 0.19, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_A4/4.0, 0.0, 0.0, 0.15, 1, 0.0, 0.0, 0.0 },

 { 0, 0.0, PITCH_C4/4.0, 0.0, 0.0, 0.10, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_B4/4.0, 0.0, 0.0, 0.20, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_D4/4.0, 0.0, 0.0, 0.15, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_C4_SHARP/4.0, 0.0, 0.0, 0.20, 1, 0.0, 0.0, 0.0 },

 { 0, 0.0, PITCH_E4/4.0, 0.0, 0.0, 0.12, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_D4_SHARP/4.0, 0.0, 0.0, 0.16, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_F4_SHARP/4.0, 0.0, 0.0, 0.13, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_F4/4.0, 0.0, 0.0, 0.15, 1, 0.0, 0.0, 0.0 },

 { 0, 0.0, PITCH_A4_SHARP/2.0, 0.0, 0.0, 0.14, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_A4/2.0, 0.0, 0.0, 0.17, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_C4/2.0, 0.0, 0.0, 0.09, 1, 0.0, 0.0, 0.0 },
 { 0, 0.0, PITCH_C4_SHARP/2.0, 0.0, 0.0, 0.16, 1, 0.0, 0.0, 0.0 }

};

int num_active_reson = 0;

double osc1 = 0.0; /* instrument 1 */
double osc2 = 0.0; /* instrument 2 */
double osc3 = 0.0; /* instrument 3 */
double osc4 = 0.0; /* instrument 3 */

double osc1_phase = 0.0; /* instrument 1 */
double osc2_phase = 0.0; /* instrument 2 */
double osc3_phase = 0.0; /* instrument 3 */
double osc4_phase = 0.0; /* instrument 3 */

int filter1 = 16;   /* instrument 1 */
int filter2 = 16;   /* instrument 3 */

double vca1 = 0.75; /* instrument 1 */
double vca2 = 0.75; /* instrument 2 */
double vca3 = 0.75; /* instrument 3 */

double instrument1 = 0.0;
double instrument2 = 0.0;
double instrument3 = 0.0;

double L1 = 1.0;
double L2 = 1.0;
double L3 = 1.0;
double L4 = 1.0;

void
status ()
{
  Serial.println("Status:");
  Serial.printf("*** osc1 = %lf Hz osc1_phase = %lf ***\n", osc1, osc1_phase);
  Serial.printf("*** osc2 = %lf Hz osc2_phase = %lf ***\n", osc2, osc2_phase);
  Serial.printf("*** osc3 = %lf Hz osc3_phase = %lf ***\n", osc3, osc3_phase);
  Serial.printf("*** osc4 = %lf Hz osc4_phase = %lf ***\n", osc4, osc4_phase);
  Serial.printf("*** vca1 = %lf ***\n", vca1);
  Serial.printf("*** vca2 = %lf ***\n", vca2);
  Serial.printf("*** vca3 = %lf ***\n", vca3);
  Serial.printf("*** filter1 = %d ***\n", filter1);
  Serial.printf("*** filter2 = %d ***\n", filter2);
  Serial.printf("*** time=%lf seconds ***\n", t);
  Serial.printf("*** samples=%ld ***\n", num_samples);
  Serial.printf("*** loops=%ld ***\n", loop_count);
}

void
reset()
{
  int i;
  audio_L = 0;
  audio_R = 0;

  osc1 = 0.0;
  osc2 = 0.0;
  osc3 = 0.0;
  osc4 = 0.0;
  vca1 = 0.75;
  vca2 = 0.75;
  vca3 = 0.75;
  filter1 = 16;
  filter2 = 16;

  for (i=0; i<NUM_RESONATORS; ++i) {
    resonator_data[i].signal = 0.0;
    resonator_data[i].amp = 0.0;
    resonator_data[i].enabled = 0;
    resonator_data[i].trigger_t = 0.0;
    resonator_data[i].phase = 0.0;
    resonator_data[i].f2 = INTERVAL1 * resonator_data[i].f1;
    resonator_data[i].f3 = INTERVAL2 * resonator_data[i].f1;
  }

}

void
trigger_resonator(int rn)
{
  double p;

  rn = rn%NUM_RESONATORS;
  resonator_data[rn].enabled = 1;
  resonator_data[rn].trigger_t = t;
  resonator_data[rn].signal = 0.0;
  resonator_data[rn].amp = 0.0;

  /* adjust phase such that wave form starts (zero crossing) at this time */
  p = t * resonator_data[rn].f1;
  p -= floor(p);
  resonator_data[rn].phase = 1.0 - p;

  if (debug) {
    Serial.printf("*** r%d triggered time=%lf root freq=%lf phase=%lf phase_constant=%lf *** \n",
    rn, resonator_data[rn].trigger_t, resonator_data[rn].f1, p, resonator_data[rn].phase);
  }
}

#define VALUE_TO_GAIN(V)  ((double)V / 1024.0)


double
sawtooth(double f, double phase, double t)
{
  double s, p;

  /* compute time position in waveform cycle */
  /* where p is from 0.0 to 1.0 */
  p = f*t + phase;
  p -= floor(p);

  s = -1.0 + 2.0 * p;

  return s;
}

double
triangle(double f, double phase, double t)
{
  double s, p;
  /* compute time position in waveform cycle */
  /* where p is from 0.0 to 1.0 */
  p = f*t + phase;
  p -= floor(p);

  if (p<0.5) {
    s = -1.0 + 4.0*p;
  } else {
    s = 3.0 - 4.0*p;
  }

  return s;
}

double
sine_wave(double f, double phase, double t)
{
  double p;
  unsigned int index;
  /* compute time position in waveform cycle */
  /* where p is from 0.0 to 1.0 */
  p = f*t + phase;
  p -= floor(p);

  index = (unsigned int)(p*(double)SINE_WAVE_TABLE_SIZE);
  return sine_wave_table[index];
}

double
value_to_freq(int value)
{
  const double twelfth_root_of_two = 1.059463094359295;
  const double a_pitch = 220.0;
  const double a_value = 101.0;
  const double half_step = 5.0;

  double half_steps = (value - a_value) / half_step;
  double freq = a_pitch * pow(twelfth_root_of_two, half_steps);

  if (debug) {
    Serial.printf("value=%d half_steps=%lf freq=%lf\n", value, half_steps, freq);
  }

  return freq;
}

/* change oscilator freuqnecy to avoid discontinutity by adjusting phase */
void
change_oscilator(double *osc, double *phase, double new_freq)
{
  double p, pnew, pdelta;

  /* compute where we are in current waveform cycle*/
  p = (*osc) * t + (*phase);
  p -= floor(p);

  /* compute where we would be at new freq with 0 phase shift*/
  pnew = new_freq * t;
  pnew -= floor(pnew);

 /* phase delta */
  pdelta = p - pnew;

  if (debug) {
    Serial.printf("f=%lfHz fnew=%lfHz p=%lf pnew=%lf pdelta=%lf phase=%lf\n", *osc, new_freq, p, pnew, pdelta, *phase);
  }

  /* set the new phase constant to this waveform position */
  *phase = pdelta;

  /* update the frequencey */
  *osc = new_freq;

}

void
set_sound_parameter(int addr, int value)
{
  switch (addr) {
    case 000: change_oscilator(&osc1, &osc1_phase, value_to_freq(value)); break;
    case 001: change_oscilator(&osc2, &osc2_phase, value_to_freq(value)); break;
    case 002: change_oscilator(&osc3, &osc3_phase, value_to_freq(value)); break;
    case 003: change_oscilator(&osc4, &osc4_phase, value_to_freq(value)); break;
    case 005: filter1 = value; break;
    case 007: filter2 = value; break;
    case 014: vca1 = VALUE_TO_GAIN(value); break;
    case 020: vca2 = VALUE_TO_GAIN(value); break;
    case 024: vca3 = VALUE_TO_GAIN(value); break;
  }
  //Serial.printf("*** set sound parameter %o to value %d ***\n", addr, value);
}

void
skip_whitespace(char **p)
{
  for (   ; **p && isspace(**p); ++*p) { /*NOP*/ }
}

void
skip_digits(char **p)
{
  for (   ; **p && isdigit(**p); ++*p) { /*NOP*/ }
}

// send 16 bits of button data (two rows) based on sw
void
send_button_data(int sw)
{
  int data;

  data = button_state[2*sw+1]<<8 | button_state[2*sw];
 
  Serial.printf("B:%04x\n", data);
}

void
process_cmd()
{
  char *p = command_buffer;
  int rn;
  int addr;
  int value;
  int sw;
  skip_whitespace(&p);

  switch (*p) {
    case 'r':
      ++p;
      skip_whitespace(&p);
      rn = atoi(p);
      trigger_resonator(rn);
      break;
    case 's':
       ++p;
      skip_whitespace(&p);
      addr = atoi(p);
      skip_digits(&p);
      skip_whitespace(&p);
      value = atoi(p);
      set_sound_parameter(addr,value);
      break;
    case 'c':
      reset();
      break;
    case 'i':
      ++p;
      skip_whitespace(&p);
      sw = atoi(p) % 4;
      send_button_data(sw);
      break;
    case '?':
      status();
      break;
    case 'd':
      ++p;
      skip_whitespace(&p);
      debug = atoi(p);
      Serial.printf("*** debug set to %d ***\n", debug);
      break;
  }
}

void process_serial()
{
  int i, c;
  int n = Serial.available();

  for (i=0; (i<n) && (command_buffer_index<COMMAND_BUFFER_SIZE); ++i) {
    c = Serial.read();
    if (c == '\n') {
      command_buffer[command_buffer_index] = 0;
      process_cmd();
      command_buffer_index = 0;
      command_buffer[command_buffer_index] = 0;
      return;
    }
    if (c == -1) {
      return;
    }
    command_buffer[command_buffer_index] = c;
    ++command_buffer_index;
  }
}

// scan process for buttons, sample one row per call
// update button state

void buttons()
{
  int row_state;
  int col;
  // read buttons from prior loop selected row  (inputs should be settled)
  // reads each column switch on selected row 
  // input will be LOW if button pushed on the selected row on the input line
  row_state = 0;
  for (col=0; col<8; ++col) {
    // if button input sample line is LOW for that column then button is pushed
    row_state |= (digitalRead(buttons_in[col]) == LOW) ? (0x1 << col) : (0x0); 
  }
  button_state[button_index] = row_state;

  // advance scan row
  digitalWrite(buttons_out[button_index], HIGH);
  button_index = (button_index + 1) % 8;
  digitalWrite(buttons_out[button_index], LOW);
}

void setup() {
  int i;

  pinMode(PIN_DAC_BCK, OUTPUT);
  pinMode(PIN_DAC_DIN, OUTPUT);
  pinMode(PIN_DAC_WS, OUTPUT);
  digitalWrite(PIN_DAC_BCK, 0);
  digitalWrite(PIN_DAC_DIN, 0);
  digitalWrite(PIN_DAC_WS, 1);

  for (i=0; i<8; ++i) {
    pinMode(buttons_in[i], INPUT_PULLUP);
    pinMode(buttons_out[i], OUTPUT_OPENDRAIN);
    digitalWrite(buttons_out[i], HIGH);
  }
  
  for(i=0; i<SINE_WAVE_TABLE_SIZE; ++i) {
    sine_wave_table[i] = sin(TWOPI * ((double)i/(double)SINE_WAVE_TABLE_SIZE));
  }

  reset();

  /* note: in teensy the baud rate passed is ignored */
  /* Serial communications to host will be much faster than this over USB */
  Serial.begin(9600);

  /* start sending audio data to DAC */
  myTimer.begin(output_audio_sample, DAC_SAMPLE_INTERVAL);

  Serial.println("*** Network IV Sound Processor ***");
}

void loop() {
  int i;
  double t_now = t;

  /* Compute Resonators */

  for (i=0; i<NUM_RESONATORS; ++i) {

    if (resonator_data[i].enabled) {
      if ((num_active_reson>RESON_COMPUTE_THRESH) && ((loop_count%RESON_COMPUTE_BINS) != (unsigned) (i%RESON_COMPUTE_BINS))) {
        /* when more than RESON_COMPUTE_THRESH active compute each every RESON_COMPUTE_BINS pass*/
        continue;
      }
      double decay = exp(-(t_now-resonator_data[i].trigger_t) / resonator_data[i].T_decay);
      if (decay < 0.01) {
         resonator_data[i].enabled = 0;
         continue;
       }
       double signal_1 = sine_wave(resonator_data[i].f1,  resonator_data[i].phase, t_now);
       double signal_2 = sine_wave(resonator_data[i].f2,  resonator_data[i].phase, t_now);
       double signal_3 = sine_wave(resonator_data[i].f3,  resonator_data[i].phase, t_now);
       if (resonator_data[i].clip) {
         signal_1 = 2.0*CLIP(signal_1, CLIP_V);
         signal_2 = 2.0*CLIP(signal_2, CLIP_V);
         signal_3 = 2.0*CLIP(signal_3, CLIP_V);
       }
       resonator_data[i].signal = decay * (signal_1 + signal_2 + signal_3);
       resonator_data[i].amp = decay;
    }
  }

  /* mix resonators */
  num_active_reson = 0;
  double reson_mix = 0.0;
  double reson_amp = 0.0;

  for (i=0; i<NUM_RESONATORS; ++i) {
    reson_mix += resonator_data[i].signal;
    reson_amp += resonator_data[i].amp;
    if (resonator_data[i].enabled) {
       ++num_active_reson;
    }
  }
  if (reson_amp > RESON_THRESH) {
    reson_mix *= (RESON_THRESH / reson_amp);
  }

  /* Compute Instrument 1 */
  instrument1 = vca1 * triangle(osc1, osc1_phase, t_now);

  /* Compute Instrument 2 */
  instrument2 = vca2 * triangle(osc2, osc2_phase, t_now);

  /* Compute Instrument 3 */
  instrument3 = vca3 * (triangle(osc3, osc3_phase, t_now) * triangle(osc4, osc4_phase, t_now));

  /* to do: reverb and VCF effects */

  /* mix, level, convert to 16 bits */

  double R_mix = A*(L3*instrument3 + L4*reson_mix);
  double L_mix = A*(L1*instrument1 + L2*instrument2);

  audio_R =  DAC_CLIP(R_mix);
  audio_L =  DAC_CLIP(L_mix);

  buttons();
  process_serial();
  // delayMicroseconds(10);
  ++loop_count;

}
