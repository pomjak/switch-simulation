CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall
LDFLAGS = -lsimlib -lm

SRC = model.cc
HEADERS = model.h
EXEC = model
EXPERIMENTS = exp1 exp2
FOLDER = experiments

all: 
	$(CXX) $(CXXFLAGS) $(SRC) -o $(EXEC) $(LDFLAGS)

run : $(EXPERIMENTS)

create-folder:
	mkdir -p $(FOLDER)

exp1: create-folder all
	./model -t 1 -g 1 -l 1 -s 64 -o experiments/exp1.out

exp2 : create-folder all
	./model -t 0.1 -g 1 -o experiments/exp2.out

clean:
	rm -f $(OBJS) $(EXEC) $(FOLDER)/*.out model.out xpomsa00.zip
	rm -rf $(FOLDER)

zip: 
	zip xpomsa00.zip $(SRC) $(HEADERS) Makefile

.PHONY: all $(EXPERIMENTS)
