FILESEXTRAPATHS_prepend_ni-txgrl := "${THISDIR}/files:"

SRC_URI_prepend_ni-txgrl = " \
    file://0001-misc-cros_ec-Add-trivial-support-for-software-sync.patch \
    file://0002-Add-support-of-NI-Txgrl-board.patch \
    file://0003-Add-support-of-NI-Txgrl-board-rev-D.patch \
"
