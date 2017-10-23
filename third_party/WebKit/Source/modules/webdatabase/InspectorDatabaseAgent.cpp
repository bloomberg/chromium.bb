/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webdatabase/InspectorDatabaseAgent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/frame/LocalFrame.h"
#include "core/html/VoidCallback.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseClient.h"
#include "modules/webdatabase/DatabaseTracker.h"
#include "modules/webdatabase/InspectorDatabaseResource.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLResultSet.h"
#include "modules/webdatabase/SQLResultSetRowList.h"
#include "modules/webdatabase/SQLStatementCallback.h"
#include "modules/webdatabase/SQLStatementErrorCallback.h"
#include "modules/webdatabase/SQLTransaction.h"
#include "modules/webdatabase/SQLTransactionCallback.h"
#include "modules/webdatabase/SQLTransactionErrorCallback.h"
#include "modules/webdatabase/sqlite/SQLValue.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"

typedef blink::protocol::Database::Backend::ExecuteSQLCallback
    ExecuteSQLCallback;

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace DatabaseAgentState {
static const char kDatabaseAgentEnabled[] = "databaseAgentEnabled";
};

namespace {

class ExecuteSQLCallbackWrapper : public RefCounted<ExecuteSQLCallbackWrapper> {
 public:
  static scoped_refptr<ExecuteSQLCallbackWrapper> Create(
      std::unique_ptr<ExecuteSQLCallback> callback) {
    return WTF::AdoptRef(new ExecuteSQLCallbackWrapper(std::move(callback)));
  }
  ~ExecuteSQLCallbackWrapper() {}
  ExecuteSQLCallback* Get() { return callback_.get(); }

  void ReportTransactionFailed(SQLError* error) {
    std::unique_ptr<protocol::Database::Error> error_object =
        protocol::Database::Error::create()
            .setMessage(error->message())
            .setCode(error->code())
            .build();
    callback_->sendSuccess(Maybe<protocol::Array<String>>(),
                           Maybe<protocol::Array<protocol::Value>>(),
                           std::move(error_object));
  }

 private:
  explicit ExecuteSQLCallbackWrapper(
      std::unique_ptr<ExecuteSQLCallback> callback)
      : callback_(std::move(callback)) {}
  std::unique_ptr<ExecuteSQLCallback> callback_;
};

class StatementCallback final : public SQLStatementCallback {
 public:
  static StatementCallback* Create(
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback) {
    return new StatementCallback(std::move(request_callback));
  }

  ~StatementCallback() override {}

  virtual void Trace(blink::Visitor* visitor) {
    SQLStatementCallback::Trace(visitor);
  }

  bool handleEvent(SQLTransaction*, SQLResultSet* result_set) override {
    SQLResultSetRowList* row_list = result_set->rows();

    std::unique_ptr<protocol::Array<String>> column_names =
        protocol::Array<String>::create();
    const Vector<String>& columns = row_list->ColumnNames();
    for (size_t i = 0; i < columns.size(); ++i)
      column_names->addItem(columns[i]);

    std::unique_ptr<protocol::Array<protocol::Value>> values =
        protocol::Array<protocol::Value>::create();
    const Vector<SQLValue>& data = row_list->Values();
    for (size_t i = 0; i < data.size(); ++i) {
      const SQLValue& value = row_list->Values()[i];
      switch (value.GetType()) {
        case SQLValue::kStringValue:
          values->addItem(protocol::StringValue::create(value.GetString()));
          break;
        case SQLValue::kNumberValue:
          values->addItem(protocol::FundamentalValue::create(value.Number()));
          break;
        case SQLValue::kNullValue:
          values->addItem(protocol::Value::null());
          break;
      }
    }
    request_callback_->Get()->sendSuccess(std::move(column_names),
                                          std::move(values),
                                          Maybe<protocol::Database::Error>());
    return true;
  }

 private:
  StatementCallback(scoped_refptr<ExecuteSQLCallbackWrapper> request_callback)
      : request_callback_(std::move(request_callback)) {}
  scoped_refptr<ExecuteSQLCallbackWrapper> request_callback_;
};

class StatementErrorCallback final : public SQLStatementErrorCallback {
 public:
  static StatementErrorCallback* Create(
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback) {
    return new StatementErrorCallback(std::move(request_callback));
  }

  ~StatementErrorCallback() override {}

  virtual void Trace(blink::Visitor* visitor) {
    SQLStatementErrorCallback::Trace(visitor);
  }

  bool handleEvent(SQLTransaction*, SQLError* error) override {
    request_callback_->ReportTransactionFailed(error);
    return true;
  }

 private:
  StatementErrorCallback(
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback)
      : request_callback_(std::move(request_callback)) {}
  scoped_refptr<ExecuteSQLCallbackWrapper> request_callback_;
};

class TransactionCallback final : public SQLTransactionCallback {
 public:
  static TransactionCallback* Create(
      const String& sql_statement,
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback) {
    return new TransactionCallback(sql_statement, std::move(request_callback));
  }

  ~TransactionCallback() override {}

  virtual void Trace(blink::Visitor* visitor) {
    SQLTransactionCallback::Trace(visitor);
  }

  bool handleEvent(SQLTransaction* transaction) override {
    Vector<SQLValue> sql_values;
    SQLStatementCallback* callback =
        StatementCallback::Create(request_callback_);
    SQLStatementErrorCallback* error_callback =
        StatementErrorCallback::Create(request_callback_);
    transaction->ExecuteSQL(sql_statement_, sql_values, callback,
                            error_callback, IGNORE_EXCEPTION_FOR_TESTING);
    return true;
  }

 private:
  TransactionCallback(const String& sql_statement,
                      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback)
      : sql_statement_(sql_statement),
        request_callback_(std::move(request_callback)) {}
  String sql_statement_;
  scoped_refptr<ExecuteSQLCallbackWrapper> request_callback_;
};

class TransactionErrorCallback final : public SQLTransactionErrorCallback {
 public:
  static TransactionErrorCallback* Create(
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback) {
    return new TransactionErrorCallback(std::move(request_callback));
  }

  ~TransactionErrorCallback() override {}

  virtual void Trace(blink::Visitor* visitor) {
    SQLTransactionErrorCallback::Trace(visitor);
  }

  bool handleEvent(SQLError* error) override {
    request_callback_->ReportTransactionFailed(error);
    return true;
  }

 private:
  TransactionErrorCallback(
      scoped_refptr<ExecuteSQLCallbackWrapper> request_callback)
      : request_callback_(std::move(request_callback)) {}
  scoped_refptr<ExecuteSQLCallbackWrapper> request_callback_;
};

class TransactionSuccessCallback final : public VoidCallback {
 public:
  static TransactionSuccessCallback* Create() {
    return new TransactionSuccessCallback();
  }

  ~TransactionSuccessCallback() override {}

  void handleEvent() override {}

 private:
  TransactionSuccessCallback() {}
};

}  // namespace

void InspectorDatabaseAgent::RegisterDatabaseOnCreation(
    blink::Database* database) {
  DidOpenDatabase(database, database->GetSecurityOrigin()->Host(),
                  database->StringIdentifier(), database->version());
}

void InspectorDatabaseAgent::DidOpenDatabase(blink::Database* database,
                                             const String& domain,
                                             const String& name,
                                             const String& version) {
  if (InspectorDatabaseResource* resource =
          FindByFileName(database->FileName())) {
    resource->SetDatabase(database);
    return;
  }

  InspectorDatabaseResource* resource =
      InspectorDatabaseResource::Create(database, domain, name, version);
  resources_.Set(resource->Id(), resource);
  // Resources are only bound while visible.
  DCHECK(enabled_);
  DCHECK(GetFrontend());
  resource->Bind(GetFrontend());
}

void InspectorDatabaseAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  // FIXME(dgozman): adapt this for out-of-process iframes.
  if (frame != page_->MainFrame())
    return;

  resources_.clear();
}

InspectorDatabaseAgent::InspectorDatabaseAgent(Page* page)
    : page_(page), enabled_(false) {}

InspectorDatabaseAgent::~InspectorDatabaseAgent() {}

Response InspectorDatabaseAgent::enable() {
  if (enabled_)
    return Response::OK();
  enabled_ = true;
  state_->setBoolean(DatabaseAgentState::kDatabaseAgentEnabled, enabled_);
  if (DatabaseClient* client = DatabaseClient::FromPage(page_))
    client->SetInspectorAgent(this);
  DatabaseTracker::Tracker().ForEachOpenDatabaseInPage(
      page_, WTF::Bind(&InspectorDatabaseAgent::RegisterDatabaseOnCreation,
                       WrapPersistent(this)));
  return Response::OK();
}

Response InspectorDatabaseAgent::disable() {
  if (!enabled_)
    return Response::OK();
  enabled_ = false;
  state_->setBoolean(DatabaseAgentState::kDatabaseAgentEnabled, enabled_);
  if (DatabaseClient* client = DatabaseClient::FromPage(page_))
    client->SetInspectorAgent(nullptr);
  resources_.clear();
  return Response::OK();
}

void InspectorDatabaseAgent::Restore() {
  if (state_->booleanProperty(DatabaseAgentState::kDatabaseAgentEnabled,
                              false)) {
    enable();
  }
}

Response InspectorDatabaseAgent::getDatabaseTableNames(
    const String& database_id,
    std::unique_ptr<protocol::Array<String>>* names) {
  if (!enabled_)
    return Response::Error("Database agent is not enabled");

  *names = protocol::Array<String>::create();

  blink::Database* database = DatabaseForId(database_id);
  if (database) {
    Vector<String> table_names = database->TableNames();
    unsigned length = table_names.size();
    for (unsigned i = 0; i < length; ++i)
      (*names)->addItem(table_names[i]);
  }
  return Response::OK();
}

void InspectorDatabaseAgent::executeSQL(
    const String& database_id,
    const String& query,
    std::unique_ptr<ExecuteSQLCallback> prp_request_callback) {
  std::unique_ptr<ExecuteSQLCallback> request_callback =
      std::move(prp_request_callback);

  if (!enabled_) {
    request_callback->sendFailure(
        Response::Error("Database agent is not enabled"));
    return;
  }

  blink::Database* database = DatabaseForId(database_id);
  if (!database) {
    request_callback->sendFailure(Response::Error("Database not found"));
    return;
  }

  scoped_refptr<ExecuteSQLCallbackWrapper> wrapper =
      ExecuteSQLCallbackWrapper::Create(std::move(request_callback));
  SQLTransactionCallback* callback =
      TransactionCallback::Create(query, wrapper);
  SQLTransactionErrorCallback* error_callback =
      TransactionErrorCallback::Create(wrapper);
  VoidCallback* success_callback = TransactionSuccessCallback::Create();
  database->transaction(callback, error_callback, success_callback);
}

InspectorDatabaseResource* InspectorDatabaseAgent::FindByFileName(
    const String& file_name) {
  for (DatabaseResourcesHeapMap::iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    if (it->value->GetDatabase()->FileName() == file_name)
      return it->value.Get();
  }
  return nullptr;
}

blink::Database* InspectorDatabaseAgent::DatabaseForId(
    const String& database_id) {
  DatabaseResourcesHeapMap::iterator it = resources_.find(database_id);
  if (it == resources_.end())
    return nullptr;
  return it->value->GetDatabase();
}

void InspectorDatabaseAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  visitor->Trace(resources_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
