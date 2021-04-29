CC = g++ --std=c++11
all:
	make saladmaker chef
saladmaker:
	$(CC) saladmaker.cpp -o saladmaker.o -lpthread
chef:
	$(CC) chef.cpp -o chef.o -lpthread
clean:
	rm chef.o saladmaker.o log.txt
	touch log.txt