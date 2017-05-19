#!/bin/sh
IP=${1:-172.16.22.10}
if [ -n "$2" ];then
	rm modules/lib/modules/*/build
	rm modules/lib/modules/*/source
	ssh ${IP} "mount /dev/mmcblk0p3 /media/ && rm -r /media/lib/modules/"
	scp -r ./modules/lib ${IP}:/media/
fi

scp chrome/kernel.img ${IP}:/dev/mmcblk0p2
sync&
echo updated
ssh ${IP} "cgpt add /dev/mmcblk0 -i 2 -S 1 -P 15 -T 1 && cgpt add /dev/mmcblk0 -i 4 -S 1 -P 14 -T 15 && reboot -f&" &
