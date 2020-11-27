FILESEXTRAPATHS_prepend := "${THISDIR}/linux-yocto_5.4:"

SRC_URI_append_ni-txgrl = " \
                         file://defconfig \
                         file://txgrl.scc \
                         "

COMPATIBLE_MACHINE_ni-txgrl = "ni-txgrl"

KCONFIG_MODE ?= "--alldefconfig"
