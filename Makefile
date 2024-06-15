all:
	g++ -o test bandwidth_test.cc -lnuma -lpthread
	./test 
