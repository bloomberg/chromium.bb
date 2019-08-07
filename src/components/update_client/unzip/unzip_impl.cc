// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/unzip/unzip_impl.h"

#include "components/services/unzip/public/cpp/unzip.h"
#include "services/service_manager/public/cpp/connector.h"

namespace update_client {

namespace {

class UnzipperImpl : public Unzipper {
 public:
  explicit UnzipperImpl(std::unique_ptr<service_manager::Connector> connector)
      : connector_(std::move(connector)) {}

  void Unzip(const base::FilePath& zip_file,
             const base::FilePath& destination,
             UnzipCompleteCallback callback) override {
    unzip::Unzip(connector_->Clone(), zip_file, destination,
                 std::move(callback));
  }

 private:
  std::unique_ptr<service_manager::Connector> connector_;
};

}  // namespace

UnzipChromiumFactory::UnzipChromiumFactory(
    std::unique_ptr<service_manager::Connector> connector)
    : connector_(std::move(connector)) {}

std::unique_ptr<Unzipper> UnzipChromiumFactory::Create() const {
  return std::make_unique<UnzipperImpl>(connector_->Clone());
}

UnzipChromiumFactory::~UnzipChromiumFactory() = default;

}  // namespace update_client
