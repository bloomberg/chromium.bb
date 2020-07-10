// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "extensions/browser/extension_prefs_scope.h"

namespace base {
class ListValue;
}

namespace content_settings {
class OriginIdentifierValueMap;
class RuleIterator;
}

namespace extensions {

// This class is the backend for extension-defined content settings. It is used
// by the content_settings::CustomExtensionProvider to integrate its settings
// into the HostContentSettingsMap and by the content settings extension API to
// provide extensions with access to content settings.
class ContentSettingsStore
    : public base::RefCountedThreadSafe<ContentSettingsStore> {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when a content setting changes in the
    // ContentSettingsStore.
    virtual void OnContentSettingChanged(
        const std::string& extension_id,
        bool incognito) = 0;
  };

  ContentSettingsStore();

  // //////////////////////////////////////////////////////////////////////////

  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType type,
      const content_settings::ResourceIdentifier& identifier,
      bool incognito) const;

  // Sets the content |setting| for |pattern| of extension |ext_id|. The
  // |incognito| flag allow to set whether the provided setting is for
  // incognito mode only.
  // Precondition: the extension must be registered.
  // This method should only be called on the UI thread.
  void SetExtensionContentSetting(
      const std::string& ext_id,
      const ContentSettingsPattern& embedded_pattern,
      const ContentSettingsPattern& top_level_pattern,
      ContentSettingsType type,
      const content_settings::ResourceIdentifier& identifier,
      ContentSetting setting,
      ExtensionPrefsScope scope);

  // Clears all contents settings set by the extension |ext_id|.
  void ClearContentSettingsForExtension(const std::string& ext_id,
                                        ExtensionPrefsScope scope);

  // Clears all contents settings set by the extension |ext_id| for the
  // content type |content_type|.
  void ClearContentSettingsForExtensionAndContentType(
      const std::string& ext_id,
      ExtensionPrefsScope scope,
      ContentSettingsType content_type);

  // Serializes all content settings set by the extension with ID |extension_id|
  // and returns them as a ListValue. The caller takes ownership of the returned
  // value.
  std::unique_ptr<base::ListValue> GetSettingsForExtension(
      const std::string& extension_id,
      ExtensionPrefsScope scope) const;

  // Deserializes content settings rules from |list| and applies them as set by
  // the extension with ID |extension_id|.
  void SetExtensionContentSettingFromList(const std::string& extension_id,
                                           const base::ListValue* list,
                                           ExtensionPrefsScope scope);

  // //////////////////////////////////////////////////////////////////////////

  // Registers the time when an extension |ext_id| is installed.
  void RegisterExtension(const std::string& ext_id,
                         const base::Time& install_time,
                         bool is_enabled);

  // Deletes all entries related to extension |ext_id|.
  void UnregisterExtension(const std::string& ext_id);

  // Hides or makes the extension content settings of the specified extension
  // visible.
  void SetExtensionState(const std::string& ext_id, bool is_enabled);

  // Adds |observer|. This method should only be called on the UI thread.
  void AddObserver(Observer* observer);

  // Remove |observer|. This method should only be called on the UI thread.
  void RemoveObserver(Observer* observer);

 private:
  friend class base::RefCountedThreadSafe<ContentSettingsStore>;

  struct ExtensionEntry;

  // A list of the entries, maintained in reverse-chronological order (most-
  // recently installed items first) to facilitate search.
  using ExtensionEntries = std::vector<std::unique_ptr<ExtensionEntry>>;

  virtual ~ContentSettingsStore();

  content_settings::OriginIdentifierValueMap* GetValueMap(
      const std::string& ext_id,
      ExtensionPrefsScope scope);

  const content_settings::OriginIdentifierValueMap* GetValueMap(
      const std::string& ext_id,
      ExtensionPrefsScope scope) const;

  void NotifyOfContentSettingChanged(const std::string& extension_id,
                                     bool incognito);

  bool OnCorrectThread();

  ExtensionEntry* FindEntry(const std::string& ext_id) const;
  ExtensionEntries::iterator FindIterator(const std::string& ext_id);

  // The entries.
  ExtensionEntries entries_;

  base::ObserverList<Observer, false>::Unchecked observers_;

  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsStore);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTENT_SETTINGS_CONTENT_SETTINGS_STORE_H_
