# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dragino_gw_fwd
PKG_VERSION:=2.8.4
PKG_RELEASE:=0

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)/Default
    TITLE:=Dragino lora-gateway forward
    URL:=http://www.dragino.com
    MAINTAINER:=dragino
endef

define Package/$(PKG_NAME)
$(call Package/$(PKG_NAME)/Default)
    SECTION:=utils
    CATEGORY:=Utilities
	DEPENDS:=+libsqlite3 +libmpsse 
endef

define Package/$(PKG_NAME)/description
  Dragino-gw is a gateway based on 
  a Semtech LoRa multi-channel RF receiver (a.k.a. concentrator).
endef

define Package/$(PKG_NAME)/extra_provides
	echo 'libmbedcrypto.so.0';\
	echo 'libmbedtls.so.10';\
	echo 'libmbedx509.so.0'
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/?* $(PKG_BUILD_DIR)/
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/lora-gateway
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/sx1302hal/inc/*.h $(1)/usr/include/lora-gateway
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libmqtt/libpahomqtt3c.so $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sx1302hal/libsx1302hal.so $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sx1301hal/libsx1301hal.so $(1)/usr/lib
	#$(LN) libsx1302hal.so $(1)/usr/lib/libloragw.so
	#$(CP) $(PKG_BUILD_DIR)/libmbedtls/library/libmbedcrypto.so.0 $(1)/usr/lib
	#$(CP) $(PKG_BUILD_DIR)/libmbedtls/library/libmbedtls.so.10 $(1)/usr/lib
	#$(CP) $(PKG_BUILD_DIR)/libmbedtls/library/libmbedx509.so.0 $(1)/usr/lib
	#$(LN) libmbedcrypto.so $(1)/usr/lib/libmbedcrypto.so.0
	#$(LN) libmbedtls.so $(1)/usr/lib/libmbedtls.so.10
	#$(LN) libmbedx509.so $(1)/usr/lib/libmbedx509.so.0
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build_fwd_sx1302/fwd_sx1302 $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build_fwd_sx1301/fwd_sx1301 $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build_fwd_sx1302/lbt_test_utily $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sx1302hal/test_* $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sx1301hal/test_loragw_hal $(1)/usr/bin/sx1301_test_hal
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build_station_sx1302/build-mips-openwrt-dragino/bin/station_sx1302 $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build_station_sx1301/build-mips-openwrt-dragino/bin/station_sx1301 $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/station/bin/rinit.sh $(1)/usr/bin
	$(LN) test_loragw_hal_rx $(1)/usr/bin/sx1302_rx_test
	$(LN) test_loragw_hal_tx $(1)/usr/bin/sx1302_tx_test
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/tools/reset_lgw.sh $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tools/rssh_client $(1)/usr/bin

	$(INSTALL_DIR) $(1)/etc/lora
	#$(INSTALL_CONF) $(PKG_BUILD_DIR)/config/gwcfg.json $(1)/etc/lora/local_conf.json
	#$(INSTALL_CONF) $(PKG_BUILD_DIR)/config/sxcfg.json $(1)/etc/lora/global_conf.json

	$(INSTALL_DIR) $(1)/etc/station
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/station/config/station*.conf $(1)/etc/station/

	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libmqtt/libpahomqtt3c.so $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sx1301hal/libsx1301hal.so $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sx1302hal/libsx1302hal.so $(1)/usr/lib
	$(LN) libsx1302hal.so $(1)/usr/lib/libloragw.so

	$(INSTALL_DIR) $(1)/etc/lora/cfg
	$(CP) $(PKG_BUILD_DIR)/config/*json* $(1)/etc/lora/cfg
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
