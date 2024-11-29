CXXFLAGS = -std=c++11 -O2 -Wall
LINKS = -lsimlib -lm

all:
	g++ $(CXXFLAGS) model.cc -o model $(LINKS)

.PHONY: all