// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PPD_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PPD_PROVIDER_FACTORY_H_

#include "base/memory/ref_counted.h"

class Profile;

namespace chromeos {

class PpdProvider;

scoped_refptr<PpdProvider> CreatePpdProvider(Profile* profile);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PPD_PROVIDER_FACTORY_H_
