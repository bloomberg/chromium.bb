// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/font_handler.h"

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/web_ui.h"

#if defined(OS_MAC)
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#endif

namespace settings {

FontHandler::FontHandler(Profile* profile) {
#if defined(OS_MAC)
  // Perform validation for saved fonts.
  settings_utils::ValidateSavedFonts(profile->GetPrefs());
#endif
}

FontHandler::~FontHandler() {}

void FontHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "fetchFontsData", base::BindRepeating(&FontHandler::HandleFetchFontsData,
                                            base::Unretained(this)));
}

void FontHandler::OnJavascriptAllowed() {}

void FontHandler::OnJavascriptDisallowed() {}

void FontHandler::HandleFetchFontsData(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();

  AllowJavascript();
  content::GetFontListAsync(base::BindOnce(&FontHandler::FontListHasLoaded,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           callback_id));
}

void FontHandler::FontListHasLoaded(std::string callback_id,
                                    std::unique_ptr<base::ListValue> list) {
  base::Value::ListView list_view = list->GetList();
  // Font list. Selects the directionality for the fonts in the given list.
  for (auto& i : list_view) {
    DCHECK(i.is_list());
    base::Value::ConstListView font = i.GetList();

    DCHECK(font.size() >= 2u && font[1].is_string());
    std::u16string value = base::UTF8ToUTF16(font[1].GetString());

    bool has_rtl_chars = base::i18n::StringContainsStrongRTLChars(value);
    i.Append(has_rtl_chars ? "rtl" : "ltr");
  }

  base::DictionaryValue response;
  response.SetKey("fontList", base::Value::FromUniquePtrValue(std::move(list)));

  ResolveJavascriptCallback(base::Value(callback_id), response);
}

}  // namespace settings
