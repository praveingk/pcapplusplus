PCAPPLUSPLUS_HOME := /home/pravein/pcapplusplus_dpdk
RTE_SDK := /home/pravein/pcapplusplus/dpdk-stable-17.11.1/

USE_DPDK := 1


### COMMON ###

# includes
PCAPPP_INCLUDES := -I$(PCAPPLUSPLUS_HOME)/Dist/header

# libs dir
PCAPPP_LIBS_DIR := -L$(PCAPPLUSPLUS_HOME)/Dist

# libs
PCAPPP_LIBS := -lPcap++ -lPacket++ -lCommon++

# post build
PCAPPP_POST_BUILD :=

# build flags
PCAPPP_BUILD_FLAGS :=

### LINUX ###

# includes
PCAPPP_INCLUDES += -I/usr/include/netinet

# libs
PCAPPP_LIBS += -lpcap -lpthread


### DPDK ###

# includes
PCAPPP_INCLUDES += -I$(RTE_SDK)/build/include

# libs dir
PCAPPP_LIBS_DIR += -L$(RTE_SDK)/build/lib -L/lib64

#flags
PCAPPP_BUILD_FLAGS += -msse -msse2 -msse3 -Wall

# libs
PCAPPP_LIBS += -Wl,--whole-archive -lrte_pmd_bond -lrte_pmd_vmxnet3_uio -lrte_pmd_virtio -lrte_pmd_enic -lrte_pmd_i40e -lrte_pmd_fm10k -lrte_pmd_ixgbe -lrte_net -lrte_pmd_e1000 -lrte_pmd_ring -lrte_pmd_af_packet -lrte_ethdev -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring -lrte_kvargs -lrte_hash -lrte_cmdline -lrt -lm -ldl -lpthread -Wl,--no-whole-archive


### DPDK >= 17.11 ###

# libs
PCAPPP_LIBS += -Wl,--whole-archive -lrte_pci -lrte_bus_pci -lrte_bus_vdev -lrte_mempool_ring -lnuma -Wl,--no-whole-archive
