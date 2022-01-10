// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_H_

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace web_package {

class WebBundleParser : public mojom::WebBundleParser {
 public:
  WebBundleParser(mojo::PendingReceiver<mojom::WebBundleParser> receiver,
                  mojo::PendingRemote<mojom::BundleDataSource> data_source,
                  const GURL& base_url);

  WebBundleParser(const WebBundleParser&) = delete;
  WebBundleParser& operator=(const WebBundleParser&) = delete;

  ~WebBundleParser() override;

 private:
  class MetadataParser;
  class ResponseParser;

  class SharedBundleDataSource
      : public base::RefCounted<SharedBundleDataSource> {
   public:
    class Observer;

    explicit SharedBundleDataSource(
        mojo::PendingRemote<mojom::BundleDataSource> pending_data_source);

    SharedBundleDataSource(const SharedBundleDataSource&) = delete;
    SharedBundleDataSource& operator=(const SharedBundleDataSource&) = delete;

    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);

    void Read(uint64_t offset,
              uint64_t length,
              mojom::BundleDataSource::ReadCallback callback);

   private:
    friend class base::RefCounted<SharedBundleDataSource>;

    ~SharedBundleDataSource();

    void OnDisconnect();

    mojo::Remote<mojom::BundleDataSource> data_source_;
    base::flat_set<Observer*> observers_;
  };

  // mojom::WebBundleParser implementation.
  void ParseMetadata(ParseMetadataCallback callback) override;
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override;

  mojo::Receiver<mojom::WebBundleParser> receiver_;
  scoped_refptr<SharedBundleDataSource> data_source_;
  const GURL base_url_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_PARSER_H_
