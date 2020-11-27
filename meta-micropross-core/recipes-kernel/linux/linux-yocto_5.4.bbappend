LINUX_VERSION="5.4.78"

SRCREV_machine="59b9881ae7bb7795afb44687759c40c788bdd546"

FILESEXTRAPATHS_prepend := "${THISDIR}/linux-yocto_5.4:"

SRC_URI_append = " \
                   file://core.scc \
                   file://core.cfg \
                 "

KERNEL_VERSION_SANITY_SKIP = "1"