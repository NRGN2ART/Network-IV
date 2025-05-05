#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_ENV "NW4_SOUND_DEVICE"
char *device = "/dev/ttyACM0";

#define BUFFER_SIZE 4096

int 
main(int argc, char *argv[])
{

  int fd;
  char buffer[BUFFER_SIZE];
  ssize_t numr, numw;
  size_t n;
  char *env;
  int i;
  
  env = getenv(DEVICE_ENV);
  if (env) {
    device = env;
  }
  
  fd = open(device, O_RDWR);

  
  if (fd<0) {
    fprintf(stderr, "Can't open device %s\n", device);
    exit(1);
  }

  printf("** Opened device %s with fd=%d ***\n", device, fd);

  //  fcntl(fd, F_SETFL, O_NONBLOCK);
    
  buffer[0] = '\0';
  for (i=1; (argc>1) && (i<argc); ++i) {
    strcat(buffer, " ");
    if (strcmp(argv[i],",")==0) {
      strcat(buffer, "\n");
    } else {
      strcat(buffer, argv[i]);
    }
    strcat(buffer, " ");
  }
  strcat(buffer, "\n");

  n =strlen(buffer);
  numw = write(fd, buffer, n);
  if (n!=numw) {
    printf("wrote %s request bytes:%d actual bytes:%d\n", buffer, n, numw);
  }

  usleep(5000);
  
  buffer[0] = '\0';
  n = BUFFER_SIZE - 1;
  numr = read(fd, buffer, n);
  buffer[numr] = '\0';

  printf("%s\n", buffer);
  
  close(fd);
  exit(0);
}
