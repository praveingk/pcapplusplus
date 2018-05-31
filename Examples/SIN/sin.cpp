#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <memory>
#if defined(WIN32) || defined(WINx64)
#include <winsock2.h>
#endif
#include <MacAddress.h>
#include <IpAddress.h>
#include <PcapPlusPlusVersion.h>
#include <SystemUtils.h>
#include <PlatformSpecificUtils.h>
#include <PcapLiveDeviceList.h>
#include <PcapLiveDevice.h>
#include <EthLayer.h>
#include <TsLayer.h>
#include <SINLayer.h>
#include <TimeSyncLayer.h>
#include <ArpLayer.h>
#include <Logger.h>
#include <time.h>
#include <DpdkDeviceList.h>
#include <QmetadataLayer.h>
#include <IPv4Layer.h>
#include <UdpLayer.h>
#include <PayloadLayer.h>
#include <iostream>
#include <thread>
#include <signal.h>

#if !defined(WIN32) && !defined(WINx64) //for using ntohl, ntohs, etc.
#include <in.h>
#endif
#include <getopt.h>

using namespace std::chrono;
using namespace std;
using namespace pcpp;

#define COMMAND_STARTSORT 0x1
#define COMMAND_ENDSORT 0x2





#define MBUFF_POOL_SIZE 1023  // (2^10 - 1) allow DPDK to hold these many packets in memory (at max).
                              // See "sending algorithm" in DpdkDevice.h
#define CORE_MASK 341  // in binary it is 101010101. Meaning core # 8,6,4, 2 and 0 would be given to DPDK
                       // core 0 would be used as the DPDK master core by default. To change this, need to
                       // change DpdkDeviceList::initDpdk() in DpdkDeviceList.cpp and rebuild PcapPlusPlus


int TIMESYNC_DPDK_PORT = 0;
#define TIMESYNC_THREAD_CORE 18  // vcpu # (as shown by lstopo). This is the last core on socket #0
#define TIMESYNC_RECEIVE_CORE 14
int CROSSTRAFFIC_DPDK_PORT = 0;
#define CROSSTRAFFIC_THREAD_CORE 16  // vcpu # (as shown by lstopo). This is the second last core on socket #0
#define MTU_LENGTH 1500
#define SEND_RATE_SKIP_PACKETS 318 // helps reduce send rate to 5 Gbps
#define DEFAULT_TTL 12

#define NUM_BURSTS 5  // the bursts are separated by 1ms gap
#define NUM_PKTS_IN_BURST 42

FILE *timesync_log;
string iface = "";
static struct option L3FwdOptions[] =
{
	{"interface",  required_argument, 0, 'i'},
	{"reset_time",  no_argument, 0, 'p'},
	{"timesync",  no_argument, 0, 't'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'v'},
    {0, 0, 0, 0}
};


void printUsage() {
 printf("\nUsage:\n"
		 "------\n"
		 "%s [-hv] -i interface_ip -c victim_ip -g gateway_ip\n"
		 "\nOptions:\n\n"
		 "    -i interface_ip   : The IPv4 address of interface to use\n"
		 "    -h                : Displays this help message and exits\n"
		 "    -v                : Displays the current version and exists\n", AppName::get().c_str());

 exit(0);
}


/**
* Print application version
*/
void printAppVersion()
{
 printf("%s %s\n", AppName::get().c_str(), getPcapPlusPlusVersionFull().c_str());
 printf("Built: %s\n", getBuildDateTime().c_str());
 printf("Built from: %s\n", getGitInfo().c_str());
 exit(0);
}


void do_sin(PcapLiveDevice* pDevice) {

    pcpp::Packet newPacket(MTU_LENGTH);
    pcpp::MacAddress srcMac("3c:fd:fe:b7:e7:f4");
    pcpp::MacAddress dstMac("09:0e:10:11:12:13");
    pcpp::IPv4Address srcIP(std::string("20.1.1.2"));
    pcpp::IPv4Address dstIP(std::string("40.1.1.2"));
    uint16_t srcPort = 0x2341;
    uint16_t dstPort = 7777;
    uint8_t command = COMMAND_STARTSORT;
    uint8_t num_entries = 10;
    uint8_t keys[] = {100, 70, 30, 80, 40, 90, 20, 120, 10, 85};

    pcpp::EthLayer newEthLayer(srcMac, dstMac, PCPP_ETHERTYPE_IP); // 0x0800 -> IPv4
    pcpp::IPv4Layer newIPv4Layer(srcIP, dstIP);
    newIPv4Layer.getIPv4Header()->timeToLive = DEFAULT_TTL;
    pcpp::UdpLayer newUDPLayer(srcPort, dstPort);
    pcpp::SINLayer newSINLayer(COMMAND_STARTSORT, num_entries, keys);

    int length_so_far = newEthLayer.getHeaderLen() + newIPv4Layer.getHeaderLen() + 
                        newUDPLayer.getHeaderLen() + newSINLayer.getHeaderLen();
    //int payload_length = ;

    printf("Header length is %d\n", length_so_far);
    
    // uint8_t payload[payload_length];
    // int datalen = 4;
    // char data[datalen] = {'D','A','T','A'};
    // // Put the data values into the payload
    // int j = 0;
    // for(int i=0; i < payload_length; i++){
    //     payload[i] = (uint8_t) int(data[j]);
    //     j = (j + 1) % datalen;
    // }

    // construct the payload layer
    //pcpp::PayloadLayer newPayLoadLayer(payload, payload_length, false);

    newPacket.addLayer(&newEthLayer);
    newPacket.addLayer(&newIPv4Layer);
    newPacket.addLayer(&newUDPLayer);
    newPacket.addLayer(&newSINLayer);

    // compute the calculated fields
    newUDPLayer.computeCalculateFields();
    newUDPLayer.calculateChecksum(true);
    newIPv4Layer.computeCalculateFields(); // this takes care of the IPv4 checksum


    pDevice->sendPacket(&newPacket);

}

int main(int argc, char* argv[])
{
	AppName::init(argc, argv);
	int DPDK = -1;
	//Get arguments from user for incoming interface and outgoing interface

	string source_mac = "", dest_mac = "";
	int reset_time = 0;
	int timesync = 0;
	int optionIndex = 0;
	char opt = 0;
	while((opt = getopt_long (argc, argv, "i:hv", L3FwdOptions, &optionIndex)) != -1)
	{
		switch (opt)
		{
			case 0:
				break;
			case 'i':
				iface = optarg;
				break;
			case 'r':
				break;
			case 'h':
				printUsage();
				break;
			case 'v':
				printAppVersion();
				break;
			default:
				printUsage();
				exit(-1);
		}
	}

	//Both incoming and outgoing interfaces must be provided by user
	if(iface == "")
	{
		printf("No Interface specified, Trying to use dpdk\n");
	}


  // Never Reached !!!
	//IPv4Address ifaceAddr(iface);
	printf("%s\n", iface.c_str());
	PcapLiveDevice* pIfaceDevice = PcapLiveDeviceList::getInstance().getPcapLiveDeviceByName(iface);
	if (DPDK == -1) {
		if (pIfaceDevice == NULL)
		{
			printf("Cannot find interface. Exiting...\n");
			exit(-1);
		}
		if (!pIfaceDevice->open())
		{
			printf("Cannot open interface. Exiting...\n");
			exit(-1);
		}

		do_sin(pIfaceDevice);

	}
}
