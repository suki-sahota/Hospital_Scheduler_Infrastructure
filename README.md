# README #

### Description ###

This is a hospital scheduling system that uses three hospitals and a central server known as the scheduler to schedule appointments for a client. It works by having a client submit a request with their location to the scheduler. The scheduler then sends a request to each of the three hospitals. The hospitals in return, send a score back to the scheduler indicating how well they can server the client based on several markers (location, occupancy, capacity). Then the scheduler assigns a hospital to the client based on the hospital who sent the highest score.

### How do I get set up? ###

#### Summary of set up ####

Make all executable files and execute each of the programs in it's own commandline/terminal window in this order: 

scheduler.cpp  
hospitalA.cpp  
hospitalB.cpp  
hospitalC.cpp  
client.cpp  

***Make file instructions***  
Run "***make ***" or "***make all ***" to make all five executables.  
Run "***make clean ***" to remove all executables and .o and .out files.

***Usage***

* ./scheduler  
* ./hospitalA <location A> <capacity A> <initial occupancy A>  
* ./hospitalB <location B> <capacity B> <initial occupancy B>  
* ./hospitalC <location C> <capacity C> <initial occupancy C>  
* ./client <client location>  

Note: client can be run multiple times while scheduler and hospitals are up and running

***Exit from program***  
The client exits after receiving a response from the scheduler.  
The scheduler and hospitals can exit from execution by pressing <Ctrl>-C

***Configuration***  
Include a valid map.txt file in the same level directory as the hospital files. There may be multiple map files but only the one named explicitly as "map.txt" will be used during execution of the program.

***Dependencies***  
There are no dependencies in this project aside from the include statements contained within the c++ files

***Database configuration***  
There is no database configuration required

### How it works ###

The client talks to the scheduler using TCP socket communication. The scheduler talks to each of the three hospitals using UDP communication. Since the scheduler and the hospitals are activing as servers in this system, they all have pre-defined port numbers for TCP and UDP communication; however, the client is assigned a TCP port number dynamically when he sends a request to the scheduler.

The client location is checked against a map.txt file that the hospitals use Dijkstra's algorithm to find the shortest path from the client to their location. The hospitals then use a simple formula to find score ((1 / d * (1.1 - a)) where d = shortest diance to client and a = availability ((cap - occ) / cap)).

The project is run as five different programs, where the scheduler, each of the three hospitals, and the client get their own terminal window and program execution.

### Assigning a hospital ###

When the scheduler assigns a hospital, it chooses the hospital that sent back the highest score. If scores are tied, then it chooses the hospital with the shortest distance. If distances are also tied, ***A is assigned before B, and B is assigned before C***.

There are 6 cases to consider when assigning a hospital.

Case 1: Client's location is invalid

Case 2: All hospitals already at full capacity before client request

Case 3: All scores returned from hospitals are "None"

Case 4: All scores are unique -- assign hospital with highest score

Case 5: 2-way tie -- assign hospital w/ highest score & shortest distance

Case 6: 3-way tie -- assign hospital with shortest distance

#### Data Structures used ####

The scheduler has a double array with size of two for each hospital. Hospital A has bufA[2], Hospital B has bufB[2], and Hospital C has bufC[2]. This buffer contains all of the information sent or received by the scheduler to the respective hospital. When assigning a score, the scheduler reads each buffer as such:

(Example using Hospital#'s buf#):

buf#[0] == -1 means hospital capacity was full -- 'None'

buf#[0] ==  0 means hospital send a score of 'None'

buf#[1] == -1 means location is invalid -- 'None'

buf#[1] ==  0 means that client is already at hospital -- 'None'


The scheduler also keeps track of the highest score and how many ties there are for the highest score. This is used for figuring out which hospital is assigned to the client.

highscore == 0 means all scores returned 'None'

highscore == -1 means that client location is invalid

ties == 1 means there is a tie

ties == 2 means there is a 3-way tie

### Program Responsibilities ###

#### scheduler.cpp ###

The scheduler is responsible for all communication in this project. It is located in the center of the network. Since the scheduler is critical in all communication, it is the first program to be run in the program execution list. The scheduler first boots up and creates a socket for TCP and creates a socket for UDP communication, and then listens for communication from the hospitals. Once the scheduler receives initial occupancy and capacity numbers from the hospitals, the scheduler begins receiving client requests. 

When a client request is received by the scheduler, the scheduler checks which of the three hospitals have spare capacity to serve the client. The scheduler sends the client request to each of the hospitals that have spare capacity to the server the client. Upon recepit of the hospital response, the scheduler then picks the hospital with the highest score and assigns that hospital to the client. A printout message is printed along every step of the way. Finally, if a hospital was assigned to a client, then the scheduler sends Confirmation of assignment to that respective hospital so that the hospital can update it's internal occupancy numbers. The scheduler also updates it's occupancy number for the hospital that was assigned to the client. The child process talks to the parent process via a return code that is then captured in a status variable with wait() system call. This tells the parent process which hospital needs to update its occupancy.

#### hospitalA.cpp ###

Hospital A starts program execution by reading the map.txt file of the Los Angeles area. Hospital A stores each of the locations along with the distances between individual points. Then Hospital A runs  Dijkstra's Algorithm to find the shortest distance from each location to Hospital A's location. After Hospital A has found the shortest location from it to every other location *** in the map ***, Hospital A establishes connection with a UDP socket to the scheduler and sends it's initial occupancy and capacity information to the scheduler.

After the initial Boot up phase, Hospital A waits for a request from the scheduler on behalf of a client. Hospital A receives the request and conducts a quick analysis to calculate a score based on it's availability and distance to client. Then the hospital sends the score and distance it calculated to the scheduler. Then the hospital waits for either a confirmation of assignment to client, or another client request. 

#### hospitalB.cpp ###

Hospital B starts program execution by reading the map.txt file of the Los Angeles area. Hospital B stores each of the locations along with the distances between individual points. Then Hospital B runs  Dijkstra's Algorithm to find the shortest distance from each location to Hospital B's location. After Hospital B has found the shortest location from it to every other location *** in the map ***, Hospital B establishes connection with a UDP socket to the scheduler and sends it's initial occupancy and capacity information to the scheduler.

After the initial Boot up phase, Hospital B waits for a request from the scheduler on behalf of a client. Hospital B receives the request and conducts a quick analysis to calculate a score based on it's availability and distance to client. Then the hospital sends the score and distance it calculated to the scheduler. Then the hospital waits for either a confirmation of assignment to client, or another client request. 


#### hospitalC.cpp ###

Hospital C starts program execution by reading the map.txt file of the Los Angeles area. Hospital C stores each of the locations along with the distances between individual points. Then Hospital C runs  Dijkstra's Algorithm to find the shortest distance from each location to Hospital C's location. After Hospital C has found the shortest location from it to every other location *** in the map ***, Hospital C establishes connection with a UDP socket to the scheduler and sends it's initial occupancy and capacity information to the scheduler.

After the initial Boot up phase, Hospital C waits for a request from the scheduler on behalf of a client. Hospital C receives the request and conducts a quick analysis to calculate a score based on it's availability and distance to client. Then the hospital sends the score and distance it calculated to the scheduler. Then the hospital waits for either a confirmation of assignment to client, or another client request. 

#### client.cpp ###

The client starts execution with it's location and terminates execution after receiving a response from the scheduler. There is separation from the client and the hospitals so that the client never has to talk directly to the hospitals. To the client, the hospitals are completely transparent. Upon receipt of a response from the scheduler, the client will be notified if it is assigned to one of the three hospitals and terminate connection with the scheduler. 

Either the client is assigned to a hospital, or it is not. If a client's location is not in the map, it will not receive an assignment. If the client is already at the location of one of the three hospitals, it will not receive an assignment from any of the three hospitals. If all three hospitals are full (full capacity), then the client will not be assigned to any of the three hospitals. So it is not guaranteed that the client will be assigned to any hospital when requesting assignment from the scheduler.

### Format of messages exchanged ###

The format for messages exchanged is a line of text followed by a list of information. <indent> will show up as indent in the program output. The following shows example output:

Hospital A has:  
<indent>total capacity 5  
<indent>initial occupancy 3

The Scheduler has received information from Hospital A:  
<indent>total capacity is 5  
<indent>initial occupancy is 3

The Scheduler has received a request from a client:   
<indent>location = 8  
<indent>client is using TCP  
<indent>port = 34570

Hospital B has sent to the Scheduler:  
<indent>score = 0.153846  
<indent>distance = 13.000000

The Scheduler has received map information from Hospital A:  
<indent>score = 0.119048  
<indent>distance = 12.000000

### Contribution guidelines ###

*** Dijkstra's Algorithm *** was used to find the shortest paths from each of the hospital's locations. Dijkstra's Algorithm is well-known and fairly standard. A reference from Wikipedia was used to run the algorithm in each of the hosital's programs.

### Final Remarks ###

This project is intended for a Linux (Ubuntu) environment.

### Who do I talk to? ###

*** Suki Sahota *** is the author of this multi-program project and is the only point-of-contact for all inquiries about implementation, bugs, etc.
