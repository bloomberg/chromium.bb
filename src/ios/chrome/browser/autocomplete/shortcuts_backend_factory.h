// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_BACKEND_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_BACKEND_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class ShortcutsBackend;

namespace ios {
// Singleton that owns all ShortcutsBackends and associates them with
// ChromeBrowserState.
class ShortcutsBackendFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static scoped_refptr<ShortcutsBackend> GetForBrowserState(
      ChromeBrowserState* browser_state);
  static scoped_refptr<ShortcutsBackend> GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);
  static ShortcutsBackendFactory* GetInstance();

 private:
  friend class base::NoDestructor<ShortcutsBackendFactory>;

  ShortcutsBackendFactory();
  ~ShortcutsBackendFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(ShortcutsBackendFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_SHORTCUTS_BACKEND_FACTORY_H_
