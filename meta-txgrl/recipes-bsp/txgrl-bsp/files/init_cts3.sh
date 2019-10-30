#!/bin/sh

OVERLAY_LIST="txgrl-pl spi-nor interrupts pcie"
OVERLAY_DIR=/lib/firmware

##
# Misc pre-overlay stuff
##

# Set SPI NOR buffer direction to be able detect the SPI NOR
gpioset 0 56=0

##
# Load overlays
##
for overlay in $OVERLAY_LIST
do
    echo "Load $overlay device tree overlay."
    mkdir -p /sys/kernel/config/device-tree/overlays/$overlay
    cat $OVERLAY_DIR/$overlay.dtbo > /sys/kernel/config/device-tree/overlays/$overlay/dtbo
done

# Wait for kernel to start hardware driver
sleep 1

##
# Misc post-overlay stuff
##
