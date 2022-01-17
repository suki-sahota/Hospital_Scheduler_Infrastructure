# Author: Suki Sahota
#
# This is the Makefile that can be used to create the 
# scheduler, hospital?, and client  executables.
#
# To create all executables, do:
#	make all
#
# To create scheduler executable, do:
#	make scheduler
#
# To create hospitalA executable, do:
#	make hospitalA
#
# To create hospitalB executable, do:
#	make hospitalB
#
# To create hospitalC executable, do:
#	make hospitalC
#
# To create client executable, do:
#	make client
#
# To clean project, do:
#	make clean
#
CXXFLAGS = -ggdb -Wall -std=c++11

all: scheduler hospitalA hospitalB hospitalC client

scheduler: scheduler.cpp
	g++ $(CXXFLAGS) -o scheduler scheduler.cpp
hospitalA: hospitalA.cpp
	g++ $(CXXFLAGS) -o hospitalA hospitalA.cpp
hospitalB: hospitalB.cpp
	g++ $(CXXFLAGS) -o hospitalB hospitalB.cpp
hospitalC: hospitalC.cpp
	g++ $(CXXFLAGS) -o hospitalC hospitalC.cpp
client: client.cpp
	g++ $(CXXFLAGS) -o client client.cpp

clean:
	rm -f *.o *.out scheduler hospital? client

