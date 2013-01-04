/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license.
 *
 * Some functions commonly found in sys/endian.h, a header file not available
 * on Windows platforms.
 */

#ifndef _SCRYPT_SYSENDIAN_H
#define _SCRYPT_SYSENDIAN_H

static __inline void be32enc(void *buf, uint32_t u)
{
  uint8_t *p = (uint8_t *)buf;
  p[0] = (uint8_t)((u >> 24) & 0xff);
  p[1] = (uint8_t)((u >> 16) & 0xff);
  p[2] = (uint8_t)((u >> 8) & 0xff);
  p[3] = (uint8_t)(u & 0xff);
}

static __inline void le32enc(void *buf, uint32_t u)
{
  uint8_t *p = (uint8_t *)buf;
  p[0] = (uint8_t)(u & 0xff);
  p[1] = (uint8_t)((u >> 8) & 0xff);
  p[2] = (uint8_t)((u >> 16) & 0xff);
  p[3] = (uint8_t)((u >> 24) & 0xff);
}

static __inline uint32_t be32dec(const void *buf)
{
  const uint8_t *p = (const uint8_t *)buf;
  return ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static __inline uint32_t le32dec(const void *buf)
{
  const uint8_t *p = (const uint8_t *)buf;
  return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}

#endif  // _SCRYPT_SYSENDIAN_H
