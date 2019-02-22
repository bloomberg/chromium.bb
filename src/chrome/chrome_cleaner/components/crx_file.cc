// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Stolen from extensions/common/crx_file.cc
// TODO(crbug.com/889575): The above file no longer exists. It's been
// refactored and now lives in components/crx_file. Should pull in that
// component directly.

#include "chrome/chrome_cleaner/components/crx_file.h"

#include "base/memory/ptr_util.h"

namespace chrome_cleaner {

namespace {

// The current version of the crx format.
static const uint32_t kCurrentVersion = 2;

// The current version of the crx diff format.
static const uint32_t kCurrentDiffVersion = 0;

// The maximum size the crx parser will tolerate for a public key.
static const uint32_t kMaxPublicKeySize = 1 << 16;

// The maximum size the crx parser will tolerate for a signature.
static const uint32_t kMaxSignatureSize = 1 << 16;

}  // namespace

// The magic string embedded in the header.
const char kCrxFileHeaderMagic[] = "Cr24";
const char kCrxDiffFileHeaderMagic[] = "CrOD";

std::unique_ptr<CrxFile> CrxFile::Parse(const CrxFile::Header& header,
                                        CrxFile::Error* error) {
  if (HeaderIsValid(header, error))
    return base::WrapUnique<CrxFile>(new CrxFile(header));
  return nullptr;
}

std::unique_ptr<CrxFile> CrxFile::Create(const uint32_t key_size,
                                         const uint32_t signature_size,
                                         CrxFile::Error* error) {
  CrxFile::Header header;
  memcpy(&header.magic, kCrxFileHeaderMagic, kCrxFileHeaderMagicSize);
  header.version = kCurrentVersion;
  header.key_size = key_size;
  header.signature_size = signature_size;
  if (HeaderIsValid(header, error))
    return base::WrapUnique<CrxFile>(new CrxFile(header));
  return nullptr;
}

CrxFile::CrxFile(const Header& header) : header_(header) {}

bool CrxFile::HeaderIsDelta(const CrxFile::Header& header) {
  return !strncmp(kCrxDiffFileHeaderMagic, header.magic, sizeof(header.magic));
}

bool CrxFile::HeaderIsValid(const CrxFile::Header& header,
                            CrxFile::Error* error) {
  bool valid = false;
  bool diffCrx = false;
  if (!strncmp(kCrxDiffFileHeaderMagic, header.magic, sizeof(header.magic)))
    diffCrx = true;
  if (strncmp(kCrxFileHeaderMagic, header.magic, sizeof(header.magic)) &&
      !diffCrx)
    *error = kWrongMagic;
  else if (header.version != kCurrentVersion &&
           !(diffCrx && header.version == kCurrentDiffVersion))
    *error = kInvalidVersion;
  else if (header.key_size > kMaxPublicKeySize)
    *error = kInvalidKeyTooLarge;
  else if (header.key_size == 0)
    *error = kInvalidKeyTooSmall;
  else if (header.signature_size > kMaxSignatureSize)
    *error = kInvalidSignatureTooLarge;
  else if (header.signature_size == 0)
    *error = kInvalidSignatureTooSmall;
  else
    valid = true;
  return valid;
}

}  // namespace chrome_cleaner
