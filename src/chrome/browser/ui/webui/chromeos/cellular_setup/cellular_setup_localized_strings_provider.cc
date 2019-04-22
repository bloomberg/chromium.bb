// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_localized_strings_provider.h"

#include "base/stl_util.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

namespace cellular_setup {

namespace {

// TODO(azeemarshad): Add localized strings for cellular setup flow.
constexpr LocalizedString kLocalizedStringsWithoutPlaceholders[] = {
    {"cancel", IDS_CANCEL},
};

}  //  namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  AddLocalizedStringsBulk(html_source, kLocalizedStringsWithoutPlaceholders,
                          base::size(kLocalizedStringsWithoutPlaceholders));
}

void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder) {
  for (const auto& entry : kLocalizedStringsWithoutPlaceholders)
    builder->Add(entry.name, entry.id);
}

}  // namespace cellular_setup

}  // namespace chromeos
