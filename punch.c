#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>



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
  case  600:   brate=B600; break;
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



int main (int argc, char *argv[])
{
  int serfd, filefd, ret;
  char * cmd;
  char ch; 

  if (argc!=3) {
    fprintf(stderr, "wrong number of arguments");
    exit(1);
  }
  fprintf (stderr, "serieport: %s fil: %s\n", argv[1], argv[2]); 
  serfd = serialport_init(argv[1], 600);
  if (serfd==-1) {
    fprintf (stderr, "Failed to open serial port: %s\n", argv[1] );
    exit(0);
  }
  sleep(1);
  tcflush(serfd, TCIOFLUSH);
  sleep(1);
  fprintf (stderr, "Opened serial port OK\n");
  filefd = open (argv[2], O_RDWR, 0666);
  if (filefd==-1) {
    fprintf (stderr, "Failed to open destination file: %s\n", argv[2] );
    exit(0);
  }
  fprintf (stderr, "Opened file OK\n");
  while ((ret = read (filefd,&ch, 1)) == 1) { 
      write (serfd, &ch, 1);
  };
  close(serfd);
  close(filefd);
}
