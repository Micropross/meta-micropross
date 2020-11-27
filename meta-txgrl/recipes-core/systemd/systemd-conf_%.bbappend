do_install_append() {
  sed -i -e 's/#RuntimeWatchdogSec=0/RuntimeWatchdogSec=10/g' ${D}${systemd_unitdir}/system.conf.d/00-${PN}.conf
}
