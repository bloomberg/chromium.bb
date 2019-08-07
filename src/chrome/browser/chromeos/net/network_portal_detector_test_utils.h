// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace base {
class HistogramSamples;
}

namespace chromeos {

// Checks enum values in a histogram.
class EnumHistogramChecker {
 public:
  EnumHistogramChecker(const std::string& histogram, int count,
                       base::HistogramSamples* base);
  ~EnumHistogramChecker();

  // Sets expectation for a given enum key. |key| must be between 0
  // and expect_.size().
  EnumHistogramChecker* Expect(int key, int value);

  // Actually accesses histogram and checks values for all keys.
  bool Check();

 private:
  // Name of a histogram.
  std::string histogram_;

  // List of expectations.
  std::vector<int> expect_;

  // When not NULL, expected values are compared with actual values
  // minus base.
  base::HistogramSamples* base_;

  DISALLOW_COPY_AND_ASSIGN(EnumHistogramChecker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_DETECTOR_TEST_UTILS_H_
