// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREF_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREF_H_

#include <memory>
#include <string>
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"

namespace base {
class Value;
}

namespace extensions {
namespace api {
namespace settings_private {
struct PrefObject;
}  // namespace settings_private
}  // namespace api

namespace settings_private {

// Base class for generated preference implementation.
// These are the "preferences" that exist in settings_private API only
// to simplify creating Settings UI for something not directly attached to
// user preference.
class GeneratedPref {
 public:
  class Observer {
   public:
    Observer();
    virtual ~Observer();

    // This method is called to notify observer that visible value
    // of the preference has changed.
    virtual void OnGeneratedPrefChanged(const std::string& pref_name) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  virtual ~GeneratedPref();

  // Returns fully populated PrefObject.
  virtual std::unique_ptr<api::settings_private::PrefObject> GetPrefObject()
      const = 0;

  // Updates "preference" value.
  virtual SetPrefResult SetPref(const base::Value* value) = 0;

  // Modify observer list.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  GeneratedPref();

  // Call this when the pref value changes.
  void NotifyObservers(const std::string& pref_name);

 private:
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedPref);
};

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREF_H_
