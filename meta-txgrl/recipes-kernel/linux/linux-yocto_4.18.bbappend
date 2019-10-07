FILESEXTRAPATHS_prepend := "${THISDIR}/linux-yocto_4.18:"

SRC_URI_append_ni-txgrl = " \
                         file://defconfig \
                         file://txgrl.scc \
                         "

COMPATIBLE_MACHINE_ni-txgrl = "ni-txgrl"

KCONFIG_MODE ?= "--alldefconfig"
