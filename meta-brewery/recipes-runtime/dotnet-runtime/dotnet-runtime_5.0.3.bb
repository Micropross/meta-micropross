SUMMARY = "dotnet-runtime: Microsoft's .NET 5.0.3"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=9fc642ff452b28d62ab19b7eea50dfb9"

SRC_URI = "https://download.visualstudio.microsoft.com/download/pr/94f3d0cd-6ccc-4eac-bac5-7fd1396581d5/b51a89d445f3fb7b2a795f0119fc0575/dotnet-runtime-5.0.3-linux-arm.tar.gz;subdir=dotnet-${PV}"
SRC_URI[sha512sum] = "d89c1769dfdfe30092825a94aa0037ca99ef83a0935ba24755377813db9e4a2e49e41611d02cf24aa4a423fb44bc566cdd935f62db61fe04a5257932bed4abf4"

DEPENDS = "zlib"
RDEPENDS_${PN} = "lttng-ust libgssapi-krb5"

INSANE_SKIP_${PN} += "already-stripped libdir textrel staticdev"

S = "${WORKDIR}/dotnet-${PV}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

FILES_${PN} += " \
	${datadir}/dotnet \
	"

do_install () {
	install -d ${D}${bindir}

	install -d ${D}${datadir}/dotnet/
	cp -r ${S} ${D}${datadir}/dotnet/

	cd ${D}${bindir}
	ln -s ../share/dotnet/dotnet ${D}${bindir}/dotnet
}

