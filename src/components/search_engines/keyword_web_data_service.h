// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_KEYWORD_WEB_DATA_SERVICE_H__
#define COMPONENTS_SEARCH_ENGINES_KEYWORD_WEB_DATA_SERVICE_H__

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/template_url_id.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database.h"

namespace base {
class SingleThreadTaskRunner;
}

class WDTypedResult;
class WebDatabaseService;
struct TemplateURLData;

struct WDKeywordsResult {
  WDKeywordsResult();
  WDKeywordsResult(const WDKeywordsResult& other);
  ~WDKeywordsResult();

  KeywordTable::Keywords keywords;
  // Identifies the ID of the TemplateURL that is the default search. A value of
  // 0 indicates there is no default search provider.
  int64_t default_search_provider_id;
  // Version of the built-in keywords. A value of 0 indicates a first run.
  int builtin_keyword_version;
};

class WebDataServiceConsumer;

class KeywordWebDataService : public WebDataServiceBase {
 public:
  // Instantiate this to turn on batch mode on the provided |service|
  // until the scoper is destroyed.  When batch mode is on, calls to any of the
  // three keyword table modification functions below will result in locally
  // queueing the operation; on setting this back to false, all the
  // modifications will be performed at once.  This is a performance
  // optimization; see comments on KeywordTable::PerformOperations().
  //
  // If multiple scopers are in-scope simultaneously, batch mode will only be
  // exited when all are destroyed.  If |service| is NULL, the object will do
  // nothing.
  class BatchModeScoper {
   public:
    explicit BatchModeScoper(KeywordWebDataService* service);
    ~BatchModeScoper();

   private:
    KeywordWebDataService* service_;

    DISALLOW_COPY_AND_ASSIGN(BatchModeScoper);
  };

  KeywordWebDataService(
      scoped_refptr<WebDatabaseService> wdbs,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      const ProfileErrorCallback& callback);

  // As the database processes requests at a later date, all deletion is done on
  // the background sequence.
  //
  // Many of the keyword related methods do not return a handle. This is because
  // the caller (TemplateURLService) does not need to know when the request is
  // done.

  void AddKeyword(const TemplateURLData& data);
  void RemoveKeyword(TemplateURLID id);
  void UpdateKeyword(const TemplateURLData& data);

  // Fetches the keywords.
  // On success, consumer is notified with WDResult<KeywordTable::Keywords>.
  Handle GetKeywords(WebDataServiceConsumer* consumer);

  // Sets the ID of the default search provider.
  void SetDefaultSearchProviderID(TemplateURLID id);

  // Sets the version of the builtin keywords.
  void SetBuiltinKeywordVersion(int version);

 protected:
  ~KeywordWebDataService() override;

 private:
  // Called by the BatchModeScoper (see comments there).
  void AdjustBatchModeLevel(bool entering_batch_mode);

  //////////////////////////////////////////////////////////////////////////////
  //
  // The following methods are only invoked on the DB sequence.
  //
  //////////////////////////////////////////////////////////////////////////////
  WebDatabase::State PerformKeywordOperationsImpl(
      const KeywordTable::Operations& operations,
      WebDatabase* db);
  std::unique_ptr<WDTypedResult> GetKeywordsImpl(WebDatabase* db);
  WebDatabase::State SetDefaultSearchProviderIDImpl(TemplateURLID id,
                                                    WebDatabase* db);
  WebDatabase::State SetBuiltinKeywordVersionImpl(int version, WebDatabase* db);

  size_t batch_mode_level_;
  KeywordTable::Operations queued_keyword_operations_;

  DISALLOW_COPY_AND_ASSIGN(KeywordWebDataService);
};

#endif  // COMPONENTS_SEARCH_ENGINES_KEYWORD_WEB_DATA_SERVICE_H__
