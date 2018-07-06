include /usr/local/etc/PcapPlusPlus.mk

all:
	g++ $(PCAPPP_INCLUDES) -std=c++11 -o sin.o -c sin.cpp
	g++ $(PCAPPP_LIBS_DIR) -static-libstdc++ -o sin sin.o $(PCAPPP_LIBS)

clean:
	rm sin.o
	rm sin
