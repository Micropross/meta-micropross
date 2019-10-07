SUMMARY = "A console-only image with a development/debug \
environment."

#require version-image.inc

IMAGE_FEATURES += "ssh-server-openssh \
                   tools-sdk \
                   debug-tweaks \
                   dev-pkgs \
                  "

EXTRA_IMAGE_FEATURES += "package-management"

LICENSE = "MIT"

CORE_IMAGE_EXTRA_INSTALL = "\
    python3 \
    i2c-tools \
    devmem2 \
    screen \
    cmake \
    gsl \
    htop \
	nano \
    glib-2.0 \
    iperf3 \
    openocd \
	openssh-sftp \
    openssh-sftp-server \
    ethtool \
    tzdata \
    pciutils \
	gdbserver \
    "

inherit core-image image-buildinfo
TOOLCHAIN_HOST_TASK += "nativesdk-python3-setuptools \
                       "
