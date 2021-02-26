// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_GZIPPER_H_
#define SERVICES_DATA_DECODER_GZIPPER_H_

#include "services/data_decoder/public/mojom/gzipper.mojom.h"

namespace data_decoder {

class Gzipper : public mojom::Gzipper {
 public:
  Gzipper();
  ~Gzipper() override;
  Gzipper(const Gzipper&) = delete;
  Gzipper operator=(const Gzipper&) = delete;

 public:
  // mojom::Gzipper:
  void Compress(mojo_base::BigBuffer data, CompressCallback callback) override;
  void Uncompress(mojo_base::BigBuffer compressed_data,
                  UncompressCallback callback) override;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_GZIPPER_H_
