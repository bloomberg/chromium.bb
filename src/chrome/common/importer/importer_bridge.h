// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_IMPORTER_BRIDGE_H_
#define CHROME_COMMON_IMPORTER_IMPORTER_BRIDGE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/importer_url_row.h"
#include "components/favicon_base/favicon_usage_data.h"

class GURL;
struct ImportedBookmarkEntry;
struct ImporterAutofillFormDataEntry;

namespace autofill {
struct PasswordForm;
}

namespace importer {
struct SearchEngineInfo;
}

class ImporterBridge : public base::RefCountedThreadSafe<ImporterBridge> {
 public:
  ImporterBridge();

  virtual void AddBookmarks(
      const std::vector<ImportedBookmarkEntry>& bookmarks,
      const base::string16& first_folder_name) = 0;

  virtual void AddHomePage(const GURL& home_page) = 0;

  virtual void SetFavicons(
      const favicon_base::FaviconUsageDataList& favicons) = 0;

  virtual void SetHistoryItems(const std::vector<ImporterURLRow>& rows,
                               importer::VisitSource visit_source) = 0;

  virtual void SetKeywords(
      const std::vector<importer::SearchEngineInfo>& search_engines,
      bool unique_on_host_and_path) = 0;

  // The search_engine_data vector contains XML data retrieved from the Firefox
  // profile and its sqlite db.
  virtual void SetFirefoxSearchEnginesXMLData(
      const std::vector<std::string>& search_engine_data) = 0;

  virtual void SetPasswordForm(const autofill::PasswordForm& form) = 0;

  virtual void SetAutofillFormData(
      const std::vector<ImporterAutofillFormDataEntry>& entries) = 0;

  // Notifies the coordinator that the import operation has begun.
  virtual void NotifyStarted() = 0;

  // Notifies the coordinator that the collection of data for the specified
  // item has begun.
  virtual void NotifyItemStarted(importer::ImportItem item) = 0;

  // Notifies the coordinator that the collection of data for the specified
  // item has completed.
  virtual void NotifyItemEnded(importer::ImportItem item) = 0;

  // Notifies the coordinator that the entire import operation has completed.
  virtual void NotifyEnded() = 0;

  // For InProcessImporters this calls l10n_util. For ExternalProcessImporters
  // this calls the set of strings we've ported over to the external process.
  // It's good to avoid having to create a separate ResourceBundle for the
  // external import process, since the importer only needs a few strings.
  virtual base::string16 GetLocalizedString(int message_id) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ImporterBridge>;

  virtual ~ImporterBridge();

  DISALLOW_COPY_AND_ASSIGN(ImporterBridge);
};

#endif  // CHROME_COMMON_IMPORTER_IMPORTER_BRIDGE_H_
