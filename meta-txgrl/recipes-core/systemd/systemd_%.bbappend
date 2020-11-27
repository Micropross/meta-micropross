inherit systemd

PACKAGECONFIG_append = " networkd resolved"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI_append_ni-txgrl = " \
    file://eth0.network \
    file://timesyncd.conf \
"

FILES_${PN}_append_ni-txgrl = " \
    ${sysconfdir}/systemd/network/eth0.network \
	  ${sysconfdir}/systemd/timesyncd.conf \
"

FILES_${PN}_append_ni-txgrl = " \
    /data/* \
"

do_install_append_ni-txgrl() {
  if ${@bb.utils.contains('PACKAGECONFIG','networkd','true','false',d)}; then
        install -d ${D}/data/network

        install -m 0644 ${WORKDIR}/eth0.network ${D}/data/network/eth0.network

        install -d ${D}${sysconfdir}/systemd/network
        ln -sf /data/network/eth0.network ${D}${sysconfdir}/systemd/network/eth0.network

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

  sed -i -e 's/#RuntimeWatchdogSec=0/RuntimeWatchdogSec=10/g' ${D}${sysconfdir}/systemd/system.conf
}
