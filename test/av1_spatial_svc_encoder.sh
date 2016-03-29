#!/bin/sh
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
## This file tests the libaom av1_spatial_svc_encoder example. To add new
## tests to to this file, do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to vp9_spatial_svc_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: $YUV_RAW_INPUT is required.
av1_spatial_svc_encoder_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    echo "Libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
}

# Runs av1_spatial_svc_encoder. $1 is the test name.
av1_spatial_svc_encoder() {
  local readonly \
    encoder="${LIBAOM_BIN_PATH}/av1_spatial_svc_encoder${AOM_TEST_EXE_SUFFIX}"
  local readonly test_name="$1"
  local readonly \
    output_file="${AOM_TEST_OUTPUT_DIR}/vp9_ssvc_encoder${test_name}.ivf"
  local readonly frames_to_encode=10
  local readonly max_kf=9999

  shift

  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${encoder}" -w "${YUV_RAW_INPUT_WIDTH}" \
    -h "${YUV_RAW_INPUT_HEIGHT}" -k "${max_kf}" -f "${frames_to_encode}" \
    "$@" "${YUV_RAW_INPUT}" "${output_file}" ${devnull}

  [ -e "${output_file}" ] || return 1
}

# Each test is run with layer count 1-$vp9_ssvc_test_layers.
vp9_ssvc_test_layers=5

vp9_spatial_svc() {
  if [ "$(vp9_encode_available)" = "yes" ]; then
    local readonly test_name="vp9_spatial_svc"
    for layers in $(seq 1 ${vp9_ssvc_test_layers}); do
      av1_spatial_svc_encoder "${test_name}" -sl ${layers}
    done
  fi
}

readonly vp9_spatial_svc_tests="DISABLED_vp9_spatial_svc_mode_i
                                DISABLED_vp9_spatial_svc_mode_altip
                                DISABLED_vp9_spatial_svc_mode_ip
                                DISABLED_vp9_spatial_svc_mode_gf
                                vp9_spatial_svc"

if [ "$(aom_config_option_enabled CONFIG_SPATIAL_SVC)" = "yes" ]; then
  run_tests \
    av1_spatial_svc_encoder_verify_environment \
    "${vp9_spatial_svc_tests}"
fi
