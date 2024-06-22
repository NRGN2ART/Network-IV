#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "nova_defs.h"

extern int32 debug_PC;

/*
 * Network IV I/O Handling
 * Andrew Seawright
 */

/* IOT fields -- from nova_defs.h

 opcode field 

#define ioNIO           0                               
#define ioDIA           1
#define ioDOA           2
#define ioDIB           3
#define ioDOB           4
#define ioDIC           5
#define ioDOC           6
#define ioSKP           7

 pulse field

#define iopN            0                               /*
#define iopS            1
#define iopC            2
#define iopP            3

 */

static const char*
pulse_str(int32 pulse)
{
  char *str = "";
  switch(pulse) {
  case iopN: str = "-"; break;
  case iopS: str = "S"; break;
  case iopC: str = "C"; break;
  case iopP: str = "P"; break;
  }
  return str;
}

static const char*
opcode_str(int32 code)
{
  char *str = "";
  switch (code) {
  case ioNIO: str = "NIO"; break;                              
  case ioDIA: str = "DIA"; break;
  case ioDOA: str = "DOA"; break;
  case ioDIB: str = "DIB"; break;
  case ioDOB: str = "DOB"; break;
  case ioDIC: str = "DIC"; break;
  case ioDOC: str = "DOC"; break;
  case ioSKP: str = "SKP"; break;
  }
  return str;
}

#define IO_MODE_NONE 0
#define IO_MODE_L 1
#define IO_MODE_S 2
#define IO_MODE_X 3


static int initialized = 0;
static int mode = IO_MODE_NONE;
static int sw_bank = 0;
static int sw_selected = 0;
static int debug = 0;
static int count = 0;

static int latched_lamp = 0;
static int32 latched_lamp_value = 0;

#define DISPLAY_COLS 64
#define DISPLAY_ROWS 16

static int display_state[DISPLAY_ROWS][DISPLAY_COLS]; 
static int sound_debug = 0;
static int display_debug = 0;
static int input_debug = 0;
static char *sound_device = NULL;
static char *display_device = NULL;
static int sound_fd = -1;
static int display_fd = -1;

#define DISPLAY_MODE_BY_LAMP 0
#define DISPLAY_MODE_BY_ROW 1

static int display_mode = DISPLAY_MODE_BY_LAMP;

#define BUTTON_BANKS 4
#define BUTTON_CHECK_RATE 200
static int32 button_state[BUTTON_BANKS] = { 0, 0, 0, 0 };
static int push_buttons = 0;
static int push_rate = 0;
static int read_buttons = 0;

static const char *
sound_parameter_description_str(int addr) 
{
  char *str = "?";
  switch (addr) {
  case 000: str = "Instrument 1 Osc 1 pitch control voltage"; break;
  case 005: str = "Instrument 1 filter control voltage"; break;
  case 014: str = "Instrument 1 VCA control voltage"; break;

  case 001: str = "Instrument 2 Osc 2 pitch control voltage"; break;
  case 020: str = "Instrument 2 VCA control voltage"; break;

  case 002: str = "Instrument 3 Osc 3 pitch control voltage"; break;
  case 003: str = "Instrument 3 Osc 4 pitch control voltage"; break;
  case 007: str = "Instrument 3 filter control voltage"; break;
  case 024: str = "Instrument 3 VCA control voltage"; break;
  }

  return str;
}

static void
display_clear_state()
{
  int i, j;

  for (i=0; i<DISPLAY_ROWS; ++i) {
    for (j=0; j<DISPLAY_COLS; ++j) {
      display_state[i][j] = 0;
    }
  }
}

static void
display_print() {
  int i, j;

  for (i=0; i<DISPLAY_ROWS; ++i) {
    for (j=0; j<DISPLAY_COLS; ++j) {
      printf("%s", display_state[i][j] ? "*" : " ");
    }
    printf("\n\r");
  }
}

static char
nibble2char(int nibble)
{
  char c = '?';
  switch(nibble) {
  case 0: c = '0'; break;
  case 1: c = '1'; break;
  case 2: c = '2'; break;
  case 3: c = '3'; break;
  case 4: c = '4'; break;
  case 5: c = '5'; break;
  case 6: c = '6'; break;
  case 7: c = '7'; break;
  case 8: c = '8'; break;
  case 9: c = '9'; break;
  case 10: c = 'a'; break;
  case 11: c = 'b'; break;
  case 12: c = 'c'; break;
  case 13: c = 'd'; break;
  case 14: c = 'e'; break;
  case 15: c = 'f'; break;
  }
  return c;
}

unsigned int
char2nibble(char c)
{
  unsigned int nibble = 0;
  switch (c) {
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
  return nibble;
}

#define CMD_BUFFER_SIZE 1024
char cmd_buffer[CMD_BUFFER_SIZE];

#define INPUT_BUFFER_SIZE 256
char input_buffer[INPUT_BUFFER_SIZE];

//
// Read buttons
//
	/* input switch data is handled by sound device */

static void
buttons()
{
  int i;
  int32 data;

  if (read_buttons && (sound_fd>=0)) {
  
    if ((count%BUTTON_CHECK_RATE) == 0) {
      for (i=0; i<BUTTON_BANKS; ++i) {
	size_t n;
	ssize_t nw, nr;
	sprintf(cmd_buffer, "i %d\n", i);
	n = strlen(cmd_buffer);
	nw = write(sound_fd, cmd_buffer, n); 
	if (n != nw) {
	  printf("failed write to sound device attempted %d bytes wrote %d bytes\n", n, nw);
	}
	input_buffer[0] = '\0';
	input_buffer[1] = '\0';
	n = 7;
	nr = read(sound_fd, input_buffer, n);
	if ((n==nr) && (input_buffer[0]=='B')&&(input_buffer[1]==':')) {
	  data = char2nibble(input_buffer[2]) << 12;
	  data |= char2nibble(input_buffer[3]) << 8;
	  data |= char2nibble(input_buffer[4]) << 4;
	  data |= char2nibble(input_buffer[5]);	
	}
	button_state[i] = data;
      }
    }
  }
}

static void
nw4_io_init() 
{
  int gpio_status = 0;
  
  if (initialized) {
    return;
  }

  display_clear_state();

  if (getenv("NW4_IO_DEBUG")) {
    printf("** IO Debug Enabled ***\n\r");
    debug = 1;
  }
  if (getenv("NW4_SOUND_DEBUG")) {
    printf("** Sound Debug Enabled ***\n\r");
    sound_debug = 1;
  }
  if (getenv("NW4_DISPLAY_DEBUG")) {
    printf("** Display Debug Enabled ***\n\r");
    display_debug = 1;
  }
  if (getenv("NW4_DISPLAY_MODE_BY_ROW")) {
    display_mode = DISPLAY_MODE_BY_ROW;
    printf("** Display By Row Enabled ***\n\r");
  }
  if (getenv("NW4_READ_BUTTONS")) {
    printf("** Read Buttons Enabled ***\n\r");
    read_buttons = 1;
  }
  if (getenv("NW4_INPUT_DEBUG")) {
    printf("** Input Debug Enabled ***\n\r");
    input_debug = 1;
  }
  
  char *push = getenv("NW4_PUSH_BUTTONS");
  if (push) {
    printf("** Simulate Push Buttons Enabled ***\n\r");
    push_buttons = 1;
    push_rate = atoi(push);
  }

  sound_device = getenv("NW4_SOUND_DEVICE"); 
  display_device = getenv("NW4_DISPLAY_DEVICE"); 

  if (sound_device) {
    sound_fd = open(sound_device, O_RDWR);
  }
  if (sound_fd>=0) {
    printf("*** Opened sound device '%s' as fd=%d ***\n\r", sound_device, sound_fd);
  }

  if (display_device) {
    display_fd = open(display_device, O_RDWR);
  }
  if (display_fd>=0) {
    printf("*** Opened display device '%s' as fd=%d ***\n\r", display_device, display_fd);
  }

  initialized = 1;
}

		  
static void
trigger_reasonator(int r)
{
  if (sound_fd >= 0) {
    size_t n;
    ssize_t nw;
    sprintf(cmd_buffer, "r %d\n", r);
    n = strlen(cmd_buffer);
    nw = write(sound_fd, cmd_buffer, n); 
    if (n != nw) {
      printf("failed write to sound device attempted %d bytes wrote %d bytes\n", n, nw);
    }
  }
}

static void
set_sound_parameter(int addr, int data)
{
  if (sound_fd >= 0) {
    size_t n;
    ssize_t nw;
    sprintf(cmd_buffer, "s %d %d\n", addr, data);
    n = strlen(cmd_buffer);
    nw = write(sound_fd, cmd_buffer, n); 
    if (n != nw) {
      printf("failed write to sound device attempted %d bytes wrote %d bytes\n", n, nw);
    }
  }
}

static void
lamp_on(int row, int col)
{
  if (display_fd >= 0) {
    size_t n;
    ssize_t nw;
    sprintf(cmd_buffer, "l %d %d\n", row, col);
    n = strlen(cmd_buffer);
    nw = write(display_fd, cmd_buffer, n); 
    if (n != nw) {
      printf("failed write to display device attempted %d bytes wrote %d bytes\n", n, nw);
    }
  }
}

static void
clear_display()
{
  if (display_fd >= 0) {
    size_t n;
    ssize_t nw;
    sprintf(cmd_buffer, "c\n");
    n = strlen(cmd_buffer);
    nw = write(display_fd, cmd_buffer, n); 
    if (n != nw) {
      printf("failed write to display device attempted %d bytes wrote %d bytes\n", n, nw);
    }
  }
}


static void
update_display()
{
  int row, i, nibble;
  size_t n;
  ssize_t nw;
  char row_data[(DISPLAY_COLS/4) + 1];

  //printf("*** in update display ***\n");
  //fflush(stdout);

  if (display_fd >= 0) {
    for (row=0; row<DISPLAY_ROWS; ++row) {

      //printf("  row=%d\n", row);
      //fflush(stdout);

      for (i=0; i<(DISPLAY_COLS/4); ++i) {

	//printf("  i=%d\n", i);
	//fflush(stdout);

	nibble = display_state[row][4*i] ? 0x8 : 0;
	nibble |= display_state[row][4*i+1] ? 0x4 : 0;
	nibble |= display_state[row][4*i+2] ? 0x2 : 0;
	nibble |= display_state[row][4*i+3] ? 0x1 : 0;	
	row_data[i] = nibble2char(nibble);
      }
      row_data[i] = '\0';
      sprintf(cmd_buffer, "r %d %s\n", row, row_data);
      n = strlen(cmd_buffer);
      //printf("CMD: %s", cmd_buffer);
      //fflush(stdout);
      nw = write(display_fd, cmd_buffer, n); 
      if (n != nw) {
	printf("failed write to display device attempted %d bytes wrote %d bytes\n", n, nw);
      }
    }
  }
}

/*
 * Network IV Nova Device 076 Routine
 * Andrew Seawright
 */

int32
nw4_io(int32 pulse, int32 code, int32 AC)
{
  int32 iodata = 0;

  nw4_io_init();

  buttons();
  
  if (debug) {
    printf("NW4 IO: PC=%o %s%s data=%x\n\r", 
	   debug_PC, opcode_str(code), pulse_str(pulse), AC);
  }
  
  if ((pulse == iopP) && (code == ioDOC) && (AC==0140000)) {

    if (display_debug) {
      display_print();
      printf("*** Display Clear***\n\r");
    }

    if (display_mode == DISPLAY_MODE_BY_ROW) {
      update_display();
    }
    display_clear_state();
    if (display_mode == DISPLAY_MODE_BY_LAMP) {
      clear_display();
    }

  } else if ((pulse == iopN) && (code == ioDOC) && (AC==0040000)) {

    mode = IO_MODE_L;
    if (debug) {
      printf("***MODE L***\n\r");
    }

  } else if ((pulse == iopN) && (code == ioDOC) && (AC==0000000)) {

    mode = IO_MODE_S;
    if (debug) {
      printf("***MODE S***\n\r");
    }

  } else if ((pulse == iopN) && (code == ioDOC) && (AC==0100000)) {

    mode = IO_MODE_X;
    if (debug) {
      printf("***MODE X***\n\r");
    }

  } else if ((mode == IO_MODE_L) && (pulse == iopN) && (code == ioDOA)) {
    /* latch lamp value */
    latched_lamp_value = AC;
    latched_lamp = 1;

  } else if ((mode == IO_MODE_L) && (latched_lamp) && (pulse == iopP) && (code == ioNIO)) {
    /* turn on lamp  */
    int x = (latched_lamp_value>>8) & 0x0F; /* row */
    int y = (latched_lamp_value & 0x3F); /* col */
    display_state[x][y] = 1;
    latched_lamp = 0;
    if (display_debug) {
      printf("*** lamp on: row=%d col=%d ***\n\r", x, y);
    }

    if (display_mode == DISPLAY_MODE_BY_LAMP) {
      lamp_on(x/*row*/,y/*col*/);
    }

  } else if ((mode == IO_MODE_S) && (pulse == iopP) && (code == ioDOB)) {

    int r = (AC & 0x7C00) >> 10;
    if (sound_debug) {
      printf("*** PC=%5o trigger resonator %d ***\n\r", debug_PC, r);
    }
    
    trigger_reasonator(r);

  } else if ((mode == IO_MODE_X) && (pulse == iopP) && (code == ioDOB)) {

    int addr = (AC & 0x7C00) >> 10;
    int data = (AC & 0x03FF);
    if (sound_debug) {
      printf("*** PC=%5o Set sound parameter addr=%o(oct): %s <- %d (decimal)\n\r", debug_PC, addr, sound_parameter_description_str(addr), data);
    }

    set_sound_parameter(addr, data);
  }

  /* handle switches */

  if ((pulse==iopN) && (code==ioDOA) && (AC<=3) && (AC>=0)) {

    sw_bank = AC;
    sw_selected = 1;
    if (input_debug) {
      printf("*** select switch bank %d ***\n\r", sw_bank);
    }

  } else if ((sw_selected) && (pulse==iopN) && (code==ioDIA)) {


    if (read_buttons) {
      iodata = button_state[sw_bank];
    }

    if (push_buttons && ((count%push_rate) == 7)) {
      iodata = 0x7C00;
    }
    if (input_debug) {
      printf("*** read switch bank %d data = %x ***\n\r", sw_bank, iodata);
    }
    sw_selected = 0;
  }

  ++count;
  return iodata;
}
