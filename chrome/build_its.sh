#!/bin/bash

iter=1
dtb_dir="arch/arm64/boot/dts/rockchip"
image="arch/arm64/boot/Image.lz4"

cat <<-EOF || die
/dts-v1/;

/ {
	description = "Chrome OS kernel image with one or more FDT blobs";
	#address-cells = <1>;

	images {
		kernel@1 {
			data = /incbin/("../${image}");
			type = "kernel_noload";
			arch = "arm64";
			os = "linux";
			compression = "lz4";
			load = <0>;
			entry = <0>;
		};
EOF

for dtb in `ls ${dtb_dir}/*.dtb` ; do
	cat <<-EOF || die
		fdt@${iter} {
			description = "$(basename ${dtb})";
			data = /incbin/("../${dtb}");
			type = "flat_dt";
			arch = "arm64";
			compression = "none";
			hash@1 {
				algo = "sha1";
			};
		};
	EOF
	((++iter))
done

cat <<-EOF
	};
	configurations {
		default = "conf@1";
EOF

for i in $(seq 1 $((iter-1))) ; do
	cat <<-EOF || die
		conf@${i} {
			kernel = "kernel@1";
			fdt = "fdt@${i}";
		};
	EOF
done

echo "  };"
echo "};"
