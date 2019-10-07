LINUX_VERSION="4.18.16"

SRCREV_machine="6b3252287aa248cb49befa5de2a68fed6413c814"

FILESEXTRAPATHS_prepend := "${THISDIR}/linux-yocto_4.18:"

SRC_URI_append = " \
                   file://core.scc \
                   file://core.cfg \
                 "