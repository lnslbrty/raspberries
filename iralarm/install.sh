#!/usr/bin/env bash
set -e

groupadd --system iralarm || true
useradd --system --shell /bin/false --comment 'IR-Alarm Daemon' --no-create-home --gid iralarm iralarmd || true

[ -z "${1}" ] || DESTDIR="${1}"
[ -z "${DESTDIR}" ] && DESTDIR=""
install -s ./iralarmctl "${DESTDIR}/usr/bin"
install -s ./iralarmd "${DESTDIR}/usr/sbin"
install ./initscript "${DESTDIR}/etc/init.d/iralarmd"
install ./initdefault "${DESTDIR}/etc/default/iralarmd"
touch /etc/irxmppasswd
chmod 0600 /etc/irxmppasswd
chown iralarmd /etc/irxmppasswd
ln -f -s "${DESTDIR}/usr/bin/iralarmctl" "${DESTDIR}/usr/bin/iralarmshell"
update-rc.d iralarmd defaults

