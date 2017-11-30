/* arm_features.h -- SoC features detection.
 * Copyright (C) 2017 ARM, Inc.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef __ARM_FEATURES__
#define __ARM_FEATURES__

void arm_check_features(void);

/* Wished I could use 'bool'. */
unsigned char arm_supports_crc32();

unsigned char arm_supports_pmull();

#endif
