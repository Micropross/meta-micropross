DESCRIPTION = "image for the flasher"

PACKAGE_INSTALL = "busybox flasher libgpiod-tools mtd-utils"
IMAGE_LINGUAS = ""
LICENSE = "MIT"

IMAGE_FSTYPES = "${INITRAMFS_FSTYPES}"

inherit image

BAD_RECOMMENDATIONS += "busybox-syslog"
