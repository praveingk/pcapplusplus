#ifndef PACKETPP_TS_LAYER
#define PACKETPP_TS_LAYER

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
	struct ts_header {
		uint32_t id;
		uint8_t ingressTs[6];
		uint8_t ingressMacTs[6];
		uint8_t egressTs[6];
		uint32_t enqTs;
		uint32_t deqDelta;
	};
#pragma pack(pop)

	/**
	 * @class EthLayer
	 * Represents an Ethernet protocol layer
	 */
	class TsLayer : public Layer
	{
	public:
		/**
		 * A constructor that creates the layer from an existing packet raw data
		 * @param[in] data A pointer to the raw data (will be casted to ether_header)
		 * @param[in] dataLen Size of the data in bytes
		 * @param[in] prevLayer A pointer to the previous layer
		 * @param[in] packet A pointer to the Packet instance where layer will be stored in
		 */
		TsLayer(uint8_t* data, size_t dataLen, Layer* prevLayer, Packet* packet) : Layer(data, dataLen, prevLayer, packet) { m_Protocol = TS; }

		/**
		 * A constructor that creates a new TS header and allocates the data
		 * @param[in] sourceMac The source MAC address
		 * @param[in] destMac The destination MAC address
		 * @param[in] etherType The EtherType to be used. It's an optional parameter, a value of 0 will be set if not provided
		 */
		TsLayer(const uint32_t id, const uint8_t* ingressTs, const uint8_t* ingressMacTs, const uint8_t* egressTs, const uint32_t enqTs, const uint32_t deqDelta);

		TsLayer();

		/**
		 * Get a pointer to the Ethernet header. Notice this points directly to the data, so every change will change the actual packet data
		 * @return A pointer to the ether_header
		 */
		inline ts_header* getTsHeader() { return (ts_header*)m_Data; }

		uint32_t getId();

		uint64_t getingressTs();

		uint64_t getingressMacTs();

		uint64_t getegressTs();

	  uint32_t getenqTs();

	  uint32_t getdeqDelta();


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
		inline size_t getHeaderLen() { return sizeof(ts_header); }

		void dumpHeader();
		void dumpString();

		std::string toString();

		OsiModelLayer getOsiModelLayer() { return OsiModelDataLinkLayer; }
	};

} // namespace pcpp

#endif /* PACKETPP_ETH_LAYER */
