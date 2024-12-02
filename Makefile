CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall
LDFLAGS = -lsimlib -lm

SRC = model.cc
HEADERS = model.h
EXEC = model
EXPERIMENTS = exp1 exp2 exp3 exp4
FOLDER = experiments

all: 
	$(CXX) $(CXXFLAGS) $(SRC) -o $(EXEC) $(LDFLAGS)

run : clean $(EXPERIMENTS)

create-folder:
	mkdir -p $(FOLDER)

exp: create-folder all 
	./model -t 1 -c 5 -l 1 -g 1 -s 64 -o experiments/datasheet_validation.out

exp2 : create-folder all 
	./model -t 1 -c 10000 -l 1 -g 1 -s 64 -o experiments/mac_floods.out

exp3: create-folder all
	./model -o experiments/normal_traffic.out

exp4: create-folder all
	./model -l 1 -g 5 -e 1 -o experiments/single_encapsulate.out
	./model -l 1 -g 5 -e 2 -o experiments/double_encapsulate.out

exp5: create-folder all
	./model -l 1 -g 5 -s 64 -t 0.1 -o experiments/tail_drop_base.out
	./model -l 1 -g 5 -s 64 -t 0.1 -e 2 -o experiments/tail_drop_double_enc.out
	./model -l 1 -g 5 -s 64 -t 0.1 -e 5 -o experiments/tail_drop_enc_per_port.out

clean:
	rm -f $(OBJS) $(EXEC) model.out xpomsa00.zip
	rm -rf $(FOLDER)

zip: 
	zip xpomsa00.zip $(SRC) $(HEADERS) Makefile

.PHONY: all $(EXPERIMENTS)
