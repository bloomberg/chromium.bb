# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_TESTDATA_THUMB_MASKS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_TESTDATA_THUMB_MASKS_H_

#define HIGH_TWO #0xC0000000
#define LOW_FOUR #0xF
#define DMASK(r) \
    bic r, r, HIGH_TWO
#define CMASK(r) \
    bic r, r, HIGH_TWO; \
    orr r, r, LOW_FOUR

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_TESTDATA_THUMB_MASKS_H_ */
