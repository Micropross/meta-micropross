#!/bin/sh

OVERLAY_DIR=/lib/firmware

# Load spi_nor overlay
echo "Load spi_nor device tree overlay."
gpioset gpiochip0 56=0
mkdir -p /sys/kernel/config/device-tree/overlays/spi_nor
cat $OVERLAY_DIR/spi_nor.dtbo > /sys/kernel/config/device-tree/overlays/spi_nor/dtbo

# Load pcie overlay
echo "Load pcie fevice tree overlay."
mkdir -p /sys/kernel/config/device-tree/overlays/pcie
cat $OVERLAY_DIR/pcie.dtbo > /sys/kernel/config/device-tree/overlays/pcie/dtbo