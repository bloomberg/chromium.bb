// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/toolbar/test_toolbar_model.h"

#if defined(TOOLKIT_VIEWS)
#include "components/toolbar/vector_icons.h"  // nogncheck
#endif

TestToolbarModel::TestToolbarModel()
    : security_level_(security_state::NONE),
#if defined(TOOLKIT_VIEWS)
      icon_(&toolbar::kHttpIcon),
#endif
      should_display_url_(true) {
}

TestToolbarModel::~TestToolbarModel() {}

base::string16 TestToolbarModel::GetFormattedFullURL() const {
  return formatted_full_url_;
}

base::string16 TestToolbarModel::GetURLForDisplay() const {
  return url_for_display_;
}

GURL TestToolbarModel::GetURL() const {
  return url_;
}

security_state::SecurityLevel TestToolbarModel::GetSecurityLevel(
    bool ignore_editing) const {
  return security_level_;
}

const gfx::VectorIcon& TestToolbarModel::GetVectorIcon() const {
  return *icon_;
}

base::string16 TestToolbarModel::GetSecureVerboseText() const {
  return base::string16();
}

base::string16 TestToolbarModel::GetSecureAccessibilityText() const {
  return base::string16();
}

base::string16 TestToolbarModel::GetEVCertName() const {
  return (security_level_ == security_state::EV_SECURE) ? ev_cert_name_
                                                        : base::string16();
}

bool TestToolbarModel::ShouldDisplayURL() const {
  return should_display_url_;
}

bool TestToolbarModel::IsOfflinePage() const {
  return offline_page_;
}
