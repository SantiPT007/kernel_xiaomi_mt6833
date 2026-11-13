#!/bin/bash
set -e
R=$(cd "$(dirname "$0")/../.." && pwd)
cd "$R"
[ -f camellia/local.env ] && set -a && . camellia/local.env && set +a
S=${BFM_WORK:-$R/out/bfm}; mkdir -p $S
if [ "$SKIPBUILD" != 1 ]; then
  if [ "$1" = full ]; then shift; bash camellia/build.sh > $S/build.log 2>&1 || { tail -40 $S/build.log; exit 1; }; else bash camellia/build.sh mtk-only > $S/build.log 2>&1 || { tail -40 $S/build.log; exit 1; }; fi
fi
MK1=0; bash camellia/mkboot.sh > $S/mkboot.log 2>&1 || MK1=1
python3 camellia/tools/select-ramdisk-modules.py out/bootimg/camellia66.dtb out > $S/rd.txt 2>$S/rd.err || { cat $S/rd.err; exit 1; }
if [ $MK1 = 1 ] || ! cmp -s $S/rd.txt camellia/ramdisk-modules.txt; then
  cp $S/rd.txt camellia/ramdisk-modules.txt
  bash camellia/mkboot.sh > $S/mkboot.log 2>&1 || { tail -30 $S/mkboot.log; exit 1; }
fi
echo BUILT
[ "$1" = noflash ] && exit 0
adb reboot bootloader
for i in $(seq 60); do fastboot devices | grep -q fastboot && break; sleep 1; done
fastboot flash boot_a out/bootimg/boot-6.6-test-A.img
fastboot reboot
adb wait-for-device
for i in $(seq 120); do [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && break; sleep 2; done
adb shell uname -r
