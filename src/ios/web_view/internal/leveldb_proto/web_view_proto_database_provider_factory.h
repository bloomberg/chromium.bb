// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_LEVELDB_PROTO_WEB_VIEW_PROTO_DATABASE_PROVIDER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_LEVELDB_PROTO_WEB_VIEW_PROTO_DATABASE_PROVIDER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace ios_web_view {
class WebViewBrowserState;
}  // namespace ios_web_view

namespace leveldb_proto {
class ProtoDatabaseProvider;

// A factory for ProtoDatabaseProvider, a class that provides proto databases
// stored in the appropriate directory given an
// ios_web_view::WebViewBrowserState object.
class WebViewProtoDatabaseProviderFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns singleton instance of WebViewProtoDatabaseProviderFactory.
  static WebViewProtoDatabaseProviderFactory* GetInstance();

  // Returns ProtoDatabaseProvider associated with |context|, so we can
  // instantiate ProtoDatabases that use the appropriate profile directory.
  static ProtoDatabaseProvider* GetForBrowserState(
      ios_web_view::WebViewBrowserState* browser_state);

 protected:
  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

 private:
  friend class base::NoDestructor<WebViewProtoDatabaseProviderFactory>;

  WebViewProtoDatabaseProviderFactory();
  ~WebViewProtoDatabaseProviderFactory() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewProtoDatabaseProviderFactory);
};

}  // namespace leveldb_proto

#endif  // IOS_WEB_VIEW_INTERNAL_LEVELDB_PROTO_WEB_VIEW_PROTO_DATABASE_PROVIDER_FACTORY_H_
