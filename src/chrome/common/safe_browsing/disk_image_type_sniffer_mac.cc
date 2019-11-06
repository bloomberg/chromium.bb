// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/disk_image_type_sniffer_mac.h"
#include "base/threading/scoped_blocking_call.h"

namespace safe_browsing {

namespace {

const uint8_t kKolySignature[4] = {'k', 'o', 'l', 'y'};
constexpr size_t kSizeKolySignatureInBytes = sizeof(kKolySignature);
const size_t kSizeKolyTrailerInBytes = 512;

}  // namespace

DiskImageTypeSnifferMac::DiskImageTypeSnifferMac() {}

// static
bool DiskImageTypeSnifferMac::IsAppleDiskImage(const base::FilePath& dmg_file) {
  // TODO(drubery): Macs accept DMGs with koly blocks at the beginning of the
  // file. Investigate if this is a problem, and if so, update this function.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::File file(dmg_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return false;

  char data[kSizeKolySignatureInBytes];

  if (file.Seek(base::File::FROM_END, -1 * kSizeKolyTrailerInBytes) == -1)
    return false;

  if (file.ReadAtCurrentPos(data, kSizeKolySignatureInBytes) !=
      kSizeKolySignatureInBytes)
    return false;

  return IsAppleDiskImageTrailer(base::span<uint8_t>(
      reinterpret_cast<uint8_t*>(data), kSizeKolySignatureInBytes));
}

// static
bool DiskImageTypeSnifferMac::IsAppleDiskImageTrailer(
    const base::span<const uint8_t>& trailer) {
  if (trailer.size() < kSizeKolySignatureInBytes)
    return false;

  const base::span<const uint8_t> subspan =
      trailer.last(kSizeKolySignatureInBytes);

  return (memcmp(subspan.data(), kKolySignature, kSizeKolySignatureInBytes) ==
          0);
}

// static
size_t DiskImageTypeSnifferMac::AppleDiskImageTrailerSize() {
  return kSizeKolyTrailerInBytes;
}

DiskImageTypeSnifferMac::~DiskImageTypeSnifferMac() = default;

}  // namespace safe_browsing
