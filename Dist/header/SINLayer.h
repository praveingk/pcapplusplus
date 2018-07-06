#ifndef PACKETPP_SIN_LAYER
#define PACKETPP_SIN_LAYER

#include "Layer.h"


/// @file

/**
 * \namespace pcpp
 * \brief The main namespace for the PcapPlusPlus lib
 */
namespace pcpp
{

	/**
	 * @struct ts_header
	 * Represents an TimeSync header
	 */
#pragma pack(push, 1)
	struct sin_header {
		uint8_t command;
		uint8_t touch_range;
		uint8_t num_entries;
		uint16_t keys[10];
	};
#pragma pack(pop)

	/**
	 * @class EthLayer
	 * Represents an Ethernet protocol layer
	 */
	class SINLayer : public Layer
	{
	public:
		/**
		 * A constructor that creates the layer from an existing packet raw data
		 * @param[in] data A pointer to the raw data (will be casted to ether_header)
		 * @param[in] dataLen Size of the data in bytes
		 * @param[in] prevLayer A pointer to the previous layer
		 * @param[in] packet A pointer to the Packet instance where layer will be stored in
		 */
		SINLayer(uint8_t* data, size_t dataLen, Layer* prevLayer, Packet* packet) : Layer(data, dataLen, prevLayer, packet) { m_Protocol = SIN; }

		/**
		 * A constructor that creates a new TS header and allocates the data
		 * @param[in] sourceMac The source MAC address
		 * @param[in] destMac The destination MAC address
		 * @param[in] etherType The EtherType to be used. It's an optional parameter, a value of 0 will be set if not provided
		 */
		SINLayer(const uint8_t command, const uint8_t touch_range, const uint8_t num_entries, const uint16_t* keys);

		SINLayer();

		/**
		 * Get a pointer to the Ethernet header. Notice this points directly to the data, so every change will change the actual packet data
		 * @return A pointer to the ether_header
		 */
		inline sin_header* getSINHeader() { return (sin_header*)m_Data; }

		uint8_t getCommand();

		uint8_t getTouchRange();
		
		uint8_t getNumEntries();

		uint16_t* getKeys();

		void parseNextLayer();
		void computeCalculateFields();
		/**
		 * @return Size of ether_header
		 */
		inline size_t getHeaderLen() { return sizeof(sin_header); }

		void dumpHeader();
		void dumpString();

		std::string toString();

		OsiModelLayer getOsiModelLayer() { return OsiModelDataLinkLayer; }
	};

} // namespace pcpp

#endif /* PACKETPP_ETH_LAYER */
