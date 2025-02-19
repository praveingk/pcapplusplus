#!/bin/bash
set -e # Exit with nonzero exit code if anything fails

INSTALL_DIR=/usr/local

# copy libs
mkdir -p $INSTALL_DIR/lib
cp libCommon++.a libPacket++.a libPcap++.a $INSTALL_DIR/lib

# copy header files
mkdir -p $INSTALL_DIR/include
mkdir -p $INSTALL_DIR/include/pcapplusplus
cp header/* $INSTALL_DIR/include/pcapplusplus

# copy examples
mkdir -p $INSTALL_DIR/bin
cp examples/* $INSTALL_DIR/bin

# create template makefile 
cp mk/PcapPlusPlus.mk PcapPlusPlus.mk
sed -i.bak '/PCAPPLUSPLUS_HOME :=/d' PcapPlusPlus.mk && rm PcapPlusPlus.mk.bak 
sed -i.bak '/# libs dir/d' PcapPlusPlus.mk && rm PcapPlusPlus.mk.bak
sed -i.bak '/PCAPPP_LIBS_DIR :=/d' PcapPlusPlus.mk && rm PcapPlusPlus.mk.bak
sed -i.bak "s|PCAPPP_INCLUDES :=.*|PCAPPP_INCLUDES := -I$INSTALL_DIR/include/pcapplusplus|g" PcapPlusPlus.mk && rm PcapPlusPlus.mk.bak

# create PcapPlusPlus.pc
echo prefix=$INSTALL_DIR>PcapPlusPlus.pc
echo 'exec_prefix=${prefix}'>>PcapPlusPlus.pc
echo 'libdir=${exec_prefix}/lib'>>PcapPlusPlus.pc
echo 'includedir=${prefix}/include'>>PcapPlusPlus.pc
echo>>PcapPlusPlus.pc
echo 'Name: PcapPlusPlus'>>PcapPlusPlus.pc
echo 'Description: a multiplatform C++ network sniffing and packet parsing and crafting framework. It is meant to be lightweight, efficient and easy to use'>>PcapPlusPlus.pc
printf 'Version: '>>PcapPlusPlus.pc
grep '#define PCAPPLUSPLUS_VERSION ' header/PcapPlusPlusVersion.h | cut -d " " -f3 | tr -d "\"" | tr -d '\n'>>PcapPlusPlus.pc
printf '\n'>>PcapPlusPlus.pc
echo 'URL: https://seladb.github.io/PcapPlusPlus-Doc'>>PcapPlusPlus.pc
printf 'Libs: '>>PcapPlusPlus.pc
grep PCAPPP_LIBS PcapPlusPlus.mk | cut -d " " -f3- | tr -d '\r' | tr '\n' ' '>>PcapPlusPlus.pc
printf '\nCFlags: '>>PcapPlusPlus.pc
grep PCAPPP_INCLUDES PcapPlusPlus.mk | cut -d " " -f3- | tr -d '\r' | tr '\n' ' '>>PcapPlusPlus.pc
printf '\n'>>PcapPlusPlus.pc

# copy template makefile
mkdir -p $INSTALL_DIR/etc
mv PcapPlusPlus.mk $INSTALL_DIR/etc

# copy PcapPlusPlus.pc
PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-$INSTALL_DIR/lib/pkgconfig}"
mkdir -p $PKG_CONFIG_PATH
mv PcapPlusPlus.pc $PKG_CONFIG_PATH
