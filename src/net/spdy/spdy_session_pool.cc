// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/trace_constants.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_source.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_handle.h"
#include "net/spdy/spdy_session.h"
#include "net/third_party/quiche/src/spdy/core/hpack/hpack_constants.h"
#include "net/third_party/quiche/src/spdy/core/hpack/hpack_huffman_table.h"
#include "net/third_party/quiche/src/spdy/core/hpack/hpack_static_table.h"

namespace net {

namespace {

enum SpdySessionGetTypes {
  CREATED_NEW                 = 0,
  FOUND_EXISTING              = 1,
  FOUND_EXISTING_FROM_IP_POOL = 2,
  IMPORTED_FROM_SOCKET        = 3,
  SPDY_SESSION_GET_MAX        = 4
};

}  // namespace

SpdySessionPool::SpdySessionRequest::Delegate::Delegate() = default;
SpdySessionPool::SpdySessionRequest::Delegate::~Delegate() = default;

SpdySessionPool::SpdySessionRequest::SpdySessionRequest(
    const SpdySessionKey& key,
    bool enable_ip_based_pooling,
    bool is_websocket,
    Delegate* delegate,
    SpdySessionPool* spdy_session_pool)
    : key_(key),
      enable_ip_based_pooling_(enable_ip_based_pooling),
      is_websocket_(is_websocket),
      delegate_(delegate),
      spdy_session_pool_(spdy_session_pool) {}

SpdySessionPool::SpdySessionRequest::~SpdySessionRequest() {
  if (spdy_session_pool_)
    spdy_session_pool_->RemoveRequestForSpdySession(this);
}

void SpdySessionPool::SpdySessionRequest::OnRemovedFromPool() {
  DCHECK(spdy_session_pool_);
  spdy_session_pool_ = nullptr;
}

SpdySessionPool::SpdySessionPool(
    HostResolver* resolver,
    SSLConfigService* ssl_config_service,
    HttpServerProperties* http_server_properties,
    TransportSecurityState* transport_security_state,
    const quic::QuicTransportVersionVector& quic_supported_versions,
    bool enable_ping_based_connection_checking,
    bool support_ietf_format_quic_altsvc,
    size_t session_max_recv_window_size,
    const spdy::SettingsMap& initial_settings,
    const base::Optional<GreasedHttp2Frame>& greased_http2_frame,
    SpdySessionPool::TimeFunc time_func,
    NetworkQualityEstimator* network_quality_estimator)
    : http_server_properties_(http_server_properties),
      transport_security_state_(transport_security_state),
      ssl_config_service_(ssl_config_service),
      resolver_(resolver),
      quic_supported_versions_(quic_supported_versions),
      enable_sending_initial_data_(true),
      enable_ping_based_connection_checking_(
          enable_ping_based_connection_checking),
      support_ietf_format_quic_altsvc_(support_ietf_format_quic_altsvc),
      session_max_recv_window_size_(session_max_recv_window_size),
      initial_settings_(initial_settings),
      greased_http2_frame_(greased_http2_frame),
      time_func_(time_func),
      push_delegate_(nullptr),
      network_quality_estimator_(network_quality_estimator),
      weak_ptr_factory_(this) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  if (ssl_config_service_)
    ssl_config_service_->AddObserver(this);
  CertDatabase::GetInstance()->AddObserver(this);
}

SpdySessionPool::~SpdySessionPool() {
  DCHECK(spdy_session_request_map_.empty());
  // TODO(bnc): CloseAllSessions() is also called in HttpNetworkSession
  // destructor, one of the two calls should be removed.
  CloseAllSessions();

  while (!sessions_.empty()) {
    // Destroy sessions to enforce that lifetime is scoped to SpdySessionPool.
    // Write callbacks queued upon session drain are not invoked.
    RemoveUnavailableSession((*sessions_.begin())->GetWeakPtr());
  }

  if (ssl_config_service_)
    ssl_config_service_->RemoveObserver(this);
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  CertDatabase::GetInstance()->RemoveObserver(this);
}

base::WeakPtr<SpdySession>
SpdySessionPool::CreateAvailableSessionFromSocketHandle(
    const SpdySessionKey& key,
    bool is_trusted_proxy,
    std::unique_ptr<ClientSocketHandle> client_socket_handle,
    const NetLogWithSource& net_log) {
  TRACE_EVENT0(NetTracingCategory(),
               "SpdySessionPool::CreateAvailableSessionFromSocketHandle");

  std::unique_ptr<SpdySession> new_session =
      CreateSession(key, is_trusted_proxy, net_log.net_log());
  new_session->InitializeWithSocketHandle(std::move(client_socket_handle),
                                          this);
  return InsertSession(key, std::move(new_session), net_log);
}

base::WeakPtr<SpdySession> SpdySessionPool::CreateAvailableSessionFromSocket(
    const SpdySessionKey& key,
    bool is_trusted_proxy,
    std::unique_ptr<StreamSocket> socket_stream,
    const LoadTimingInfo::ConnectTiming& connect_timing,
    const NetLogWithSource& net_log) {
  TRACE_EVENT0(NetTracingCategory(),
               "SpdySessionPool::CreateAvailableSessionFromSocket");

  std::unique_ptr<SpdySession> new_session =
      CreateSession(key, is_trusted_proxy, net_log.net_log());

  new_session->InitializeWithSocket(std::move(socket_stream), connect_timing,
                                    this);

  return InsertSession(key, std::move(new_session), net_log);
}

base::WeakPtr<SpdySession> SpdySessionPool::FindAvailableSession(
    const SpdySessionKey& key,
    bool enable_ip_based_pooling,
    bool is_websocket,
    const NetLogWithSource& net_log) {
  auto it = LookupAvailableSessionByKey(key);
  if (it != available_sessions_.end() &&
      (!is_websocket || it->second->support_websocket())) {
    if (key == it->second->spdy_session_key()) {
      UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet", FOUND_EXISTING,
                                SPDY_SESSION_GET_MAX);
      net_log.AddEvent(
          NetLogEventType::HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION,
          it->second->net_log().source().ToEventParametersCallback());
      return it->second;
    }

    if (enable_ip_based_pooling) {
      UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                                FOUND_EXISTING_FROM_IP_POOL,
                                SPDY_SESSION_GET_MAX);
      net_log.AddEvent(
          NetLogEventType::
              HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
          it->second->net_log().source().ToEventParametersCallback());
      return it->second;
    }

    // Remove session from available sessions and from aliases, and remove
    // key from the session's pooled alias set, so that a new session can be
    // created with this |key|.
    it->second->RemovePooledAlias(key);
    UnmapKey(key);
    RemoveAliases(key);
    return base::WeakPtr<SpdySession>();
  }

  if (!enable_ip_based_pooling)
    return base::WeakPtr<SpdySession>();

  // Look up IP addresses from resolver cache.
  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver_->CreateRequest(key.host_port_pair(), net_log, parameters);

  int rv = request->Start(base::BindOnce([](int error) { NOTREACHED(); }));
  DCHECK_NE(rv, ERR_IO_PENDING);
  if (rv != OK)
    return base::WeakPtr<SpdySession>();

  // Check if we have a session through a domain alias.
  for (const auto& address : request->GetAddressResults().value().endpoints()) {
    auto range = aliases_.equal_range(address);
    for (auto alias_it = range.first; alias_it != range.second; ++alias_it) {
      // We found an alias.
      const SpdySessionKey& alias_key = alias_it->second;

      // We can reuse this session only if the proxy and privacy
      // settings match.
      if (!(alias_key.proxy_server() == key.proxy_server()) ||
          !(alias_key.privacy_mode() == key.privacy_mode()) ||
          !(alias_key.is_proxy_session() == key.is_proxy_session())) {
        continue;
      }

      auto available_session_it = LookupAvailableSessionByKey(alias_key);
      if (available_session_it == available_sessions_.end()) {
        NOTREACHED();  // It shouldn't be in the aliases table if we can't get
                       // it!
        continue;
      }

      // Make copy of WeakPtr as call to UnmapKey() will delete original.
      const base::WeakPtr<SpdySession> available_session =
          available_session_it->second;
      DCHECK(base::ContainsKey(sessions_, available_session.get()));

      if (is_websocket && !available_session->support_websocket())
        continue;

      // If the session is a secure one, we need to verify that the
      // server is authenticated to serve traffic for |host_port_proxy_pair|
      // too.
      if (!available_session->VerifyDomainAuthentication(
              key.host_port_pair().host())) {
        UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 0, 2);
        continue;
      }

      bool adding_pooled_alias = true;

      // If socket tags differ, see if session's socket tag can be changed.
      if (alias_key.socket_tag() != key.socket_tag()) {
        SpdySessionKey old_key = available_session->spdy_session_key();
        SpdySessionKey new_key(old_key.host_port_pair(), old_key.proxy_server(),
                               old_key.privacy_mode(),
                               old_key.is_proxy_session(), key.socket_tag());

        // If there is already a session with |new_key|, skip this one.
        // It will be found in |aliases_| in a future iteration.
        if (available_sessions_.find(new_key) != available_sessions_.end())
          continue;

        if (!available_session->ChangeSocketTag(key.socket_tag()))
          continue;

        DCHECK(available_session->spdy_session_key() == new_key);

        // This isn't a pooled alias, it's the actual session.
        adding_pooled_alias = false;

        // Remap main session key.
        UnmapKey(old_key);
        MapKeyToAvailableSession(new_key, available_session);

        // Remap alias.
        aliases_.insert(AliasMap::value_type(alias_it->first, new_key));
        aliases_.erase(alias_it);

        // Remap pooled session keys.
        const auto& aliases = available_session->pooled_aliases();
        for (auto it = aliases.begin(); it != aliases.end();) {
          // Ignore aliases this loop is inserting.
          if (it->socket_tag() == key.socket_tag()) {
            ++it;
            continue;
          }
          UnmapKey(*it);
          SpdySessionKey new_pool_alias_key = SpdySessionKey(
              it->host_port_pair(), it->proxy_server(), it->privacy_mode(),
              it->is_proxy_session(), key.socket_tag());
          MapKeyToAvailableSession(new_pool_alias_key, available_session);
          auto old_it = it;
          ++it;
          available_session->RemovePooledAlias(*old_it);
          available_session->AddPooledAlias(new_pool_alias_key);
        }
      }

      UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 1, 2);
      UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                                FOUND_EXISTING_FROM_IP_POOL,
                                SPDY_SESSION_GET_MAX);
      net_log.AddEvent(
          NetLogEventType::
              HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
          available_session->net_log().source().ToEventParametersCallback());
      if (adding_pooled_alias) {
        // Add this session to the map so that we can find it next time.
        MapKeyToAvailableSession(key, available_session);
        available_session->AddPooledAlias(key);
      }
      return available_session;
    }
  }

  return base::WeakPtr<SpdySession>();
}

base::WeakPtr<SpdySession> SpdySessionPool::RequestSession(
    const SpdySessionKey& key,
    bool enable_ip_based_pooling,
    bool is_websocket,
    const NetLogWithSource& net_log,
    base::RepeatingClosure on_request_destroyed_callback,
    SpdySessionRequest::Delegate* delegate,
    std::unique_ptr<SpdySessionRequest>* spdy_session_request,
    bool* is_first_request_for_session) {
  DCHECK(delegate);

  base::WeakPtr<SpdySession> spdy_session =
      FindAvailableSession(key, enable_ip_based_pooling, is_websocket, net_log);
  if (spdy_session) {
    // This value doesn't really matter, but best to always populate it, for
    // consistency.
    *is_first_request_for_session = true;
    return spdy_session;
  }

  RequestSet* request_set = &spdy_session_request_map_[key];
  *is_first_request_for_session = request_set->empty();
  *spdy_session_request = std::make_unique<SpdySessionRequest>(
      key, enable_ip_based_pooling, is_websocket, delegate, this);
  request_set->insert(spdy_session_request->get());

  if (on_request_destroyed_callback && !*is_first_request_for_session) {
    spdy_session_pending_request_map_[key].push_back(
        on_request_destroyed_callback);
  }
  return nullptr;
}

void SpdySessionPool::MakeSessionUnavailable(
    const base::WeakPtr<SpdySession>& available_session) {
  UnmapKey(available_session->spdy_session_key());
  RemoveAliases(available_session->spdy_session_key());
  const std::set<SpdySessionKey>& aliases = available_session->pooled_aliases();
  for (auto it = aliases.begin(); it != aliases.end(); ++it) {
    UnmapKey(*it);
    RemoveAliases(*it);
  }
  DCHECK(!IsSessionAvailable(available_session));
}

void SpdySessionPool::RemoveUnavailableSession(
    const base::WeakPtr<SpdySession>& unavailable_session) {
  DCHECK(!IsSessionAvailable(unavailable_session));

  unavailable_session->net_log().AddEvent(
      NetLogEventType::HTTP2_SESSION_POOL_REMOVE_SESSION);

  auto it = sessions_.find(unavailable_session.get());
  CHECK(it != sessions_.end());
  std::unique_ptr<SpdySession> owned_session(*it);
  sessions_.erase(it);
}

// Make a copy of |sessions_| in the Close* functions below to avoid
// reentrancy problems. Since arbitrary functions get called by close
// handlers, it doesn't suffice to simply increment the iterator
// before closing.

void SpdySessionPool::CloseCurrentSessions(Error error) {
  CloseCurrentSessionsHelper(error, "Closing current sessions.",
                             false /* idle_only */);
}

void SpdySessionPool::CloseCurrentIdleSessions() {
  CloseCurrentSessionsHelper(ERR_ABORTED, "Closing idle sessions.",
                             true /* idle_only */);
}

void SpdySessionPool::CloseAllSessions() {
  auto is_draining = [](const SpdySession* s) { return s->IsDraining(); };
  // Repeat until every SpdySession owned by |this| is draining.
  while (!std::all_of(sessions_.begin(), sessions_.end(), is_draining)) {
    CloseCurrentSessionsHelper(ERR_ABORTED, "Closing all sessions.",
                               false /* idle_only */);
  }
}

std::unique_ptr<base::Value> SpdySessionPool::SpdySessionPoolInfoToValue()
    const {
  auto list = std::make_unique<base::ListValue>();

  for (auto it = available_sessions_.begin(); it != available_sessions_.end();
       ++it) {
    // Only add the session if the key in the map matches the main
    // host_port_proxy_pair (not an alias).
    const SpdySessionKey& key = it->first;
    const SpdySessionKey& session_key = it->second->spdy_session_key();
    if (key == session_key)
      list->Append(it->second->GetInfoAsValue());
  }
  return std::move(list);
}

void SpdySessionPool::OnIPAddressChanged() {
  WeakSessionList current_sessions = GetCurrentSessions();
  for (WeakSessionList::const_iterator it = current_sessions.begin();
       it != current_sessions.end(); ++it) {
    if (!*it)
      continue;

// For OSs that terminate TCP connections upon relevant network changes,
// attempt to preserve active streams by marking all sessions as going
// away, rather than explicitly closing them. Streams may still fail due
// to a generated TCP reset.
#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
    (*it)->MakeUnavailable();
    (*it)->StartGoingAway(kLastStreamId, ERR_NETWORK_CHANGED);
    (*it)->MaybeFinishGoingAway();
#else
    (*it)->CloseSessionOnError(ERR_NETWORK_CHANGED,
                               "Closing current sessions.");
    DCHECK((*it)->IsDraining());
#endif  // defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
    DCHECK(!IsSessionAvailable(*it));
  }
}

void SpdySessionPool::OnSSLConfigChanged() {
  CloseCurrentSessions(ERR_NETWORK_CHANGED);
}

void SpdySessionPool::OnCertDBChanged() {
  CloseCurrentSessions(ERR_CERT_DATABASE_CHANGED);
}

void SpdySessionPool::RemoveRequestForSpdySession(SpdySessionRequest* request) {
  DCHECK_EQ(this, request->spdy_session_pool());

  const SpdySessionKey& spdy_session_key = request->key();
  // Resume all pending requests now that |request| is done/canceled.
  if (spdy_session_pending_request_map_.find(spdy_session_key) !=
      spdy_session_pending_request_map_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpdySessionPool::UpdatePendingRequests,
                       weak_ptr_factory_.GetWeakPtr(), spdy_session_key));
  }

  auto iter = spdy_session_request_map_.find(spdy_session_key);
  DCHECK(iter != spdy_session_request_map_.end());
  RequestSet& request_set = iter->second;
  DCHECK(base::ContainsKey(request_set, request));
  RemoveRequestInternal(iter, request_set.find(request));
}

void SpdySessionPool::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {
  if (sessions_.empty())
    return;
  size_t total_size = 0;
  size_t buffer_size = 0;
  size_t cert_count = 0;
  size_t cert_size = 0;
  size_t num_active_sessions = 0;
  for (auto* session : sessions_) {
    StreamSocket::SocketMemoryStats stats;
    bool is_session_active = false;
    total_size += session->DumpMemoryStats(&stats, &is_session_active);
    buffer_size += stats.buffer_size;
    cert_count += stats.cert_count;
    cert_size += stats.cert_size;
    if (is_session_active)
      num_active_sessions++;
  }
  total_size +=
      base::trace_event::EstimateMemoryUsage(spdy::ObtainHpackHuffmanTable()) +
      base::trace_event::EstimateMemoryUsage(spdy::ObtainHpackStaticTable()) +
      base::trace_event::EstimateMemoryUsage(push_promise_index_);
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(base::StringPrintf(
          "%s/spdy_session_pool", parent_dump_absolute_name.c_str()));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  total_size);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  sessions_.size());
  dump->AddScalar("active_session_count",
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  num_active_sessions);
  dump->AddScalar("buffer_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  buffer_size);
  dump->AddScalar("cert_count",
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  cert_count);
  dump->AddScalar("cert_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  cert_size);
}

bool SpdySessionPool::IsSessionAvailable(
    const base::WeakPtr<SpdySession>& session) const {
  for (auto it = available_sessions_.begin(); it != available_sessions_.end();
       ++it) {
    if (it->second.get() == session.get())
      return true;
  }
  return false;
}

void SpdySessionPool::MapKeyToAvailableSession(
    const SpdySessionKey& key,
    const base::WeakPtr<SpdySession>& session) {
  DCHECK(base::ContainsKey(sessions_, session.get()));
  std::pair<AvailableSessionMap::iterator, bool> result =
      available_sessions_.insert(std::make_pair(key, session));
  CHECK(result.second);
}

SpdySessionPool::AvailableSessionMap::iterator
SpdySessionPool::LookupAvailableSessionByKey(
    const SpdySessionKey& key) {
  return available_sessions_.find(key);
}

void SpdySessionPool::UnmapKey(const SpdySessionKey& key) {
  auto it = LookupAvailableSessionByKey(key);
  CHECK(it != available_sessions_.end());
  available_sessions_.erase(it);
}

void SpdySessionPool::RemoveAliases(const SpdySessionKey& key) {
  // Walk the aliases map, find references to this pair.
  // TODO(mbelshe):  Figure out if this is too expensive.
  for (auto it = aliases_.begin(); it != aliases_.end();) {
    if (it->second == key) {
      auto old_it = it;
      ++it;
      aliases_.erase(old_it);
    } else {
      ++it;
    }
  }
}

SpdySessionPool::WeakSessionList SpdySessionPool::GetCurrentSessions() const {
  WeakSessionList current_sessions;
  for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
    current_sessions.push_back((*it)->GetWeakPtr());
  }
  return current_sessions;
}

void SpdySessionPool::CloseCurrentSessionsHelper(Error error,
                                                 const std::string& description,
                                                 bool idle_only) {
  WeakSessionList current_sessions = GetCurrentSessions();
  for (base::WeakPtr<SpdySession>& session : current_sessions) {
    if (!session)
      continue;

    if (idle_only && session->is_active())
      continue;

    if (session->IsDraining())
      continue;

    session->CloseSessionOnError(error, description);

    DCHECK(!IsSessionAvailable(session));
    DCHECK(!session || session->IsDraining());
  }
}

std::unique_ptr<SpdySession> SpdySessionPool::CreateSession(
    const SpdySessionKey& key,
    bool is_trusted_proxy,
    NetLog* net_log) {
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet", IMPORTED_FROM_SOCKET,
                            SPDY_SESSION_GET_MAX);

  return std::make_unique<SpdySession>(
      key, http_server_properties_, transport_security_state_,
      ssl_config_service_, quic_supported_versions_,
      enable_sending_initial_data_, enable_ping_based_connection_checking_,
      support_ietf_format_quic_altsvc_, is_trusted_proxy,
      session_max_recv_window_size_, initial_settings_, greased_http2_frame_,
      time_func_, push_delegate_, network_quality_estimator_, net_log);
}

base::WeakPtr<SpdySession> SpdySessionPool::InsertSession(
    const SpdySessionKey& key,
    std::unique_ptr<SpdySession> new_session,
    const NetLogWithSource& source_net_log) {
  base::WeakPtr<SpdySession> available_session = new_session->GetWeakPtr();
  sessions_.insert(new_session.release());
  MapKeyToAvailableSession(key, available_session);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SpdySessionPool::UpdatePendingRequests,
                                weak_ptr_factory_.GetWeakPtr(), key));

  source_net_log.AddEvent(
      NetLogEventType::HTTP2_SESSION_POOL_IMPORTED_SESSION_FROM_SOCKET,
      available_session->net_log().source().ToEventParametersCallback());

  // Look up the IP address for this session so that we can match
  // future sessions (potentially to different domains) which can
  // potentially be pooled with this one. Because GetPeerAddress()
  // reports the proxy's address instead of the origin server, check
  // to see if this is a direct connection.
  if (key.proxy_server().is_direct()) {
    IPEndPoint address;
    if (available_session->GetPeerAddress(&address) == OK)
      aliases_.insert(AliasMap::value_type(address, key));
  }

  return available_session;
}

void SpdySessionPool::UpdatePendingRequests(const SpdySessionKey& key) {
  auto it = LookupAvailableSessionByKey(key);
  if (it != available_sessions_.end()) {
    base::WeakPtr<SpdySession> new_session = it->second->GetWeakPtr();
    bool is_pooled = (key != new_session->spdy_session_key());
    while (new_session && new_session->IsAvailable()) {
      // Each iteration may empty out the RequestSet for |spdy_session_key| in
      // |spdy_session_request_map_|. So each time, check for RequestSet and use
      // the first one. Could just keep track if the last iteration removed the
      // final request, but it's possible that responding to one request will
      // result in cancelling another one.
      //
      // TODO(willchan): If it's important, switch RequestSet out for a FIFO
      // queue (Order by priority first, then FIFO within same priority).
      // Unclear that it matters here.
      auto iter = spdy_session_request_map_.find(key);
      if (iter == spdy_session_request_map_.end())
        break;
      RequestSet* request_set = &iter->second;
      // Find a request that can use the socket, if any.
      RequestSet::iterator request;
      for (request = request_set->begin(); request != request_set->end();
           ++request) {
        // If the request is for use with websockets, and the session doesn't
        // support websockets, skip over the request.
        if ((*request)->is_websocket() && !new_session->support_websocket())
          continue;
        // Don't use IP pooled session if not allowed.
        if (!(*request)->enable_ip_based_pooling() && is_pooled)
          continue;
        break;
      }
      if (request == request_set->end())
        break;

      SpdySessionRequest::Delegate* delegate = (*request)->delegate();
      RemoveRequestInternal(iter, request);
      delegate->OnSpdySessionAvailable(new_session);
    }
  }

  auto iter = spdy_session_pending_request_map_.find(key);
  if (iter == spdy_session_pending_request_map_.end())
    return;
  // Remove all pending requests, if there are any. As a result, if one of these
  // callbacks triggers a new RequestSession() call,
  // |is_first_request_for_session| will be true.
  std::list<base::RepeatingClosure> pending_requests = std::move(iter->second);
  spdy_session_pending_request_map_.erase(iter);

  // Resume any pending requests. This needs to be after the
  // OnSpdySessionAvailable() calls, to prevent requests from calling into the
  // socket pools in cases where that's not necessary.
  for (auto callback : pending_requests) {
    callback.Run();
  }
}

void SpdySessionPool::RemoveRequestInternal(
    SpdySessionRequestMap::iterator request_map_iterator,
    RequestSet::iterator request_set_iterator) {
  SpdySessionRequest* request = *request_set_iterator;
  request_map_iterator->second.erase(request_set_iterator);
  if (request_map_iterator->second.empty())
    spdy_session_request_map_.erase(request_map_iterator);
  request->OnRemovedFromPool();
}

}  // namespace net
