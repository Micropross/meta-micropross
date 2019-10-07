FILESEXTRAPATHS_prepend_ni-txgrl := "${THISDIR}/files:"

SRC_URI_prepend_ni-txgrl = " \
    file://0001-Add-support-of-NI-Txgrl-board.patch \
    file://0002-Change-fdtdec_setup_memory_size-to-fdtdec_setup_mem_.patch \
    file://0003-FSBL-for-txgrl-board.patch \
"
