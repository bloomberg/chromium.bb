#!/bin/bash

set -x

ICUROOT="$(dirname "$0")/.."

echo "Build the necessary tools"
"${ICUROOT}/source/runConfigureICU" --enable-debug --disable-release Linux/gcc --disable-tests
make -j 120

echo "Build the filtered data for common"
(cd data && make clean)
$ICUROOT/scripts/config_data.sh common
make -j 120
$ICUROOT/scripts/copy_data.sh common

echo "Build the filtered data for chromeos"
(cd data && make clean)
$ICUROOT/scripts/config_data.sh chromeos
make -j 120
# For now just copy the build result from common
$ICUROOT/scripts/copy_data.sh chromeos

echo "Build the filtered data for Cast"
(cd data && make clean)
$ICUROOT/scripts/config_data.sh cast
$ICUROOT/cast/patch_locale.sh
make -j 120
$ICUROOT/scripts/copy_data.sh cast

echo "Build the filtered data for Android"
(cd data && make clean)
$ICUROOT/scripts/config_data.sh android
make -j 120
$ICUROOT/scripts/copy_data.sh android

echo "Build the filtered data for AndroidSmall"
(cd data && make clean)
$ICUROOT/scripts/config_data.sh android_small
make -j 120
$ICUROOT/scripts/copy_data.sh android_small

echo "Build the filtered data for iOS"
(cd data && make clean)
$ICUROOT/scripts/config_data.sh ios
make -j 120
$ICUROOT/scripts/copy_data.sh ios

echo "Build the filtered data for Flutter"
(cd data && make clean)
$ICUROOT/scripts/config_data.sh flutter
$ICUROOT/flutter/patch_brkitr.sh
make -j 120
$ICUROOT/scripts/copy_data.sh flutter

echo "Clean up the git"
$ICUROOT/scripts/clean_up_data_source.sh
