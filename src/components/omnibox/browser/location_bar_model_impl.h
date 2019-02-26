// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_IMPL_H_
#define COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_IMPL_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

class LocationBarModelDelegate;

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class LocationBarModelImpl : public LocationBarModel {
 public:
  LocationBarModelImpl(LocationBarModelDelegate* delegate,
                       size_t max_url_display_chars);
  ~LocationBarModelImpl() override;

  // LocationBarModel:
  base::string16 GetFormattedFullURL() const override;
  base::string16 GetURLForDisplay() const override;
  GURL GetURL() const override;
  security_state::SecurityLevel GetSecurityLevel(
      bool ignore_editing) const override;
  bool IsSecurityInfoInitialized() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetSecureDisplayText() const override;
  base::string16 GetSecureAccessibilityText() const override;
  base::string16 GetEVCertName() const override;
  bool ShouldDisplayURL() const override;
  bool IsOfflinePage() const override;

 private:
  struct SecureChipText {
    base::string16 display_text_;
    base::string16 accessibility_label_;
    SecureChipText(base::string16 display_text,
                   base::string16 accessibility_label)
        : display_text_(display_text),
          accessibility_label_(accessibility_label) {}
    SecureChipText(base::string16 display_text)
        : display_text_(display_text), accessibility_label_(display_text) {}
  };

  // Get the security chip labels for the current security state.
  SecureChipText GetSecureChipText() const;

  base::string16 GetFormattedURL(
      url_formatter::FormatUrlTypes format_types) const;

  LocationBarModelDelegate* delegate_;
  const size_t max_url_display_chars_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocationBarModelImpl);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCATION_BAR_MODEL_IMPL_H_
