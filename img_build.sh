#!/bin/bash -e

cd `dirname $0`
main_dir=`pwd`
cd $main_dir

LOCALPATH=$(pwd)
OUT=${LOCALPATH}/out
EXTLINUXPATH=${LOCALPATH}/extlinux

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
DEFCONFIG=rockchip_linux_defconfig
UBOOT_DEFCONFIG=baina_nopmu-rk3288_defconfig
DTB=rk3288-core-board-v1.dtb
CHIP="rk3288"
BOARD=rk3288-core

version_gt() { test "$(echo "$@" | tr " " "\n" | sort -V | head -n 1)" != "$1"; }

finish() {
	echo -e "\e[31m MAKE KERNEL IMAGE FAILED.\e[0m"
	exit -1
}
trap finish ERR


[ ! -d ${OUT} ] && mkdir ${OUT}
[ ! -d ${OUT}/kernel ] && mkdir ${OUT}/kernel


echo -e "\e[36m Building kernel for ${BOARD} board! \e[0m"
echo -e "\e[36m Using ${DEFCONFIG} \e[0m"

cd ${LOCALPATH}
make ${DEFCONFIG}
make -j8

KERNEL_VERSION=$(cat ${LOCALPATH}/include/config/kernel.release)

if version_gt "${KERNEL_VERSION}" "4.5"; then
	if [ "${DTB_MAINLINE}" ]; then
		DTB=${DTB_MAINLINE}
	fi
fi

if [ "${ARCH}" == "arm" ]; then
	cp ${LOCALPATH}/arch/arm/boot/zImage ${OUT}/kernel/
	cp ${LOCALPATH}/arch/arm/boot/dts/${DTB} ${OUT}/kernel/
else
	cp ${LOCALPATH}/arch/arm64/boot/Image ${OUT}/kernel/
	cp ${LOCALPATH}/arch/arm64/boot/dts/rockchip/${DTB} ${OUT}/kernel/
fi

# Change extlinux.conf according board
sed -e "s,fdt .*,fdt /$DTB,g" \
	-i ${EXTLINUXPATH}/${CHIP}.conf

BOOT=${OUT}/boot.img
rm -rf ${BOOT}

echo -e "\e[36m Generate Boot image start\e[0m"

# 32 Mb
mkfs.vfat -n "BOOT" -S 512 -C ${BOOT} $((32 * 1024))

mmd -i ${BOOT} ::/extlinux
mcopy -i ${BOOT} -s ${EXTLINUXPATH}/${CHIP}.conf ::/extlinux/extlinux.conf
mcopy -i ${BOOT} -s ${OUT}/kernel/* ::

echo -e "\e[36m Generate Boot image : ${BOOT} success! \e[0m"

#./build/mk-image.sh -c ${CHIP} -t boot

echo -e "\e[36m Kernel build success! \e[0m"
