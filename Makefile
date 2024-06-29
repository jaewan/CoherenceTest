all:
	#g++ -o test bandwidth_test.cc -lnuma -lpthread
	#g++ -std=c++20 -o test atomic_test.cc -lnuma -lpthread
	#g++ -std=c++20 -o test -I ../Embarcadero/third_party/cxxopts/include false_sharing.cc -lnuma -lpthread
	g++ -std=c++20 -o test -I ../Embarcadero/third_party/cxxopts/include coherence.cc -lpthread
	./test -r 1
coherence:
	g++ -std=c++20 -o test -I ../Embarcadero/third_party/cxxopts/include coherence_test.cc -lpthread
	./test -r 0
