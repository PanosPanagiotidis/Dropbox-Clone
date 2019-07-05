CC=g++
CPPFLAGS = -Wall -std=c++11
.PHONY: run1 run2 run3

all:mirror_client sender receiver

mirror_client: mirror_client.o mirror_functions.o
	$(CC) $(CPPFLAGS) -o mirror_client.out mirror_client.o mirror_functions.o

mirror_client.o: mirror_client.cpp
	$(CC) $(CPPFLAGS) -c mirror_client.cpp 

mirror_functions.o: mirror_functions.cpp
	$(CC) $(CPPFLAGS) -c  mirror_functions.cpp

sender: sender.cpp 
	$(CC) $(CPPFLAGS) -o sender.out sender.cpp

receiver: receiver.cpp 
	$(CC) $(CPPFLAGS) -o receiver.out receiver.cpp

run1:
	./mirror_client.out -n 1 -c ./common -i ./input1 -m ./mirror1 -b 100 -l log_file1

run2:
	./mirror_client.out -n 2 -c ./common -i ./input2 -m ./mirrors/mirror2 -b 200 -l log_file2

run3:
	./mirror_client.out -n 2 -c ./common -i ./input3 -m ./mirrors/mirror3 -b 400 -l log_file3

clean:
	rm -rf *.o
	rm -rf *.out
	bash clear_mirrors.sh
