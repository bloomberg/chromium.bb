// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PROCESS_MAP_H_
#define EXTENSIONS_BROWSER_PROCESS_MAP_H_

#include <stddef.h>

#include <set>
#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/features/feature.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;

// Contains information about which extensions are assigned to which processes.
//
// The relationship between extensions and processes is complex:
//
// - Extensions can be either "split" mode or "spanning" mode.
// - In spanning mode, extensions share a single process between all incognito
//   and normal windows. This was the original mode for extensions.
// - In split mode, extensions have separate processes in incognito windows.
// - There are also hosted apps, which are a kind of extensions, and those
//   usually have a process model similar to normal web sites: multiple
//   processes per-profile.
// - A single hosted app can have more than one SiteInstance in the same process
//   if we're over the process limit and force them to share a process.
//
// In general, we seem to play with the process model of extensions a lot, so
// it is safest to assume it is many-to-many in most places in the codebase.
//
// Note that because of content scripts, frames, and other edge cases in
// Chrome's process isolation, extension code can still end up running outside
// an assigned process.
//
// But we only allow high-privilege operations to be performed by an extension
// when it is running in an assigned process.
//
// ===========================================================================
// WARNINGS - PLEASE UNDERSTAND THESE BEFORE CALLING OR MODIFYING THIS CLASS
// ===========================================================================
//
// 1. This class contains the processes for hosted apps as well as extensions
//    and packaged apps. Just because a process is present here *does not* mean
//    it is an "extension process" (e.g., for UI purposes). It may contain only
//    hosted apps. See crbug.com/102533.
//
// 2. An extension can show up in multiple processes. That is why there is no
//    GetExtensionProcess() method here. There are two cases: a) The extension
//    is actually a hosted app, in which case this is normal, or b) there is an
//    incognito window open and the extension is "split mode". It is *not safe*
//    to assume that there is one process per extension. If you only care about
//    extensions (not hosted apps), and you are on the UI thread, and you don't
//    care about incognito version of this extension (or vice versa if you're in
//    an incognito profile) then use
//    extensions::ProcessManager::GetSiteInstanceForURL()->[Has|Get]Process().
//
// 3. The process ids contained in this class are *not limited* to the Profile
//    you got this map from. They can also be associated with that profile's
//    incognito/normal twin. If you care about this, use
//    RenderProcessHost::FromID() and check the profile of the resulting object.
//
// TODO(aa): The above warnings suggest this class could use improvement :).
//
// TODO(kalman): This class is not threadsafe, but is used on both the UI and
//               IO threads. Somebody should fix that, either make it
//               threadsafe or enforce single thread. Investigation required.
class ProcessMap : public KeyedService {
 public:
  ProcessMap();
  ~ProcessMap() override;

  // Returns the instance for |browser_context|. An instance is shared between
  // an incognito and a regular context.
  static ProcessMap* Get(content::BrowserContext* browser_context);

  size_t size() const { return items_.size(); }

  bool Insert(const std::string& extension_id, int process_id,
              int site_instance_id);

  bool Remove(const std::string& extension_id, int process_id,
              int site_instance_id);
  int RemoveAllFromProcess(int process_id);

  bool Contains(const std::string& extension_id, int process_id) const;
  bool Contains(int process_id) const;

  std::set<std::string> GetExtensionsInProcess(int process_id) const;

  // Gets the most likely context type for the process with ID |process_id|
  // which hosts Extension |extension|, if any (may be NULL). Context types are
  // renderer (JavaScript) concepts but the browser can do a decent job in
  // guessing what the process hosts.
  //
  // |extension| is the funky part - unfortunately we need to trust the
  // caller of this method to be correct that indeed the context does feature
  // an extension. This matters for iframes, where an extension could be
  // hosted in another extension's process (privilege level needs to be
  // downgraded) or in a web page's process (privilege level needs to be
  // upgraded).
  //
  // The latter of these is slightly problematic from a security perspective;
  // if a web page renderer gets owned it could try to pretend it's an
  // extension and get access to some unprivileged APIs. Luckly, when OOP
  // iframes lauch, it won't be an issue.
  //
  // Anyhow, the expected behaviour is:
  //   - For hosted app processes, this will be blessed_web_page.
  //   - For processes of platform apps running on lock screen, this will be
  //     lock_screen_extension.
  //   - For other extension processes, this will be blessed_extension.
  //   - For WebUI processes, this will be a webui.
  //   - For any other extension we have the choice of unblessed_extension or
  //     content_script. Since content scripts are more common, guess that.
  //     We *could* in theory track which web processes have extension frames
  //     in them, and those would be unblessed_extension, but we don't at the
  //     moment, and once OOP iframes exist then there won't even be such a
  //     thing as an unblessed_extension context.
  //   - For anything else, web_page.
  Feature::Context GetMostLikelyContextType(const Extension* extension,
                                            int process_id) const;

  void set_is_lock_screen_context(bool is_lock_screen_context) {
    is_lock_screen_context_ = is_lock_screen_context;
  }

 private:
  struct Item;

  typedef std::set<Item> ItemSet;
  ItemSet items_;

  // Whether the process map belongs to the browser context used on Chrome OS
  // lock screen.
  bool is_lock_screen_context_ = false;

  DISALLOW_COPY_AND_ASSIGN(ProcessMap);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PROCESS_MAP_H_
