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
#include <TimeSyncLayer.h>
#include <ArpLayer.h>
#include <Logger.h>
#include <time.h>
#if !defined(WIN32) && !defined(WINx64) //for using ntohl, ntohs, etc.
#include <in.h>
#endif
#include <getopt.h>

using namespace std;
using namespace pcpp;

#define COMMAND_TIMERESET 0x1
#define COMMAND_TIMESYNC_REQ 0x2

static struct option L3FwdOptions[] =
{
	{"interface",  required_argument, 0, 'i'},
	{"victim", required_argument, 0, 'c'},
	{"gateway", required_argument, 0, 'g'},
	{"reset_time",  no_argument, 0, 'p'},
	{"timesync",  no_argument, 0, 't'},
	{"receive", no_argument, 0, 'r'},
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
		 "    -p timesync   : Send the current time out\n"
		 "    -t timesync   : Get the current time\n"
		 "    -c victim_ip      : The IPv4 address of the victim\n"
		 "    -g gateway_ip     : The IPv4 address of the gateway\n"
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

/**
 * A callback function for the async capture which is called each time a packet is captured
 */
// static void receivePackets(pcpp::RawPacket* packet, pcpp::PcapLiveDevice* dev, void* cookie)
// {
// 	// parsed the raw packet
// 	printf("Got TS Packet");
//
// 	pcpp::Packet parsedPacket(packet);
// 	TsLayer* tsLayer = parsedPacket.getLayerOfType<TsLayer>();
//
// 	printf("%s", tsLayer->toString().c_str());
//
// }

void do_receive(PcapLiveDevice* dev) {
	ProtoFilter protocolFilter(TS);
	if (!dev->setFilter(protocolFilter)) {
		printf("Cannot set TS filter on device. Exiting...\n");
		exit(-1);
	}
	printf("Waiting for Timsync packets...\n");

	pcpp::RawPacketVector packetVec;

	dev->startCapture(packetVec);
	PCAP_SLEEP(10);
	dev->stopCapture();


	for (pcpp::RawPacketVector::ConstVectorIterator iter = packetVec.begin(); iter != packetVec.end(); iter++)
	{
		// parse raw packet
		pcpp::Packet parsedPacket(*iter);
		if (parsedPacket.isPacketOfType(pcpp::TS)) {
			TsLayer* tsLayer = parsedPacket.getLayerOfType<TsLayer>();
			tsLayer->dumpString();
			//printf("%s", tsLayer->toString().c_str());
		}
	}

	// int res = dev->startCapture(receivePackets, NULL);
	// if (!res) {
	// 	printf("Cannot start capturing packets\n");
	// 	exit(-1);
	// }

}

void do_receive_timesync(PcapLiveDevice* dev) {
	ProtoFilter protocolFilter(TIMESYNC);
	if (!dev->setFilter(protocolFilter)) {
		printf("Cannot set TIMESYNC filter on device. Exiting...\n");
		exit(-1);
	}
	printf("Waiting for Timsync packets...\n");

	pcpp::RawPacketVector packetVec;

	dev->startCapture(packetVec);
	PCAP_SLEEP(2);
	dev->stopCapture();


	for (pcpp::RawPacketVector::ConstVectorIterator iter = packetVec.begin(); iter != packetVec.end(); iter++)
	{
		// parse raw packet
		pcpp::Packet parsedPacket(*iter);
		if (parsedPacket.isPacketOfType(pcpp::TIMESYNC)) {
			TimeSyncLayer* tsLayer = parsedPacket.getLayerOfType<TimeSyncLayer>();
			tsLayer->dumpString();
			//printf("%s", tsLayer->toString().c_str());
		}
	}

	// int res = dev->startCapture(receivePackets, NULL);
	// if (!res) {
	// 	printf("Cannot start capturing packets\n");
	// 	exit(-1);
	// }

}


void do_reset_time(PcapLiveDevice* pDevice) {
	struct timespec tsp;
	pcpp::Packet newPacket(100);
	pcpp::EthLayer newEthernetLayer(pDevice->getMacAddress(), pcpp::MacAddress("ff:ff:ff:ff:ff:ff"), 0x1235);
	clock_gettime(CLOCK_REALTIME, &tsp);   //Call clock_gettime to fill tsp
	pcpp::TimeSyncLayer newTimeSyncLayer((uint8_t)COMMAND_TIMERESET, (uint8_t)0,(uint32_t)tsp.tv_nsec, (uint32_t)tsp.tv_sec, (uint32_t)0);
	newPacket.addLayer(&newEthernetLayer);
	newPacket.addLayer(&newTimeSyncLayer);
	pDevice->sendPacket(&newPacket);
	printf("Sent a timestamp of ref_hi=%lu, ref_lo=%lu\n", tsp.tv_sec, tsp.tv_nsec);
}

void do_timesync(PcapLiveDevice* pDevice) {
	pcpp::Packet newPacket(100);
	pcpp::EthLayer newEthernetLayer(pDevice->getMacAddress(), pcpp::MacAddress("ff:ff:ff:ff:ff:ff"), 0x1235);
	pcpp::TimeSyncLayer newTimeSyncLayer((uint8_t)COMMAND_TIMESYNC_REQ, (uint8_t)0,(uint32_t)0, (uint32_t)0, (uint32_t)0);
	newPacket.addLayer(&newEthernetLayer);
	newPacket.addLayer(&newTimeSyncLayer);
	pDevice->sendPacket(&newPacket);
	do_receive_timesync(pDevice);

}

bool do_ts(PcapLiveDevice* pDevice, const string source_mac, const string dest_mac) {
	// create a packet with initial capacity of 100 bytes (will grow automatically if needed)
	// create a new Ethernet layer
  uint8_t ingressTs[6];
	uint8_t ingressMacTs[6];
	uint8_t egressTs[6];
	uint32_t enqDepth = 0;
	uint32_t deqDelta = 0;
	for (int i=0;i<5;i++) {
		pcpp::Packet newPacket(100);
		pcpp::EthLayer newEthernetLayer(pcpp::MacAddress(source_mac), pcpp::MacAddress(dest_mac), 0x1234);
		pcpp::TsLayer newTsLayer(i, ingressTs, ingressMacTs, egressTs, enqDepth, deqDelta);
		// add all the layers we created
		newPacket.addLayer(&newEthernetLayer);
		newPacket.addLayer(&newTsLayer);
		pDevice->sendPacket(&newPacket);
	}

	return true;
}
int main(int argc, char* argv[])
{
	AppName::init(argc, argv);

	//Get arguments from user for incoming interface and outgoing interface

	string iface = "", source_mac = "", dest_mac = "";
	int receive = 0;
	int reset_time = 0;
	int timesync = 0;
	int optionIndex = 0;
	char opt = 0;
	while((opt = getopt_long (argc, argv, "i:c:g:ptrhv", L3FwdOptions, &optionIndex)) != -1)
	{
		switch (opt)
		{
			case 0:
				break;
			case 'i':
				iface = optarg;
				break;
			case 'c':
				source_mac = optarg;
				break;
			case 'g':
				dest_mac = optarg;
				break;
			case 'p':
				reset_time  = 1;
				break;
			case 't':
				timesync  = 1;
				break;
			case 'r':
				receive  = 1;
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
		printUsage();
		exit(-1);
	}

	//IPv4Address ifaceAddr(iface);
	printf("%s\n", iface.c_str());
	//printf ("%s\n",ifaceAddr.toString().c_str());

	PcapLiveDevice* pIfaceDevice = PcapLiveDeviceList::getInstance().getPcapLiveDeviceByName(iface);

	//Verifying interface is valid
	if (pIfaceDevice == NULL)
	{
		printf("Cannot find interface. Exiting...\n");
		exit(-1);
	}


	//Opening interface device
	if (!pIfaceDevice->open())
	{
		printf("Cannot open interface. Exiting...\n");
		exit(-1);
	}

	if (reset_time == 1) {
		do_reset_time(pIfaceDevice);
		exit(0);
	}

	if (timesync == 1) {
		do_timesync(pIfaceDevice);
		exit(0);
	}

	if (receive == 1) {
		do_receive(pIfaceDevice);
	} else {
		return(!do_ts(pIfaceDevice, source_mac, dest_mac));
	}
}
