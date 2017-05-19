#!/bin/sh
#su - -c rockusb
while ! sudo lsusb -d 2207:330c; do sleep 1; done
sudo ./upgrade_tool db rk3399miniloader_V1.01.bin
#sudo ./upgrade_tool wl 0 chromeos.img
sudo ./chrome/upgrade_tool wl 20480 ./chrome/kernel.img
ls --color
