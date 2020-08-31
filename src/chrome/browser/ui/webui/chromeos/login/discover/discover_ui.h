// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_UI_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"

namespace base {
class DictionaryValue;
}

namespace content {
class WebUI;
class WebContents;
}

namespace chromeos {

class DiscoverHandler;

class DiscoverUI {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // This is called when Discover UI becomes initialized.
    // (I.e. when JS reports "initialized".)
    virtual void OnInitialized() {}
  };

  DiscoverUI();

  ~DiscoverUI();

  // Pointer to DiscoverUI is attached to WebContents. This method returns
  // stored pointer, or nullptr if no data was found.
  static DiscoverUI* GetDiscoverUI(const content::WebContents* web_contents);

  // Register WebUI handlers
  void RegisterMessages(content::WebUI* web_ui);

  // Returns localized strings and data.
  void GetAdditionalParameters(base::DictionaryValue* dict);

  // This is called when Web UI reports "initialized" state.
  void Initialize();

  void Show();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const content::WebUI* web_ui() const { return web_ui_; }

 private:
  bool initialized_ = false;

  content::WebUI* web_ui_ = nullptr;

  // Non-owninng.
  // Handlers are owned by WebUI, but we need to keep this list to be able to
  // to refresh string resources.
  std::vector<DiscoverHandler*> handlers_;

  base::ObserverList<Observer> observers_;

  JSCallsContainer js_calls_container_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_UI_H_
