// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_URL_LOADER_FACTORY_GETTER_IMPL_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_URL_LOADER_FACTORY_GETTER_IMPL_H_

#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace download {

// Class for retrieving a fixed SharedURLLoaderFactory.
class COMPONENTS_DOWNLOAD_EXPORT DownloadURLLoaderFactoryGetterImpl
    : public DownloadURLLoaderFactoryGetter {
 public:
  explicit DownloadURLLoaderFactoryGetterImpl(
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory);

  // download::DownloadURLLoaderFactoryGetter implementation.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 protected:
  ~DownloadURLLoaderFactoryGetterImpl() override;

 private:
  // Only one of the following two members is ever set. Initially that would be
  // |url_loader_factory_info_|, but after GetURLLoaderFactory is called for the
  // first time instead |url_loader_factory_| will be set. This is safe because
  // GetURLLoaderFactory is always called from the same thread.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory_info_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadURLLoaderFactoryGetterImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_URL_LOADER_FACTORY_GETTER_IMPL_H_
