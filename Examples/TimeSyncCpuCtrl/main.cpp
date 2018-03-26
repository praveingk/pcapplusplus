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
#include <TimeSyncCPULayer.h>
#include <ArpLayer.h>
#include <Logger.h>
#include <time.h>
#if !defined(WIN32) && !defined(WINx64) //for using ntohl, ntohs, etc.
#include <in.h>
#endif
#include <getopt.h>

using namespace std;
using namespace pcpp;

#define COMMAND_CPUCTRL_INC 0x1
#define COMMAND_CPUCTRL_REQUEST 0x2
#define COMMAND_CPUCTRL_RESPONSE 0x3

static struct option L3FwdOptions[] =
{
	{"interface",  required_argument, 0, 'i'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'v'},
    {0, 0, 0, 0}
};


void printUsage() {
 printf("\nUsage:\n"
		 "------\n"
		 "%s [-hv] -i interface_ip \n"
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


uint64_t do_receive_cpuctrl(PcapLiveDevice* dev) {
	ProtoFilter protocolFilter(TIMESYNCCPU);
	printf("Do receive cpuctrl\n");
	if (!dev->setFilter(protocolFilter)) {
		printf("Cannot set TIMESYNC filter on device. Exiting...\n");
		exit(-1);
	}
	pcpp::RawPacketVector packetVec;
	dev->startCapture(packetVec);
	PCAP_SLEEP(1);
	dev->stopCapture();
	for (pcpp::RawPacketVector::ConstVectorIterator iter = packetVec.begin(); iter != packetVec.end(); iter++)
	{
		// parse raw packet
		pcpp::Packet parsedPacket(*iter);
		if (parsedPacket.isPacketOfType(pcpp::TIMESYNCCPU)) {
			TimeSyncCPULayer* tsLayer = parsedPacket.getLayerOfType<TimeSyncCPULayer>();
			tsLayer->dumpString();
			if (tsLayer->getCommand() == COMMAND_CPUCTRL_RESPONSE) {
				return tsLayer->getGlobalTs();
			}
			//printf("%s", tsLayer->toString().c_str());
		}
	}
	return 0;
}

uint64_t do_sendEra(PcapLiveDevice* pDevice) {
	pcpp::Packet newPacket(100);
	pcpp::EthLayer newEthernetLayer(pDevice->getMacAddress(), pcpp::MacAddress("ff:ff:ff:ff:ff:ff"), 0x7777);
	pcpp::TimeSyncCPULayer newTimeSyncCpuLayer((uint8_t)COMMAND_CPUCTRL_INC,(uint32_t)0);
	newPacket.addLayer(&newEthernetLayer);
	newPacket.addLayer(&newTimeSyncCpuLayer);
	pDevice->sendPacket(&newPacket);
	return do_receive_cpuctrl(pDevice);
}

uint64_t send_probe(PcapLiveDevice* pDevice) {
	uint8_t globalTs[6];
	pcpp::Packet newPacket(100);
	pcpp::EthLayer newEthernetLayer(pDevice->getMacAddress(), pcpp::MacAddress("ff:ff:ff:ff:ff:ff"), 0x7777);
	pcpp::TimeSyncCPULayer newTimeSyncCpuLayer((uint8_t)COMMAND_CPUCTRL_REQUEST, globalTs);

	newPacket.addLayer(&newEthernetLayer);
	newPacket.addLayer(&newTimeSyncCpuLayer);
	printf("Sending Probe\n");
	pDevice->sendPacket(&newPacket);

	return do_receive_cpuctrl(pDevice);
}


void do_probe(PcapLiveDevice* pDevice) {
	uint64_t sampleTs1 = 0;
	uint64_t sampleTs2 = 0;
	while(true) {
		sampleTs1 = send_probe(pDevice);
		printf("SampleTS1 = %lu\n", sampleTs1);
		PCAP_SLEEP(2);
		sampleTs2 = send_probe(pDevice);
		printf("SampleTS2 = %lu\n", sampleTs1);
		if (sampleTs2 < sampleTs1) {
			printf(" Wrap detected. Incrementing ERA");
			do_sendEra(pDevice);
		}
	}
}



int main(int argc, char* argv[])
{
	AppName::init(argc, argv);

	//Get arguments from user for incoming interface and outgoing interface
	string iface = "";
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

	do_probe(pIfaceDevice);


}
