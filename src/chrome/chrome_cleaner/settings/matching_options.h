// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SETTINGS_MATCHING_OPTIONS_H_
#define CHROME_CHROME_CLEANER_SETTINGS_MATCHING_OPTIONS_H_

namespace chrome_cleaner {

class MatchingOptions {
 public:
  bool only_one_footprint() const;
  void set_only_one_footprint(bool only_one_footprint);

  bool find_incomplete_matches() const;
  void set_find_incomplete_matches(bool find_incomplete_matches);

 private:
  bool only_one_footprint_ = false;
  bool find_incomplete_matches_ = false;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SETTINGS_MATCHING_OPTIONS_H_
