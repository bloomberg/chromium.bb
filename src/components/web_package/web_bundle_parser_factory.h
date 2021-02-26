// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_FACTORY_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_FACTORY_H_

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace web_package {

class WebBundleParserFactory : public mojom::WebBundleParserFactory {
 public:
  WebBundleParserFactory();
  ~WebBundleParserFactory() override;

  std::unique_ptr<mojom::BundleDataSource> CreateFileDataSourceForTesting(
      mojo::PendingReceiver<mojom::BundleDataSource> receiver,
      base::File file);

 private:
  // mojom::WebBundleParserFactory implementation.
  void GetParserForFile(mojo::PendingReceiver<mojom::WebBundleParser> receiver,
                        base::File file) override;
  void GetParserForDataSource(
      mojo::PendingReceiver<mojom::WebBundleParser> receiver,
      mojo::PendingRemote<mojom::BundleDataSource> data_source) override;

  DISALLOW_COPY_AND_ASSIGN(WebBundleParserFactory);
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_FACTORY_H_
