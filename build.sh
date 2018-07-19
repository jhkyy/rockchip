#!/bin/bash

make ARCH=arm64 rockchip_defconfig 
make ARCH=arm64 rk3399-binocular-818-android.img -j8
