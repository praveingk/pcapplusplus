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
#include <DpdkDeviceList.h>
#include <QmetadataLayer.h>
#include <IPv4Layer.h>
#include <UdpLayer.h>
#include <PayloadLayer.h>
#include <iostream>
#include <thread>
#include <signal.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_errno.h>
#if !defined(WIN32) && !defined(WINx64) //for using ntohl, ntohs, etc.
#include <in.h>
#endif
#include <getopt.h>

using namespace std::chrono;
using namespace std;
using namespace pcpp;

#define COMMAND_TIMERESET 0x1
#define COMMAND_TIMESYNC_REQ 0x2
#define COMMAND_TIMESYNC_RESPONSE 0x3
#define COMMAND_TIMESYNC_TRANSDELAY 0x4
#define COMMAND_TIMESYNC_TRANSDELAY_RESPONSE 0x5


struct timespec delaytsp;
struct timespec reqtsp;
struct timespec reqsenttsp;
struct timespec dpdkreqtsp;

struct timespec delaypktrecvtsp;
struct timespec recvtsp;

struct timespec delaysendtsp;
struct timespec delaysend2tsp;
struct timespec reqsendtsp;
struct timespec reqsend2tsp;

uint64_t delaytsc;
uint64_t reqtsc;
uint64_t reqsenttsc;
int clientdelta = 100L; //1ms
struct timespec clientdeltatsp;

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

std::thread timesync_request_thread;
std::thread timesync_receive_thread;
std::thread crossThread;
std::thread timerThread;
bool stopSending = false;
bool stopBursting = false;

DpdkDevice *sendPacketsTo;
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
		 "    -p timesync   : Send the current time out\n"
		 "    -t timesync   : Get the current time\n"
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

uint32_t max_ns = 1000000000;
uint64_t trans_delay_offset = 0;
int event = 0;


static bool onPacketArrivesBlockingMode(pcpp::RawPacket* packet, pcpp::PcapLiveDevice* dev, void* cookie)
{
	clock_gettime(CLOCK_REALTIME, &recvtsp);
	printf("Packet Arrvies\n");
	// parsed the raw packet
	pcpp::Packet parsedPacket(packet);
	printf("here\n");
	printf("Packet : %s\n", parsedPacket.toString(true).c_str());
	//printf("Protocol = %lX\n", parsedPacket.m_ProtocolTypes);
	struct timespec calctsp;
	if (parsedPacket.isPacketOfType(pcpp::TIMESYNC) || parsedPacket.isPacketOfType(pcpp::TS)) {
		//printf("here\n");
		TimeSyncLayer* tsLayer = parsedPacket.getLayerOfType<TimeSyncLayer>();
		if (tsLayer->getCommand() == COMMAND_TIMESYNC_TRANSDELAY_RESPONSE) {
			tsLayer->dumpString();
			trans_delay_offset = tsLayer->getIgTs();
		}
		if (tsLayer->getCommand() == COMMAND_TIMESYNC_RESPONSE) {
			//Not resorting to functions due to optimization.
			tsLayer->dumpString();
			uint32_t calc_ref_sec = tsLayer->getReference_ts_hi();
			uint32_t calc_ref_nsec = tsLayer->getReference_ts_lo();
			uint32_t era_hi = tsLayer->getEraTs();
			uint32_t switch_delay = (tsLayer->getEgTs() - tsLayer->getIgTs());
			uint32_t e2edelay = (recvtsp.tv_sec - reqtsp.tv_sec) * (max_ns) + (recvtsp.tv_nsec - reqtsp.tv_nsec);
			uint32_t elapsed_lo = tsLayer->getEgTs() % max_ns;
			uint32_t elapsed_hi = tsLayer->getEgTs() / max_ns;
			uint64_t interpacket_clientdelay = (reqtsp.tv_sec - delaytsp.tv_sec) * (max_ns) + (reqtsp.tv_nsec - delaytsp.tv_nsec);
			uint64_t clientsentdelay = (reqsenttsp.tv_sec - reqtsp.tv_sec) * (max_ns) + (reqsenttsp.tv_nsec - reqtsp.tv_nsec);
			uint32_t interpacket_switchdelay = (tsLayer->getIgTs() - trans_delay_offset);
			long reqdelay = interpacket_switchdelay - clientdelta;

			clock_gettime(CLOCK_REALTIME, &calctsp);
			calc_ref_sec = calc_ref_sec + era_hi + elapsed_hi + (e2edelay/max_ns) + (calctsp.tv_sec - recvtsp.tv_sec);
			calc_ref_nsec = calc_ref_nsec + elapsed_lo + e2edelay%max_ns + (calctsp.tv_nsec - recvtsp.tv_nsec);

			/* Old NTP based one-way delay measurement based on symmetric assumption */
			double replydelay_ntp = e2edelay/2;
			double replydelay_switchdelaybased = ((e2edelay - switch_delay)/2) + switch_delay;

			double replydelay_onewaybased;


			printf("\n************************************************\n");
			printf("Inter-packet Delay from client = %luns\n", interpacket_clientdelay);
			printf("re-sent Delay from client = %luns\n", clientsentdelay);
      printf("Inter-packet arrival delay at switch = %uns\n", interpacket_switchdelay);
      printf("Calculated Oneway request Delay = %ldns\n", reqdelay);
      printf("Switch Delay = %uns\n", switch_delay);
			printf("End-to-End Delay = %uns\n", e2edelay);

			printf("Reply Delay NTP Based = %lfns\n", replydelay_ntp);
			printf("Reply Delay Switch Delay based = %lfns\n", replydelay_switchdelaybased);

			//printf("Reply Delay 2-probe based= %luns\n", replydelay_onewaybased);
			printf("-----------------\n");
			printf("Elapsed hi =%us, lo=%uns\n", elapsed_hi, elapsed_lo);
			printf("Calc delay hi= %lus, lo=%luns\n", calctsp.tv_sec - recvtsp.tv_sec, calctsp.tv_nsec - recvtsp.tv_nsec);
      //printf("dpdk req hi= %lus, lo=%luns\n", dpdkreqtsp.tv_sec, dpdkreqtsp.tv_nsec);

			printf("-----------------\n");
			printf("Time calculated : hi = %u, lo = %u\n", calc_ref_sec, calc_ref_nsec);
			printf("Time cur ref : hi = %lu, lo = %lu\n", calctsp.tv_sec, calctsp.tv_nsec);
			printf("************************************************\n");
      event++;
      //fprintf(timesync_log,"%d, %lf, %lf, %lf, %u, %u, %u\n", event, replydelay_ntp, replydelay_switchdelaybased, replydelay_onewaybased ,interpacket_switchdelay, switch_delay);

		}
		//printf("%s", tsLayer->toString().c_str());
	}
	//printf("Done\n");
	return false;
}

void interruptHandler(int s){

    printf("\nCaught interrupt. Stopping the sending thread...\n");
    stopSending = true;

    timesync_request_thread.join();

}

void do_receive_timesync_blocking(PcapLiveDevice* dev) {
	ProtoFilter protocolFilter(TIMESYNC);
	int cookie;
	if (!dev->setFilter(protocolFilter)) {
		printf("Cannot set TIMESYNC filter on device. Exiting...\n");
		exit(-1);
	}
	printf("Waiting for Timsync packets...\n");

	pcpp::RawPacketVector packetVec;

	dev->startCaptureBlockingMode(onPacketArrivesBlockingMode, &cookie, 1);
}

void do_receive_dpdk_timesync(DpdkDevice* dev) {
	pcpp::Packet* packetArr = NULL;
	pcpp::Packet* packetArr2 = NULL;
	int packetArrLen = 0;
	RawPacketVector rawPacketVec;
	int total_packets = 2;
	Packet timesync_pkt;

	while(true) {
		bool status = dev->receivePackets(&packetArr, packetArrLen, (uint16_t)0, &recvtsp);
		//printf("Status = %d, Received %d Packets.\n", status, packetArrLen);

		if (packetArrLen == 0) continue;
		printf("%d ", packetArrLen);
		for (int i=0;i< packetArrLen;i++) {

			if (packetArr[i].isPacketOfType(pcpp::TIMESYNC) || packetArr[i].isPacketOfType(pcpp::TS)) {
				timesync_pkt = packetArr[i];

				struct timespec calctsp;
				TimeSyncLayer* tsLayer = timesync_pkt.getLayerOfType<TimeSyncLayer>();
				if (tsLayer->getCommand() == COMMAND_TIMESYNC_RESPONSE) {
					//Not resorting to functions due to optimization.
					tsLayer->dumpString();
					uint32_t calc_ref_sec = tsLayer->getReference_ts_hi();
					uint32_t calc_ref_nsec = tsLayer->getReference_ts_lo();
					uint32_t era_hi = tsLayer->getEraTs();
					uint32_t switch_delay = (tsLayer->getEgTs() - tsLayer->getIgTs());
					uint32_t e2edelay = (recvtsp.tv_sec - reqsendtsp.tv_sec) * (max_ns) + (recvtsp.tv_nsec - reqsendtsp.tv_nsec);
					uint32_t e2edelay_2probe = (recvtsp.tv_sec - delaysendtsp.tv_sec) * (max_ns) + (recvtsp.tv_nsec - delaysendtsp.tv_nsec);
					uint32_t elapsed_lo = tsLayer->getEgTs() % max_ns;
					uint32_t elapsed_hi = tsLayer->getEgTs() / max_ns;
					uint64_t diff_send2_send = (reqsend2tsp.tv_sec - reqsendtsp.tv_sec) * (max_ns) + (reqsend2tsp.tv_nsec - reqsendtsp.tv_nsec);
					uint32_t interpacket_clientdelay = (reqsendtsp.tv_sec - delaysendtsp.tv_sec) * (max_ns) + (reqsendtsp.tv_nsec - delaysendtsp.tv_nsec);

					uint32_t interpacket_switchdelay = tsLayer->getEgDelta();
					long reqdelay = interpacket_clientdelay + interpacket_switchdelay;
					long reqdelay_2probe = interpacket_switchdelay - interpacket_clientdelay;

					clock_gettime(CLOCK_REALTIME, &calctsp);
					calc_ref_sec = calc_ref_sec + era_hi + elapsed_hi + (e2edelay/max_ns) + (calctsp.tv_sec - recvtsp.tv_sec);
					calc_ref_nsec = calc_ref_nsec + elapsed_lo + e2edelay%max_ns + (calctsp.tv_nsec - recvtsp.tv_nsec);

					/* Old NTP based one-way delay measurement based on symmetric assumption */
					double replydelay_ntp = e2edelay/2;
					double replydelay_switchdelaybased = ((e2edelay - switch_delay)/2) + switch_delay;
					double replydelay_2probe = e2edelay - reqdelay_2probe;

					printf("\n***********************%d************************\n", event);
					printf("Inter-packet Delay from client = %uns\n", interpacket_clientdelay);
					printf("Delay to send pkt= %luns\n", diff_send2_send);

					//printf("req-sent Delay from client = %luns\n", clientsentdelay);
					printf("Inter-packet arrival delay at switch = %uns\n", interpacket_switchdelay);
					printf("Calculated Oneway request Delay = %ldns\n", reqdelay_2probe);
					printf("Switch Delay = %uns\n", switch_delay);
					printf("End-to-End Delay = %uns\n", e2edelay);
					printf("End-to-End Delay (2probe) = %uns\n", e2edelay_2probe);
					//printf("End-to-End Delay old way = %uns\n", e2edelay_old);

					printf("Reply Delay NTP Based = %lfns\n", replydelay_ntp);
					printf("Reply Delay Switch Delay based = %lfns\n", replydelay_switchdelaybased);

					printf("Reply Delay 2 probe based= %lfns\n", replydelay_2probe);
					printf("-----------------\n");
					printf("Elapsed hi =%us, lo=%uns\n", elapsed_hi, elapsed_lo);
					printf("Calc delay hi= %lus, lo=%luns\n", calctsp.tv_sec - recvtsp.tv_sec, calctsp.tv_nsec - recvtsp.tv_nsec);
					printf("dpdk req hi= %lus, lo=%luns\n", dpdkreqtsp.tv_sec, dpdkreqtsp.tv_nsec);
					printf("-----------------\n");
					printf("Time calculated : hi = %u, lo = %u\n", calc_ref_sec, calc_ref_nsec);
					printf("Time cur ref : hi = %lu, lo = %lu\n", calctsp.tv_sec, calctsp.tv_nsec);
					printf("************************************************\n");
					event++;
					fprintf(timesync_log,"%d, %lf, %lf, %lf, %u, %u, %u\n", event, replydelay_ntp, replydelay_switchdelaybased, replydelay_2probe ,interpacket_switchdelay, interpacket_clientdelay, switch_delay);
				}
			} else {
				printf("!");
			}
		}
	}
}

void do_reset_time(PcapLiveDevice* pDevice) {
	struct timespec tsp;
	uint8_t igTs[6];
	uint8_t egTs[6];
	pcpp::Packet newPacket(100);
	pcpp::EthLayer newEthernetLayer(pDevice->getMacAddress(), pcpp::MacAddress("ff:ff:ff:ff:ff:ff"), 0x1235);
	clock_gettime(CLOCK_REALTIME, &tsp);   //Call clock_gettime to fill tsp
	pcpp::TimeSyncLayer newTimeSyncLayer((uint8_t)COMMAND_TIMERESET, (uint8_t)0,(uint32_t)tsp.tv_nsec, (uint32_t)tsp.tv_sec, (uint32_t)0, (uint32_t)0, igTs, egTs);
	newPacket.addLayer(&newEthernetLayer);
	newPacket.addLayer(&newTimeSyncLayer);
	pDevice->sendPacket(&newPacket);
	printf("Sent a timestamp of ref_hi=%lu, ref_lo=%lu\n", tsp.tv_sec, tsp.tv_nsec);
}

void do_dpdk_timesync(DpdkDevice* dev, pcpp::Packet delayPacket, pcpp::Packet reqPacket, PcapLiveDevice* pDevice, bool* stopSending){
  usleep(100000); // sleep for 100ms to allow thread affinity to be set
  pcpp::Packet* pkt_burst[2];
  std::fill_n(pkt_burst, 1, &delayPacket);
  std::fill_n(pkt_burst+1, 1, &reqPacket);
  const pcpp::Packet** burst_ptr = (const pcpp::Packet**) pkt_burst;
	printf("Starting Timesync Protocol!\n");
	clientdeltatsp.tv_sec = 0;
	clientdeltatsp.tv_nsec = clientdelta;
	struct timespec dummytsp;
   while (true) {
    printf("Sending DPDK Timesync\n");
    int stat = rte_eth_timesync_read_tx_timestamp(TIMESYNC_DPDK_PORT, &dpdkreqtsp);
		//printf("Status of tx timestamp = %s\n",rte_strerror(stat));
		//Enable below to send burst of 2 packets.
    //dev->sendPackets(burst_ptr, 2, 0);
		//Enable below to send packets individually
		clock_gettime(CLOCK_REALTIME, &delaytsp);
		//delaytsc = rte_rdtsc();
    dev->sendPacket(delayPacket, 0, &delaysendtsp, &delaysend2tsp);
		//nanosleep(&clientdeltatsp, &dummytsp);
		//clock_gettime(CLOCK_REALTIME, &reqtsp);
		//uint64_t now = rte_rdtsc();
		//reqtsc = rte_rdtsc();
		//printf("Now = %lu\n", now);
    dev->sendPacket(reqPacket, 0, &reqsendtsp, &reqsend2tsp);
		clock_gettime(CLOCK_REALTIME, &reqsenttsp);
		//reqsenttsc = rte_rdtsc();
		//do_receive_dpdk_timesync(dev);
    //do_receive_timesync_blocking(pDevice);
		sleep(2);
	}
}

void do_crosstraffic(DpdkDevice* dev, pcpp::Packet parsedPacket, bool* stopSending){

    printf("\n[Sending Thread] Starting packet sending ...\n");

    pcpp::QmetadataLayer* qmetadatalayer = parsedPacket.getLayerOfType<pcpp::QmetadataLayer>();
		struct timespec dummytsp;

    //in i = 0;
    while(!*stopSending){
      dev->sendPacket(parsedPacket,0);
			nanosleep(&clientdeltatsp, &dummytsp);
    }

    printf("\n[Sending Thread] Stopping packet sending ...\n");
}

void do_burst(DpdkDevice* dev, pcpp::Packet parsedPacket, bool* stopBursting){

    usleep(100000); // sleep for 100ms to allow thread affinity to be set
    printf("[BURSTING Thread] Starting microbursts ...\n");

    pcpp::Packet* pkt_burst[NUM_PKTS_IN_BURST];
    std::fill_n(pkt_burst, NUM_PKTS_IN_BURST, &parsedPacket);

    const pcpp::Packet** burst_ptr = (const pcpp::Packet**) pkt_burst;

    //for(int i=0; i < NUM_BURSTS; i++){
    //while(!stopBursting) {
		while(true) {
        dev->sendPackets(burst_ptr, NUM_PKTS_IN_BURST); // default Tx queue is 0
        usleep(1); // 1 ms gap between the bursts
    }

    printf("[BURSTING Thread] Stopping microbursts ...\n");
}


void do_timesync(PcapLiveDevice* pDevice) {
	pcpp::Packet delayPacket(100);
	pcpp::Packet reqPacket(100);
	uint8_t igTs[6];
	uint8_t egTs[6];

	pcpp::EthLayer transDelayEthernetLayer(pDevice->getMacAddress(), pcpp::MacAddress("ff:ff:ff:ff:ff:ff"), 0x1235);
	pcpp::TimeSyncLayer transDelayTimeSyncLayer((uint8_t)COMMAND_TIMESYNC_TRANSDELAY, (uint8_t)0,(uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0, igTs, egTs);
	pcpp::EthLayer newEthernetLayer(pDevice->getMacAddress(), pcpp::MacAddress("ff:ff:ff:ff:ff:ff"), 0x1235);
	pcpp::TimeSyncLayer newTimeSyncLayer((uint8_t)COMMAND_TIMESYNC_REQ, (uint8_t)0,(uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0, igTs, egTs);

	delayPacket.addLayer(&transDelayEthernetLayer);
	delayPacket.addLayer(&transDelayTimeSyncLayer);

	reqPacket.addLayer(&newEthernetLayer);
	reqPacket.addLayer(&newTimeSyncLayer);
	// record Time snapshot1 for end-to-end delay measurement.
	clock_gettime(CLOCK_REALTIME, &delaytsp);
	clock_gettime(CLOCK_REALTIME, &reqtsp);
	pDevice->sendPacket(&delayPacket);
	//clock_gettime(CLOCK_REALTIME, &reqtsp);
	uint64_t now = rte_rdtsc();
	pDevice->sendPacket(&reqPacket);
	do_receive_timesync_blocking(pDevice);
}

void start_timesync_dpdk() {
	timesync_log = fopen("timesync.log","w");
	fprintf(timesync_log,"Timesync, Reply_symmetric-based, Reply_switchdelaybased, Reply_2probebased, inter-packet-switch-arrival,  interpacket_clientdelay, Switch_reply_delay \n");
	if(DpdkDeviceList::initDpdk(CORE_MASK, MBUFF_POOL_SIZE))
		printf("DPDK initialization completed successfully\n");
	else{
		printf("DPDK initialization failed!!\n");
		exit(1);
	}

	printf("DPDK devices initialized:\n");
	vector<DpdkDevice*> deviceList = DpdkDeviceList::getInstance().getDpdkDeviceList();
	for(vector<DpdkDevice*>::iterator iter = deviceList.begin(); iter != deviceList.end(); iter++) {
			DpdkDevice* dev = *iter;
			printf("    Port #%d: MAC address='%s'; PCI address='%s'; PMD='%s'\n",
							dev->getDeviceId(),
							dev->getMacAddress().toString().c_str(),
							dev->getPciAddress().toString().c_str(),
							dev->getPMDName().c_str());
	}

	int sendPacketsToPort = TIMESYNC_DPDK_PORT;
	sendPacketsTo = DpdkDeviceList::getInstance().getDeviceByPort(sendPacketsToPort);
	if (sendPacketsTo != NULL && !sendPacketsTo->open()) {
			printf("Could not open port#%d for sending packets\n", sendPacketsToPort);
			exit(1);
	}

	DpdkDevice::LinkStatus linkstatus;
	sendPacketsTo->getLinkStatus(linkstatus);

	printf("The link is %s\n",linkstatus.linkUp?"UP":"Down");

	if(!linkstatus.linkUp){
			printf("Exiting...\n");
			exit(1);
	}

	/* Initialize rx and tx queues. I guess this is needed to receive any packets.*/

	// if (!sendPacketsTo->openMultiQueues(sendPacketsTo->getTotalNumOfRxQueues(), sendPacketsTo->getTotalNumOfTxQueues())) {
	// 	printf("Device queue initialization failed!\n");
	// }


	/* Enable timesync timestamping for the Ethernet device */
	int ts_stat = rte_eth_timesync_enable(sendPacketsToPort);
	printf("Enabling eth timesnc, status =%d\n", ts_stat);

	PcapLiveDevice *recvDevice = NULL;
	if (iface != "") {
		printf("%s\n", iface.c_str());
		recvDevice = PcapLiveDeviceList::getInstance().getPcapLiveDeviceByName(iface);
		if (!recvDevice->open()) {
			printf("Cannot open interface. Exiting...\n");
			exit(-1);
		}
	}
	cpu_set_t cpuset_sender;
	CPU_ZERO(&cpuset_sender);
	CPU_SET(TIMESYNC_THREAD_CORE, &cpuset_sender);

	cpu_set_t cpuset_receiver;
	CPU_ZERO(&cpuset_receiver);
	CPU_SET(TIMESYNC_RECEIVE_CORE, &cpuset_receiver);

	bool stopSending = false;
	pcpp::Packet delayPacket(100);
	pcpp::Packet reqPacket(100);
	uint8_t igTs[6];
	uint8_t egTs[6];

	pcpp::MacAddress sourceMac = sendPacketsTo->getMacAddress();//pcpp::MacAddress("3c:fd:fe:b7:e8:e8"); //
	pcpp::EthLayer transDelayEthernetLayer(sourceMac, pcpp::MacAddress("ff:ff:ff:ff:ff:ff"), 0x1235);
	pcpp::TimeSyncLayer transDelayTimeSyncLayer((uint8_t)COMMAND_TIMESYNC_TRANSDELAY, (uint8_t)0,(uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0, igTs, egTs);
	pcpp::EthLayer newEthernetLayer(sourceMac, pcpp::MacAddress("ff:ff:ff:ff:ff:ff"), 0x1235);
	pcpp::TimeSyncLayer newTimeSyncLayer((uint8_t)COMMAND_TIMESYNC_REQ, (uint8_t)0,(uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0, igTs, egTs);

	delayPacket.addLayer(&transDelayEthernetLayer);
	delayPacket.addLayer(&transDelayTimeSyncLayer);

	reqPacket.addLayer(&newEthernetLayer);
	reqPacket.addLayer(&newTimeSyncLayer);

	timesync_receive_thread = std::thread(do_receive_dpdk_timesync, sendPacketsTo);
	int aff = pthread_setaffinity_np(timesync_receive_thread.native_handle(), sizeof(cpu_set_t), &cpuset_receiver);
	printf("Receive thread now running on vcpu #%d\n", TIMESYNC_RECEIVE_CORE);

	timesync_request_thread = std::thread(do_dpdk_timesync, sendPacketsTo, delayPacket, reqPacket, recvDevice, &stopSending);
	aff = pthread_setaffinity_np(timesync_request_thread.native_handle(), sizeof(cpu_set_t), &cpuset_sender);
	printf("Sending thread now running on vcpu #%d\n", TIMESYNC_THREAD_CORE);
}

void start_samenic_dpdk() {
	if (sendPacketsTo ==NULL) {
			printf("Could not open port#%d for sending packets\n", TIMESYNC_DPDK_PORT);
			exit(1);
	}

	DpdkDevice::LinkStatus linkstatus;
	sendPacketsTo->getLinkStatus(linkstatus);

	printf("The link is %s\n",linkstatus.linkUp?"UP":"Down");

	if(!linkstatus.linkUp){
			printf("Exiting...\n");
			exit(1);
	}

	// Build Packet
	pcpp::Packet newPacket(MTU_LENGTH);
	pcpp::MacAddress srcMac("3c:fd:fe:b7:e7:f4");
	pcpp::MacAddress dstMac("aa:aa:aa:aa:aa:aa");
	pcpp::IPv4Address srcIP(std::string("20.1.1.2"));
	pcpp::IPv4Address dstIP(std::string("40.1.1.2"));
	uint16_t srcPort = 37777;
	uint16_t dstPort = 7777;

	pcpp::EthLayer newEthLayer(srcMac, dstMac, PCPP_ETHERTYPE_IP); // 0x0800 -> IPv4
	pcpp::IPv4Layer newIPv4Layer(srcIP, dstIP);
	newIPv4Layer.getIPv4Header()->timeToLive = DEFAULT_TTL;
	pcpp::UdpLayer newUDPLayer(srcPort, dstPort);
	pcpp::QmetadataLayer newQmetadataLayer(0);

	int length_so_far = newEthLayer.getHeaderLen() + newIPv4Layer.getHeaderLen() +
											newUDPLayer.getHeaderLen() + newQmetadataLayer.getHeaderLen();
	int payload_length = MTU_LENGTH - length_so_far;

	printf("Header length before the payload is %d\n", length_so_far);

	uint8_t payload[payload_length];
	int datalen = 4;
	char data[datalen] = {'D','A','T','A'};
	// Put the data values into the payload
	int j = 0;
	for(int i=0; i < payload_length; i++){
			payload[i] = (uint8_t) int(data[j]);
			j = (j + 1) % datalen;
	}

	// construct the payload layer
	pcpp::PayloadLayer newPayLoadLayer(payload, payload_length, false);

	newPacket.addLayer(&newEthLayer);
	newPacket.addLayer(&newIPv4Layer);
	newPacket.addLayer(&newUDPLayer);
	newPacket.addLayer(&newQmetadataLayer);
	newPacket.addLayer(&newPayLoadLayer);

	// compute the calculated fields
	newUDPLayer.computeCalculateFields();
	newUDPLayer.calculateChecksum(true);
	newIPv4Layer.computeCalculateFields(); // this takes care of the IPv4 checksum

	cpu_set_t cpuset2;
	CPU_ZERO(&cpuset2);
	CPU_SET(CROSSTRAFFIC_THREAD_CORE, &cpuset2);

	//sleep(10);
	//crossThread = std::thread(do_crosstraffic, sendPacketsTo, newPacket, &stopSending);

	crossThread = std::thread(do_burst, sendPacketsTo, newPacket, &stopBursting);

	int aff = pthread_setaffinity_np(crossThread.native_handle(), sizeof(cpu_set_t), &cpuset2);
	printf("####################################################################3\n");
	printf("Bursting thread now running on vcpu #%d\n", CROSSTRAFFIC_THREAD_CORE);
}

void start_crosstraffic_dpdk() {
	if (sendPacketsTo == NULL) {
			printf("Could not open port#%d for sending packets\n", CROSSTRAFFIC_DPDK_PORT);
			exit(1);
	}

	DpdkDevice::LinkStatus linkstatus;
	sendPacketsTo->getLinkStatus(linkstatus);

	printf("The link is %s\n",linkstatus.linkUp?"UP":"Down");

	if(!linkstatus.linkUp){
			printf("Exiting...\n");
			exit(1);
	}

	// Build Packet
	pcpp::Packet newPacket(MTU_LENGTH);
	pcpp::MacAddress srcMac("3c:fd:fe:b7:e7:f4");
	pcpp::MacAddress dstMac("aa:aa:aa:aa:aa:aa");
	pcpp::IPv4Address srcIP(std::string("20.1.1.2"));
	pcpp::IPv4Address dstIP(std::string("40.1.1.2"));
	uint16_t srcPort = 37777;
	uint16_t dstPort = 7777;

	pcpp::EthLayer newEthLayer(srcMac, dstMac, PCPP_ETHERTYPE_IP); // 0x0800 -> IPv4
	pcpp::IPv4Layer newIPv4Layer(srcIP, dstIP);
	newIPv4Layer.getIPv4Header()->timeToLive = DEFAULT_TTL;
	pcpp::UdpLayer newUDPLayer(srcPort, dstPort);
	pcpp::QmetadataLayer newQmetadataLayer(0);

	int length_so_far = newEthLayer.getHeaderLen() + newIPv4Layer.getHeaderLen() +
											newUDPLayer.getHeaderLen() + newQmetadataLayer.getHeaderLen();
	int payload_length = MTU_LENGTH - length_so_far;

	printf("Header length before the payload is %d\n", length_so_far);

	uint8_t payload[payload_length];
	int datalen = 4;
	char data[datalen] = {'D','A','T','A'};
	// Put the data values into the payload
	int j = 0;
	for(int i=0; i < payload_length; i++){
			payload[i] = (uint8_t) int(data[j]);
			j = (j + 1) % datalen;
	}

	// construct the payload layer
	pcpp::PayloadLayer newPayLoadLayer(payload, payload_length, false);

	newPacket.addLayer(&newEthLayer);
	newPacket.addLayer(&newIPv4Layer);
	newPacket.addLayer(&newUDPLayer);
	newPacket.addLayer(&newQmetadataLayer);
	newPacket.addLayer(&newPayLoadLayer);

	// compute the calculated fields
	newUDPLayer.computeCalculateFields();
	newUDPLayer.calculateChecksum(true);
	newIPv4Layer.computeCalculateFields(); // this takes care of the IPv4 checksum

	cpu_set_t cpuset2;
	CPU_ZERO(&cpuset2);
	CPU_SET(CROSSTRAFFIC_THREAD_CORE, &cpuset2);

	//sleep(10);
	//crossThread = std::thread(do_crosstraffic, sendPacketsTo, newPacket, &stopSending);

	crossThread = std::thread(do_burst, sendPacketsTo, newPacket, &stopBursting);

	int aff = pthread_setaffinity_np(crossThread.native_handle(), sizeof(cpu_set_t), &cpuset2);
	printf("Bursting thread now running on vcpu #%d\n", CROSSTRAFFIC_THREAD_CORE);
}

void start_timer() {
	int total_time = 20; // Allow to run for 20 sec_
	int i = 0;

	printf("%%%% Starting Timer..");
	while(true) {
		sleep(1);
		i++;
		printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^i=%d\n", i);
		if (i == 20) {
			stopBursting = true;
			printf("Command : Stop Burst\n");
		}
		if (i == 30) {
			stopSending = true;
			printf("Command : Stop Sending\n");
			break;
		}
	}
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
  DPDK = 1;
	if (DPDK == 1) {
		//timerThread = std::thread(start_timer);
		start_timesync_dpdk();
		//start_crosstraffic_dpdk();
		start_samenic_dpdk();

    // code to handle keyboard interrupt
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = interruptHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    pause();
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
		if (reset_time == 1) {
			do_reset_time(pIfaceDevice);
			exit(0);
		}
		if (timesync == 1) {
			do_timesync(pIfaceDevice);
			exit(0);
		}
	}
}
