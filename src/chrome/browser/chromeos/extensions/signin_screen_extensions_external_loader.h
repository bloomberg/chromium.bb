// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SIGNIN_SCREEN_EXTENSIONS_EXTERNAL_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SIGNIN_SCREEN_EXTENSIONS_EXTERNAL_LOADER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/extensions/external_cache_delegate.h"
#include "chrome/browser/chromeos/extensions/external_cache_impl.h"
#include "chrome/browser/extensions/external_loader.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace chromeos {

// Loader of extensions force-installed into the sign-in profile using the
// DeviceLoginScreenExtensions policy.
//
// Overview of the initialization flow:
//   StartLoading()
//   => SubscribeAndInitializeFromPrefs()
//   => UpdateStateFromPrefs()
//   => OnExtensionListsUpdated()
//   => {LoadFinished()|OnUpdated()}.
class SigninScreenExtensionsExternalLoader : public extensions::ExternalLoader,
                                             public ExternalCacheDelegate {
 public:
  explicit SigninScreenExtensionsExternalLoader(Profile* profile);
  SigninScreenExtensionsExternalLoader(
      const SigninScreenExtensionsExternalLoader&) = delete;
  SigninScreenExtensionsExternalLoader& operator=(
      const SigninScreenExtensionsExternalLoader&) = delete;

  // extensions::ExternalLoader:
  void StartLoading() override;

  // ExternalCacheDelegate:
  void OnExtensionListsUpdated(const base::DictionaryValue* prefs) override;
  void OnExtensionLoadedInCache(const std::string& id) override;
  void OnExtensionDownloadFailed(const std::string& id) override;
  std::string GetInstalledExtensionVersion(const std::string& id) override;

 private:
  friend class base::RefCounted<SigninScreenExtensionsExternalLoader>;

  ~SigninScreenExtensionsExternalLoader() override;

  // Called when the pref service gets initialized asynchronously.
  void OnPrefsInitialized(bool success);
  // Starts loading the force-installed extensions specified via prefs and
  // observing the dynamic changes of the prefs.
  void SubscribeAndInitializeFromPrefs();
  // Starts loading the force-installed extensions specified via prefs.
  void UpdateStateFromPrefs();

  Profile* const profile_;
  ExternalCacheImpl external_cache_;
  PrefChangeRegistrar pref_change_registrar_;
  // Whether the list of extensions was already passed via LoadFinished().
  bool initial_load_finished_ = false;

  // Must be the last member.
  base::WeakPtrFactory<SigninScreenExtensionsExternalLoader> weak_factory_{
      this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SIGNIN_SCREEN_EXTENSIONS_EXTERNAL_LOADER_H_
