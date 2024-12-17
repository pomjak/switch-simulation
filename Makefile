CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall
LDFLAGS = -lsimlib -lm

SRC = model.cc
HEADERS = model.h
EXEC = model
EXPERIMENTS = exp1 exp2 exp3 exp4 exp5
FOLDER = experiments

all: 
	$(CXX) $(CXXFLAGS) $(SRC) -o $(EXEC) $(LDFLAGS)

run: all
	./model

exp : clean $(EXPERIMENTS)

create-folder:
	mkdir -p $(FOLDER)

exp1: create-folder all 
	./model -t 1 -c 5 -l 1 -g 1 -s 64 -o experiments/1-datasheet_validation.out

exp2 : create-folder all 
	./model -t 1 -c 10000 -l 1 -g 1 -s 64 -o experiments/2-mac_floods.out

exp3: create-folder all
	./model -o experiments/3-normal_traffic.out

exp4: create-folder all
	./model -l 1 -g 5 -e 1 -o experiments/4-single_encapsulate.out
	./model -l 1 -g 5 -e 2 -o experiments/4-double_encapsulate.out

exp5: create-folder all
	./model -l 1 -g 5 -s 64 -t 0.1 -o experiments/5-tail_drop_base.out
	./model -l 1 -g 5 -s 64 -t 0.1 -e 2 -o experiments/5-tail_drop_double_enc.out
	./model -l 1 -g 5 -s 64 -t 0.1 -e 5 -o experiments/5-tail_drop_enc_per_port.out
	

clean:
	rm -f $(OBJS) $(EXEC) model.out 06_xpomsa00_xcagal00.zip
	rm -rf $(FOLDER)

zip: 
	zip 06_xpomsa00_xcagal00.zip $(SRC) $(HEADERS) Makefile experiments/* simulation_study.pdf

.PHONY: all $(EXPERIMENTS)
