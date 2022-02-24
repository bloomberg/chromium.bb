// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_USER_EDUCATION_USER_EDUCATION_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_USER_EDUCATION_USER_EDUCATION_INTERNALS_PAGE_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "chrome/browser/ui/webui/internals/user_education/user_education_internals.mojom.h"
#include "content/public/browser/web_ui_data_source.h"

namespace base {
struct Feature;
}  // namespace base

namespace content {
class WebUI;
}  // namespace content

class FeaturePromoSpecification;

class UserEducationInternalsPageHandlerImpl
    : public mojom::user_education_internals::
          UserEducationInternalsPageHandler {
 public:
  explicit UserEducationInternalsPageHandlerImpl(content::WebUI* web_ui,
                                                 Profile* profile);
  ~UserEducationInternalsPageHandlerImpl() override;

  UserEducationInternalsPageHandlerImpl(
      const UserEducationInternalsPageHandlerImpl&) = delete;
  UserEducationInternalsPageHandlerImpl& operator=(
      const UserEducationInternalsPageHandlerImpl&) = delete;

  // mojom::user_education_internals::UserEducationInternalsPageHandler:
  void GetTutorials(GetTutorialsCallback callback) override;
  void StartTutorial(const std::string& tutorial_id) override;

  void GetFeaturePromos(GetFeaturePromosCallback callback) override;
  void ShowFeaturePromo(const std::string& title,
                        ShowFeaturePromoCallback callback) override;

 private:
  const std::string GetTitleFromFeaturePromoData(
      const base::Feature* feature,
      const FeaturePromoSpecification& spec);

  raw_ptr<TutorialService> tutorial_service_ = nullptr;
  raw_ptr<content::WebUI> web_ui_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_USER_EDUCATION_USER_EDUCATION_INTERNALS_PAGE_HANDLER_IMPL_H_
