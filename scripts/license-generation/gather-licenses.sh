#!/bin/bash
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

source "${T}/environment" || { echo "failed sourcing ${T}/environment"; exit 1;}
if [[ "${QA_LICENSE_IGNORE}" == "yes" ]]; then
    echo "license/copyright gathering suppressed for ${CATEGORY}/${PN}-${PVR}"
    exit 0
fi

# This will be enabled when it's finished and tested.
#$(dirname $(readlink -f $0))/licenses/process-pkg.py \
#    "${CATEGORY}/${PN}-${PVR}" \
#    "${CHROME_FORCE_LICENSE-${LICENSE}}" \
#    "${HOMEPAGE}" "${D}" "${WORKDIR}" "${PORTDIR}/licenses"
#exit $(( $? ))
exit 0
