#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define SCHED_TCP_PORT "34570"  // port number for TCP connection with scheduler
// #define LOCAL_HOST "127.0.0.1"  // Host address
#define MAXDATASIZE 1           // max number of bytes we can get at once 

// get sockaddr (IPv4 only)
void *get_in_addr(struct sockaddr *sa) {
  return &(((struct sockaddr_in*)sa)->sin_addr);
}

int main(int argc, char *argv[]) {
  int sockfd, numbytes;  
  double buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET_ADDRSTRLEN];

  if (argc != 2) {
    fprintf(stderr,"usage: client <location index>\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(NULL, SCHED_TCP_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue; // Try to connect with a different socket
    }
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue; // Try to connect with a different socket
    }
    break; // Found a suitable connection
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  inet_ntop(p->ai_family, 
            get_in_addr((struct sockaddr *)p->ai_addr), 
            s, sizeof s);
  printf("\nThe client is up and running\n");

  freeaddrinfo(servinfo); // all done with this structure

  buf[0] = atof(argv[1]);
  // This is for sending to server
  if ((numbytes = send(sockfd, buf, sizeof(double) * MAXDATASIZE , 0)) == -1) {
    perror("send");
  }
  printf("The client has sent query to Scheduler using TCP: ");
  printf("client location %i\n", (int) buf[0]);

  buf[0] = 127; // Test
  // This is for receiving from server
  if ((numbytes = recv(sockfd, buf, sizeof(double) * MAXDATASIZE, 0)) == -1) {
    perror("recv");
    exit(1);
  }

  int ret = buf[0]; // Return value stored as int
  switch (ret) {
    case 1:
      printf("The client has received results from the Scheduler: ");
      printf("assigned to Hospital A\n");
      break;
    case 2:
      printf("The client has received results from the Scheduler: ");
      printf("assigned to Hospital B\n");
      break;
    case 3:
      printf("The client has received results from the Scheduler: ");
      printf("assigned to Hospital C\n");
      break;
    case -1:
      printf("Location %s not found\n", argv[1]);
      break;
    case 0:
      printf("Score = None, No assignment\n");
      break;
    default:
      printf("Panic: something went wrong!\n");
      break;
  }

  printf("\n");
  close(sockfd);
  return 0;
}
