// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "weblayer/public/browser.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace base {
class FilePath;
}

namespace content {
class WebContents;
struct WebPreferences;
}

namespace weblayer {

class BrowserPersister;
class ProfileImpl;
class TabImpl;

class BrowserImpl : public Browser {
 public:
  BrowserImpl(const BrowserImpl&) = delete;
  BrowserImpl& operator=(const BrowserImpl&) = delete;
  ~BrowserImpl() override;

  BrowserPersister* browser_persister() { return browser_persister_.get(); }

  ProfileImpl* profile() { return profile_; }

  // Creates and adds a Tab from session restore. The returned tab is owned by
  // this Browser.
  TabImpl* CreateTabForSessionRestore(
      std::unique_ptr<content::WebContents> web_contents,
      const std::string& guid);

#if defined(OS_ANDROID)
  bool CompositorHasSurface();

  void AddTab(JNIEnv* env,
              long native_tab);
  void RemoveTab(JNIEnv* env,
                 long native_tab);
  base::android::ScopedJavaLocalRef<jobjectArray> GetTabs(JNIEnv* env);
  void SetActiveTab(JNIEnv* env,
                    long native_tab);
  base::android::ScopedJavaLocalRef<jobject> GetActiveTab(JNIEnv* env);
  void PrepareForShutdown(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetPersistenceId(JNIEnv* env);
  void SaveBrowserPersisterIfNecessary(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jbyteArray> GetBrowserPersisterCryptoKey(
      JNIEnv* env);
  base::android::ScopedJavaLocalRef<jbyteArray> GetMinimalPersistenceState(
      JNIEnv* env);
  void RestoreStateIfNecessary(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_persistence_id,
      const base::android::JavaParamRef<jbyteArray>& j_persistence_crypto_key,
      const base::android::JavaParamRef<jbyteArray>&
          j_minimal_persistence_state);
  void WebPreferencesChanged(JNIEnv* env);
  void OnFragmentStart(JNIEnv* env);
#endif

  // Used in tests to specify a non-default max (0 means use the default).
  std::vector<uint8_t> GetMinimalPersistenceState(int max_size_in_bytes);

  // Used by tests to specify a callback to listen to changes to visible
  // security state.
  void set_visible_security_state_callback_for_tests(
      base::OnceClosure closure) {
    visible_security_state_changed_callback_for_tests_ = std::move(closure);
  }

  bool GetPasswordEchoEnabled();
  void SetWebPreferences(content::WebPreferences* prefs);

  // Browser:
  Tab* AddTab(std::unique_ptr<Tab> tab) override;
  std::unique_ptr<Tab> RemoveTab(Tab* tab) override;
  void SetActiveTab(Tab* tab) override;
  Tab* GetActiveTab() override;
  std::vector<Tab*> GetTabs() override;
  void PrepareForShutdown() override;
  std::string GetPersistenceId() override;
  std::vector<uint8_t> GetMinimalPersistenceState() override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;
  void VisibleSecurityStateOfActiveTabChanged() override;

 private:
  // For creation.
  friend class Browser;

#if defined(OS_ANDROID)
  friend BrowserImpl* CreateBrowserForAndroid(
      ProfileImpl*,
      const base::android::JavaParamRef<jobject>&);
#endif

  explicit BrowserImpl(ProfileImpl* profile);

  void RestoreStateIfNecessary(const PersistenceInfo& persistence_info);

  // Returns the path used by |browser_persister_|.
  base::FilePath GetBrowserPersisterDataPath();

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
#endif
  base::ObserverList<BrowserObserver> browser_observers_;
  ProfileImpl* const profile_;
  std::vector<std::unique_ptr<Tab>> tabs_;
  TabImpl* active_tab_ = nullptr;
  std::string persistence_id_;
  std::unique_ptr<BrowserPersister> browser_persister_;
  base::OnceClosure visible_security_state_changed_callback_for_tests_;
  static int browser_count_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_IMPL_H_
