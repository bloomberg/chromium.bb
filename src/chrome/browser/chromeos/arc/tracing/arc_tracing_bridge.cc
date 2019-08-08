// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_tracing_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/posix/unix_domain_socket.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

using ChromeEventBundleHandle =
    protozero::MessageHandle<perfetto::protos::pbzero::ChromeEventBundle>;

namespace arc {

namespace {

// The maximum size used to store one trace event. The ad hoc trace format for
// atrace is 1024 bytes. Here we add additional size as we're using JSON and
// have additional data fields.
constexpr size_t kArcTraceMessageLength = 1024 + 512;

constexpr char kChromeTraceEventLabel[] = "traceEvents";

// The prefix of the categories to be shown on the trace selection UI.
// The space at the end of the string is intentional as the separator between
// the prefix and the real categories.
constexpr char kCategoryPrefix[] = TRACE_DISABLED_BY_DEFAULT("android ");

// Singleton factory for ArcTracingBridge.
class ArcTracingBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcTracingBridge,
          ArcTracingBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcTracingBridgeFactory";

  static ArcTracingBridgeFactory* GetInstance() {
    return base::Singleton<ArcTracingBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcTracingBridgeFactory>;
  ArcTracingBridgeFactory() = default;
  ~ArcTracingBridgeFactory() override = default;
};

// Perfetto data source which coordinates ARC tracing sessions with perfetto's
// ProducerClient when perfetto is used as the tracing backend.
class ArcTracingDataSource : public tracing::ProducerClient::DataSourceBase {
 public:
  static ArcTracingDataSource* GetInstance() {
    static base::NoDestructor<ArcTracingDataSource> instance;
    return instance.get();
  }

  // Called after constructing |bridge|.
  void RegisterBridgeOnUI(ArcTracingBridge* bridge) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_EQ(ArcTracingBridge::State::kDisabled, bridge->state());
    bool success = bridges_.insert(bridge).second;
    DCHECK(success);

    if (producer_client_ && !stop_complete_callback_) {
      // We're currently tracing, so start the new bridge, too.
      // |this| never gets destructed, so it's OK to bind an unretained pointer.
      bridge->StartTracing(
          data_source_config_.chrome_config().trace_config(),
          base::BindOnce(&ArcTracingDataSource::OnTracingStartedOnUI,
                         base::Unretained(this)));
    }
  }

  // Called when destructing |bridge|.
  void UnregisterBridgeOnUI(ArcTracingBridge* bridge) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    const size_t erase_count = bridges_.erase(bridge);
    DCHECK_EQ(1u, erase_count);

    // Make sure we don't continue to wait for any of the bridge's callbacks.
    OnTracingStartedOnUI(false /*success*/);
    OnTraceDataOnUI(std::string() /*data*/);
  }

 private:
  friend class base::NoDestructor<ArcTracingDataSource>;

  ArcTracingDataSource()
      : DataSourceBase(tracing::mojom::kArcTraceDataSourceName),
        perfetto_task_runner_(
            tracing::ProducerClient::Get()->GetTaskRunner()->task_runner()) {
    tracing::ProducerClient::Get()->AddDataSource(this);
  }

  // Note that ArcTracingDataSource is a singleton that's never destroyed.
  ~ArcTracingDataSource() override = default;

  // tracing::ProducerClient::DataSourceBase.
  void StartTracing(
      tracing::ProducerClient* producer_client,
      const perfetto::DataSourceConfig& data_source_config) override {
    // |this| never gets destructed, so it's OK to bind an unretained pointer.
    // |producer_client| is a singleton that is never destroyed.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ArcTracingDataSource::StartTracingOnUI,
                       base::Unretained(this), producer_client,
                       data_source_config));
  }

  void StopTracing(base::OnceClosure stop_complete_callback) override {
    // |this| never gets destructed, so it's OK to bind an unretained pointer.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ArcTracingDataSource::StopTracingOnUI,
                       base::Unretained(this),
                       std::move(stop_complete_callback)));
  }

  void Flush(base::RepeatingClosure flush_complete_callback) override {
    // ARC's tracing service doesn't currently support flushing while recording.
    flush_complete_callback.Run();
  }

  // Starts all registered bridges.
  void StartTracingOnUI(tracing::ProducerClient* producer_client,
                        const perfetto::DataSourceConfig& data_source_config) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    DCHECK(!producer_client_);
    producer_client_ = producer_client;
    data_source_config_ = data_source_config;

    for (ArcTracingBridge* bridge : bridges_) {
      // |this| never gets destructed, so it's OK to bind an unretained pointer.
      bridge->StartTracing(
          data_source_config_.chrome_config().trace_config(),
          base::BindOnce(&ArcTracingDataSource::OnTracingStartedOnUI,
                         base::Unretained(this)));
    }
  }

  // Stops all registered bridges. Calls |stop_complete_callback| when all
  // bridges have stopped.
  void StopTracingOnUI(base::OnceClosure stop_complete_callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // We may receive a StopTracing without StartTracing.
    if (!producer_client_)
      return;

    // We may still be in startup. In this case, store a callback to rerun
    // StopTracingOnUI() once startup is complete.
    if (IsAnyBridgeStarting()) {
      DCHECK(!pending_stop_tracing_);
      pending_stop_tracing_ = base::BindOnce(
          &ArcTracingDataSource::StopTracingOnUI, base::Unretained(this),
          std::move(stop_complete_callback));
      return;
    }

    stop_complete_callback_ = std::move(stop_complete_callback);

    for (ArcTracingBridge* bridge : bridges_) {
      // StopTracingOnUI should only be called once all bridges have completed
      // or abandoned startup.
      DCHECK_NE(ArcTracingBridge::State::kStarting, bridge->state());
      if (bridge->state() != ArcTracingBridge::State::kEnabled)
        continue;
      // |this| never gets destructed, so it's OK to bind an unretained pointer.
      bridge->StopAndFlush(base::BindOnce(
          &ArcTracingDataSource::OnTraceDataOnUI, base::Unretained(this)));
    }

    // There may not have been any bridges left in State::kEnabled. This will
    // call the callback if all bridges are already stopped.
    OnTraceDataOnUI(std::string());
  }

  // Called by each bridge when it has started tracing. Also called when a
  // bridge is unregisted.
  void OnTracingStartedOnUI(bool success) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!IsAnyBridgeStarting() && pending_stop_tracing_)
      std::move(pending_stop_tracing_).Run();
  }

  // Called by each bridge when it has stopped tracing. Also called when a
  // bridge is unregisted. Records the supplied |data| into the
  // |producer_client_|'s buffer.
  void OnTraceDataOnUI(const std::string& data) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // When a bridge unregisters, we may not actually be stopping.
    if (!stop_complete_callback_)
      return;

    DCHECK(producer_client_);

    if (!data.empty()) {
      std::unique_ptr<perfetto::TraceWriter> trace_writer =
          producer_client_->CreateTraceWriter(
              data_source_config_.target_buffer());
      DCHECK(trace_writer);
      perfetto::TraceWriter::TracePacketHandle trace_packet_handle =
          trace_writer->NewTracePacket();
      ChromeEventBundleHandle event_bundle =
          ChromeEventBundleHandle(trace_packet_handle->set_chrome_events());
      auto* legacy_json_trace = event_bundle->add_legacy_json_trace();
      legacy_json_trace->set_type(
          perfetto::protos::pbzero::ChromeLegacyJsonTrace::USER_TRACE);
      legacy_json_trace->set_data(data.data(), data.length());
    }

    if (AreAllBridgesStopped()) {
      producer_client_ = nullptr;
      perfetto_task_runner_->PostTask(FROM_HERE,
                                      std::move(stop_complete_callback_));
    }
  }

  bool IsAnyBridgeStarting() const {
    for (ArcTracingBridge* bridge : bridges_) {
      if (bridge->state() == ArcTracingBridge::State::kStarting)
        return true;
    }
    return false;
  }

  bool AreAllBridgesStopped() const {
    for (ArcTracingBridge* bridge : bridges_) {
      if (bridge->state() != ArcTracingBridge::State::kDisabled)
        return false;
    }
    return true;
  }

  base::SequencedTaskRunner* perfetto_task_runner_;
  std::set<ArcTracingBridge*> bridges_;
  // In case StopTracing() is called before tracing was started for all bridges,
  // this stores a callback to StopTracing() that's executed when all bridges
  // have started.
  base::OnceClosure pending_stop_tracing_;
  // Called when all bridges have completed stopping, notifying ProducerClient.
  base::OnceClosure stop_complete_callback_;
  tracing::ProducerClient* producer_client_ = nullptr;
  perfetto::DataSourceConfig data_source_config_;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingDataSource);
};

}  // namespace

struct ArcTracingBridge::Category {
  // The name used by Android to trigger tracing.
  std::string name;
  // The full name shown in the tracing UI in chrome://tracing.
  std::string full_name;
};

// static
ArcTracingBridge* ArcTracingBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcTracingBridgeFactory::GetForBrowserContext(context);
}

ArcTracingBridge::ArcTracingBridge(content::BrowserContext* context,
                                   ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      agent_(this),
      reader_(std::make_unique<ArcTracingReader>()),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->tracing()->AddObserver(this);
  ArcTracingDataSource::GetInstance()->RegisterBridgeOnUI(this);
}

ArcTracingBridge::~ArcTracingBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ArcTracingDataSource::GetInstance()->UnregisterBridgeOnUI(this);
  arc_bridge_service_->tracing()->RemoveObserver(this);

  // Delete the reader on the IO thread.
  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO})
      ->DeleteSoon(FROM_HERE, reader_.release());
}

void ArcTracingBridge::GetCategories(std::set<std::string>* category_set) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& category : categories_) {
    category_set->insert(category.full_name);
  }
}

void ArcTracingBridge::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::TracingInstance* tracing_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->tracing(), QueryAvailableCategories);
  if (!tracing_instance)
    return;
  tracing_instance->QueryAvailableCategories(base::BindOnce(
      &ArcTracingBridge::OnCategoriesReady, weak_ptr_factory_.GetWeakPtr()));
}

void ArcTracingBridge::OnCategoriesReady(
    const std::vector<std::string>& categories) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // There is no API in TraceLog to remove a category from the UI. As an
  // alternative, the old category that is no longer in |categories_| will be
  // ignored when calling |StartTracing|.
  categories_.clear();
  for (const auto& category : categories)
    categories_.emplace_back(Category{category, kCategoryPrefix + category});
}

void ArcTracingBridge::StartTracing(const std::string& config,
                                    SuccessCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ != State::kDisabled) {
    DLOG(WARNING) << "Cannot start tracing, it is already enabled.";
    std::move(callback).Run(false /*success*/);
    return;
  }
  state_ = State::kStarting;

  base::trace_event::TraceConfig trace_config(config);

  base::ScopedFD write_fd, read_fd;
  bool success =
      trace_config.IsSystraceEnabled() && CreateSocketPair(&read_fd, &write_fd);

  if (!success) {
    OnArcTracingStarted(std::move(callback), false /*success*/);
    return;
  }

  mojom::TracingInstance* tracing_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->tracing(), StartTracing);
  if (!tracing_instance) {
    OnArcTracingStarted(std::move(callback), false /*success*/);
    return;
  }

  std::vector<std::string> selected_categories;
  for (const auto& category : categories_) {
    if (trace_config.IsCategoryGroupEnabled(category.full_name))
      selected_categories.push_back(category.name);
  }

  tracing_instance->StartTracing(
      selected_categories, mojo::WrapPlatformFile(write_fd.release()),
      base::BindOnce(&ArcTracingBridge::OnArcTracingStarted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  // |reader_| will be destroyed after us on the IO thread, so it's OK to use an
  // unretained pointer.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ArcTracingReader::StartTracing,
                     base::Unretained(reader_.get()), std::move(read_fd)));
}

void ArcTracingBridge::OnArcTracingStarted(SuccessCallback callback,
                                           bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(State::kStarting, state_);
  state_ = success ? State::kEnabled : State::kDisabled;
  std::move(callback).Run(success);
}

void ArcTracingBridge::StopAndFlush(TraceDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ != State::kEnabled) {
    DLOG(WARNING) << "Cannot stop tracing, it is not enabled.";
    std::move(callback).Run(std::string());
    return;
  }
  state_ = State::kStopping;

  mojom::TracingInstance* tracing_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->tracing(), StopTracing);
  if (!tracing_instance) {
    OnArcTracingStopped(std::move(callback), false);
    return;
  }
  tracing_instance->StopTracing(
      base::BindOnce(&ArcTracingBridge::OnArcTracingStopped,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcTracingBridge::OnArcTracingStopped(
    TraceDataCallback tracing_stopped_callback,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(State::kStopping, state_);
  if (!success) {
    DLOG(WARNING) << "Failed to stop ARC tracing, closing file anyway.";
  }
  // |reader_| will be destroyed after us on the IO thread, so it's OK to use an
  // unretained pointer.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ArcTracingReader::StopTracing,
                     base::Unretained(reader_.get())),
      base::BindOnce(&ArcTracingBridge::OnTracingReaderStopped,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(tracing_stopped_callback)));
}

void ArcTracingBridge::OnTracingReaderStopped(
    TraceDataCallback tracing_stopped_callback,
    const std::string& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(State::kStopping, state_);
  state_ = State::kDisabled;
  std::move(tracing_stopped_callback).Run(data);
}

ArcTracingBridge::ArcTracingAgent::ArcTracingAgent(ArcTracingBridge* bridge)
    : BaseAgent(

          kChromeTraceEventLabel,
          tracing::mojom::TraceDataType::ARRAY,
          base::kNullProcessId),
      bridge_(bridge) {
}

ArcTracingBridge::ArcTracingAgent::~ArcTracingAgent() = default;

void ArcTracingBridge::ArcTracingAgent::GetCategories(
    std::set<std::string>* category_set) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_->GetCategories(category_set);
}

void ArcTracingBridge::ArcTracingAgent::StartTracing(
    const std::string& config,
    base::TimeTicks coordinator_time,
    Agent::StartTracingCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bridge_->StartTracing(config, std::move(callback));
}

void ArcTracingBridge::ArcTracingAgent::StopAndFlush(
    tracing::mojom::RecorderPtr recorder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  recorder_ = std::move(recorder);
  // |bridge_| owns us, so it's OK to pass an unretained pointer.
  bridge_->StopAndFlush(
      base::BindOnce(&ArcTracingAgent::OnTraceData, base::Unretained(this)));
}

void ArcTracingBridge::ArcTracingAgent::OnTraceData(const std::string& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(recorder_);
  recorder_->AddChunk(data);
  recorder_.reset();
}

ArcTracingBridge::ArcTracingReader::ArcTracingReader() = default;

ArcTracingBridge::ArcTracingReader::~ArcTracingReader() {
  DCHECK(!fd_watcher_);
}

void ArcTracingBridge::ArcTracingReader::StartTracing(base::ScopedFD read_fd) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  read_fd_ = std::move(read_fd);
  // We own |fd_watcher_|, so it's OK to use an unretained pointer.
  fd_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      read_fd_.get(),
      base::BindRepeating(&ArcTracingReader::OnTraceDataAvailable,
                          base::Unretained(this)));
}

void ArcTracingBridge::ArcTracingReader::OnTraceDataAvailable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  char buf[kArcTraceMessageLength];
  std::vector<base::ScopedFD> unused_fds;
  ssize_t n = base::UnixDomainSocket::RecvMsg(
      read_fd_.get(), buf, kArcTraceMessageLength, &unused_fds);
  if (n < 0) {
    DPLOG(WARNING) << "Unexpected error while reading trace from client.";
    // Do nothing here as StopTracing will do the clean up and the existing
    // trace logs will be returned.
    return;
  }

  if (n == 0) {
    // When EOF is reached, stop listening for changes since there's never
    // going to be any more data to be read. The rest of the cleanup will be
    // done in StopTracing.
    fd_watcher_.reset();
    return;
  }

  if (n > static_cast<ssize_t>(kArcTraceMessageLength)) {
    DLOG(WARNING) << "Unexpected data size when reading trace from client.";
    return;
  }
  ring_buffer_.SaveToBuffer(std::string(buf, n));
}

std::string ArcTracingBridge::ArcTracingReader::StopTracing() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  fd_watcher_.reset();
  read_fd_.reset();

  bool append_comma = false;
  std::string data;
  for (auto it = ring_buffer_.Begin(); it; ++it) {
    if (append_comma)
      data.append(",");
    else
      append_comma = true;
    data.append(**it);
  }
  ring_buffer_.Clear();

  return data;
}

}  // namespace arc
