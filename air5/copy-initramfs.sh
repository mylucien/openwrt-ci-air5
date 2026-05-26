# Copy initramfs ITB to firmware output directory
# Add this step BEFORE "Organize Firmware" in the workflow

    - name: Copy initramfs ITB
      run: |
        INITRAMFS_FILE=$(find /mnt/openwrt/build_dir/target-aarch64_cortex-a53_musl/linux-qualcommax_ipq807x/ -name "*initramfs*itb" | head -1)
        if [ -f "$INITRAMFS_FILE" ]; then
          cp -v "$INITRAMFS_FILE" /mnt/openwrt/bin/targets/qualcommax/ipq807x/
          echo "initramfs ITB copied"
        else
          echo "No initramfs ITB found - check KERNEL_INITRAMFS"
        fi
