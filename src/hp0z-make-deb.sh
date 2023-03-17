#!/bin/bash

# hp0d(rasp)  spi=spi0.0 i2c=i2c-1 reset=23
# hp0c(h3)    spi=spi1.0 i2c=i2c-0 reset=20  //(new) i2c=i2c-1
# wrt(He)     spi=/dev/spi1.0 i2c=i2c-0 reset=23
#
#
#

VER=$(date -u '+%Y-%m-%d')

[[ -z $1 ]] && exit 1

## rassberry pi use hp0d option

prepare() {
    [[ "$1" = "rasp301" ]] && cp -f sx1301hal/inc/loragw_spi.h.rasp sx1301hal/inc/loragw_spi.h
    [[ ! -d build_fwd_sx1301 ]] && cp -rf fwd build_fwd_sx1301
    [[ ! -d build_fwd_sx1302 ]] && cp -rf fwd build_fwd_sx1302
    [[ -d station ]] && cp -f station/setup.gmk.arm  station/setup.gmk
    [[ ! -d build_station_sx1301 ]] && cp -rf station build_station_sx1301
    [[ ! -d build_station_sx1302 ]] && cp -rf station build_station_sx1302
}

case "$1" in 
    "d")
        board="hp0d"
        ;;
    "c")
        board="hp0c"
        ;;
    "r1")
        board="rasp301"
        ;;
    "r2")
        board="rasp302"
        ;;
    *)
        board="hp0c"
        ;;
esac

echo "Starting build draginofwd for board: $board"

prepare "$board"

if [[ "$board" = "rasp301" ]]; then
    echo "start compile libmpsse..."
    make all -e -C libmpsse  || exit 1
    make install  -C libmpsse  || exit 1
    echo "start compile fwd..."
    make -f makefile-hp0z RASPMOD=1 ||  exit 1
else
    echo "start compile fwd..."
    make -f makefile-hp0z  ||  exit 1
fi

echo "start make deb package..."

echo "FWD build succeed, start make deb package..."

echo "Deb package: draginofwd-${board}_${VER}.deb"

rm -rf pi_pkg
rm -rf draginofwd-*.deb

sed -i "s/^.*VER=.*/VER=\"${VER}\"/" hp0z-postinst
sed -i "s/^.*Version:.*/Version: ${VER}/" DEBIAN/control
mkdir -p pi_pkg/lib/systemd/system
mkdir -p pi_pkg/etc/lora
mkdir -p pi_pkg/etc/station
mkdir -p pi_pkg/usr/lib
mkdir -p pi_pkg/usr/bin


case "$board" in 
    "hp0c")
        cp -f hp0z-postinst DEBIAN/postinst
        cp -f hp0z-preinst DEBIAN/preinst
        cp -rf config/cfg-30? pi_pkg/etc/lora/
        install -m 755 sx1302hal/libsx1302hal.so pi_pkg/usr/lib/libsx1302hal.so
        install -m 755 sx1302hal/test_loragw_hal_rx pi_pkg/usr/bin/rx_test
        install -m 755 sx1302hal/test_loragw_hal_tx pi_pkg/usr/bin/tx_test
        install -m 755 sx1302hal/test_loragw_toa pi_pkg/usr/bin/toa_test
        install -m 755 sx1302hal/test_loragw_com pi_pkg/usr/bin/com_test
        install -m 755 sx1302hal/test_loragw_com_sx1250 pi_pkg/usr/bin/sx1250_test
        install -m 755 build_fwd_sx1302/fwd_sx1302 pi_pkg/usr/bin
        install -m 755 build_station_sx1302/build-arm-dragino/bin/station_sx1302 pi_pkg/usr/bin
        install -m 644 config/hp0c-global_conf.json pi_pkg/etc/lora/global_conf.json
        install -m 755 tools/reset_lgw-hp0c.sh pi_pkg/usr/bin/reset_lgw.sh
        ln -sf /usr/bin/fwd_sx1302 pi_pkg/usr/bin/fwd
        ;;
    "hp0d")
        cp -f hp0z-postinst DEBIAN/postinst
        cp -f hp0z-preinst DEBIAN/preinst
        cp -rf config/cfg-30? pi_pkg/etc/lora/
        install -m 755 sx1302hal/libsx1302hal.so pi_pkg/usr/lib/libsx1302hal.so
        install -m 755 sx1302hal/test_loragw_hal_rx pi_pkg/usr/bin/rx_test
        install -m 755 sx1302hal/test_loragw_hal_tx pi_pkg/usr/bin/tx_test
        install -m 755 sx1302hal/test_loragw_toa pi_pkg/usr/bin/toa_test
        install -m 755 sx1302hal/test_loragw_com pi_pkg/usr/bin/com_test
        install -m 755 sx1302hal/test_loragw_com_sx1250 pi_pkg/usr/bin/sx1250_test
        install -m 755 build_fwd_sx1302/fwd_sx1302 pi_pkg/usr/bin
        install -m 755 build_station_sx1302/build-arm-dragino/bin/station_sx1302 pi_pkg/usr/bin
        cp -f config/hp0d-global_conf.json pi_pkg/etc/lora/global_conf.json
        install -m 755 tools/reset_lgw-hp0d.sh pi_pkg/usr/bin/reset_lgw.sh
        ln -sf /usr/bin/fwd_sx1302 pi_pkg/usr/bin/fwd
        ;;
    "rasp301")
        cp -f rasp-postinst DEBIAN/postinst
        cp -rf config/cfg-301 pi_pkg/etc/lora/
        install -m 644 cfg-301/EU-global_conf.json pi_pkg/etc/lora/global_conf.json
        install -m 755 libmpsse/libmpsse.so pi_pkg/usr/lib/libmpsse.so
        install -m 755 sx1301hal/libsx1301hal.so pi_pkg/usr/lib/libsx1301hal.so
        install -m 755 build_fwd_sx1301/fwd_sx1301 pi_pkg/usr/bin
        install -m 755 build_station_sx1301/build-arm-dragino/bin/station_sx1301 pi_pkg/usr/bin
        install -m 755 tools/reset_lgw-hp0d.sh pi_pkg/usr/bin/reset_lgw.sh
        ln -sf /usr/bin/fwd_sx1301 pi_pkg/usr/bin/fwd
        ;;
    "rasp302")
        cp -f rasp-postinst DEBIAN/postinst
        cp -rf config/cfg-302 pi_pkg/etc/lora/
        install -m 755 sx1302hal/libsx1302hal.so pi_pkg/usr/lib/libsx1302hal.so
        install -m 755 sx1302hal/test_loragw_hal_rx pi_pkg/usr/bin/rx_test
        install -m 755 sx1302hal/test_loragw_hal_tx pi_pkg/usr/bin/tx_test
        install -m 755 sx1302hal/test_loragw_toa pi_pkg/usr/bin/toa_test
        install -m 755 sx1302hal/test_loragw_com pi_pkg/usr/bin/com_test
        install -m 755 sx1302hal/test_loragw_com_sx1250 pi_pkg/usr/bin/sx1250_test
        install -m 755 build_fwd_sx1302/fwd_sx1302 pi_pkg/usr/bin
        install -m 755 build_station_sx1302/build-arm-dragino/bin/station_sx1302 pi_pkg/usr/bin
        cp -f config/hp0d-global_conf.json pi_pkg/etc/lora/global_conf.json
        install -m 755 tools/reset_lgw-hp0d.sh pi_pkg/usr/bin/reset_lgw.sh
        ln -sf /usr/bin/fwd_sx1302 pi_pkg/usr/bin/fwd
        ;;
    *)
        cp -f rasp-postinst DEBIAN/postinst
        cp -rf config/cfg-30? pi_pkg/etc/lora/
        install -m 755 sx1302hal/libsx1302hal.so pi_pkg/usr/lib/libsx1302hal.so
        install -m 755 sx1302hal/test_loragw_hal_rx pi_pkg/usr/bin/rx_test
        install -m 755 sx1302hal/test_loragw_hal_tx pi_pkg/usr/bin/tx_test
        install -m 755 sx1302hal/test_loragw_toa pi_pkg/usr/bin/toa_test
        install -m 755 sx1302hal/test_loragw_com pi_pkg/usr/bin/com_test
        install -m 755 sx1302hal/test_loragw_com_sx1250 pi_pkg/usr/bin/sx1250_test
        install -m 755 build_fwd_sx1302/fwd_sx1302 pi_pkg/usr/bin
        install -m 755 build_station_sx1302/build-arm-dragino/bin/station_sx1302 pi_pkg/usr/bin
        cp -f config/hp0d-global_conf.json pi_pkg/etc/lora/global_conf.json
        install -m 755 tools/reset_lgw-hp0d.sh pi_pkg/usr/bin/reset_lgw.sh
        ln -sf /usr/bin/fwd_sx1302 pi_pkg/usr/bin/fwd
        ;;
esac

install -m 755 libmqtt/libpahomqtt3c.so pi_pkg/usr/lib/libpahomqtt3c.so
install -m 755 station/bin/rinit.sh pi_pkg/usr/bin

cp -f station/config/station-sx1302.conf pi_pkg/etc/station/station.conf

cp -f config/local_conf.json pi_pkg/etc/lora

cp -rf DEBIAN pi_pkg/
cp -rf draginofwd.service pi_pkg/lib/systemd/system
cp -rf draginostation.service pi_pkg/lib/systemd/system

dpkg-deb -Zgzip -b pi_pkg draginofwd-${board}_${VER}.deb

rm -rf pi_pkg

