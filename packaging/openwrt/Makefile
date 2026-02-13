# Minimal OpenWrt package recipe (skeleton)
include $(TOPDIR)/rules.mk

PKG_NAME:=irr
PKG_RELEASE:=1
PKG_VERSION:=0.1.0

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/irr
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Internet Reliability Recorder
  DEPENDS:=+libpthread
endef

define Package/irr/description
  Internet Reliability Recorder: TCP/DNS/ICMP/PMTU reliability logger with HTML reports.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./ $(PKG_BUILD_DIR)/
endef

define Build/Configure
	(cd $(PKG_BUILD_DIR); cmake -S . -B build \
	-DCMAKE_BUILD_TYPE=Release \
	-DIRR_ENABLE_PCAP=OFF \
	-DCMAKE_INSTALL_PREFIX=/usr)
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/build
endef

define Package/irr/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build/irr $(1)/usr/bin/irr
endef

$(eval $(call BuildPackage,irr))
