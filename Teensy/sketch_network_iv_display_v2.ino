
/* Network IV - emulation - display processor  */
/* Sketch for the Teensy 4.0 https://www.pjrc.com/store/teensy40.html */
/* Andrew Seawright */
/* Copyright (c) 2024 Andrew Seawright. All Rights Reserved. */

#include <ctype.h>
#include <OctoWS2811.h>

/****************/
/* time related */
/****************/

#define DISPLAY_UPDATE_INTERVAL 10000 /*microseconds (100Hz) */
IntervalTimer myTimer;
double t = 0.0; /* current time in seconds */
double time_incr = (((double)(DISPLAY_UPDATE_INTERVAL))/1000000.0);
unsigned long long update_count = 0;
unsigned long long loop_count = 0;


/* serial command processing buffer */

#define COMMAND_BUFFER_SIZE 1024
char command_buffer[COMMAND_BUFFER_SIZE];
int command_buffer_index = 0;


/* display info */

#define NUM_ROWS 16
#define NUM_COLS 64
#define LEDS_PER_STRIP 256
#define NUM_STRIPS 4
#define PANELX 16
#define PANELY 16
DMAMEM int displayMemory[6*LEDS_PER_STRIP];
int drawingMemory[6*LEDS_PER_STRIP];
const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(LEDS_PER_STRIP, displayMemory, drawingMemory, config);

#define NEON 0x00160300

void
update_display()
{
  if (!leds.busy()) {
    leds.show();
  }
  t += time_incr;
  ++update_count;
}

void
clear()
{
  int i;
  for(i=0; i< NUM_STRIPS*LEDS_PER_STRIP; ++i) {
    leds.setPixel(i, 0);
  }
}

void
set_light(int row, int col, uint32_t color)
{
  int panel, led, px, py;
  
  panel = col / PANELX;

  py = PANELY-row-1;
  px = col % PANELX;

  led = LEDS_PER_STRIP*panel + PANELY*py + ((py%2) ? (PANELX-px-1) : (px));  

  //Serial.printf("row=%d col=%d panel=%d px=%d py=%d led=%d\n", row, col, panel, px, py, led);

  leds.setPixel(led, color);
}

void
status ()
{
  Serial.println("Status:");
  Serial.printf("loop count: %lld\n", loop_count);
  Serial.printf("update count: %lld\n", update_count);
  Serial.printf("time: %lf seconds", t);
}

void
reset()
{
  clear();
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
update_row(int row, char **p)
{
  int i, nibble;
  for (i=0 ; **p && (i<16); ++*p, ++i) {
    nibble = 0;
    switch (**p) {
      case '0': nibble=0; break;
      case '1': nibble=1; break;
      case '2': nibble=2; break;
      case '3': nibble=3; break;
      case '4': nibble=4; break;
      case '5': nibble=5; break;
      case '6': nibble=6; break;
      case '7': nibble=7; break;
      case '8': nibble=8; break;
      case '9': nibble=9; break;
      case 'a': nibble=10; break;
      case 'A': nibble=10; break;
      case 'b': nibble=11; break;
      case 'B': nibble=11; break;
      case 'c': nibble=12; break;
      case 'C': nibble=12; break;
      case 'd': nibble=13; break;
      case 'D': nibble=13; break;
      case 'e': nibble=14; break;
      case 'E': nibble=14; break;
      case 'f': nibble=15; break;
      case 'F': nibble=15; break;
    }
  
    set_light(row, 4*i,   (nibble&0x8) ? NEON : 0);
    set_light(row, 4*i+1, (nibble&0x4) ? NEON : 0);
    set_light(row, 4*i+2, (nibble&0x2) ? NEON : 0);
    set_light(row, 4*i+3, (nibble&0x1) ? NEON : 0);
  }
}


void 
process_cmd()
{
  char *p = command_buffer;
  int row;
  int col;
  skip_whitespace(&p);

  switch (*p) {
    case 'r':
      ++p;
      skip_whitespace(&p);
      row = atoi(p) % NUM_ROWS;
      skip_digits(&p);
      skip_whitespace(&p);
      update_row(row, &p);
      break;
    case 'l':
       ++p;
      skip_whitespace(&p);
      row = atoi(p) % NUM_ROWS;
      skip_digits(&p);
      skip_whitespace(&p);
      col = atoi(p) % NUM_COLS;
      set_light(row,col,NEON);
      break;
    case 'c':
      clear();
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

void setup() 
{
  reset(); 
  leds.begin();
  leds.show();

  /* note: in teensy the baud rate passed is ignored */
  /* Serial communications to host will be much faster than this over USB */
  Serial.begin(9600);
  Serial.println("*** Network IV Display Processor ***");

  myTimer.begin(update_display, DISPLAY_UPDATE_INTERVAL);
}

void loop() 
{
  process_serial();
  ++loop_count;
}
