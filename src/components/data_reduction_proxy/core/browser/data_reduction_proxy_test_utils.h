// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_use_measurement/core/data_use_measurement.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/backoff_entry.h"
#include "net/base/proxy_server.h"
#include "testing/gmock/include/gmock/gmock.h"

class TestingPrefServiceSimple;

namespace data_reduction_proxy {

class DataReductionProxySettings;


// Test version of |DataReductionProxyService|, which permits mocking of various
// methods.
class MockDataReductionProxyService : public DataReductionProxyService {
 public:
  MockDataReductionProxyService(
      data_use_measurement::DataUseMeasurement* data_use_measurement,
      DataReductionProxySettings* settings,
      PrefService* prefs,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~MockDataReductionProxyService() override;

  MOCK_METHOD2(SetProxyPrefs, void(bool enabled, bool at_startup));
  MOCK_METHOD7(UpdateContentLengths,
               void(int64_t data_used,
                    int64_t original_size,
                    bool data_reduction_proxy_enabled,
                    const std::string& mime_type,
                    bool is_user_traffic,
                    data_use_measurement::DataUseUserData::DataUseContentType
                        content_type,
                    int32_t service_hash_code));
  MOCK_METHOD3(UpdateDataUseForHost,
               void(int64_t network_bytes,
                    int64_t original_bytes,
                    const std::string& host));
};

// Test version of |DataReductionProxyService|, which bypasses initialization in
// the constructor in favor of explicitly passing in its owned classes. This
// permits the use of test/mock versions of those classes.
class TestDataReductionProxyService : public DataReductionProxyService {
 public:
  TestDataReductionProxyService(
      data_use_measurement::DataUseMeasurement* data_use_measurement,
      DataReductionProxySettings* settings,
      PrefService* prefs,
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner);
  ~TestDataReductionProxyService() override;

  bool ignore_blocklist() const { return ignore_blocklist_; }

 private:
  // Whether the long term blocklist rules should be ignored.
  bool ignore_blocklist_ = false;
};

// Test version of |DataStore|. Uses an in memory hash map to store data.
class TestDataStore : public data_reduction_proxy::DataStore {
 public:
  TestDataStore();

  ~TestDataStore() override;

  void InitializeOnDBThread() override {}

  DataStore::Status Get(base::StringPiece key, std::string* value) override;

  DataStore::Status Put(const std::map<std::string, std::string>& map) override;

  DataStore::Status Delete(base::StringPiece key) override;

  DataStore::Status RecreateDB() override;

  std::map<std::string, std::string>* map() { return &map_; }

 private:
  std::map<std::string, std::string> map_;
};

// Builds a test version of the Data Reduction Proxy stack for use in tests.
// Takes in various |TestContextOptions| which controls the behavior of the
// underlying objects.
class DataReductionProxyTestContext {
 public:
  // Allows for a fluent builder interface to configure what kind of objects
  // (test vs mock vs real) are used by the |DataReductionProxyTestContext|.
  class Builder {
   public:
    Builder();

    ~Builder();

    // Specifies the use of |MockDataReductionProxyConfig| instead of
    // |TestDataReductionProxyConfig|.
    Builder& WithMockConfig();

    // Specifies the use of |MockDataReductionProxyService| instead of
    // |DataReductionProxyService|.
    Builder& WithMockDataReductionProxyService();

    // Construct, but do not initialize the |DataReductionProxySettings| object.
    Builder& SkipSettingsInitialization();

    // Specifies a settings object to use.
    Builder& WithSettings(std::unique_ptr<DataReductionProxySettings> settings);

    // Creates a |DataReductionProxyTestContext|. Owned by the caller.
    std::unique_ptr<DataReductionProxyTestContext> Build();

   private:
    bool use_mock_config_;
    bool use_mock_service_;
    bool skip_settings_initialization_;
    std::unique_ptr<DataReductionProxySettings> settings_;
    std::unique_ptr<data_use_measurement::DataUseMeasurement>
        data_use_measurement_;
  };

  DataReductionProxyTestContext(const DataReductionProxyTestContext&) = delete;
  DataReductionProxyTestContext& operator=(
      const DataReductionProxyTestContext&) = delete;

  virtual ~DataReductionProxyTestContext();

  // Registers, sets, and gets the preference used to enable the Data Reduction
  // Proxy, respectively.
  void RegisterDataReductionProxyEnabledPref();
  void SetDataReductionProxyEnabled(bool enabled);
  bool IsDataReductionProxyEnabled() const;

  // Waits while executing all tasks on the current SingleThreadTaskRunner.
  void RunUntilIdle();

  // Initializes the |DataReductionProxySettings| object. Can only be called if
  // built with SkipSettingsInitialization.
  void InitSettings();

  // Destroys the |DataReductionProxySettings| object and waits until objects on
  // the DB task runner are destroyed.
  void DestroySettings();

  // Takes ownership of the |DataReductionProxyService| object. Can only be
  // called if built with SkipSettingsInitialization.
  std::unique_ptr<DataReductionProxyService> TakeService();

  // Enable the Data Reduction Proxy, simulating a successful secure proxy
  // check. This can only be called if not built with WithTestConfigurator,
  // |settings_| has been initialized, and |this| was built with a
  // |net::MockClientSocketFactory| specified.
  void EnableDataReductionProxyWithSecureProxyCheckSuccess();


  DataReductionProxyService* data_reduction_proxy_service() const;

  // Returns the underlying |TestDataReductionProxyService|. This can only be
  // called if not built with WithMockDataReductionProxyService.
  TestDataReductionProxyService* test_data_reduction_proxy_service() const;

  // Returns the underlying |MockDataReductionProxyService|. This can only
  // be called if built with WithMockDataReductionProxyService.
  MockDataReductionProxyService* mock_data_reduction_proxy_service() const;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  TestingPrefServiceSimple* pref_service() {
    return simple_pref_service_.get();
  }

  DataReductionProxySettings* settings() const { return settings_.get(); }

  void InitSettingsWithoutCheck();


 private:
  DataReductionProxyTestContext(
      std::unique_ptr<data_use_measurement::DataUseMeasurement>
          data_use_measurement,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<TestingPrefServiceSimple> simple_pref_service,
      std::unique_ptr<DataReductionProxySettings> settings,
      std::unique_ptr<DataReductionProxyService> service,
      unsigned int test_context_flags);

  std::unique_ptr<data_use_measurement::DataUseMeasurement>
      data_use_measurement_;

  unsigned int test_context_flags_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<TestingPrefServiceSimple> simple_pref_service_;

  std::unique_ptr<DataReductionProxySettings> settings_;
  raw_ptr<DataReductionProxyService> data_reduction_proxy_service_;
  std::unique_ptr<DataReductionProxyService> service_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
