#define LOG_MODULE PacketLogModuleTsLayer

#include "SINLayer.h"
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

SINLayer::SINLayer(const uint8_t command, const uint8_t touch_range, const uint8_t num_entries, const uint16_t* keys) : Layer()
{
	m_DataLen = sizeof(sin_header);
	m_Data = new uint8_t[m_DataLen];
	memset(m_Data, 0, m_DataLen);


	sin_header* tsHdr = (sin_header*)m_Data;
	//tsHdr->ingressTs = //(uint8_t*)malloc(6 * sizeof(uint8_t));
	memcpy(tsHdr->keys, keys, num_entries);
	tsHdr->command = command;
	tsHdr->touch_range = touch_range;
	tsHdr->num_entries = num_entries;
	m_Protocol = SIN;
}


SINLayer::SINLayer() : Layer()
{
	m_DataLen = sizeof(sin_header);
	m_Data = new uint8_t[m_DataLen];
	memset(m_Data, 0, m_DataLen);
	m_Protocol = SIN;
}

void SINLayer::computeCalculateFields()
{
	return;
}

void SINLayer::parseNextLayer()
{
	return;

}

uint8_t SINLayer::getCommand() {
	return getSINHeader()->command;
}

uint8_t SINLayer::getTouchRange() {
	return getSINHeader()->touch_range;
}

uint8_t SINLayer::getNumEntries() {
	return getSINHeader()->num_entries;
}

uint16_t* SINLayer::getKeys() {
	return getSINHeader()->keys;
}


void SINLayer::dumpHeader() {
	for (size_t i =0;i< m_DataLen;i++) {
		printf("%X ", m_Data[i]);
	}
	printf("Total =%lu\n", m_DataLen);
}

std::string SINLayer::toString() {
	char TS[500];
	sprintf(TS, "SIN=> Command:%X, %X, Num_Entries: %d, Keys=", getCommand(), getTouchRange(), getNumEntries());
	uint8_t num = getNumEntries();
	uint16_t *keys = getKeys();
	for (int i=0;i< num;i++) {
		sprintf(TS, "%d,", keys[i]);
	}
	sprintf(TS,"\n");
	return std::string(TS);
}


} // namespace pcpp
