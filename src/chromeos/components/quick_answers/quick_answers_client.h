// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/components/quick_answers/result_loader.h"
#include "chromeos/components/quick_answers/understanding/intent_generator.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace quick_answers {

struct QuickAnswer;
struct QuickAnswersRequest;
struct IntentInfo;
enum class IntentType;
enum class ResultType;

// A delegate interface for the QuickAnswersClient.
class QuickAnswersDelegate {
 public:
  using AccessTokenCallback =
      base::OnceCallback<void(const std::string& access_token)>;

  QuickAnswersDelegate(const QuickAnswersDelegate&) = delete;
  QuickAnswersDelegate& operator=(const QuickAnswersDelegate&) = delete;

  // Invoked when the |quick_answer| is received. Note that |quick_answer| may
  // be |nullptr| if no answer found for the selected content.
  virtual void OnQuickAnswerReceived(
      std::unique_ptr<QuickAnswer> quick_answer) {}

  // Invoked when the query is rewritten.
  virtual void OnRequestPreprocessFinished(
      const QuickAnswersRequest& processed_request) {}

  // Invoked when there is a network error.
  virtual void OnNetworkError() {}

  // Request for the access token associated with the active user's profile.
  // Request is handled asynchronously if the token is not available.
  // AccessTokenCallbacks are invoked as soon as the token if fetched.
  // If the token is available, AccessTokenCallbacks are invoked
  // synchronously before RequestAccessToken() returns.
  virtual void RequestAccessToken(AccessTokenCallback callback) {}

 protected:
  QuickAnswersDelegate() = default;
  virtual ~QuickAnswersDelegate() = default;
};

// Quick answers client to load and parse quick answer results.
class QuickAnswersClient : public ResultLoader::ResultLoaderDelegate {
 public:
  // Method that can be used in tests to change the result loader returned by
  // |CreateResultLoader| in tests.
  using ResultLoaderFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<ResultLoader>()>;

  // Method that can be used in tests to change the intent generator returned by
  // |CreateResultLoader| in tests.
  using IntentGeneratorFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<IntentGenerator>()>;

  QuickAnswersClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      QuickAnswersDelegate* delegate);

  QuickAnswersClient(const QuickAnswersClient&) = delete;
  QuickAnswersClient& operator=(const QuickAnswersClient&) = delete;

  ~QuickAnswersClient() override;

  // ResultLoaderDelegate:
  void OnNetworkError() override;
  void OnQuickAnswerReceived(
      std::unique_ptr<QuickAnswer> quick_answer) override;
  void RequestAccessToken(AccessTokenCallback callback) override;

  // Send a quick answer request for preprocessing only.
  void SendRequestForPreprocessing(
      const QuickAnswersRequest& quick_answers_request);
  // Fetch quick answers result from the server.
  void FetchQuickAnswers(const QuickAnswersRequest& processed_request);
  // Send a quick answer request. The request is preprocessed before fetching
  // the result from the server. Virtual for testing.
  virtual void SendRequest(const QuickAnswersRequest& quick_answers_request);

  // User clicks on the Quick Answers result. Virtual for testing.
  virtual void OnQuickAnswerClick(ResultType result_type);

  // Quick Answers is dismissed. Virtual for testing.
  virtual void OnQuickAnswersDismissed(ResultType result_type, bool is_active);

  static void SetResultLoaderFactoryForTesting(
      ResultLoaderFactoryCallback* factory);

  static void SetIntentGeneratorFactoryForTesting(
      IntentGeneratorFactoryCallback* factory);

 private:
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest, SendRequest);
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest,
                           NotSendRequestForUnknownIntent);
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest, PreprocessDefinitionIntent);
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest, PreprocessTranslationIntent);
  FRIEND_TEST_ALL_PREFIXES(QuickAnswersClientTest,
                           PreprocessUnitConversionIntent);

  // Creates a |ResultLoader| instance.
  std::unique_ptr<ResultLoader> CreateResultLoader(IntentType intent_type);

  // Creates an |IntentGenerator| instance.
  std::unique_ptr<IntentGenerator> CreateIntentGenerator(
      const QuickAnswersRequest& request,
      bool skip_fetch);

  // Preprocesses the |QuickAnswersRequest| and fetch quick answers result. Only
  // preprocesses the request and skip fetching result if |skip_fetch| is true.
  void SendRequestInternal(const QuickAnswersRequest& quick_answers_request,
                           bool skip_fetch);
  void IntentGeneratorCallback(const QuickAnswersRequest& quick_answers_request,
                               bool skip_fetch,
                               const IntentInfo& intent_info);
  base::TimeDelta GetImpressionDuration() const;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  QuickAnswersDelegate* delegate_ = nullptr;
  std::unique_ptr<ResultLoader> result_loader_;
  std::unique_ptr<IntentGenerator> intent_generator_;
  // Time when the quick answer is received.
  base::TimeTicks quick_answer_received_time_;

  base::WeakPtrFactory<QuickAnswersClient> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_
