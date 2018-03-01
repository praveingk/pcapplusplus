#define LOG_MODULE PacketLogModuleTsLayer

#include "TsLayer.h"
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

TsLayer::TsLayer(const uint32_t id, const uint8_t* ingressTs, const uint8_t* ingressMacTs, const uint8_t* egressTs, const uint32_t enqTs, const uint32_t deqDelta) : Layer()
{
	m_DataLen = sizeof(ts_header);
	m_Data = new uint8_t[m_DataLen];
	memset(m_Data, 0, m_DataLen);


	ts_header* tsHdr = (ts_header*)m_Data;
	//tsHdr->ingressTs = //(uint8_t*)malloc(6 * sizeof(uint8_t));
	memcpy(tsHdr->ingressTs, ingressTs, 6);
	memcpy(tsHdr->ingressMacTs, ingressMacTs, 6);
	memcpy(tsHdr->egressTs, egressTs, 6);
	tsHdr->enqTs = enqTs;
	tsHdr->deqDelta = deqDelta;
	tsHdr->id = id;
	m_Protocol = TS;
}


TsLayer::TsLayer() : Layer()
{
	m_DataLen = sizeof(ts_header);
	m_Data = new uint8_t[m_DataLen];
	memset(m_Data, 0, m_DataLen);
	m_Protocol = TS;
}

void TsLayer::computeCalculateFields()
{
	return;
}

void TsLayer::parseNextLayer()
{
	return;

}

uint32_t TsLayer::getId() {
	uint32_t id = 0;
	for (size_t i = 0; i< 4;i++) {
		//printf("ID : %X\n", m_Data[i]);
		id = (id | m_Data[i]) << 8;
	}
	id = id >> 8;
	printf("ID:%X,", id);
	return id;
}



uint64_t TsLayer::getingressTs() {
	uint64_t timestamp = 0;
	for (int i=4; i<10; i++) {
		//printf("ingressTS :%X\n", m_Data[i]);
		timestamp = (timestamp | m_Data[i]) << 8;
	}
	timestamp = timestamp >>8;
	printf("ingressTS:%lX,", timestamp);
	return timestamp;
}

uint64_t TsLayer::getingressMacTs() {
	uint64_t timestamp = 0;
	for (int i=10; i<16; i++) {
		//printf("ingressMacTS :%X\n", m_Data[i]);
		timestamp = (timestamp | m_Data[i]) << 8;
	}
	timestamp = timestamp >>8;
	printf("ingressMacTS:%lX,", timestamp);
	return timestamp;
}

uint64_t TsLayer::getegressTs() {
	uint64_t timestamp = 0;
	for (int i=16; i<22; i++) {
		//printf("egressTS :%X\n", m_Data[i]);
		timestamp = (timestamp | m_Data[i]) << 8;
	}
	timestamp = timestamp >>8;
	printf("getingressTs:%lX,", timestamp);

	return timestamp;
}

uint32_t TsLayer::getenqTs() {
	uint32_t enqTs = 0;
	int i = 22;
	for (i=22; i<25; i++) {
		enqTs = (enqTs | m_Data[i]) << 8;
	}
	enqTs = enqTs | m_Data[i];

	printf("eqnTs :%X,", enqTs);

	return enqTs;
}

uint32_t TsLayer::getdeqDelta() {
	uint32_t deqDelta = 0;
	int i =26;
	for (i=26; i<29; i++) {
		deqDelta = (deqDelta | m_Data[i]) << 8;
	}
	deqDelta = deqDelta | m_Data[i];

	printf("deqDelta :%X\n", deqDelta);
	return deqDelta;
}

void TsLayer::dumpHeader() {
	for (size_t i =0;i< m_DataLen;i++) {
		printf("%X ", m_Data[i]);
	}
	printf("Total =%lu\n", m_DataLen);
}

std::string TsLayer::toString() {
	char TS[500];
	sprintf( TS,"TimeSync=>ID:%X, ingressTs: %X, ingressMacTs: %X, egressTs: %X, enqDepth: %X, deqDelta: %X"+ getId(), getingressTs(), getingressMacTs(), getegressTs(), getenqTs(), getdeqDelta());
	return std::string(TS);
}

void TsLayer::dumpString() {
	printf("TimeSync==> ");
	getId();
	getingressTs();
	getingressMacTs();
	getegressTs();
	getenqTs();
	getdeqDelta();
}
} // namespace pcpp
