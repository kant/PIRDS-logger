/************************************
MIT License

Copyright (c) 2020 Public Invention

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 This is a datalogger for the VentMon project
 It can be started to listen for tcp connections or
 ucp packets. The packets must be formatted in the PIRDS
 format. The data is logged to simple unix file in the
 current directory based on the ip address of the sender.
 These means that there will be a collision if there are
 multiple senders behind a single nat.
 I'll fix this later.
         Geoff

 Written by Geoff Mulligan @2020
 Modified by Robert L. Read May 2020, to use PIRDS library

***************************************/

#define VERSION 1.7

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#if __linux__
#include <sys/prctl.h> // prctl(), PR_SET_PDEATHSIG
#endif
#include <stdbool.h>
#include "PIRDS-1-1.h"

// Message Types
struct {
  char type;
  char *value;
} message_types[] = {
  '{', "JSON DATA",
  '!', "Emergency",
  'A', "Alarm",
  'B', "Battery",
  'C', "Control",
  'D', "Unknown",
  'E', "Event",
  'F', "Failure",
  'G', "Unknown G",
  'H', "Unknown H",
  'I', "Unknown I",
  'J', "Unknown J",
  'K', "Unknown K",
  'L', "Limits",
  'M', "Measurement",
  'N', "Unknown N",
  'O', "Unknown O",
  'P', "PARAMETERS",
  'Q', "Unknown Q",
  'R', "Unknown R",
  'S', "Assertion",
  'T', "Unknown T",
  'U', "Unknown U",
  'V', "Unknown V",
  'W', "Unknown W",
  'X', "Unknown X",
  'Y', "Unknown Y",
  'Z', "Unknown Z",
  '\0', "THE END"
};

#define UDP 0
#define TCP 1

uint8_t gDEBUG = 0;

#define USESELECT
#ifdef USESELECT
#define DATA_TIMEOUT 60 // for TCP connections
#endif

#define BSIZE 65*1024
uint8_t buffer[BSIZE];

void handle_udp_connx(int listenfd);
void handle_tcp_connx(int listenfd);

FILE *gFOUTPUT;

int main(int argc, char* argv[]) {
  uint8_t mode = UDP;

  int opt;
  while ((opt = getopt(argc, argv, "Dt")) != -1) {
    switch (opt) {
    case 'D': gDEBUG++; break;
    case 't': mode = TCP; break;
    default: printf("Usage: %s [-D] [-t] [port]\n", argv[0]);
      exit(1);
    }
  }

  gFOUTPUT = stderr;
  if (gDEBUG > 1)
    gFOUTPUT = stdout;

 char *port = "6111";
  if (mode == TCP)
    port = "6110";

  if (optind < argc) {
    port = argv[optind];
  }

  if (gDEBUG)
    fprintf(gFOUTPUT, "%s Server started %son port %s%s\n",
	    mode == TCP ? "TCP":"UDP",
	    "\033[92m", port, "\033[0m");

  // getaddrinfo for host
  struct addrinfo hints, *res;
  memset (&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  if (mode == TCP)
    hints.ai_socktype = SOCK_STREAM;
  else
    hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  if (getaddrinfo( NULL, port, &hints, &res) != 0) {
    perror ("getaddrinfo() error");
    exit(1);
  }

  // socket and bind
  struct addrinfo *p;
  int listenfd;
  for (p = res; p != NULL; p = p->ai_next) {
    listenfd = socket (p->ai_family, p->ai_socktype, 0);
    if (listenfd == -1) continue;
    int option = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);
    if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
  }

  freeaddrinfo(res);

  if (p == NULL) {
    perror ("socket() or bind()");
    exit(1);
  }
  if (gDEBUG)
    fprintf(gFOUTPUT, "LOOP!\n");

  while (1) {
    if (mode == TCP)
      handle_tcp_connx(listenfd);
    else
      handle_udp_connx(listenfd);
  }
}

void
send_params(char *peer, char *addr) {
#if 0
  printf("HTTP/1.1 200 OK\n\n");
  printf("PARAM_WAIT: 300\n");
  fflush(stdout);
#endif
}

// For debugging...
void render_measurement(Measurement* m) {
  fprintf(stderr,"Measurement:\n" );
  fprintf(stderr,"Event %c\n", m->event);
  fprintf(stderr,"type %c\n", m->type);
  fprintf(stderr,"loc %c\n", m->loc);
  fprintf(stderr,"num %u\n", m->num);
  fprintf(stderr,"ms %u\n", m->ms);
  fprintf(stderr,"val %d\n", m->val);
}

/*
void
log_measurement_bytecode(char *peer, void *buff, bool limit) {
  struct __attribute__((__packed__)) measurement_t *measurement = (struct measurement_t *) buff;
*/
void
log_measurement_bytecode(char *peer, void *buff, bool limit) {

  Measurement measurement = get_measurement_from_buffer(buff,13);

  // xxx need file locking
  char fname[30];
  strcpy(fname, "0Logfile.");
  strcpy(fname + 9, peer);

  FILE *fp = fopen(fname, "a");
  if (!fp) return;

  if (limit) fprintf(fp, "-"); // make timestamp negative if limit message
  //  fprintf(fp, "%lu:%c:%c:%u:%u:%d\n", time(NULL), measurement->type, measurement->loc,
  //	  measurement->num, measurement->ms, measurement->val);
    fprintf(fp, "%lu:%c:%c:%c:%u:%u:%d\n", time(NULL),
            measurement.event,
            measurement.type, measurement.loc,
  	  measurement.num, measurement.ms, measurement.val);
  fclose(fp);
}

void
log_event_bytecode(char *peer, void *buff, bool limit) {

  Message message = get_message_from_buffer(buff,263);

  // xxx need file locking
  char fname[30];
  strcpy(fname, "0Logfile.");
  strcpy(fname + 9, peer);

  FILE *fp = fopen(fname, "a");
  if (!fp) return;

  // We're no longer doing this
  //  if (limit) fprintf(fp, "-");
  fprintf(fp, "%lu:%c:%c:%u:\"%s\"\n",
          time(NULL),
          message.event,
          message.type,
          message.ms,
          message.buff);
  fclose(fp);
}

void
log_json(char *peer, void *buff) {
  // xxx need file locking
  char fname[30];
  strcpy(fname, "0Logfile.");
  strcpy(fname+9, peer);

  FILE *fp = fopen(fname, "a");
  if (!fp) return;

  char *ptr = strchr((char *)buff, '\n');
  if (ptr) *ptr = '\0';
  if ((ptr = strchr((char*)buff, '\r')))
    *ptr = '\0';

  if (((char *)buff)[0] == '[') {
    fprintf(fp, "[ {\"TimeStamp\": %lu}, %s\n", time(NULL), (char *)buff+1);
  } else {
    fprintf(fp, "{\"TimeStamp\": %lu, %s\n", time(NULL), (char *)buff+1);
  }
  fclose(fp);
}
void print_message(Message message,bool limit);

void
print_event_bytecode(void *buff, bool limit) {
  char second_char = ((char *)buff)[1];
  if (second_char == 'M') {
    Message message = get_message_from_buffer(buff,263);
    print_message(message,limit);
  }
}

void print_message(Message message,bool limit) {
  fprintf(gFOUTPUT, "  MESSAGE||%s||\n", message.buff);
}

void print_measurement(Measurement measurement,bool limit);

void
print_measurement_bytecode(void *buff, bool limit) {
  Measurement measurement = get_measurement_from_buffer(buff,13);
  print_measurement(measurement,limit);
}

 void print_measurement(Measurement measurement,bool limit) {
  //  render_measurement(&measurement);
  int v = (int) measurement.val;
  float fv = (float) v;

  switch (measurement.type) {
  case 'T':
    fprintf(gFOUTPUT, "  Temp%s %c%d (%u): %f C\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
            fv /100.0);
    break;
  case 'P':
    fprintf(gFOUTPUT, "  Pressure%s %c%d (%u): %f cm\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
            fv /100.0);
    break;
  case 'D':
    fprintf(gFOUTPUT, "  DifferentialPressure%s %c%d (%u): %f cm\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
            fv /10.0);
    break;
  case 'F':
    fprintf(gFOUTPUT, "  Flow%s %c%d (%u): %f l\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
            fv /100.0);
    break;
  case 'O':
    fprintf(gFOUTPUT, "  FractionalO2%s %c%d (%u): %f%%\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
            fv /100.0);
    break;
  case 'H':
    fprintf(gFOUTPUT, "  Humidity%s %c%d (%u): %f%%\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
	   fv/100.0);
    break;
  case 'V':
    fprintf(gFOUTPUT, "  Volume%s %c%d (%u): %d ml\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
	   v);
    break;
  case 'B':
    fprintf(gFOUTPUT, "  Breaths%s %c%d (%u): %d\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
	   v/10);
    break;
  case 'G':
    fprintf(gFOUTPUT, "  Gas%s %c%d (%u): %d\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
	   v);
    break;
  case 'A':
    fprintf(gFOUTPUT, "  Altitude%s %c%d (%u): %d m\n", limit ? "LIMIT" : "",
	   measurement.loc, measurement.num, measurement.ms,
	   v);
    break;
  default: fprintf(gFOUTPUT, "Invalid measurement type\n");
  }
}

void
print_json(void *buff) {
  char *ptr = strchr(buff, '\n');
  if (ptr) *ptr = '\0';
  if ((ptr = strchr(buff, '\r')))
    *ptr = '\0';
  fprintf(gFOUTPUT, "%s\n", (char *)buff);
}

#if 0
void zombie_hunter(int sig)
{
  int status;
  waitpid(pid, &status, 0);
  fprintf(gFOUTPUT, "Got status %d from child\n",status);
  finished=1;
}
#endif

int
handle_event(uint8_t *buffer, int fd, struct sockaddr_in *clientaddr, char *peer) {
  uint8_t x = 0;
  while (message_types[x].type != '\0') {
    if (buffer[0] == message_types[x].type) break;
    x++;
  }

  if (message_types[x].type == '\0') {
    if (gDEBUG)
      fprintf(gFOUTPUT, "  Invalid Message\n");
    return 0;
  }

  int flags = 0;
#if __linux__
  flags = MSG_CONFIRM;
#endif

  int8_t rvalue = 0;
  switch(message_types[x].type) {
  case '{':
    if (gDEBUG)
      print_json(buffer);
    log_json(peer, buffer);
    if (clientaddr)
      sendto(fd, "OK\n", 3, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "OK\n", 3);
    break;
  case '!':
    if (gDEBUG)
      fprintf(gFOUTPUT, "  Emergency Message\n");
    if (clientaddr)
      sendto(fd, "NOP\n", 4, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "NOP\n", 4);
    rvalue = 1;
    break;
  case 'A':
    if (gDEBUG)
      fprintf(gFOUTPUT, "  Alarm Message\n");
    if (clientaddr)
      sendto(fd, "NOP\n", 4, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "NOP\n", 4);
    rvalue = 1;
    break;
  case 'B':
    if (gDEBUG)
      fprintf(gFOUTPUT, "  Battery Message\n");
    if (clientaddr)
      sendto(fd, "NOP\n", 4, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "NOP\n", 4);
    rvalue = 1;
    break;
  case 'C':
    if (gDEBUG)
      fprintf(gFOUTPUT, "  Control Message\n");
    if (clientaddr)
      sendto(fd, "NOP\n", 4, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "NOP\n", 4);
    rvalue = 1;
    break;
    /* The is an *E*vent */
  case 'E':
    log_event_bytecode(peer, buffer, true);
    if (gDEBUG)
      print_event_bytecode(buffer, true);
    if (clientaddr)
      sendto(fd, "NOP\n", 4, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "NOP\n", 4);
    rvalue = 1;
    break;
  case 'F':
    if (gDEBUG)
      fprintf(gFOUTPUT, "  Failure Message\n");
    if (clientaddr)
      sendto(fd, "NOP\n", 4, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "NOP\n", 4);
    rvalue = 1;
    break;
  case 'L':
    log_measurement_bytecode(peer, buffer, true);
    if (gDEBUG)
      print_measurement_bytecode(buffer, true);
    if (clientaddr)
      sendto(fd, "OK\n", 3, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "OK\n", 3);
    break;
  case 'M':
    log_measurement_bytecode(peer, buffer, false);
    if (gDEBUG)
      print_measurement_bytecode(buffer, false);
    if (clientaddr)
      sendto(fd, "OK\n", 3, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "OK\n", 3);
    break;
  case 'P':
    if (gDEBUG)
      fprintf(gFOUTPUT, "  Param request\n");
    if (clientaddr)
      sendto(fd, "NOP\n", 4, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "NOP\n", 4);
    rvalue = 1;
    break;
  case 'S':
    if (gDEBUG)
      fprintf(gFOUTPUT, "  aSsertion Message\n");
    if (clientaddr)
      sendto(fd, "NOP\n", 4, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "NOP\n", 4);
    rvalue = 1;
    break;
  default:
    if (gDEBUG)
      fprintf(gFOUTPUT, "  Unknown %c Message\n", message_types[x].type);
    if (clientaddr)
      sendto(fd, "UNK\n", 4, flags, (struct sockaddr *) clientaddr, sizeof *clientaddr);
    else
      write(fd, "UNK\n", 4);
    rvalue = 2;
    break;
  }
  return rvalue;
}

//client connection
void handle_udp_connx(int listenfd) {
  struct sockaddr_in clientaddr;
  socklen_t addrlen = sizeof clientaddr;

  // MSG_WAITALL or 0????
  int len = recvfrom(listenfd, buffer, BSIZE-1, MSG_WAITALL, (struct sockaddr *) &clientaddr, &addrlen);

  if (gDEBUG) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    fprintf(gFOUTPUT, "%d%02d%02d %02d:%02d:%02d ", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
  }

  if (len == -1) {
    if (gDEBUG)
      fprintf(gFOUTPUT, "recvfrom error\n");
    return;
  }
  buffer[len] = '\0';

  char peer[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET, &clientaddr.sin_addr, peer, sizeof peer);

  if (gDEBUG) {
    fprintf(gFOUTPUT, "(%s) ", peer);
    fprintf(gFOUTPUT, "\x1b[32m + [%d]\x1b[0m\n", len);
  }

  handle_event(buffer, listenfd, &clientaddr, peer);
}

void handle_tcp_connx(int listenfd) {
  // listen for incoming connections
  if (listen(listenfd, 1000000) != 0 ) {
    perror("listen() error");
    return;
  }

  // Ignore SIGCHLD to avoid zombie threads
  signal(SIGCHLD,SIG_IGN);
  //  signal(SIGCHLD,zombie_hunter);

  // ACCEPT connections
  pid_t ppid = getpid();

  while (1) {
    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof clientaddr;
    int clientfd = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);

    if (gDEBUG > 2)
      fprintf(gFOUTPUT, "accept (%d)\n", clientfd);

    if (clientfd < 0) {
      if (gDEBUG)
	fprintf(gFOUTPUT, "accept error\n");
    } else {
      if (fork() == 0) { // the child
#if __linux__
	if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
	  perror("prctl for child");
	  exit(1);
	}
#endif

#if 0
	if (getppid() != ppid) exit(1);
#endif
	char peer[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientaddr.sin_addr, peer, sizeof peer);

	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	if (gDEBUG) {
	  fprintf(gFOUTPUT, "%d%02d%02d %02d:%02d:%02d ", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	  fprintf(gFOUTPUT, "(%s) Connected %d\n", peer, clientfd);
	  fflush(gFOUTPUT);
	}

	while(1) {
	  int rcvd;
#ifdef USESELECT
	  fd_set fds;
	  FD_ZERO(&fds);
	  FD_SET(clientfd, &fds);

	  struct timeval tv = {DATA_TIMEOUT, 0};

	  uint8_t a = select(clientfd+1, &fds, NULL, NULL, &tv);
	  if (a < 0) {
	    now = time(NULL);
	    tm = localtime(&now);
	    if (gDEBUG) {
	      fprintf(gFOUTPUT, "%d%02d%02d %02d:%02d:%02d ",
		      tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	      fprintf(gFOUTPUT, "(%s) select error\n", peer);
	      fflush(gFOUTPUT);
	    }
	    break;
	  }
	  if (a == 0) {
	    now = time(NULL);
	    tm = localtime(&now);
	    if (gDEBUG) {
	      fprintf(gFOUTPUT, "%d%02d%02d %02d:%02d:%02d ",
		      tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	      fprintf(gFOUTPUT, "(%s) timeout\n", peer);
	      fflush(gFOUTPUT);
	    }
	    break;
	  }
	  if (FD_ISSET(clientfd, &fds))
	    rcvd = recv(clientfd, buffer, BSIZE, 0);
	  else
	    rcvd = -1;
#else
	  rcvd = recv(clientfd, buffer, BSIZE, 0);
#endif

	  now = time(NULL);
	  tm = localtime(&now);
	  if (gDEBUG) {
	    fprintf(gFOUTPUT, "%d%02d%02d %02d:%02d:%02d ",
		    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	    fprintf(gFOUTPUT, "(%s) ", peer);
	  }

	  if (gDEBUG > 2)
	    fprintf(gFOUTPUT, "[%d] ", getpid());

	  if (rcvd < 0) {    // receive error
	    if (gDEBUG)
	      fprintf(gFOUTPUT, "  read/recv error\n");
	    break;
	  } else if (rcvd == 0) {    // receive socket closed
	    if (gDEBUG)
	      fprintf(gFOUTPUT, " Client disconnected\n");
	    break;
	  }

	  // message received
	  if (gDEBUG)
	    fprintf(gFOUTPUT, "\x1b[32m + [%d]\x1b[0m\n", rcvd);

	  handle_event(buffer, clientfd, NULL, peer);
	}
	//Closing SOCKET
	fflush(gFOUTPUT);
	shutdown(clientfd, SHUT_RDWR);         //All further send and recieve operations are DISABLED...
	close(clientfd);
      }
    }
  }
}
