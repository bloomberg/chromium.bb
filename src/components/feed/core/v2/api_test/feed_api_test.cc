// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/keyvalue_store.pb.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/chrome_client_info.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/there_and_back_again_data.pb.h"
#include "components/feed/core/proto/v2/xsurface.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/core/v2/test/test_util.h"
#include "components/feed/feed_feature_list.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace test {

std::unique_ptr<StreamModel> LoadModelFromStore(const StreamType& stream_type,
                                                FeedStore* store,
                                                StreamModel::Context* context) {
  std::unique_ptr<StreamModelUpdateRequest> data =
      StoredModelData(stream_type, store);
  if (data) {
    auto model = std::make_unique<StreamModel>(context);
    model->Update(std::move(data));
    return model;
  }
  return nullptr;
}

std::unique_ptr<StreamModelUpdateRequest> StoredModelData(
    const StreamType& stream_type,
    FeedStore* store) {
  LoadStreamFromStoreTask::Result result;
  auto complete = [&](LoadStreamFromStoreTask::Result task_result) {
    result = std::move(task_result);
  };
  LoadStreamFromStoreTask load_task(
      LoadStreamFromStoreTask::LoadType::kFullLoad, nullptr, stream_type, store,
      /*missed_last_refresh=*/false, base::BindLambdaForTesting(complete));
  // We want to load the data no matter how stale.
  load_task.IgnoreStalenessForTesting();

  base::RunLoop run_loop;
  load_task.Execute(run_loop.QuitClosure());
  run_loop.Run();

  if (result.status == LoadStreamStatus::kLoadedFromStore) {
    return std::move(result.update_request);
  }
  LOG(WARNING) << "LoadModelFromStore failed with " << result.status;
  return nullptr;
}

std::string ModelStateFor(
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    std::vector<feedstore::DataOperation> operations,
    std::vector<feedstore::DataOperation> more_operations) {
  StreamModel::Context context;
  StreamModel model(&context);
  model.Update(std::move(update_request));
  model.ExecuteOperations(operations);
  model.ExecuteOperations(more_operations);
  return model.DumpStateForTesting();
}

std::string ModelStateFor(const StreamType& stream_type, FeedStore* store) {
  StreamModel::Context context;
  auto model = LoadModelFromStore(stream_type, store, &context);
  if (model) {
    return model->DumpStateForTesting();
  }
  return "{Failed to load model from store}";
}

feedwire::FeedAction MakeFeedAction(int64_t id, size_t pad_size) {
  feedwire::FeedAction action;

  std::string pad;
  if (pad_size > 0) {
    pad = " " + std::string(pad_size - 1, 'a');
  }

  action.mutable_action_payload()->set_action_payload_data(
      base::StrCat({base::NumberToString(id), pad}));
  return action;
}

std::vector<feedstore::StoredAction> ReadStoredActions(FeedStore& store) {
  base::RunLoop run_loop;
  CallbackReceiver<std::vector<feedstore::StoredAction>> cr(&run_loop);
  store.ReadActions(cr.Bind());
  run_loop.Run();
  CHECK(cr.GetResult());
  return std::move(*cr.GetResult());
}

std::string SerializedOfflineBadgeContent() {
  feedxsurface::OfflineBadgeContent testbadge;
  std::string badge_serialized;
  testbadge.set_available_offline(true);
  testbadge.SerializeToString(&badge_serialized);
  return badge_serialized;
}

feedwire::ThereAndBackAgainData MakeThereAndBackAgainData(int64_t id) {
  feedwire::ThereAndBackAgainData msg;
  *msg.mutable_action_payload() = MakeFeedAction(id).action_payload();
  return msg;
}

TestUnreadContentObserver::TestUnreadContentObserver() = default;
TestUnreadContentObserver::~TestUnreadContentObserver() = default;
void TestUnreadContentObserver::HasUnreadContentChanged(
    bool has_unread_content) {
  calls.push_back(has_unread_content);
}

TestSurfaceBase::TestSurfaceBase(const StreamType& stream_type,
                                 FeedStream* stream)
    : FeedStreamSurface(stream_type) {
  if (stream)
    Attach(stream);
}

TestSurfaceBase::~TestSurfaceBase() {
  if (stream_)
    Detach();
}

void TestSurfaceBase::Attach(FeedStream* stream) {
  EXPECT_FALSE(stream_);
  stream_ = stream->GetWeakPtr();
  stream_->AttachSurface(this);
}

void TestSurfaceBase::Detach() {
  EXPECT_TRUE(stream_);
  stream_->DetachSurface(this);
  stream_ = nullptr;
}

void TestSurfaceBase::StreamUpdate(const feedui::StreamUpdate& stream_update) {
  DVLOG(1) << "StreamUpdate: " << stream_update;
  // Some special-case treatment for the loading spinner. We don't count it
  // toward |initial_state|.
  bool is_initial_loading_spinner = IsInitialLoadSpinnerUpdate(stream_update);
  if (!initial_state && !is_initial_loading_spinner) {
    initial_state = stream_update;
  }
  update = stream_update;

  described_updates_.push_back(CurrentState());
}
void TestSurfaceBase::ReplaceDataStoreEntry(base::StringPiece key,
                                            base::StringPiece data) {
  data_store_entries_[std::string(key)] = std::string(data);
}
void TestSurfaceBase::RemoveDataStoreEntry(base::StringPiece key) {
  data_store_entries_.erase(std::string(key));
}
ReliabilityLoggingBridge& TestSurfaceBase::GetReliabilityLoggingBridge() {
  return reliability_logging_bridge;
}

void TestSurfaceBase::Clear() {
  initial_state = absl::nullopt;
  update = absl::nullopt;
  described_updates_.clear();
}

std::string TestSurfaceBase::DescribeUpdates() {
  std::string result = base::JoinString(described_updates_, " -> ");
  described_updates_.clear();
  return result;
}

std::string TestSurfaceBase::DescribeState() {
  return described_updates_.empty() ? "" : described_updates_.back();
}

std::map<std::string, std::string> TestSurfaceBase::GetDataStoreEntries()
    const {
  return data_store_entries_;
}
std::string TestSurfaceBase::CurrentState() {
  if (update && IsInitialLoadSpinnerUpdate(*update))
    return "loading";

  if (!initial_state)
    return "empty";

  bool has_loading_spinner = false;
  for (int i = 0; i < update->updated_slices().size(); ++i) {
    const feedui::StreamUpdate_SliceUpdate& slice_update =
        update->updated_slices(i);
    if (slice_update.has_slice() &&
        slice_update.slice().has_zero_state_slice()) {
      CHECK(update->updated_slices().size() == 1)
          << "Zero state with other slices" << *update;
      // Returns either "no-cards" or "cant-refresh".
      return update->updated_slices()[0].slice().slice_id();
    }
    if (slice_update.has_slice() &&
        slice_update.slice().has_loading_spinner_slice()) {
      CHECK_EQ(i, update->updated_slices().size() - 1)
          << "Loading spinner in an unexpected place" << *update;
      has_loading_spinner = true;
    }
  }
  std::stringstream ss;
  if (has_loading_spinner) {
    ss << update->updated_slices().size() - 1 << " slices +spinner";
  } else {
    ss << update->updated_slices().size() << " slices";
  }
  return ss.str();
}

bool TestSurfaceBase::IsInitialLoadSpinnerUpdate(
    const feedui::StreamUpdate& stream_update) {
  return stream_update.updated_slices().size() == 1 &&
         stream_update.updated_slices()[0].has_slice() &&
         stream_update.updated_slices()[0].slice().has_loading_spinner_slice();
}

TestForYouSurface::TestForYouSurface(FeedStream* stream)
    : TestSurfaceBase(kForYouStream, stream) {}
TestWebFeedSurface::TestWebFeedSurface(FeedStream* stream)
    : TestSurfaceBase(kWebFeedStream, stream) {}

TestReliabilityLoggingBridge::TestReliabilityLoggingBridge() = default;
TestReliabilityLoggingBridge::~TestReliabilityLoggingBridge() = default;

std::string TestReliabilityLoggingBridge::GetEventsString() const {
  std::ostringstream oss;
  for (const auto& event : events_)
    oss << event << '\n';
  return oss.str();
}

void TestReliabilityLoggingBridge::LogFeedLaunchOtherStart(
    base::TimeTicks timestamp) {
  events_.push_back("LogFeedLaunchOtherStart");
}

void TestReliabilityLoggingBridge::LogCacheReadStart(
    base::TimeTicks timestamp) {
  events_.push_back("LogCacheReadStart");
}

void TestReliabilityLoggingBridge::LogCacheReadEnd(
    base::TimeTicks timestamp,
    feedwire::DiscoverCardReadCacheResult result) {
  events_.push_back(
      base::StrCat({"LogCacheReadEnd result=",
                    feedwire::DiscoverCardReadCacheResult_Name(result)}));
}

void TestReliabilityLoggingBridge::LogFeedRequestStart(
    NetworkRequestId id,
    base::TimeTicks timestamp) {
  events_.push_back(base::StrCat(
      {"LogFeedRequestStart id=", base::NumberToString(id.GetUnsafeValue())}));
}

void TestReliabilityLoggingBridge::LogActionsUploadRequestStart(
    NetworkRequestId id,
    base::TimeTicks timestamp) {
  events_.push_back(base::StrCat({"LogActionsUploadRequestStart id=",
                                  base::NumberToString(id.GetUnsafeValue())}));
}

void TestReliabilityLoggingBridge::LogWebFeedRequestStart(
    NetworkRequestId id,
    base::TimeTicks timestamp) {
  events_.push_back(base::StrCat({"LogWebFeedRequestStart id=",
                                  base::NumberToString(id.GetUnsafeValue())}));
}

void TestReliabilityLoggingBridge::LogRequestSent(NetworkRequestId id,
                                                  base::TimeTicks timestamp) {
  events_.push_back(base::StrCat(
      {"LogRequestSent id=", base::NumberToString(id.GetUnsafeValue())}));
}

void TestReliabilityLoggingBridge::LogResponseReceived(
    NetworkRequestId id,
    int64_t server_receive_timestamp_ns,
    int64_t server_send_timestamp_ns,
    base::TimeTicks client_receive_timestamp) {
  events_.push_back(base::StrCat(
      {"LogResponseReceived id=", base::NumberToString(id.GetUnsafeValue())}));
}

void TestReliabilityLoggingBridge::LogRequestFinished(
    NetworkRequestId id,
    base::TimeTicks timestamp,
    int combined_network_status_code) {
  events_.push_back(
      base::StrCat({"LogRequestFinished result=",
                    base::NumberToString(combined_network_status_code),
                    " id=", base::NumberToString(id.GetUnsafeValue())}));
}

void TestReliabilityLoggingBridge::LogLoadingIndicatorShown(
    base::TimeTicks timestamp) {
  events_.push_back("LogLoadingIndicatorShown");
}

void TestReliabilityLoggingBridge::LogAboveTheFoldRender(
    base::TimeTicks timestamp,
    feedwire::DiscoverAboveTheFoldRenderResult result) {
  events_.push_back(
      base::StrCat({"LogAboveTheFoldRender result=",
                    feedwire::DiscoverAboveTheFoldRenderResult_Name(result)}));
}

void TestReliabilityLoggingBridge::LogLaunchFinishedAfterStreamUpdate(
    feedwire::DiscoverLaunchResult result) {
  events_.push_back(
      base::StrCat({"LogLaunchFinishedAfterStreamUpdate result=",
                    feedwire::DiscoverLaunchResult_Name(result)}));
}

TestImageFetcher::TestImageFetcher(
    scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory)
    : ImageFetcher(url_loader_factory) {}

ImageFetchId TestImageFetcher::Fetch(
    const GURL& url,
    base::OnceCallback<void(NetworkResponse)> callback) {
  // Emulate a response.
  NetworkResponse response = {"dummyresponse", 200};
  std::move(callback).Run(std::move(response));
  return id_generator_.GenerateNextId();
}

TestFeedNetwork::TestFeedNetwork() = default;
TestFeedNetwork::~TestFeedNetwork() = default;

void TestFeedNetwork::SendQueryRequest(
    NetworkRequestType request_type,
    const feedwire::Request& request,
    const std::string& gaia,
    base::OnceCallback<void(QueryRequestResult)> callback) {
  last_gaia = gaia;
  ++send_query_call_count;
  // Emulate a successful response.
  // The response body is currently an empty message, because most of the
  // time we want to inject a translated response for ease of test-writing.
  query_request_sent = request;
  QueryRequestResult result;

  if (error != net::Error::OK)
    result.response_info.status_code = error;
  else
    result.response_info.status_code = http_status_code;

  result.response_info.response_body_bytes = 100;
  result.response_info.fetch_duration = base::Milliseconds(42);
  result.response_info.was_signed_in = true;
  if (injected_response_) {
    result.response_body = std::make_unique<feedwire::Response>(
        std::move(injected_response_.value()));
  } else {
    result.response_body = std::make_unique<feedwire::Response>();
  }
  Reply(base::BindOnce(std::move(callback), std::move(result)));
}

template <typename API>
void DebugLogApiResponse(std::string request_bytes,
                         const FeedNetwork::RawResponse& raw_response) {
  typename API::Request request;
  if (request.ParseFromString(request_bytes)) {
    VLOG(1) << "Request: " << ToTextProto(request);
  }
  typename API::Response response;
  if (response.ParseFromString(raw_response.response_bytes)) {
    VLOG(1) << "Response: " << ToTextProto(response);
  }
}

void DebugLogResponse(NetworkRequestType request_type,
                      base::StringPiece api_path,
                      base::StringPiece method,
                      std::string request_bytes,
                      const FeedNetwork::RawResponse& raw_response) {
  VLOG(1) << "TestFeedNetwork responding to request " << method << " "
          << api_path;
  if (request_type == UploadActionsDiscoverApi::kRequestType) {
    DebugLogApiResponse<UploadActionsDiscoverApi>(request_bytes, raw_response);
  } else if (request_type == ListRecommendedWebFeedDiscoverApi::kRequestType) {
    DebugLogApiResponse<ListRecommendedWebFeedDiscoverApi>(request_bytes,
                                                           raw_response);
  } else if (request_type == ListWebFeedsDiscoverApi::kRequestType) {
    DebugLogApiResponse<ListWebFeedsDiscoverApi>(request_bytes, raw_response);
  }
}

void TestFeedNetwork::SendDiscoverApiRequest(
    NetworkRequestType request_type,
    base::StringPiece api_path,
    base::StringPiece method,
    std::string request_bytes,
    const std::string& gaia,
    base::OnceCallback<void(RawResponse)> callback) {
  last_gaia = gaia;
  api_requests_sent_[request_type] = request_bytes;
  ++api_request_count_[request_type];
  std::vector<RawResponse>& injected_responses =
      injected_api_responses_[request_type];

  bool is_feed_query_request =
      request_type == NetworkRequestType::kFeedQuery ||
      request_type == WebFeedListContentsDiscoverApi::kRequestType ||
      request_type == QueryInteractiveFeedDiscoverApi::kRequestType ||
      request_type == QueryBackgroundFeedDiscoverApi::kRequestType ||
      request_type == QueryNextPageDiscoverApi::kRequestType;

  if (is_feed_query_request) {
    feedwire::Request request_proto;
    request_proto.ParseFromString(request_bytes);
    query_request_sent = request_proto;
    send_query_call_count++;
  }

  // If there is no injected response, create a default response.
  if (injected_responses.empty()) {
    switch (request_type) {
      case UploadActionsDiscoverApi::kRequestType: {
        feedwire::UploadActionsRequest request;
        ASSERT_TRUE(request.ParseFromString(request_bytes));
        feedwire::UploadActionsResponse response_message;
        response_message.mutable_consistency_token()->set_token(
            consistency_token);
        InjectApiResponse<UploadActionsDiscoverApi>(response_message);
        break;
      }
      case ListRecommendedWebFeedDiscoverApi::kRequestType: {
        feedwire::webfeed::ListRecommendedWebFeedsRequest request;
        ASSERT_TRUE(request.ParseFromString(request_bytes));
        feedwire::webfeed::ListRecommendedWebFeedsResponse response_message;
        InjectResponse(response_message);
        break;
      }
      case ListWebFeedsDiscoverApi::kRequestType: {
        feedwire::webfeed::ListWebFeedsRequest request;
        ASSERT_TRUE(request.ParseFromString(request_bytes));
        feedwire::webfeed::ListWebFeedsResponse response_message;
        InjectResponse(response_message);
        break;
      }

        // For FeedQuery requests, emulate a successful response.
        // The response body is currently an empty message, because most of
        // the time we want to inject a translated response for ease of
        // test-writing.

      case WebFeedListContentsDiscoverApi::kRequestType: {
        feedwire::Response response;
        InjectApiResponse<WebFeedListContentsDiscoverApi>(response);
        break;
      }
      case QueryInteractiveFeedDiscoverApi::kRequestType: {
        feedwire::Response response;
        InjectApiResponse<QueryInteractiveFeedDiscoverApi>(response);
        break;
      }
      case QueryBackgroundFeedDiscoverApi::kRequestType: {
        feedwire::Response response;
        InjectApiResponse<QueryBackgroundFeedDiscoverApi>(response);
        break;
      }
      case QueryNextPageDiscoverApi::kRequestType: {
        feedwire::Response response;
        InjectApiResponse<QueryNextPageDiscoverApi>(response);
        break;
      }
      default:
        break;
    }
  }

  if (!injected_responses.empty()) {
    RawResponse response = injected_responses[0];
    injected_responses.erase(injected_responses.begin());
    DebugLogResponse(request_type, api_path, method, request_bytes, response);
    Reply(base::BindOnce(std::move(callback), std::move(response)));
    return;
  }
  ASSERT_TRUE(false) << "No API response injected, and no default is available:"
                     << api_path;
}

void TestFeedNetwork::CancelRequests() {
  NOTIMPLEMENTED();
}

void TestFeedNetwork::InjectRealFeedQueryResponse() {
  base::FilePath response_file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &response_file_path));
  response_file_path = response_file_path.AppendASCII(
      "components/test/data/feed/response.binarypb");
  std::string response_data;
  CHECK(base::ReadFileToString(response_file_path, &response_data));

  feedwire::Response response;
  CHECK(response.ParseFromString(response_data));

  injected_response_ = response;
}

void TestFeedNetwork::InjectRealFeedQueryResponseWithNoContent() {
  base::FilePath response_file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &response_file_path));
  response_file_path = response_file_path.AppendASCII(
      "components/test/data/feed/response.binarypb");
  std::string response_data;
  CHECK(base::ReadFileToString(response_file_path, &response_data));

  feedwire::Response response;
  CHECK(response.ParseFromString(response_data));
  // Keep only the first two operations, the CLEAR_ALL and root, but no
  // content.
  auto* data_operations =
      response.mutable_feed_response()->mutable_data_operation();
  data_operations->erase(data_operations->begin() + 2, data_operations->end());

  injected_response_ = response;
}

void TestFeedNetwork::InjectEmptyActionRequestResult() {
  InjectApiRawResponse<UploadActionsDiscoverApi>({});
}

absl::optional<feedwire::UploadActionsRequest>
TestFeedNetwork::GetActionRequestSent() {
  return GetApiRequestSent<UploadActionsDiscoverApi>();
}

int TestFeedNetwork::GetActionRequestCount() const {
  return GetApiRequestCount<UploadActionsDiscoverApi>();
}

void TestFeedNetwork::ClearTestData() {
  injected_api_responses_.clear();
  api_requests_sent_.clear();
  api_request_count_.clear();
  injected_response_.reset();
}

void TestFeedNetwork::SendResponse() {
  ASSERT_TRUE(send_responses_on_command_)
      << "For use only send_responses_on_command_";
  if (reply_closures_.empty()) {
    // No replies queued yet, wait for the next one.
    base::RunLoop run_loop;
    on_reply_added_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  ASSERT_FALSE(reply_closures_.empty()) << "No replies ready to send";
  auto callback = std::move(reply_closures_[0]);
  reply_closures_.erase(reply_closures_.begin());
  std::move(callback).Run();
}

void TestFeedNetwork::SendResponsesOnCommand(bool on) {
  if (send_responses_on_command_ == on)
    return;
  if (!on) {
    while (!reply_closures_.empty()) {
      SendResponse();
    }
  }
  send_responses_on_command_ = on;
}

void TestFeedNetwork::Reply(base::OnceClosure reply_closure) {
  if (send_responses_on_command_) {
    reply_closures_.push_back(std::move(reply_closure));
    if (on_reply_added_)
      on_reply_added_.Run();
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(reply_closure));
  }
}

TestWireResponseTranslator::TestWireResponseTranslator() = default;
TestWireResponseTranslator::~TestWireResponseTranslator() = default;
RefreshResponseData TestWireResponseTranslator::TranslateWireResponse(
    feedwire::Response response,
    StreamModelUpdateRequest::Source source,
    bool was_signed_in_request,
    base::Time current_time) const {
  if (!injected_responses_.empty()) {
    if (injected_responses_[0].model_update_request)
      injected_responses_[0].model_update_request->source = source;
    RefreshResponseData result = std::move(injected_responses_[0]);
    injected_responses_.erase(injected_responses_.begin());
    return result;
  }
  return WireResponseTranslator::TranslateWireResponse(
      std::move(response), source, was_signed_in_request, current_time);
}
void TestWireResponseTranslator::InjectResponse(
    std::unique_ptr<StreamModelUpdateRequest> response,
    absl::optional<std::string> session_id) {
  DCHECK(!response->stream_data.signed_in() || !session_id);
  RefreshResponseData data;
  data.model_update_request = std::move(response);
  data.session_id = std::move(session_id);
  InjectResponse(std::move(data));
}
void TestWireResponseTranslator::InjectResponse(
    RefreshResponseData response_data) {
  injected_responses_.push_back(std::move(response_data));
}
bool TestWireResponseTranslator::InjectedResponseConsumed() const {
  return injected_responses_.empty();
}

FakeRefreshTaskScheduler::FakeRefreshTaskScheduler() = default;
FakeRefreshTaskScheduler::~FakeRefreshTaskScheduler() = default;
void FakeRefreshTaskScheduler::EnsureScheduled(RefreshTaskId id,
                                               base::TimeDelta run_time) {
  scheduled_run_times[id] = run_time;
}
void FakeRefreshTaskScheduler::Cancel(RefreshTaskId id) {
  canceled_tasks.insert(id);
}
void FakeRefreshTaskScheduler::RefreshTaskComplete(RefreshTaskId id) {
  completed_tasks.insert(id);
}

void FakeRefreshTaskScheduler::Clear() {
  scheduled_run_times.clear();
  canceled_tasks.clear();
  completed_tasks.clear();
}

TestMetricsReporter::TestMetricsReporter(PrefService* prefs)
    : MetricsReporter(prefs) {}
TestMetricsReporter::~TestMetricsReporter() = default;
TestMetricsReporter::StreamMetrics::StreamMetrics() = default;
TestMetricsReporter::StreamMetrics::~StreamMetrics() = default;

void TestMetricsReporter::ContentSliceViewed(const StreamType& stream_type,
                                             int index_in_stream,
                                             int stream_slice_count) {
  slice_viewed_index = index_in_stream;
  MetricsReporter::ContentSliceViewed(stream_type, index_in_stream,
                                      stream_slice_count);
}
void TestMetricsReporter::OnLoadStream(
    const StreamType& stream_type,
    LoadStreamStatus load_from_store_status,
    LoadStreamStatus final_status,
    bool is_initial_load,
    bool loaded_new_content_from_network,
    base::TimeDelta stored_content_age,
    const ContentStats& content_stats,
    const RequestMetadata& request_metadata,
    std::unique_ptr<LoadLatencyTimes> latencies) {
  load_stream_from_store_status = load_from_store_status;
  load_stream_status = final_status;
  LOG(INFO) << "OnLoadStream: " << final_status
            << " (store status: " << load_from_store_status << ")";
  MetricsReporter::OnLoadStream(
      stream_type, load_from_store_status, final_status, is_initial_load,
      loaded_new_content_from_network, stored_content_age, content_stats,
      request_metadata, std::move(latencies));
}
void TestMetricsReporter::OnLoadMoreBegin(const StreamType& stream_type,
                                          SurfaceId surface_id) {
  load_more_surface_id = surface_id;
  MetricsReporter::OnLoadMoreBegin(stream_type, surface_id);
}
void TestMetricsReporter::OnLoadMore(const StreamType& stream_type,
                                     LoadStreamStatus final_status,
                                     const ContentStats& content_stats) {
  load_more_status = final_status;
  MetricsReporter::OnLoadMore(stream_type, final_status, content_stats);
}
void TestMetricsReporter::OnBackgroundRefresh(const StreamType& stream_type,
                                              LoadStreamStatus final_status) {
  background_refresh_status = final_status;
  Stream(stream_type).background_refresh_status = final_status;
  MetricsReporter::OnBackgroundRefresh(stream_type, final_status);
}

TestMetricsReporter::StreamMetrics& TestMetricsReporter::Stream(
    const StreamType& stream_type) {
  if (stream_type.IsForYou())
    return for_you;
  if (stream_type.IsWebFeed())
    return web_feed;
  ADD_FAILURE() << stream_type << " case not supported here";
  return for_you;
}

void TestMetricsReporter::OnClearAll(base::TimeDelta since_last_clear) {
  time_since_last_clear = since_last_clear;
  MetricsReporter::OnClearAll(time_since_last_clear.value());
}
void TestMetricsReporter::OnUploadActions(UploadActionsStatus status) {
  upload_action_status = status;
  MetricsReporter::OnUploadActions(status);
}

FeedApiTest::FeedApiTest() {
  scoped_feature_list_.InitAndEnableFeature(kWebFeed);
}
FeedApiTest::~FeedApiTest() = default;
void FeedApiTest::SetUp() {
  kTestTimeEpoch = base::Time::Now();

  // Reset to default config, since tests can change it.
  Config config;
  // Disable fetching of recommended web feeds at startup to
  // avoid a delayed task in tests that don't need it.
  config.fetch_web_feed_info_delay = base::TimeDelta();
  // `use_feed_query_requests_for_web_feeds` is a temporary option for
  // debugging, setting it to false tests the preferred endpoint.
  config.use_feed_query_requests_for_web_feeds = false;
  SetFeedConfigForTesting(config);

  feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
  feed::RegisterProfilePrefs(profile_prefs_.registry());
  metrics_reporter_ = std::make_unique<TestMetricsReporter>(&profile_prefs_);

  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_factory_);
  image_fetcher_ =
      std::make_unique<TestImageFetcher>(shared_url_loader_factory_);

  CreateStream();
}

void FeedApiTest::TearDown() {
  // Unblock network responses to allow clean teardown.
  network_.SendResponsesOnCommand(false);
  // Ensure the task queue can return to idle. Failure to do so may be
  // due to a stuck task that never called |TaskComplete()|.
  WaitForIdleTaskQueue();
  // ProtoDatabase requires PostTask to clean up.
  store_.reset();
  persistent_key_value_store_.reset();
  task_environment_.RunUntilIdle();
  // FeedStoreTest.OvewriteStream and OverwriteStreamWebFeed depends on
  // kTestTimeEpoch == UnixEpoch(). i.e. using MakeTypicalInitialModelState
  // with default arguments. Need to reset kTestTimeEpoch to avoid the tests'
  // flaky failure.
  kTestTimeEpoch = base::Time::UnixEpoch();
}
bool FeedApiTest::IsEulaAccepted() {
  return is_eula_accepted_;
}
bool FeedApiTest::IsOffline() {
  return is_offline_;
}
std::string FeedApiTest::GetSyncSignedInGaia() {
  return signed_in_gaia_;
}
void FeedApiTest::RegisterFollowingFeedFollowCountFieldTrial(
    size_t follow_count) {
  register_following_feed_follow_count_field_trial_calls_.push_back(
      follow_count);
}
DisplayMetrics FeedApiTest::GetDisplayMetrics() {
  DisplayMetrics result;
  result.density = 200;
  result.height_pixels = 800;
  result.width_pixels = 350;
  return result;
}
std::string FeedApiTest::GetLanguageTag() {
  return "en-US";
}
bool FeedApiTest::IsAutoplayEnabled() {
  return false;
}
void FeedApiTest::ClearAll() {
  if (on_clear_all_)
    on_clear_all_.Run();
}
void FeedApiTest::PrefetchImage(const GURL& url) {
  prefetched_images_.push_back(url);
  prefetch_image_call_count_++;
}

void FeedApiTest::CreateStream(bool wait_for_initialization,
                               bool start_surface) {
  ChromeInfo chrome_info;
  chrome_info.channel = version_info::Channel::STABLE;
  chrome_info.version = base::Version({99, 1, 9911, 2});
  chrome_info.start_surface = start_surface;
  stream_ = std::make_unique<FeedStream>(
      &refresh_scheduler_, metrics_reporter_.get(), this, &profile_prefs_,
      &network_, image_fetcher_.get(), store_.get(),
      persistent_key_value_store_.get(), chrome_info);
  stream_->SetWireResponseTranslatorForTesting(&response_translator_);

  if (wait_for_initialization)
    WaitForIdleTaskQueue();  // Wait for any initialization.
}

std::unique_ptr<StreamModel> FeedApiTest::CreateStreamModel() {
  return std::make_unique<StreamModel>(&stream_model_context_);
}

bool FeedApiTest::IsTaskQueueIdle() const {
  return !stream_->GetTaskQueueForTesting().HasPendingTasks() &&
         !stream_->GetTaskQueueForTesting().HasRunningTask();
}

void FeedApiTest::WaitForIdleTaskQueue() {
  RunLoopUntil(base::BindLambdaForTesting([&]() {
    return IsTaskQueueIdle() &&
           !stream_->subscriptions().is_loading_model_for_testing();
  }));
}

void FeedApiTest::UnloadModel(const StreamType& stream_type) {
  WaitForIdleTaskQueue();
  stream_->UnloadModel(stream_type);
}

std::string FeedApiTest::DumpStoreState(bool print_keys) {
  base::RunLoop run_loop;
  std::unique_ptr<std::map<std::string, feedstore::Record>> records;
  auto callback =
      [&](bool,
          std::unique_ptr<std::map<std::string, feedstore::Record>> result) {
        records = std::move(result);
        run_loop.Quit();
      };
  store_->GetDatabaseForTesting()->LoadKeysAndEntries(
      base::BindLambdaForTesting(callback));

  run_loop.Run();
  std::stringstream ss;
  for (const auto& item : *records) {
    if (print_keys)
      ss << '"' << item.first << "\": ";

    ss << item.second;
  }
  return ss.str();
}

void FeedApiTest::FollowWebFeed(const WebFeedPageInformation page_info) {
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  network_.InjectResponse(SuccessfulFollowResponse(page_info.url().host()));
  stream_->subscriptions().FollowWebFeed(page_info, callback.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
}

void FeedApiTest::UploadActions(std::vector<feedwire::FeedAction> actions) {
  size_t actions_remaining = actions.size();
  for (feedwire::FeedAction& action : actions) {
    stream_->UploadAction(action, (--actions_remaining) == 0ul,
                          base::DoNothing());
  }
}

RefreshTaskId FeedStreamTestForAllStreamTypes::GetRefreshTaskId() const {
  RefreshTaskId id;
  CHECK(GetStreamType().GetRefreshTaskId(id));
  return id;
}

void FeedStreamTestForAllStreamTypes::SetUp() {
  // Enable web feeds and inject a subscription so that we attempt to load
  // the web feed stream.
  FeedApiTest::SetUp();
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
}

}  // namespace test
}  // namespace feed
