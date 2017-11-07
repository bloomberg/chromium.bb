#!/bin/bash

set -x 

ICUROOT=$HOME/cr/t/src/third_party/icu

$ICUROOT/scripts/trim_data.sh
$ICUROOT/scripts/make_data.sh
$ICUROOT/scripts/copy_data.sh common
$ICUROOT/android/patch_locale.sh
$ICUROOT/scripts/make_data.sh
$ICUROOT/scripts/copy_data.sh android
$ICUROOT/ios/patch_locale.sh
$ICUROOT/scripts/make_data.sh
$ICUROOT/scripts/copy_data.sh ios
$ICUROOT/scripts/clean_up_data_source.sh

