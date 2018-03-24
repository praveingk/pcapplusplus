#ifndef PACKETPP_TIMESYNC_LAYER
#define PACKETPP_TIMESYNC_LAYER

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
	struct timesync_t {
		uint8_t command;
		uint8_t magic;
		uint32_t reference_ts_lo;
		uint32_t reference_ts_hi;
		uint32_t delta;
	};
#pragma pack(pop)

	/**
	 * @class EthLayer
	 * Represents an Ethernet protocol layer
	 */
	class TimeSyncLayer : public Layer
	{
	public:
		/**
		 * A constructor that creates the layer from an existing packet raw data
		 * @param[in] data A pointer to the raw data (will be casted to ether_header)
		 * @param[in] dataLen Size of the data in bytes
		 * @param[in] prevLayer A pointer to the previous layer
		 * @param[in] packet A pointer to the Packet instance where layer will be stored in
		 */
		TimeSyncLayer(uint8_t* data, size_t dataLen, Layer* prevLayer, Packet* packet) : Layer(data, dataLen, prevLayer, packet) { m_Protocol = TS; }

		/**
		 * A constructor that creates a new TS header and allocates the data
		 * @param[in] sourceMac The source MAC address
		 * @param[in] destMac The destination MAC address
		 * @param[in] etherType The EtherType to be used. It's an optional parameter, a value of 0 will be set if not provided
		 */
		TimeSyncLayer(const uint8_t command, const uint8_t magic, const uint32_t reference_ts_lo, const uint32_t reference_ts_hi, const uint32_t delta);

		TimeSyncLayer();

		/**
		 * Get a pointer to the Ethernet header. Notice this points directly to the data, so every change will change the actual packet data
		 * @return A pointer to the ether_header
		 */
		inline timesync_t* getTimeSyncHeader() { return (timesync_t*)m_Data; }

		uint8_t getCommand();

		uint8_t getMagic();

	  uint32_t getReference_ts_lo();

	  uint32_t getReference_ts_hi();

		uint32_t getDelta();

		// implement abstract methods

		/**
		 * Currently identifies the following next layers: IPv4Layer, IPv6Layer, ArpLayer, VlanLayer, PPPoESessionLayer, PPPoEDiscoveryLayer,
		 * MplsLayer.
		 * Otherwise sets PayloadLayer
		 */
		void parseNextLayer();
		void computeCalculateFields();
		/**
		 * @return Size of ether_header
		 */
		inline size_t getHeaderLen() { return sizeof(timesync_t); }

		void dumpHeader();
		void dumpString();

		std::string toString();

		OsiModelLayer getOsiModelLayer() { return OsiModelDataLinkLayer; }
	};

} // namespace pcpp

#endif /* PACKETPP_ETH_LAYER */
