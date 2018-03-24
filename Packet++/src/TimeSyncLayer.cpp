#define LOG_MODULE PacketLogModuleTsLayer

#include "TimeSyncLayer.h"
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

TimeSyncLayer::TimeSyncLayer(const uint8_t command, const uint8_t magic, const uint32_t reference_ts_lo, const uint32_t reference_ts_hi, const uint32_t delta) : Layer()
{
	m_DataLen = sizeof(timesync_t);
	m_Data = new uint8_t[m_DataLen];
	memset(m_Data, 0, m_DataLen);
	timesync_t* tsHdr = (timesync_t*)m_Data;

	tsHdr->command = command;
	tsHdr->magic = magic;
	tsHdr->reference_ts_lo = reference_ts_lo;
	tsHdr->reference_ts_hi = reference_ts_hi;
	tsHdr->delta = delta;

	m_Protocol = TS;
}


TimeSyncLayer::TimeSyncLayer() : Layer()
{
	m_DataLen = sizeof(timesync_t);
	m_Data = new uint8_t[m_DataLen];
	memset(m_Data, 0, m_DataLen);
	m_Protocol = TIMESYNC;
}

void TimeSyncLayer::computeCalculateFields()
{
	return;
}

void TimeSyncLayer::parseNextLayer()
{
	return;

}

uint8_t TimeSyncLayer::getCommand() {
	uint8_t command = getTimeSyncHeader()->command;
	printf("Command:%02x,", command);
	return command;
}

uint8_t TimeSyncLayer::getMagic() {
	uint8_t magic = getTimeSyncHeader()->magic;
	printf("magic:%02x", magic);
	return magic;
}


uint32_t TimeSyncLayer::getReference_ts_lo() {
	uint32_t reference_ts_lo = getTimeSyncHeader()->reference_ts_lo;
	printf("reference_ts_lo:%u,", reference_ts_lo);
	return reference_ts_lo;
}

uint32_t TimeSyncLayer::getReference_ts_hi() {
	uint32_t reference_ts_hi = getTimeSyncHeader()->reference_ts_hi;
	printf("reference_ts_hi:%u,", reference_ts_hi);
	return reference_ts_hi;
}

uint32_t TimeSyncLayer::getDelta() {
	uint32_t delta = getTimeSyncHeader()->delta;
	printf("delta:%u\n", delta);
	return delta;
}
void TimeSyncLayer::dumpHeader() {
	for (size_t i =0;i< m_DataLen;i++) {
		printf("%X ", m_Data[i]);
	}
	printf("Total =%lu\n", m_DataLen);
}

std::string TimeSyncLayer::toString() {
	char TS[500];
	sprintf( TS,"TimeSync=>");
	return std::string(TS);
}

void TimeSyncLayer::dumpString() {
	printf("TimeSync==> ");
	getCommand();
	getMagic();
	getReference_ts_hi();
	getReference_ts_lo();
	getDelta();
}
} // namespace pcpp
