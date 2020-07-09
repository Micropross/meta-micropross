#!/bin/sh

OVERLAY_LIST="txgrl-pl spi-nor interrupts pcie ni_cts3_spy"
OVERLAY_DIR=/lib/firmware

##
# Misc pre-overlay stuff
##

# Start LEDs rail voltage
gpioset 0 65=1

# Set SPI NOR buffer direction to be able detect the SPI NOR
gpioset 0 56=0

# Check FPGA binary md5
FPGA_PATTERN="/lib/firmware/fpga-mb_*.bin"
FPGA_SYMBOLIC="/lib/firmware/top.bin"
FPGA_COUNT=`ls -l $FPGA_PATTERN | wc -l`
if [ "$FPGA_COUNT" -eq "1" ]; then
    FPGA_BIN=`ls $FPGA_PATTERN`
    MD5_FILE=`md5sum $FPGA_PATTERN | cut -c -32`
    MD5_NAME=`ls $FPGA_PATTERN | cut -d "_" -f 3 | cut -c -32`

    if [ "$MD5_FILE" = "$MD5_NAME" ]; then
        # Symbolic file exist and point to the correct file
        if [ -L $FPGA_SYMBOLIC ]; then
            if [ `readlink -f $FPGA_SYMBOLIC` != "$FPGA_BIN" ]; then
                # Remore symbolic link
                rm -f $FPGA_SYMBOLIC

                # Create symbolic file
                ln -s $FPGA_PATTERN $FPGA_SYMBOLIC
            fi
        else
            # Create symbolic file
            ln -s $FPGA_PATTERN $FPGA_SYMBOLIC
        fi
    else
        # Error md5
        echo "Bad FPGA checksum: $FPGA_BIN"

        # Remore possible symbolic link
        rm $FPGA_SYMBOLIC > /dev/null
    fi
else
    # None or multiple binary
    echo "None or multiple FPGA binary files."

    # Remore possible symbolic link
    rm $FPGA_SYMBOLIC > /dev/null
fi

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

# Insert spy driver
modprobe ni_cts3_spy

# Create /dev/spy0 node
mknod /dev/spy0 c 242 0

# Insert DAQ driver
modprobe ni_cts3_daq

# Create DAQ node
mknod /dev/daq0 c 240 0

##
# Misc post-overlay stuff
##
mkdir -p /mnt/boot
mount /dev/mmcblk0p1 /mnt/boot

mkdir -p /mnt/data
mount /dev/mmcblk0p3 /mnt/data