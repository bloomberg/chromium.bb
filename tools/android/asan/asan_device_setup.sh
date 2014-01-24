#!/bin/bash -e
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Prepare Android device to run ASan application.


HERE="$(cd "$(dirname "$0")" && pwd)"

revert=no
extra_options=
device=

function usage {
    echo "usage: $0 [--revert] [--device device-id]"
    echo "  --revert: Uninstall ASan from the device."
    echo "  --extra_options: Extra ASAN_OPTIONS."
    echo "  --device: Install to the given device. Use 'adb devices' to find"
    echo "            device-id."
    echo
    exit 1
}

while [[ $# > 0 ]]; do
  case $1 in
    --revert)
      revert=yes
      ;;
    --extra-options)
      shift
      if [[ $# == 0 ]]; then
        echo "--extra-options requires an argument."
        exit 1
      fi
      extra_options="$1"
      ;;
    --device)
      shift
      if [[ $# == 0 ]]; then
        echo "--device requires an argument."
        exit 1
      fi
      device="$1"
      ;;
    *)
      usage
      ;;
  esac
  shift
done

ADB="adb"
if [[ x$device != x ]]; then
    ADB="adb -s $device"
fi

ASAN_RT_PATH="$HERE"
ASAN_RT="libclang_rt.asan-arm-android.so"

if [[ x$revert == xyes ]]; then
    echo '>> Uninstalling ASan'
    $ADB remount
    $ADB shell mv /system/bin/app_process.real /system/bin/app_process
    $ADB shell rm /system/bin/asanwrapper
    $ADB shell rm /system/lib/$ASAN_RT

    echo '>> Restarting shell'
    $ADB shell stop
    $ADB shell start

    echo '>> Done'
    exit 0
fi

if ! [[ -f "$ASAN_RT_PATH/$ASAN_RT" ]]; then
    echo "ASan runtime library not found at $ASAN_RT"
    exit 1
fi

TMPDIRBASE=$(mktemp -d)
echo $TMPDIRBASE
TMPDIROLD="$TMPDIRBASE/old"
TMPDIR="$TMPDIRBASE/new"
mkdir "$TMPDIROLD"

echo '>> Remounting /system rw'
$ADB remount

echo '>> Copying files from the device'
$ADB pull /system/bin/app_process "$TMPDIROLD"
$ADB pull /system/bin/app_process.real "$TMPDIROLD" || true
$ADB pull /system/bin/asanwrapper "$TMPDIROLD" || true
$ADB pull /system/lib/libclang_rt.asan-arm-android.so "$TMPDIROLD" || true
cp -r "$TMPDIROLD" "$TMPDIR"

if ! [[ -f "$TMPDIR/app_process" ]]; then
    echo "app_process missing???"
    exit 1
fi

if [[ -f "$TMPDIR/app_process.real" ]]; then
    echo "app_process.real exists, updating the wrapper"
else
    echo "app_process.real missing, new installation"
    mv "$TMPDIR/app_process" "$TMPDIR/app_process.real"
fi

echo '>> Generating wrappers'

cp "$ASAN_RT_PATH/$ASAN_RT" "$TMPDIR/"

# FIXME: alloc_dealloc_mismatch=0 prevents a failure in libdvm startup,
# which may or may not be a real bug (probably not).
ASAN_OPTIONS=start_deactivated=1,alloc_dealloc_mismatch=0
if [[ x$extra_options != x ]] ; then
    ASAN_OPTIONS="$ASAN_OPTIONS,$extra_options"
fi

# Zygote wrapper.
cat <<EOF >"$TMPDIR/app_process"
#!/system/bin/sh
ASAN_OPTIONS=$ASAN_OPTIONS \\
LD_PRELOAD=libclang_rt.asan-arm-android.so \\
exec /system/bin/app_process.real \$@

EOF

# General command-line tool wrapper (use for anything that's not started as
# zygote).
cat <<EOF >"$TMPDIR/asanwrapper"
#!/system/bin/sh
LD_PRELOAD=libclang_rt.asan-arm-android.so \\
exec \$@

EOF

if ! ( cd "$TMPDIRBASE" && diff -qr old/ new/ ) ; then
    echo '>> Pushing files to the device'
    $ADB push "$TMPDIR/$ASAN_RT" /system/lib/
    $ADB push "$TMPDIR/app_process" /system/bin/app_process
    $ADB push "$TMPDIR/app_process.real" /system/bin/app_process.real
    $ADB push "$TMPDIR/asanwrapper" /system/bin/asanwrapper
    $ADB shell chown root.shell \
        /system/bin/app_process \
        /system/bin/app_process.real \
        /system/bin/asanwrapper
    $ADB shell chmod 755 \
        /system/bin/app_process \
        /system/bin/app_process.real \
        /system/bin/asanwrapper

    echo '>> Restarting shell (asynchronous)'
    $ADB shell stop
    $ADB shell start

    echo '>> Please wait until the device restarts'
else
    echo '>> Device is up to date'
fi

rm -r "$TMPDIRBASE"
