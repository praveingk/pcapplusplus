echo ""
echo "*******************************"
echo "PcapPlusPlus setup DPDK script "
echo "*******************************"
echo ""


show_help() {
        echo "usage: setup-dpdk.sh -g AMOUNT_OF_HUGE_PAGES_TO_ALLOCATE -n NICS_TO_BIND_IN_COMMA_SEPARATED_LIST [-s] [-h]"
	echo "options:"
	echo "	-p	: amount of huge pages to allocate (huge pages are needed for DPDK's memory allocations)"
	echo "	-n	: a comma-separated list of all NICs that will be unbinded from Linux and move to DPDK control"
	echo "		  only these NICs will be used by DPDK. Example: eth0,eth1"
	echo "	-s	: display current Ethernet device settings (which are binded to Linux and which to DPDK)"
	echo "	-h	: show this help screen"
}


# setup DPDK variables
export RTE_SDK=/home/pravein/pcapplusplus/dpdk-stable-17.11.1/

# in DPDK 16.11 help scripts are stil in 'tools' dir but in 17.02 dir was renamed to 'usertools'
TOOLS_DIR=""
if [ -d $RTE_SDK/tools ]; then TOOLS_DIR=tools; else TOOLS_DIR=usertools; fi


# read and parse arguments

OPTIND=1	# Reset in case getopts has been used previously in the shell.

HUGE_PAGE_TO_ALLOCATE=0
NICS_TO_BIND=""

while getopts "h?sp:n:" opt; do
    case "$opt" in
    h|\?)
	show_help
        exit 0
        ;;
    p)  HUGE_PAGE_TO_ALLOCATE=$OPTARG
        ;;
    n)  NICS_TO_BIND=$OPTARG
        ;;
    s)  ${RTE_SDK}/${TOOLS_DIR}/dpdk-devbind.py --status
        exit 0
        ;;
    esac
done

shift $((OPTIND-1))

[ "$1" = "--" ] && shift


# verify huge page amount is indeed a string
re='^[0-9]+$'
if ! [[ $HUGE_PAGE_TO_ALLOCATE =~ $re ]] ; then
	echo "Error: Huge-page amount is not a number"
	echo
	show_help
	exit 1
fi

#verify nic list was given
if [[ $NICS_TO_BIND == "" ]] ; then
	echo "Error: List of NICs to bind was not given"
	echo
	show_help
	exit 1
fi


# setup huge-pages
CUR_HUGE=$(cat /proc/meminfo | grep -s HugePages_Total | awk '{print $2}')
if [ $CUR_HUGE != $HUGE_PAGE_TO_ALLOCATE ] ; then
	HUGEPAGE_MOUNT=/mnt/huge
	echo "echo $HUGE_PAGE_TO_ALLOCATE > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages" > .echo_tmp
	sudo sh .echo_tmp
	rm -f .echo_tmp
	sudo mkdir -p ${HUGEPAGE_MOUNT}
	sudo mount -t hugetlbfs nodev ${HUGEPAGE_MOUNT}
	echo "1. Reserve huge-pages - DONE!"
else
	echo "1. Huge-pages already allocated"
fi

# install kernel modules
IFS=","
for NIC_TO_BIND in $NICS_TO_BIND ; do	
	ifconfig | grep -s "^$NIC_TO_BIND" > /dev/null
	if [ $? -eq 1 ] ; then
		echo "2. $NIC_TO_BIND is already binded to DPDK or doesn't exist. Exiting"
		echo
		${RTE_SDK}/${TOOLS_DIR}/dpdk-devbind.py --status
		exit 1
	fi
done

lsmod | grep -s igb_uio > /dev/null
if [ $? -eq 0 ] ; then
	sudo rmmod igb_uio
fi

sudo modprobe uio
sudo insmod ${RTE_SDK}/build/kmod/igb_uio.ko
echo "2. Install kernel module - DONE!"


# bind network adapter
IFS=","
for NIC_TO_BIND in $NICS_TO_BIND ; do
	sudo ifconfig ${NIC_TO_BIND} down
	sudo ${RTE_SDK}/${TOOLS_DIR}/dpdk-devbind.py --bind=igb_uio ${NIC_TO_BIND}
done
echo "3. Bind network adapters - DONE!"

${RTE_SDK}/${TOOLS_DIR}/dpdk-devbind.py --status

echo "Setup DPDK completed"
