all:
	g++ -o atomic_test atomic_test.cc -lnuma -lpthread
	./atomic_test 
