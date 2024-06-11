
/* Network IV - emulation - sound processor - reasonator demo */
/* Sketch for the Teensy 4.0 https://www.pjrc.com/store/teensy40.html */
/* Andrew Seawright */

#include <math.h>
#include <ctype.h>

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
double TWOPI = 2.0*M_PI;

#define CLIP_V 0.5
#define CLIP(s,V) ((s>V)?(V):((s<-V)?(-V):(s)))

#define PITCH1 123.47 /* B */
#define PITCH2 138.59 /* C# */
#define PITCH3 261.63 /* C */
#define PITCH4 155.56 /* D# */
#define PITCH5 146.83 /* D */
#define PITCH6 174.61 /* F */
#define PITCH7 164.81 /* E */
#define PITCH8 196.00 /* G */
#define PITCH9 185.00 /* F# */
#define PITCH10 220.00 /* A */
#define PITCH11 207.65 /* G */
#define PITCH12 77.78 /* D# */
#define PITCH13 293.66 /* D */
#define PITCH14 87.31 /* F */
#define PITCH15 82.41 /* E */
#define PITCH16 98 /* G */

#define PITCH17 174.61 /* F */
#define PITCH18 196.00 /* G */
#define PITCH19 233.08 /* A# */
#define PITCH20 110.00 /* A */
#define PITCH21 261.63 /* C */
#define PITCH22 246.94 /* B */
#define PITCH23 146.83 /* D */ 
#define PITCH24 138.59 /* C# */
#define PITCH25 164.81 /* E */
#define PITCH26 155.56 /* D# */
#define PITCH27 185.00 /* F# */
#define PITCH28 174.61 /* F */
#define PITCH29 233.08 /* A# */
#define PITCH30 220.00 /* A */
#define PITCH31 130.81 /* C */
#define PITCH32 138.59 /* C# */

#define NUM_RESONATORS 32
#define RESON_THRESH 4.0
#define RESON_COMPUTE_THRESH 8
#define RESON_COMPUTE_BINS 4

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
} resonator_data_t;

#define INTERVAL1 (4.0/3.0)
#define INTERVAL2 (16.0/9.0)

resonator_data_t resonator_data[NUM_RESONATORS] = {

 /* bank 1*/
 { 0, 0.0, PITCH1, INTERVAL1*PITCH1, INTERVAL2*PITCH1, 0.10, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH2, INTERVAL1*PITCH2, INTERVAL2*PITCH2, 0.18, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH3, INTERVAL1*PITCH3, INTERVAL2*PITCH3, 0.20, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH4, INTERVAL1*PITCH4, INTERVAL2*PITCH4, 0.09, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH5, INTERVAL1*PITCH5, INTERVAL2*PITCH5, 0.11, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH6, INTERVAL1*PITCH6, INTERVAL2*PITCH6, 0.17, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH7, INTERVAL1*PITCH7, INTERVAL2*PITCH7, 0.22, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH8, INTERVAL1*PITCH8, INTERVAL2*PITCH8, 0.095, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH9, INTERVAL1*PITCH9, INTERVAL2*PITCH9, 0.17, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH10, INTERVAL1*PITCH10, INTERVAL2*PITCH10, 0.22, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH11, INTERVAL1*PITCH11, INTERVAL2*PITCH11, 0.19, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH12, INTERVAL1*PITCH12, INTERVAL2*PITCH12, 0.11, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH13, INTERVAL1*PITCH13, INTERVAL2*PITCH13, 0.10, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH14, INTERVAL1*PITCH14, INTERVAL2*PITCH14, 0.22, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH15, INTERVAL1*PITCH15, INTERVAL2*PITCH15, 0.21, 0, 0.0, 0.0 },
 { 0, 0.0, PITCH16, INTERVAL1*PITCH16, INTERVAL2*PITCH16, 0.09, 0, 0.0, 0.0 },

 /* bank 2*/
 { 0, 0.0, PITCH17, INTERVAL1*PITCH17, INTERVAL2*PITCH17, 0.22, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH18, INTERVAL1*PITCH18, INTERVAL2*PITCH18, 0.11, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH19, INTERVAL1*PITCH19, INTERVAL2*PITCH19, 0.19, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH20, INTERVAL1*PITCH20, INTERVAL2*PITCH20, 0.15, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH21, INTERVAL1*PITCH21, INTERVAL2*PITCH21, 0.10, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH22, INTERVAL1*PITCH22, INTERVAL2*PITCH22, 0.20, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH23, INTERVAL1*PITCH23, INTERVAL2*PITCH23, 0.15, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH24, INTERVAL1*PITCH24, INTERVAL2*PITCH24, 0.20, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH25, INTERVAL1*PITCH25, INTERVAL2*PITCH25, 0.12, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH26, INTERVAL1*PITCH26, INTERVAL2*PITCH26, 0.16, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH27, INTERVAL1*PITCH27, INTERVAL2*PITCH27, 0.23, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH28, INTERVAL1*PITCH28, INTERVAL2*PITCH28, 0.15, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH29, INTERVAL1*PITCH29, INTERVAL2*PITCH29, 0.14, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH30, INTERVAL1*PITCH30, INTERVAL2*PITCH30, 0.17, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH31, INTERVAL1*PITCH31, INTERVAL2*PITCH31, 0.09, 1, 0.0, 0.0 },
 { 0, 0.0, PITCH32, INTERVAL1*PITCH32, INTERVAL2*PITCH32, 0.16, 1, 0.0, 0.0 }

};

int num_active_reson = 0;

double osc1 = 0.0; /* instrument 1 */
double osc2 = 0.0; /* instrument 2 */
double osc3 = 0.0; /* instrument 3 */
double osc4 = 0.0; /* instrument 3 */

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
  Serial.printf("*** osc1 = %lf Hz ***\n", osc1);
  Serial.printf("*** osc2 = %lf Hz ***\n", osc2);
  Serial.printf("*** osc3 = %lf Hz ***\n", osc3);
  Serial.printf("*** osc4 = %lf Hz ***\n", osc4);
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
  }

}

void
trigger_resonator(int rn)
{
  rn = rn%NUM_RESONATORS;
  resonator_data[rn].enabled = 1;
  resonator_data[rn].trigger_t = t;
  resonator_data[rn].signal = 0.0;
  resonator_data[rn].amp = 0.0;

  //Serial.printf("*** r%d triggered ***\n", rn);
}

#define VALUE_TO_GAIN(V)  ((double)V / 1024.0)


double
sawtooth(double f, double t)
{
  double s, p;

  /* compute time position in waveform cycle */
  /* where p is from 0.0 to 1.0 */
  p = f*t;
  p -= floor(p);

  s = -1.0 + 2.0 * p;
 
  return s;
}

double
triangle(double f, double t)
{
  double s, p;
  /* compute time position in waveform cycle */
  /* where p is from 0.0 to 1.0 */
  p = f*t;
  p -= floor(p);

  if (p<0.5) {
    s = -1.0 + 4.0*p;
  } else {
    s = 3.0 - 4.0*p;
  }
 
  return s;
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

 // Serial.printf("value=%d half_steps=%lf freq=%lf\n", value, half_steps, freq);

  return freq;
}

void
set_sound_parameter(int addr, int value)
{
  switch (addr) {
    case 000: osc1 = value_to_freq(value); break;
    case 001: osc2 = value_to_freq(value); break;
    case 002: osc3 = value_to_freq(value); break;
    case 003: osc4 = value_to_freq(value); break;
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

void 
process_cmd()
{
  char *p = command_buffer;
  int rn;
  int addr;
  int value;
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
    case '?':
      status();
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

void setup() {
  
  pinMode(PIN_DAC_BCK, OUTPUT);
  pinMode(PIN_DAC_DIN, OUTPUT);
  pinMode(PIN_DAC_WS, OUTPUT);
  digitalWrite(PIN_DAC_BCK, 0);
  digitalWrite(PIN_DAC_DIN, 0);
  digitalWrite(PIN_DAC_WS, 1);

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
       double signal_1 = sin(TWOPI * resonator_data[i].f1 * t_now);  
       double signal_2 = sin(TWOPI * resonator_data[i].f2 * t_now); 
       double signal_3 = sin(TWOPI * resonator_data[i].f3 * t_now); 
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
  instrument1 = vca1 * triangle(osc1, t_now); 

  /* Compute Instrument 2 */
  instrument2 = vca2 * triangle(osc2, t_now);

  /* Compute Instrument 3 */
  instrument3 = vca3 * (triangle(osc3, t_now) * triangle(osc4, t_now));

  /* to do: reverb and VCF effects */

  /* mix, level, convert to 16 bits */
  
  double R_mix = A*(L3*instrument3 + L4*reson_mix);
  double L_mix = A*(L1*instrument1 + L2*instrument2);

  audio_R =  DAC_CLIP(R_mix); 
  audio_L =  DAC_CLIP(L_mix);

  process_serial();
  // delayMicroseconds(10);  
  ++loop_count;
 
}
