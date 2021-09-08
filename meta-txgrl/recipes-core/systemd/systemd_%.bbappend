inherit systemd

PACKAGECONFIG_append = " networkd resolved"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI_append_ni-txgrl = " \
    file://10-eth0.network \
    file://timesyncd.conf \
    file://rngd.service \
"

FILES_${PN}_append_ni-txgrl = " \
    ${sysconfdir}/systemd/network/10-eth0.network \
	  ${sysconfdir}/systemd/timesyncd.conf \
    ${sysconfdir}/systemd/system/rngd.service \
"

FILES_${PN}_append_ni-txgrl = " \
    /data/* \
"

do_install_append_ni-txgrl() {
  if ${@bb.utils.contains('PACKAGECONFIG','networkd','true','false',d)}; then
        install -d ${D}/data/network

        install -m 0644 ${WORKDIR}/10-eth0.network ${D}/data/network/10-eth0.network

        install -d ${D}${sysconfdir}/systemd/network
        ln -sf /data/network/10-eth0.network ${D}${sysconfdir}/systemd/network/10-eth0.network

        if [ -e ${D}${sysconfdir}/systemd/network/eth.network ]; then
            rm ${D}${sysconfdir}/systemd/network/eth.network
        fi
  fi

  if ${@bb.utils.contains('PACKAGECONFIG','timesyncd','true','false',d)}; then
    install -d ${D}${sysconfdir}/systemd

    if [ -e ${D}${sysconfdir}/systemd/timesyncd.conf ]; then
      rm ${D}${sysconfdir}/systemd/timesyncd.conf
    fi

    install -m 0644 ${WORKDIR}/timesyncd.conf ${D}${sysconfdir}/systemd/timesyncd.conf
  fi

  install -m 0600 ${WORKDIR}/rngd.service ${D}${sysconfdir}/systemd/system/rngd.service

}
