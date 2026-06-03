# 移除luci-app-attendedsysupgrade软件包
sed -i "/attendedsysupgrade/d" $(find ./feeds/luci/collections/ -type f -name "Makefile")

# 移除要替换的包
rm -rf feeds/packages/net/ariang

# Git稀疏克隆，只克隆指定目录到本地
function git_sparse_clone() {
  branch="$1" repourl="$2" && shift 2
  git clone --depth=1 -b $branch --single-branch --filter=blob:none --sparse $repourl
  repodir=$(echo $repourl | awk -F '/' '{print $(NF)}')
  cd $repodir && git sparse-checkout set $@
  mv -f $@ ../package
  cd .. && rm -rf $repodir
}

### PassWall & OpenClash ###

# 移除 OpenWrt Feeds 自带的核心库
rm -rf feeds/packages/net/{xray-core,v2ray-geodata,sing-box,chinadns-ng,dns2socks,hysteria,ipt2socks,microsocks,naiveproxy,shadowsocks-libev,shadowsocks-rust,shadowsocksr-libev,simple-obfs,tcping,trojan-plus,tuic-client,v2ray-plugin,xray-plugin,geoview,shadow-tls}
git clone --depth=1 https://github.com/Openwrt-Passwall/openwrt-passwall-packages package/passwall-packages

# 移除 OpenWrt Feeds 过时的LuCI版本
rm -rf feeds/luci/applications/luci-app-passwall
rm -rf feeds/luci/applications/luci-app-openclash
git clone --depth=1 https://github.com/Openwrt-Passwall/openwrt-passwall package/luci-app-passwall
git clone --depth=1 https://github.com/Openwrt-Passwall/openwrt-passwall2 package/luci-app-passwall2
git clone --depth=1 https://github.com/vernesong/OpenClash package/luci-app-openclash

# 清理 PassWall 的 chnlist 规则文件
echo "baidu.com"  > package/luci-app-passwall/luci-app-passwall/root/usr/share/passwall/rules/chnlist

rm -rf feeds/packages/net/onionshare-cli

# 处理gcc14 + fortify + Werror 的兼容性问题
sed -i 's/-Werror=format-nonliteral/-Wno-error=format-nonliteral/g' package/libs/libubox/CMakeLists.txt 2>/dev/null || true
sed -i 's/-Werror/-Wno-error/g' package/libs/libubox/CMakeLists.txt 2>/dev/null || true

# 应用watchdog compatible补丁
cp $GITHUB_WORKSPACE/patches/0001-watchdog-qcom-add-ipq807x-compatible.patch $OPENWRT_PATH/target/linux/qualcommax/patches-6.12/
