#define LOG_MODULE PacketLogModuleTsLayer

#include "TimeSyncCPULayer.h"
#include "IPv4Layer.h"
#include "IPv6Layer.h"
#include "PayloadLayer.h"
#include "ArpLayer.h"
#include "VlanLayer.h"
#include "PPPoELayer.h"
#include "MplsLayer.h"
#include <string.h>
#include <stdlib.h>
#if defined(WIN32) || defined(WINx64)
#include <winsock2.h>
#elif LINUX
#include <in.h>
#elif MAC_OS_X
#include <arpa/inet.h>
#endif

namespace pcpp
{

TimeSyncCPULayer::TimeSyncCPULayer(const uint8_t command, const uint8_t* globalTs) : Layer()
{
	m_DataLen = sizeof(cpuctrl_t);
	m_Data = new uint8_t[m_DataLen];
	memset(m_Data, 0, m_DataLen);


	cpuctrl_t* tsHdr = (cpuctrl_t*)m_Data;
	//tsHdr->ingressTs = //(uint8_t*)malloc(6 * sizeof(uint8_t));
	tsHdr->command = command;

	memcpy(tsHdr->globalTs, globalTs, 6);

	m_Protocol = TS;
}


TimeSyncCPULayer::TimeSyncCPULayer() : Layer()
{
	m_DataLen = sizeof(cpuctrl_t);
	m_Data = new uint8_t[m_DataLen];
	memset(m_Data, 0, m_DataLen);
	m_Protocol = TS;
}

void TimeSyncCPULayer::computeCalculateFields()
{
	return;
}

void TimeSyncCPULayer::parseNextLayer()
{
	return;

}

uint8_t TimeSyncCPULayer::getCommand() {
	uint8_t command = getTimeSyncCPUHeader()->command;
	printf("Command:%02x,", command);
	return command;
}



uint64_t TimeSyncCPULayer::getGlobalTs() {
	uint64_t timestamp = 0;
	for (int i=1; i<=6; i++) {
		//printf("ingressTS :%X\n", m_Data[i]);
		timestamp = (timestamp | m_Data[i]) << 8;
	}
	timestamp = timestamp >>8;
	printf("globalTS:%lu,", timestamp);
	return timestamp;
}

void TimeSyncCPULayer::dumpHeader() {
	for (size_t i =0;i< m_DataLen;i++) {
		printf("%X ", m_Data[i]);
	}
	printf("Total =%lu\n", m_DataLen);
}

std::string TimeSyncCPULayer::toString() {
	char TS[500];
	sprintf( TS,"TimeSync=>ID");
	return std::string(TS);
}

void TimeSyncCPULayer::dumpString() {
	printf("TimeSyncCPUCtrl==> ");
	getCommand();
	getGlobalTs();
}
} // namespace pcpp
