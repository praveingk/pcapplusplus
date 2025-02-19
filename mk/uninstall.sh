#!/bin/bash
set -e # Exit with nonzero exit code if anything fails

INSTALL_DIR=/usr/local

# remove libs
rm -f $INSTALL_DIR/lib/libCommon++.a $INSTALL_DIR/lib/libPacket++.a $INSTALL_DIR/lib/libPcap++.a

# remove header files
rm -rf $INSTALL_DIR/include/pcapplusplus

# remove examples
for f in examples/*; do rm $INSTALL_DIR/bin/$(basename "$f"); done

# remove template makefile
rm -f $INSTALL_DIR/etc/PcapPlusPlus.mk

# remove PcapPlusPlus.pc
PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-$INSTALL_DIR/lib/pkgconfig}"
rm -f $PKG_CONFIG_PATH/PcapPlusPlus.pc
