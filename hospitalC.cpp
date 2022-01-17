#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <bits/stdc++.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

using namespace std;

#define MY_PORT    "32570"    // my port number for UDP connection
#define SCHED_PORT "33570"    // scheduler's port number for UDP connection
#define MAXDATASIZE   2       // max number of bytes we can get at once (UDP)

// Map variables
unordered_map<int, int> rebase; // map <original location, rebased location>
int rebase_counter;
double **dynamicArray; // pointer to adjacency matrix
double *dist;          // pointer to shortest paths array

// Socket variables
int sockfd;
struct addrinfo hints, *servinfo, *p;
struct sockaddr *their_addr; // connector's address information
socklen_t addr_len;
int rv;
struct sigaction sa;

// Process variables
int my_loc;
int cap;
int occ;
double buf[MAXDATASIZE];
int numbytes;

void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;
  while(waitpid(-1, NULL, WNOHANG) > 0);
  errno = saved_errno;
}

void CreateDymArr() {
  // memory allocated for elements of rows
  dynamicArray = new double *[rebase_counter];
  
  // memory allocated for elements of each column
  for(int i = 0; i < rebase_counter; i++) {
      dynamicArray[i] = new double[rebase_counter];
  }

  // set every location in dynamic array to 0
  for (int u = 0; u < rebase_counter; ++u) {
    for (int v = 0; v < rebase_counter; ++v) {
      dynamicArray[u][v] = 0;
    }
  }
}

void CreateAdjMatx() {
  string line;
  ifstream fin("map.txt");

  // Rebase all locations
  while (getline(fin, line)) {
    istringstream sin(line);
    int loc1, loc2;
    sin >> loc1;
    sin >> loc2;
    if (rebase.find(loc1) == rebase.end()) {
      rebase.insert(make_pair(loc1, rebase_counter++));
    }
    if (rebase.find(loc2) == rebase.end()) {
      rebase.insert(make_pair(loc2, rebase_counter++));
    }
  }

  CreateDymArr();

  fin.clear();  // Clear eof and fail flags
  fin.seekg(0); // Move cursor to beginning of file

  // Fill in adjacency matrix with rebased locations
  while (getline(fin, line)) {
    istringstream sin(line);
    int loc1, loc2;
    double d;
    sin >> loc1;
    sin >> loc2;
    sin >> d;
    dynamicArray[rebase.at(loc1)][rebase.at(loc2)] = d;
    dynamicArray[rebase.at(loc2)][rebase.at(loc1)] = d;
  }

  fin.close();
}

void SetUpUDP() {
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(NULL, SCHED_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through all the results and make a socket
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("hospitalC: socket");
      continue; // Try to create socket on next iteration
    }
    break; // Found suitable socket
  }

  if (p == NULL) {
    fprintf(stderr, "hospitalC: failed to create socket\n");
    exit(1);
  }

  their_addr = p->ai_addr;
  addr_len = p->ai_addrlen;;
  freeaddrinfo(servinfo);
}

void PrintBootupMsg() {
  printf("\nHospital C is up and running using UDP on port %s.\n", MY_PORT);
  printf("Hospital C has: \n");
  printf("\ttotal capacity %i\n", cap);
  printf("\tinitial occupancy %i\n", occ);
}

int minDist(bool seen[]) {
  double min = INT_MAX;
  int min_vertex;
  for (int v = 0; v < rebase_counter; ++v) {
    if (seen[v] == false && dist[v] <= min) {
      min = dist[v];
      min_vertex = v;
    }
  }
  return min_vertex;
}

void Dijkstra(int src) {
  int rebased_src;
  if (rebase.find(src) != rebase.end()) {
    rebased_src = rebase.at(src);
  } else { 
    rebased_src = -1;
    printf("Panic - hospitalC location not in map!!!\n");
    exit(1);
  }

  bool seen[rebase_counter];
  // memory allocated for elements of rows
  dist = new double [rebase_counter];

  for (int i = 0; i < rebase_counter; ++i) {
    dist[i] = INT_MAX;
    seen[i] = false;
  }

  dist[rebased_src] = 0; // Shortest path to myself is 0

  for (int i = 0; i < rebase_counter; ++i) {
    int u = minDist(seen);
    seen[u] = true;

    for (int v = 0; v < rebase_counter; ++v) {
      if (!seen[v] && dynamicArray[u][v] && dist[u] != INT_MAX
          && dist[u] + dynamicArray[u][v] < dist[v]) {
        dist[v] = dist[u] + dynamicArray[u][v];
      }
    } // End inner for loop
  } // End outer for loop
}

void SendTo(double d1, double d2) {
  buf[0] = d1;
  buf[1] = d2;
  numbytes = sendto(sockfd, buf, sizeof(double) * MAXDATASIZE,
                    0, (struct sockaddr *) their_addr, addr_len);
  if (numbytes == -1) {
      perror("hospitalC: sendto");
      exit(1);
  }
}

void CalculateScore() {
  // my_loc is hospitalC location
  int client_loc = (int) buf[1];

  // Invalid location -- not in map
  if (rebase.find(client_loc) == rebase.end()) {
    printf("Hospital C does not have the location %i in map\n", client_loc);
    SendTo(0, -1); // return to scheduler immediately
    printf("Hospital C has sent \"location not found\" to the Scheduler\n");
    return;
  }

  int rebased_client_loc = rebase.at(client_loc);

  // Invalid location -- d == 0
  if (rebased_client_loc == rebase.at(my_loc)) {
    printf("Hospital C is at same location as client\n");
    SendTo(0, 0); // return to scheduler immediately
    printf("Hospital C has sent distance = \"None\" to the Scheduler\n");
  }

  double distance = dist[rebased_client_loc];
  double availability = (cap - occ) / (double) cap;
  printf("Hospital C has capacity = %i, occupation = %i, ", cap, occ);
  if (availability < 0 || 1 < availability) {
    availability = 0;
    printf("availability = \"None\"\n");
  } else {
    printf("availability = %f\n",  availability);
  }
  printf("Hospital C has found the shortest path to client, distance = %f\n",
         distance);

  double score;
  if (distance != 0 && availability != 0) {
    score = 1 / (distance * (1.1 - availability));
    printf("Hospital C has the score = %f\n", score);
  } else {
    score = 0;
    printf("Hospital C has the score = \"None\"\n");
  }

  SendTo(score, distance);
  printf("Hospital C has sent to the Scheduler: \n");
  if (score != 0) {
    printf("\tscore = %f\n", score);
  } else {
    printf("\tscore = \"None\"\n");
  }
  if (distance != 0) {
    printf("\tdistance = %f\n", distance);
  } else {
    printf("\tdistance = \"None\"\n");
  }
}

void RecvInfo() {
  numbytes = recvfrom(sockfd, buf, sizeof(double) * MAXDATASIZE,
                      0, (struct sockaddr *) their_addr, &addr_len);
  if (numbytes == -1) {
    perror("recvfrom");
    exit(1);
  }

  if (buf[0] == 0) { // Receive request for score and distance from scheduler
    printf("\n\nHospital C has received input from client at location %i\n", 
           (int) buf[1]);
    CalculateScore();
  } else if (buf[0] == 1) { // Receive confirmation of client assignment
    ++occ;
    double availability = (cap - occ) / (double) cap;
    printf("Hospital C has been assigned to a client: \n");
    printf("\toccupation is updated to %i\n", occ);
    printf("\tavailability is updated to %f\n", availability);
  } else {
    printf("Panic: hospitalC received an unknown message from the Scheduler\n");
  }
}

void DeallocateMemory() {
  // memory deallocated for elements of each column
  for (int i = 0; i < rebase_counter; i++) {
      delete [] dynamicArray[i];
  }

  // memory deallocated for elements of rows
  delete[] dynamicArray;

  // memory deallocated for dist array
  delete[] dist;
}

int main(int argc, char *argv[]) {

  if (argc != 4) {
    fprintf(stderr,"usage: hospitalC <location C> ");
    fprintf(stderr, "<capacity C> <initial occupancy C>\n");
    exit(1);
  }

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  rebase_counter = 0;
  CreateAdjMatx(); // Creates adjacency matrix with rebased locations
  SetUpUDP();

  my_loc = (int) atoi(argv[1]); // my location
  cap = (int) atoi(argv[2]);    // total capacity
  occ = (int) atoi(argv[3]);        // initial capacity
  Dijkstra(my_loc); // Calcualte shortest paths from hospitalC
  PrintBootupMsg();
  SendTo((double) cap, (double) occ);

  while (1) {
    RecvInfo();
  }

  close(sockfd);
  DeallocateMemory();
  return 0;
}
