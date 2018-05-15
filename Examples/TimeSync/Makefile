include /usr/local/etc/PcapPlusPlus.mk

all:
	g++ $(PCAPPP_INCLUDES) -std=c++11 -o timesync.o -c timesync.cpp
	g++ $(PCAPPP_LIBS_DIR) -static-libstdc++ -o timesync timesync.o $(PCAPPP_LIBS)

clean:
	rm timesync.o
	rm timesync
