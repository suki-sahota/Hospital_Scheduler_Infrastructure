/**
 * Author: Suki Sahota
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <iostream>
#include <cmath>

// Port numbers
#define A_PORT  "30570"
#define B_PORT  "31570"
#define C_PORT  "32570"
#define UDP_PORT  "33570"
#define TCP_PORT  "34570"

#define BACKLOG         1     // how many pending connections queue will hold
#define MAXDATASIZE_TCP 1     // max number of bytes we can get at once (TCP)
#define MAXDATASIZE     2     // max number of bytes we can get at once (UDP)

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
int sockfdA, sockfdB, sockfdC; // UDP sockets for the three hospitals
int sockUDPfd;
int numbytes;  
double buf[MAXDATASIZE_TCP]; // buffer for client communication
double bufA[MAXDATASIZE]; // buffer for Hospital A
double bufB[MAXDATASIZE]; // buffer for Hospital B
double bufC[MAXDATASIZE]; // buffer for Hospital C
struct addrinfo hints, *servinfo, *p;

// connector's address information
struct sockaddr_storage client_addr; 
struct sockaddr_storage A_addr;
struct sockaddr_storage B_addr;
struct sockaddr_storage C_addr;
socklen_t sin_size;  
socklen_t addr_len;
struct sigaction sa;
int yes=1;
char s[INET_ADDRSTRLEN];
int rv;
int ACapacity, AOccupancy;
int BCapacity, BOccupancy;
int CCapacity, COccupancy;
double high_score;
int ties;

// ----------------------------------------------------------------------------
// Utility Functions
// ----------------------------------------------------------------------------

void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;
  while(waitpid(-1, NULL, WNOHANG) > 0);
  errno = saved_errno;
}

/*
 * This function establishes a TCP connection
 * with the client.
 */
void SetUpTCP() {
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  if ((rv = getaddrinfo(NULL, TCP_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      exit(1);
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("scheduler: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("scheduler: bind");
      continue; // Try to bind with next valid socket
    }

    break; // Found suitable TCP connection
  }

  freeaddrinfo(servinfo); // all done with this structure
  if (p == NULL)  {
    fprintf(stderr, "scheduler: failed to bind\n");
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }
  sin_size = sizeof client_addr;
}

/* 
 * Establishes a UDP connection with whichever
 * port number is passed in.
 */
void SetUpUDP() {
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  rv = getaddrinfo(NULL, UDP_PORT, &hints, &servinfo);

  if (rv != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    sockUDPfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    rv = sockUDPfd;

    if (rv == -1) {
      perror("listener: socket");
      continue; // socket() failed; try different socket
    }

    rv = bind(sockUDPfd, p->ai_addr, p->ai_addrlen);

    if (rv == -1) {
      close(sockfd);
      perror("listener: bind");
      continue; // Bind failed; try different socket
    }

    break; // Found suitable UDP connection
  }

  freeaddrinfo(servinfo);
  if (p == NULL) {
    fprintf(stderr, "listener: failed to bind socket\n");
      exit(1);
  }
  addr_len = sizeof A_addr;
}

void RecvInitCapAndOcc(const char *port) {
  if (strcmp(port, A_PORT) == 0) {
    numbytes = recvfrom(sockUDPfd, bufA, sizeof(double) *  MAXDATASIZE,
                        0, (struct sockaddr *)&A_addr, &addr_len);
    if (numbytes == -1) {
      perror("recvfrom");
      exit(1);
    }
    ACapacity = bufA[0];
    AOccupancy = bufA[1];
  } else if (strcmp(port, B_PORT) == 0) {
    numbytes = recvfrom(sockUDPfd, bufB, sizeof(double) *  MAXDATASIZE,
                        0, (struct sockaddr *)&B_addr, &addr_len);
    if (numbytes == -1) {
      perror("recvfrom");
      exit(1);
    }
    BCapacity = bufB[0];
    BOccupancy = bufB[1];
  } else if (strcmp(port, C_PORT) == 0) {
    numbytes = recvfrom(sockUDPfd, bufC, sizeof(double) *  MAXDATASIZE,
                        0, (struct sockaddr *)&C_addr, &addr_len);
    if (numbytes == -1) {
      perror("recvfrom");
      exit(1);
    }
    CCapacity = bufC[0];
    COccupancy = bufC[1];
  }
}

void PrintInitCapAndOcc() {
  printf("The Scheduler has received information from Hospital A: \n");
  printf("\ttotal capacity is %i\n", ACapacity);
  printf("\tinitial occupancy is %i\n", AOccupancy);

  printf("The Scheduler has received information from Hospital B: \n");
  printf("\ttotal capacity is %i\n", BCapacity);
  printf("\tinitial occupancy is %i\n", BOccupancy);

  printf("The Scheduler has received information from Hospital C: \n");
  printf("\ttotal capacity is %i\n", CCapacity);
  printf("\tinitial occupancy is %i\n", COccupancy);
}

// get sockaddr, IPv4 only
void *get_in_addr(struct sockaddr *sa) {
  return &(((struct sockaddr_in*)sa)->sin_addr);
}

/* 
 * Resets the buffers used during UDP connection.
 * Also resets the highest score and ties.
 */
void ResetBuffers() {
  bufA[0] = 0;
  bufB[0] = 0;
  bufC[0] = 0;
  high_score = -1;
  ties = 0;
}

// ----------------------------------------------------------------------------
// Main Process Functions
// ----------------------------------------------------------------------------
/*
 * This function receives the client's location.
 */
void RecvReqFromClient() {
  numbytes = recv(new_fd, buf, sizeof(double) * MAXDATASIZE_TCP, 0);
  if (numbytes == -1) {
    perror("recv");
    exit(1);
  }
  
  printf("\n\nThe Scheduler has received a request from a client: \n");
  printf("\tlocation = %i\n", (int) buf[0]);
  printf("\tclient is using TCP\n");
  printf("\tport = %s\n", TCP_PORT);
  printf("\n");
}

/*
 * This function sends the client location to the 
 * hospitals. It does NOT send client location to 
 * hospital if the hospital is at full capacity.
 */
void SendLocToHos(const char *port) {
  if (strcmp(port, A_PORT) == 0) {
    if (AOccupancy < ACapacity && ACapacity != 0) {
      bufA[0] = 0; // bufA[0] == 0 means sending request to hospitalA
      bufA[1] = buf[0];
      numbytes = sendto(sockUDPfd, bufA, sizeof(double) * MAXDATASIZE,
                        0, (struct sockaddr *)&A_addr, addr_len);
      if (numbytes == -1) {
          perror("scheduler: sendto");
          exit(1);
      }
    } else {
      bufA[0] = -1; // bufA[0] == -1 means hospitalA has no vacancies
    }
  } else if (strcmp(port, B_PORT) == 0) {
    if (BOccupancy < BCapacity && BCapacity != 0) {
      bufB[0] = 0; // bufB[0] == 0 means sending request to hospitalB
      bufB[1] = buf[0];
      numbytes = sendto(sockUDPfd, bufB, sizeof(double) * MAXDATASIZE,
                        0, (struct sockaddr *)&B_addr, addr_len);
      if (numbytes == -1) {
          perror("scheduler: sendto");
          exit(1);
      }
    } else {
      bufB[0] = -1; // bufB[0] == -1 means hospitalB has no vacancies
    }
  } else if (strcmp(port, C_PORT) == 0) {
    if (COccupancy < CCapacity && CCapacity != 0) {
      bufC[0] = 0; // bufC[0] == 0 means sending request to hospitalC
      bufC[1] = buf[0];
      numbytes = sendto(sockUDPfd, bufC, sizeof(double) * MAXDATASIZE,
                        0, (struct sockaddr *)&C_addr, addr_len);
      if (numbytes == -1) {
          perror("scheduler: sendto");
          exit(1);
      }
    } else {
      bufC[0] = -1; // bufC[0] == -1 means hospitalC has no vacancies
    }
  }
}

void PrintLocToHos() {
  if (bufA[0] != -1) {
    printf("The Scheduler has sent client location to Hospital A ");
    printf("using UDP over port %s\n", UDP_PORT);
  }
  if (bufB[0] != -1) {
    printf("The Scheduler has sent client location to Hospital B ");
    printf("using UDP over port %s\n", UDP_PORT);
  }
  if (bufC[0] != -1) {
    printf("The Scheduler has sent client location to Hospital C ");
    printf("using UDP over port %s\n", UDP_PORT);
  }
  printf("\n");
}

/*
 * This function receives scores and distances from the 
 * hospitals. It also tracks the highest score and any
 * ties if they exist. The highest score and number of
 * tise will be used in the final calculation to see
 * which hospital the client will be assigned.
 */
void RecvScores(const char *port) {
  if (strcmp(port, A_PORT) == 0) {
    if (bufA[0] != -1) {
      numbytes = recvfrom(sockUDPfd, bufA, sizeof(double) *  MAXDATASIZE,
                          0, (struct sockaddr *)&A_addr, &addr_len);
      if (numbytes == -1) {
        perror("recvfrom");
        exit(1);
      }
      if (bufA[0] > high_score) {
        high_score = bufA[0];
        ties = 0;
      } else if (bufA[0] == high_score) {
        ++ties;
      }
    }
  } else if (strcmp(port, B_PORT) == 0) {
    if (bufB[0] != -1) {
      numbytes = recvfrom(sockUDPfd, bufB, sizeof(double) *  MAXDATASIZE,
                          0, (struct sockaddr *)&B_addr, &addr_len);
      if (numbytes == -1) {
        perror("recvfrom");
        exit(1);
      }
      if (bufB[0] > high_score) {
        high_score = bufB[0];
        ties = 0;
      } else if (bufB[0] == high_score) {
        ++ties;
      }
    }
  } else if (strcmp(port, C_PORT) == 0) {
    if (bufC[0] != -1) {
      numbytes = recvfrom(sockUDPfd, bufC, sizeof(double) *  MAXDATASIZE,
                          0, (struct sockaddr *)&C_addr, &addr_len);
      if (numbytes == -1) {
        perror("recvfrom");
        exit(1);
      }
      if (bufC[0] > high_score) {
        high_score = bufC[0];
        ties = 0;
      } else if (bufC[0] == high_score) {
        ++ties;
      }
    }
  }
}

void PrintResFromHos() {
  // All hospitals were already at full capacity before client request
  if (bufA[0] == -1 && bufB[0] == -1 && bufC[0] == -1) {
    buf[0] = 0; // No Assignment
    ResetBuffers(); // To prevent accidental assignment in later functions
    return;
  }

  if (bufA[0] != -1) {
    printf("The Scheduler has received map information from Hospital A: \n");
    if (bufA[0] > 0) { printf("\tscore = %f\n", bufA[0]); }
    else { printf("\tscore = \"None\"\n"); }
    if (bufA[1] > 0) { printf("\tdistance = %f\n", bufA[1]); }
    else { printf("\tdistance = \"None\"\n"); }
  }

  if (bufB[0] != -1) {
    printf("The Scheduler has received map information from Hospital B: \n");
    if (bufB[0] > 0) { printf("\tscore = %f\n", bufB[0]); }
    else { printf("\tscore = \"None\"\n"); }
    if (bufB[1] > 0) { printf("\tdistance = %f\n", bufB[1]); }
    else { printf("\tdistance = \"None\"\n"); }
  }

  if (bufC[0] != -1) {
    printf("The Scheduler has received map information from Hospital C: \n");
    if (bufC[0] > 0) { printf("\tscore = %f\n", bufC[0]); }
    else { printf("\tscore = \"None\"\n"); }
    if (bufC[1] > 0) { printf("\tdistance = %f\n", bufC[1]); }
    else { printf("\tdistance = \"None\"\n"); }
  }
  printf("\n");
}

/* 
 * There are 6 cases to consider when assigning a hospital.
 * Case 1: Client's location is invalid
 * Case 2: All hospitals already at full capacity before client request
 * Case 3: All scores returned from hospitals are "None"
 * Case 4: All scores are unique -- assign hospital with highest score
 * Case 5: 2-way tie -- assign hospital w/ highest score & shortest distance
 * Case 6: 3-way tie -- assign hospital with shortest distance
 * --
 * Example using Hospital?'s buf?:
 * buf?[0] == -1 means hospital? capacity was full -- 'None'
 * buf?[0] ==  0 means hospital? send a score of 'None'
 * buf?[1] == -1 means location is invalid -- 'None'
 * buf?[1] ==  0 means that client is already at hospital? -- 'None'
 * --
 * high_score == 0 means all scores returned 'None'
 * high_score == -1 means that client location is invalid
 * ties == 1 means there is a tie
 * ties == 2 means there is a 3-way tie
 */
void CalculateHighestScore() {
  // Case 1: Client's location invalid
  if (bufA[1] <= 0 || bufB[1] <= 0 || bufC[1] <= 0) {
    buf[0] = -1;
    return;
  }

  // Case 2: All hospitals were already at full capacity before client request
  if (high_score == -1 && bufA[0] == -1 && bufB[0] == -1 && bufC[0] == -1) {
    buf[0] = 0;
  }

  // Case 3: All scores returned from hospitals are "None"
  else if (high_score == 0 && bufA[0] == 0 && bufB[0] == 0 && bufC[0] == 0) {
    buf[0] = 0;
    return;
  }

  // Case 4: All scores are unique
  else if (ties == 0) {
    /* This can happen when a hospital returns a score of 0 while the other 
     * two hospitals were already at full capacity and didn't participate 
     * in the client request
     */
    if (high_score == 0) {
      buf[0] = 0;
      return;
    }
    if (bufA[0] == high_score) {
      buf[0] = 1; // assignment goes to hospital A
    } else if (bufB[0] == high_score) {
      buf[0] = 2; // assignment goes to hospital B
    } else if (bufC[0] == high_score) {
      buf[0] = 3; // assignment goes to hospital C
    }
    return;
  }

  // Case 5: 2-way tie
  else if (ties == 1) {
    /* This can happen when two hospitals return score of 0 while the third 
     * was already at full capacity, and didn't participate in the client 
     * request
     */
    if (high_score == 0) {
      buf[0] = 0;
      return;
    }
    double dist1 = -1;
    double dist2, min_dist;
    for (int i = 1; i <= 3; ++i) {
      if (i == 1) { // Checking if hospital A is tied
        if (bufA[0] == high_score) { dist1 = bufA[1]; }
      } else if (i == 2) { // Checking if hospital B is tied
        if (bufB[0] == high_score && dist1 == -1) { dist1 = bufB[1]; }
        else {
          dist2 = bufB[1];
          break;
        }
      } else if (i == 3) { // Checking if hospital C is tied
        if (bufC[0] == high_score && dist1 != -1) { dist2 = bufC[1]; }
        else { printf("Panic: something went wrong with the ties = 1 !\n"); }
      }
    }
    min_dist = fmin(dist1, dist2); // Find shortest distance to break tie
    if (bufA[0] == high_score && bufA[1] == min_dist) {
      buf[0] = 1; // assignment goes to hospital A
    } else if (bufB[0] == high_score && bufB[1] == min_dist) {
      buf[0] = 2; // assignment goes to hospital B
    } else if (bufC[0] == high_score && bufC[1] == min_dist) {
      buf[0] = 3; // assignment goes to hospital C
    } else { printf("Panic: something went wrong with the ties = 1 !!\n"); }
  }

  // Case 6: 3-way tie
  else if (ties == 2) {
    // Reply with shortest distance
    double min_dist = fmin(bufA[1], bufB[1]);
    min_dist = fmin(min_dist, bufC[1]);
    if (bufA[1] == min_dist) {
      buf[0] = 1; // assignment goes to hospital A
    } else if (bufB[1] == min_dist) {
      buf[0] = 2; // assignment goes to hospital B
    } else if (bufC[1] == min_dist) {
      buf[0] = 3; // assignment goes to hospital C
    } else { printf("Panic: something went wrong with the ties = 2\n"); }
  }
}

void PrintAssignment() {
  if (buf[0] == 1) {
    printf("The Scheduler has assigned Hospital A to the client\n");
  } else if (buf[0] == 2) {
    printf("The Scheduler has assigned Hospital B to the client\n");
  } else if (buf[0] == 3) {
    printf("The Scheduler has assigned Hospital C to the client\n");
  } else {
    printf("The Scheduler has assigned \"None\" to the client\n");
  }
}

void SendResToClient() {
  if (send(new_fd, buf, sizeof(double) * MAXDATASIZE_TCP, 0) == -1)
    perror("send");
  printf("The Scheduler has sent the result to client ");
  printf("using TCP over port %s\n", TCP_PORT);
}

void SendConfirmationToHos(int *exit_code) {
  if (buf[0] == 1) {
    bufA[0] = 1;
    bufA[1] = buf[0];
    numbytes = sendto(sockUDPfd, bufA, sizeof(double) * MAXDATASIZE,
                      0, (struct sockaddr *)&A_addr, addr_len);
    if (numbytes == -1) {
        perror("scheduler: sendto");
        exit(1);
    }
    *exit_code = 1;
  } else if (buf[0] == 2) {
    bufB[0] = 1;
    bufB[1] = buf[0];
    numbytes = sendto(sockUDPfd, bufB, sizeof(double) * MAXDATASIZE,
                      0, (struct sockaddr *)&B_addr, addr_len);
    if (numbytes == -1) {
        perror("scheduler: sendto");
        exit(1);
    }
    *exit_code = 2;
  } else if (buf[0] == 3) {
    bufC[0] = 1;
    bufC[1] = buf[0];
    numbytes = sendto(sockUDPfd, bufC, sizeof(double) * MAXDATASIZE,
                      0, (struct sockaddr *)&C_addr, addr_len);
    if (numbytes == -1) {
        perror("scheduler: sendto");
        exit(1);
    }
    *exit_code = 3;
  } else {
    *exit_code = 0; // No assignment
  }
}

void PrintConfirmation() {
  if (buf[0] < -1 || buf[0] > 3) {
    printf("Panic: something went terribly wrong with result sent to client\n");
    exit(1);
  }
  if (buf[0] == 1) {
    printf("The Scheduler has sent the result to Hospital A ");
  } else if (buf[0] == 2) {
    printf("The Scheduler has sent the result to Hospital B ");
  } else if (buf[0] == 3) {
    printf("The Scheduler has sent the result to Hospital C ");
  } else {
    printf("The Scheduler did not send a result to any Hospital\n");
  }
  if (buf[0] > 0) {
    printf("using UDP over port %s\n", UDP_PORT);
  }
}

// ----------------------------------------------------------------------------
// main()
// ----------------------------------------------------------------------------
int main(int argc, char *argv[]) {

  if (argc != 1) {
    fprintf(stderr,"usage: scheduler\n");
    exit(1);
  }

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  SetUpTCP();
  SetUpUDP();

  printf("\nThe Scheduler is up and running.\n\n");

  RecvInitCapAndOcc(A_PORT);
  RecvInitCapAndOcc(B_PORT);
  RecvInitCapAndOcc(C_PORT);
  PrintInitCapAndOcc();
  
  while(1) {  // main accept() loop
    new_fd = accept(sockfd, (struct sockaddr *) &client_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue; // Accept failed, try again on next loop
    }
    inet_ntop(client_addr.ss_family,
              get_in_addr((struct sockaddr *)&client_addr),
              s, sizeof s);
    // printf("DEBUGGER: scheduler: got connection from %s\n", s);

    if (!fork()) { // this is the child process (i.e. fork() == 0)
      close(sockfd); // child doesn't need the listener

      int exit_code; // exit code that child process returns to parent

      ResetBuffers();
      RecvReqFromClient();

      // SendLocToHos() only sends requests to hospitals with vacancies
      SendLocToHos(A_PORT); 
      SendLocToHos(B_PORT); 
      SendLocToHos(C_PORT); 
      PrintLocToHos();

      // RecvScores() contains logic to track highest score and ties
      RecvScores(A_PORT);
      RecvScores(B_PORT);
      RecvScores(C_PORT);
      PrintResFromHos();
 
      CalculateHighestScore();
      PrintAssignment();
      SendResToClient();
      SendConfirmationToHos(&exit_code);
      PrintConfirmation();

      close(new_fd);
      return exit_code;
    } // End of child process if-block
    else { // This is the parent process (i.e. fork() == -1)
      int status;
      wait(&status);

      if (WIFEXITED(status)) { // Child process exited normallly
        int return_code;
        return_code = WEXITSTATUS(status);
        
        if (return_code == 1) {
          ++AOccupancy;
        } else if (return_code == 2) {
          ++BOccupancy;
        } else if (return_code == 3) {
          ++COccupancy;
        } else if (return_code == 0) {
          // No assignment; no occupancy to update
        } else {
          printf("Panic -- something went wrong with child's return code\n");
        }
      }
    }

    close(new_fd);  // Scheduler is done talking with client
  } // End of while loop

  return 0;
}
