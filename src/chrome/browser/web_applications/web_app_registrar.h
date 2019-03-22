// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/abstract_web_app_database.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class WebApp;

class WebAppRegistrar {
 public:
  explicit WebAppRegistrar(AbstractWebAppDatabase* database);
  ~WebAppRegistrar();

  void RegisterApp(std::unique_ptr<WebApp> web_app);
  std::unique_ptr<WebApp> UnregisterApp(const AppId& app_id);

  WebApp* GetAppById(const AppId& app_id);

  const Registry& registry() const { return registry_; }

  bool is_empty() const { return registry_.empty(); }

  // Clears registry.
  void UnregisterAll();

  void Init(base::OnceClosure closure);

 private:
  void OnDatabaseOpened(base::OnceClosure closure, Registry registry);

  Registry registry_;

  // An abstract database, not owned by this registrar.
  AbstractWebAppDatabase* database_;

  base::WeakPtrFactory<WebAppRegistrar> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppRegistrar);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
