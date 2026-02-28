# 处理gcc14 + fortify + Werror 的兼容性问题
sed -i 's/-Werror=format-nonliteral/-Wno-error=format-nonliteral/g' package/libs/libubox/CMakeLists.txt 2>/dev/null || true
sed -i 's/-Werror/-Wno-error/g' package/libs/libubox/CMakeLists.txt 2>/dev/null || true
