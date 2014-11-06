#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>


int serialport_init(const char* serialport, int baud)
{
  struct termios toptions;
  int fd;
    
  //fd = open(serialport, O_RDWR | O_NOCTTY | O_NDELAY);
  fd = open(serialport, O_RDWR | O_NONBLOCK);
    
  if (fd == -1)  {
    perror("serialport_init: Unable to open port ");
    return -1;
  }
    
  //int iflags = TIOCM_DTR;
  //ioctl(fd, TIOCMBIS, &iflags);     // turn on DTR
  //ioctl(fd, TIOCMBIC, &iflags);    // turn off DTR

  if (tcgetattr(fd, &toptions) < 0) {
    perror("serialport_init: Couldn't get term attributes");
    return -1;
  }
  speed_t brate = baud; // let you override switch below if needed
  switch(baud) {
  case 4800:   brate=B4800;   break;
  case 9600:   brate=B9600;   break;
#ifdef B14400
  case 14400:  brate=B14400;  break;
#endif
  case 19200:  brate=B19200;  break;
#ifdef B28800
  case 28800:  brate=B28800;  break;
#endif
  case 38400:  brate=B38400;  break;
  case 57600:  brate=B57600;  break;
  case 115200: brate=B115200; break;
  }
  cfsetispeed(&toptions, brate);
  cfsetospeed(&toptions, brate);

  // 8N1
  toptions.c_cflag &= ~PARENB;
  toptions.c_cflag &= ~CSTOPB;
  toptions.c_cflag &= ~CSIZE;
  toptions.c_cflag |= CS8;
  // no flow control
  toptions.c_cflag &= ~CRTSCTS;

  //toptions.c_cflag &= ~HUPCL; // disable hang-up-on-close to avoid reset

  toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
  toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl

  toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
  toptions.c_oflag &= ~OPOST; // make raw

  // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
  toptions.c_cc[VMIN]  = 0;
  toptions.c_cc[VTIME] = 0;
  //toptions.c_cc[VTIME] = 20;
    
  tcsetattr(fd, TCSANOW, &toptions);
  if( tcsetattr(fd, TCSAFLUSH, &toptions) < 0) {
    perror("init_serialport: Couldn't set term attributes");
    return -1;
  }

  return fd;
}

/* Parameter "level" should be zero or non-zero for
 * setting to low or high resp.
 * Assumes fd is a global open filedescriptor
 * variable to a serial device.
 */
int setDTR(int fd, unsigned short level)
{
  int status;

  if (ioctl(fd, TIOCMGET, &status) == -1) {
    perror("setDTR()");
    return 0;
  }
  if (level) {
    status |= TIOCM_DTR;
  } else {
    status &= ~TIOCM_DTR;
  }
  if (ioctl(fd, TIOCMSET, &status) == -1) {
    perror("setDTR");
    return 0;
  }
  return 1;
}

int main (int argc, char *argv[])
{
  int serfd, filefd, ret, reading=50;
  unsigned char data;
  char * cmd;
  char b[1];  // read expects an array, so we give it a 1-byte array
  int i=0;
  int timeout;
  int preamble=1;

  if (argc!=3) {
    fprintf(stderr, "wrong number of arguments");
    exit(1);
  }
  fprintf (stderr, "serieport: %s fil: %s\n", argv[1], argv[2]); 
  serfd = serialport_init(argv[1], 4800);
  if (serfd==-1) {
    fprintf (stderr, "Failed to open serial port: %s\n", argv[1] );
    exit(0);
  }
  sleep(4);
  tcflush(serfd, TCIOFLUSH);
  sleep(4);
  fprintf (stderr, "Opened serial port OK\n");
  filefd = open (argv[2], O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (filefd==-1) {
    fprintf (stderr, "Failed to open destination file: %s\n", argv[2] );
    exit(0);
  }
  setDTR(serfd, 0);
  usleep(10000);
  timeout=50;
  //tcflush(serfd, TCIOFLUSH);
  fprintf (stderr, "Wrote start reader command command\n");
  setDTR(serfd, 1);
  usleep(1);
  setDTR(serfd, 0);
  do { 
    int n = read(serfd, b, 1);  // read a char at a time
    if( n==-1) {
      fprintf(stderr, "Failed to read one byte\n");
      exit(1);
    }
    if( n==0 ) {
      usleep( 1 * 1000 );  // wait 1 msec try again
      timeout--;
      //fprintf (stderr, "timeout=%d\n", timeout);
      continue;
    }
    setDTR(serfd, 1);
    usleep(1);
    setDTR(serfd, 0);
    timeout=50;
#ifdef SERIALPORTDEBUG  
    printf("serialport_read_until: i=%d, n=%d b='%c'\n",i,n,b[0]); // debug
#endif
    ret = write (filefd, b, 1);
    if (ret !=1) {
      fprintf (stderr, "Failed to write one byte to file\n");
      exit(0);
    }
    //fprintf (stderr, "Wrote one byte to file\n");
    if (b[0] == 0) {
      reading--;
    }
    else {
      preamble=0;
      reading=50;
    }

  } while( (reading>0||preamble) && timeout>0 );

  //tcflush(serfd, TCIOFLUSH);
  fprintf (stderr, "Wrote stop reader command\n");
  close(serfd);
  close(filefd);
}
