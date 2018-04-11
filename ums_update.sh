#! /bin/sh
mkdir -p tmp
mount /dev/sdb6 tmp && cp out/kernel/* tmp/ && sync && umount tmp && echo "Success."

