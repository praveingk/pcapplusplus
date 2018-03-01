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

uint64_t TsLayer::getingressTs() {
	uint64_t timestamp = 0;
	for (int i=6; i>0; i--) {
		timestamp = (timestamp | getTsHeader()->ingressTs[i]) << 8;
	}
	return timestamp;
}

uint64_t TsLayer::getingressMacTs() {
	uint64_t timestamp = 0;
	for (int i=6; i>0; i--) {
		timestamp = (timestamp | getTsHeader()->ingressMacTs[i]) << 8;
	}
	return timestamp;
}

uint64_t TsLayer::getegressTs() {
	uint64_t timestamp = 0;
	for (int i=6; i>0; i--) {
		timestamp = (timestamp | getTsHeader()->egressTs[i]) << 8;
	}
	return timestamp;
}


std::string TsLayer::toString()
{
	char TS[500];
	sprintf( TS,"TimeSync=>ID:%d, ingressTs: %ul, ingressMacTs: %ul, egressTs: $ul, enqDepth: %u, deqDelta: %u"+ getId(), getingressTs(), getingressMacTs(), getegressTs(), getenqTs(), getdeqDelta());
	return std::string(TS);
}

} // namespace pcpp
