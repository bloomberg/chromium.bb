// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace crostini {

class CrostiniSharePath;

class CrostiniSharePathFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniSharePath* GetForProfile(Profile* profile);
  static CrostiniSharePathFactory* GetInstance();

 private:
  friend class base::NoDestructor<CrostiniSharePathFactory>;

  CrostiniSharePathFactory();
  ~CrostiniSharePathFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CrostiniSharePathFactory);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_FACTORY_H_
