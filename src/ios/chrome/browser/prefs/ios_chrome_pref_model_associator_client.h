// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_IOS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
#define IOS_CHROME_BROWSER_PREFS_IOS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/sync_preferences/pref_model_associator_client.h"

class IOSChromePrefModelAssociatorClient
    : public sync_preferences::PrefModelAssociatorClient {
 public:
  // Returns the global instance.
  static IOSChromePrefModelAssociatorClient* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromePrefModelAssociatorClient>;

  IOSChromePrefModelAssociatorClient();
  ~IOSChromePrefModelAssociatorClient() override;

  // sync_preferences::PrefModelAssociatorClient implementation.
  bool IsMergeableListPreference(const std::string& pref_name) const override;
  bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const override;
  std::unique_ptr<base::Value> MaybeMergePreferenceValues(
      const std::string& pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override;

  DISALLOW_COPY_AND_ASSIGN(IOSChromePrefModelAssociatorClient);
};

#endif  // IOS_CHROME_BROWSER_PREFS_IOS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
