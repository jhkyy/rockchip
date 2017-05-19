#!/bin/bash
export ARCH=arm64

CONFIG=true
ZIMAGE=true
MODULES=true
DTB=true
SERIAL=true

while [ -n "$1" ];do
	case "$1" in
		c)
			CONFIG=true
			;;
		nc)
			CONFIG=false
			;;
		z)
			ZIMAGE=true
			;;
		nz)
			ZIMAGE=false
			;;
		m)
			MODULES=true
			;;
		nm)
			MODULES=false
			;;
		d)
			DTB=true
			;;
		nd)
			DTB=false
			;;
		s)
			SERIAL=true
			;;
		ns)
			SERIAL=false
			;;
	esac
	shift
done


#setup toolchain
export CROSS_COMPILE=aarch64-linux-gnu-

function config_enable() {
	sed "/# $1 is not set/d" .config -i
	echo "$1=y" >> .config
}

function config_disable() {
	sed "/$1=y/d" .config -i
	echo "# $1 is not set" >> .config
}

if "$CONFIG";then
	#generate .config
	./chromeos/scripts/prepareconfig chromiumos-rockchip64
	# make rockchip_cros_defconfig
	make oldnoconfig
	#config_enable CONFIG_VLAN_8021Q
	#config_disable CONFIG_VT
	#config_disable CONFIG_ERROR_ON_WARNING
	#config_disable CONFIG_FB_ROCKCHIP
	#config_disable CONFIG_POWERVR_ROGUE_M
	#make oldnoconfig

echo "
CONFIG_ANDROID=y
CONFIG_ANDROID_BINDER_IPC=y
CONFIG_ANDROID_BINDER_IPC_32BIT=n
CONFIG_ASHMEM=y
CONFIG_ANDROID_LOGGER=y
CONFIG_ANDROID_TIMED_OUTPUT=y
CONFIG_ANDROID_TIMED_GPIO=y
CONFIG_ANDROID_LOW_MEMORY_KILLER=y
CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES=y
CONFIG_ANDROID_INTF_ALARM_DEV=y
CONFIG_SYNC=y
CONFIG_SW_SYNC=y
CONFIG_SW_SYNC_USER=y
CONFIG_ION=y
CONFIG_INPUT_KEYCHORD=y
CONFIG_INPUT_KEYCOMBO=y
CONFIG_INPUT_KEYRESET=y
CONFIG_PM_WAKELOCKS=y
CONFIG_PM_WAKELOCKS_GC=y
CONFIG_PM_WAKELOCKS_LIMIT=100
CONFIG_RESOURCE_COUNTERS=y
CONFIG_RT_GROUP_SCHED=y
CONFIG_ION_DUMMY=y
CONFIG_ION_ROCKCHIP=y
CONFIG_IPV6_ROUTE=y
CONFIG_IPV6_MULTIPLE_TABLES=y
CONFIG_IPV6_MROUTE_MULTIPLE_TABLES=y
CONFIG_AUDIT=y
CONFIG_AUDIT_GENERIC=y
CONFIG_AUDITSYSCALL=y
CONFIG_AUDIT_TREE=y
CONFIG_AUDIT_WATCH=y
CONFIG_SECURITY_SELINUX=y
CONFIG_SECURITY_SELINUX_BOOTPARAM=y
CONFIG_SECURITY_SELINUX_BOOTPARAM_VALUE=1
CONFIG_SECURITY_SELINUX_DEVELOP=y
CONFIG_SQUASHFS_XATTR=y
CONFIG_IP6_NF_RAW=y
CONFIG_IP_NF_RAW=y
CONFIG_IP_NF_SECURITY=y
CONFIG_NETFILTER_XT_CONNMARK=y
CONFIG_NETFILTER_XT_MATCH_CONNMARK=y
CONFIG_NETFILTER_XT_MATCH_STATE=y
CONFIG_NETFILTER_XT_MATCH_U32=y
CONFIG_NETFILTER_XT_TARGET_CONNSECMARK=y
CONFIG_NF_CONNTRACK_SECMARK=y
CONFIG_DEFAULT_SECURITY_SELINUX=y
CONFIG_DEFAULT_SECURITY="selinux"
# CONFIG_PREEMPT is not set
CONFIG_PREEMPT_VOLUNTARY=y
" >> .config
	make oldnoconfig

	config_enable CONFIG_MODULE_FORCE_LOAD

	config_enable CONFIG_USB_USBNET
	config_enable CONFIG_USB_NET_AX8817X
	config_enable CONFIG_USB_NET_AX88179_178A

	config_disable CONFIG_ERROR_ON_WARNING
#	config_enable CONFIG_PCIEASPM_DEBUG

#	config_disable CONFIG_BATTERY_SBS
fi

trap "mv ../.git ." EXIT

mv .git ../
if "$ZIMAGE";then
	make Image -j`grep -E "siblings|processor" /proc/cpuinfo |wc -l` \
		|| exit
	yes 'Y' | lz4c arch/arm64/boot/Image arch/arm64/boot/Image.lz4
fi

if "$MODULES";then
	make modules -j`grep -E "siblings|processor" /proc/cpuinfo |wc -l` \
		|| exit
	rm -r modules
	make modules_install INSTALL_MOD_PATH=./modules/
fi

if "$DTB";then
        rm arch/arm64/boot/dts/rockchip/*.dtb*
        make dtbs || exit
fi

OPT=
if "$SERIAL";then
	OPT="--enable_serial=ttyS2"
fi

./chrome/build_gpt_kernel.sh $OPT --boot_args="console=ttyS2 earlycon=uart8250,mmio32,0xff1a0000 no_console_suspend=1"
