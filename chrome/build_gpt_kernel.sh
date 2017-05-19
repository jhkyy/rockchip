#!/bin/bash
PATH=$PATH:chrome
chrome/build_its.sh > chrome/its_script
mkimage -D "-I dts -O dtb -p 1024" -f chrome/its_script \
	chrome/kernel.tmp || exit

echo $@
./chrome/scripts/build_kernel_image.sh \
	--board=gru \
	--arch=arm \
	--to=chrome/kernel.img \
	--keys_dir=chrome/devkeys/ \
	--vmlinuz=chrome/kernel.tmp \
	--keep_work \
	--noenable_rootfs_verification \
	"$@"

./chrome/scripts/build_kernel_image.sh \
	--board=gru \
	--arch=arm \
	--to=chrome/kernel_recovery.img \
	--keys_dir=chrome/devkeys/ \
	--vmlinuz=chrome/kernel.tmp \
	--boot_args="noinitrd panic=60 cros_recovery " \
	--keep_work \
	--public="recovery_key.vbpubk" \
	--private="recovery_kernel_data_key.vbprivk" \
	--keyblock="recovery_kernel.keyblock" \
	--noenable_rootfs_verification \
	$@

