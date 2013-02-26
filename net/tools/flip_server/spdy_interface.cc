// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/spdy_interface.h"

#include <algorithm>
#include <string>

#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/dump_cache/url_utilities.h"
#include "net/tools/flip_server/constants.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/http_interface.h"
#include "net/tools/flip_server/spdy_util.h"

namespace net {

// static
std::string SpdySM::forward_ip_header_;

class SpdyFrameDataFrame : public DataFrame {
 public:
  SpdyFrameDataFrame(SpdyFrame* spdy_frame)
    : frame(spdy_frame) {
    data = spdy_frame->data();
    size = spdy_frame->size();
  }

  virtual ~SpdyFrameDataFrame() {
    delete frame;
  }

  const SpdyFrame* frame;
};

SpdySM::SpdySM(SMConnection* connection,
               SMInterface* sm_http_interface,
               EpollServer* epoll_server,
               MemoryCache* memory_cache,
               FlipAcceptor* acceptor)
    : buffered_spdy_framer_(new BufferedSpdyFramer(2, true)),
      valid_spdy_session_(false),
      connection_(connection),
      client_output_list_(connection->output_list()),
      client_output_ordering_(connection),
      next_outgoing_stream_id_(2),
      epoll_server_(epoll_server),
      acceptor_(acceptor),
      memory_cache_(memory_cache),
      close_on_error_(false) {
  buffered_spdy_framer_->set_visitor(this);
}

SpdySM::~SpdySM() {
  delete buffered_spdy_framer_;
}

void SpdySM::InitSMConnection(SMConnectionPoolInterface* connection_pool,
                              SMInterface* sm_interface,
                              EpollServer* epoll_server,
                              int fd,
                              std::string server_ip,
                              std::string server_port,
                              std::string remote_ip,
                              bool use_ssl) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT
          << "SpdySM: Initializing server connection.";
  connection_->InitSMConnection(connection_pool, sm_interface,
                                epoll_server, fd, server_ip, server_port,
                                remote_ip, use_ssl);
}

SMInterface* SpdySM::NewConnectionInterface() {
  SMConnection* server_connection =
    SMConnection::NewSMConnection(epoll_server_,
                                  NULL,
                                  memory_cache_,
                                  acceptor_,
                                  "http_conn: ");
  if (server_connection == NULL) {
    LOG(ERROR) << "SpdySM: Could not create server connection";
    return NULL;
  }
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Creating new HTTP interface";
  SMInterface *sm_http_interface = new HttpSM(server_connection,
                                              this,
                                              epoll_server_,
                                              memory_cache_,
                                              acceptor_);
  return sm_http_interface;
}

SMInterface* SpdySM::FindOrMakeNewSMConnectionInterface(
    std::string server_ip, std::string server_port) {
  SMInterface *sm_http_interface;
  int32 server_idx;
  if (unused_server_interface_list.empty()) {
    sm_http_interface = NewConnectionInterface();
    server_idx = server_interface_list.size();
    server_interface_list.push_back(sm_http_interface);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT
            << "SpdySM: Making new server connection on index: "
            << server_idx;
  } else {
    server_idx = unused_server_interface_list.back();
    unused_server_interface_list.pop_back();
    sm_http_interface = server_interface_list.at(server_idx);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Reusing connection on "
            << "index: " << server_idx;
  }

  sm_http_interface->InitSMInterface(this, server_idx);
  sm_http_interface->InitSMConnection(NULL, sm_http_interface,
                                      epoll_server_, -1,
                                      server_ip, server_port, "", false);

  return sm_http_interface;
}

int SpdySM::SpdyHandleNewStream(
    SpdyStreamId stream_id,
    SpdyPriority priority,
    const SpdyHeaderBlock& headers,
    std::string &http_data,
    bool* is_https_scheme) {
  *is_https_scheme = false;
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnSyn("
          << stream_id << ")";
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: # headers: "
          << headers.size();

  SpdyHeaderBlock::const_iterator url = headers.find("url");
  SpdyHeaderBlock::const_iterator method = headers.find("method");
  if (url == headers.end() || method == headers.end()) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: didn't find method or url "
            << "or method. Not creating stream";
    return 0;
  }

  SpdyHeaderBlock::const_iterator scheme = headers.find("scheme");
  if (scheme->second.compare("https") == 0) {
    *is_https_scheme = true;
  }

  // url->second here only ever seems to contain just the path. When this
  // path contains a query string with a http:// in one of its values,
  // UrlUtilities::GetUrlPath will fail and always return a / breaking
  // the request. GetUrlPath assumes the absolute URL is being passed in.
  std::string uri;
  if (url->second.compare(0,4,"http") == 0)
    uri = UrlUtilities::GetUrlPath(url->second);
  else
    uri = std::string(url->second);
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_SPDY_SERVER) {
    std::string host = UrlUtilities::GetUrlHost(url->second);
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Request: " << method->second
            << " " << uri;
    std::string filename = EncodeURL(uri, host, method->second);
    NewStream(stream_id, priority, filename);
  } else {
    SpdyHeaderBlock::const_iterator version = headers.find("version");
    http_data += method->second + " " + uri + " " + version->second + "\r\n";
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Request: " << method->second << " "
            << uri << " " << version->second;
    for (SpdyHeaderBlock::const_iterator i = headers.begin();
         i != headers.end(); ++i) {
      http_data += i->first + ": " + i->second + "\r\n";
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << i->first.c_str() << ":"
              << i->second.c_str();
    }
    if (forward_ip_header_.length()) {
      // X-Client-Cluster-IP header
      http_data += forward_ip_header_ + ": " +
                    connection_->client_ip() + "\r\n";
    }
    http_data += "\r\n";
  }

  VLOG(3) << ACCEPTOR_CLIENT_IDENT << "SpdySM: HTTP Request:\n" << http_data;
  return 1;
}

void SpdySM::OnStreamFrameData(SpdyStreamId stream_id,
                               const char* data,
                               size_t len,
                               SpdyDataFlags flags) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: StreamData(" << stream_id
          << ", [" << len << "])";
  StreamToSmif::iterator it = stream_to_smif_.find(stream_id);
  if (it == stream_to_smif_.end()) {
    VLOG(2) << "Dropping frame from unknown stream " << stream_id;
    if (!valid_spdy_session_)
      close_on_error_ = true;
    return;
  }

  SMInterface* interface = it->second;
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY)
    interface->ProcessWriteInput(data, len);
}

void SpdySM::OnSynStream(SpdyStreamId stream_id,
                         SpdyStreamId associated_stream_id,
                         SpdyPriority priority,
                         uint8 credential_slot,
                         bool fin,
                         bool unidirectional,
                         const SpdyHeaderBlock& headers) {
  std::string http_data;
  bool is_https_scheme;
  int ret = SpdyHandleNewStream(stream_id, priority, headers, http_data,
                                &is_https_scheme);
  if (!ret) {
    LOG(ERROR) << "SpdySM: Could not convert spdy into http.";
    return;
  }
  // We've seen a valid looking SYN_STREAM, consider this to have
  // been a real spdy session.
  valid_spdy_session_ = true;

  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
    std::string server_ip;
    std::string server_port;
    if (is_https_scheme) {
      server_ip = acceptor_->https_server_ip_;
      server_port = acceptor_->https_server_port_;
    } else {
      server_ip = acceptor_->http_server_ip_;
      server_port = acceptor_->http_server_port_;
    }
    SMInterface* sm_http_interface =
        FindOrMakeNewSMConnectionInterface(server_ip, server_port);
    stream_to_smif_[stream_id] = sm_http_interface;
    sm_http_interface->SetStreamID(stream_id);
    sm_http_interface->ProcessWriteInput(http_data.c_str(),
                                         http_data.size());
  }
}

void SpdySM::OnSynReply(SpdyStreamId stream_id,
                        bool fin,
                        const SpdyHeaderBlock& headers) {
  // TODO(willchan): if there is an error parsing headers, we
  // should send a RST_STREAM.
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnSynReply("
          << stream_id << ")";
}

void SpdySM::OnHeaders(SpdyStreamId stream_id,
                       bool fin,
                       const SpdyHeaderBlock& headers) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnHeaders("
          << stream_id << ")";
}

void SpdySM::OnRstStream(SpdyStreamId stream_id,
                         SpdyRstStreamStatus status) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnRstStream("
          << stream_id << ")";
  client_output_ordering_.RemoveStreamId(stream_id);
}

size_t SpdySM::ProcessReadInput(const char* data, size_t len) {
  return buffered_spdy_framer_->ProcessInput(data, len);
}

size_t SpdySM::ProcessWriteInput(const char* data, size_t len) {
  return 0;
}

bool SpdySM::MessageFullyRead() const {
  return buffered_spdy_framer_->MessageFullyRead();
}

bool SpdySM::Error() const {
  return close_on_error_ || buffered_spdy_framer_->HasError();
}

const char* SpdySM::ErrorAsString() const {
  DCHECK(Error());
  return SpdyFramer::ErrorCodeToString(buffered_spdy_framer_->error_code());
}

void SpdySM::ResetForNewInterface(int32 server_idx) {
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Reset for new interface: "
          << "server_idx: " << server_idx;
  unused_server_interface_list.push_back(server_idx);
}

void SpdySM::ResetForNewConnection() {
  // seq_num is not cleared, intentionally.
  delete buffered_spdy_framer_;
  buffered_spdy_framer_ = new BufferedSpdyFramer(2, true);
  buffered_spdy_framer_->set_visitor(this);
  valid_spdy_session_ = false;
  client_output_ordering_.Reset();
  next_outgoing_stream_id_ = 2;
}

// Send a settings frame
int SpdySM::PostAcceptHook() {
  SettingsMap settings;
  settings[SETTINGS_MAX_CONCURRENT_STREAMS] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, 100);
  SpdyFrame* settings_frame =
      buffered_spdy_framer_->CreateSettings(settings);

  VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Sending Settings Frame";
  EnqueueDataFrame(new SpdyFrameDataFrame(settings_frame));
  return 1;
}

void SpdySM::NewStream(uint32 stream_id,
                       uint32 priority,
                       const std::string& filename) {
  MemCacheIter mci;
  mci.stream_id = stream_id;
  mci.priority = priority;
  if (acceptor_->flip_handler_type_ == FLIP_HANDLER_SPDY_SERVER) {
    if (!memory_cache_->AssignFileData(filename, &mci)) {
      // error creating new stream.
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Sending ErrorNotFound";
      SendErrorNotFound(stream_id);
    } else {
      AddToOutputOrder(mci);
    }
  } else {
    AddToOutputOrder(mci);
  }
}

void SpdySM::AddToOutputOrder(const MemCacheIter& mci) {
  client_output_ordering_.AddToOutputOrder(mci);
}

void SpdySM::SendEOF(uint32 stream_id) {
  SendEOFImpl(stream_id);
}

void SpdySM::SendErrorNotFound(uint32 stream_id) {
  SendErrorNotFoundImpl(stream_id);
}

void SpdySM::SendOKResponse(uint32 stream_id, std::string* output) {
  SendOKResponseImpl(stream_id, output);
}

size_t SpdySM::SendSynStream(uint32 stream_id, const BalsaHeaders& headers) {
  return SendSynStreamImpl(stream_id, headers);
}

size_t SpdySM::SendSynReply(uint32 stream_id, const BalsaHeaders& headers) {
  return SendSynReplyImpl(stream_id, headers);
}

void SpdySM::SendDataFrame(uint32 stream_id, const char* data, int64 len,
                   uint32 flags, bool compress) {
  SpdyDataFlags spdy_flags = static_cast<SpdyDataFlags>(flags);
  SendDataFrameImpl(stream_id, data, len, spdy_flags, compress);
}

void SpdySM::SendEOFImpl(uint32 stream_id) {
  SendDataFrame(stream_id, NULL, 0, DATA_FLAG_FIN, false);
  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending EOF: " << stream_id;
  KillStream(stream_id);
  stream_to_smif_.erase(stream_id);
}

void SpdySM::SendErrorNotFoundImpl(uint32 stream_id) {
  BalsaHeaders my_headers;
  my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "404", "Not Found");
  SendSynReplyImpl(stream_id, my_headers);
  SendDataFrame(stream_id, "wtf?", 4, DATA_FLAG_FIN, false);
  client_output_ordering_.RemoveStreamId(stream_id);
}

void SpdySM::SendOKResponseImpl(uint32 stream_id, std::string* output) {
  BalsaHeaders my_headers;
  my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "200", "OK");
  SendSynReplyImpl(stream_id, my_headers);
  SendDataFrame(
      stream_id, output->c_str(), output->size(), DATA_FLAG_FIN, false);
  client_output_ordering_.RemoveStreamId(stream_id);
}

void SpdySM::KillStream(uint32 stream_id) {
  client_output_ordering_.RemoveStreamId(stream_id);
}

void SpdySM::CopyHeaders(SpdyHeaderBlock& dest, const BalsaHeaders& headers) {
  for (BalsaHeaders::const_header_lines_iterator hi =
       headers.header_lines_begin();
       hi != headers.header_lines_end();
       ++hi) {
    // It is illegal to send SPDY headers with empty value or header
    // names.
    if (!hi->first.length() || !hi->second.length())
      continue;

    // Key must be all lower case in SPDY headers.
    std::string key = hi->first.as_string();
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    SpdyHeaderBlock::iterator fhi = dest.find(key);
    if (fhi == dest.end()) {
      dest[key] = hi->second.as_string();
    } else {
      dest[key] = (
          std::string(fhi->second.data(), fhi->second.size()) + "\0" +
          std::string(hi->second.data(), hi->second.size()));
    }
  }

  // These headers have no value
  dest.erase("X-Associated-Content");  // TODO(mbelshe): case-sensitive
  dest.erase("X-Original-Url");  // TODO(mbelshe): case-sensitive
}

size_t SpdySM::SendSynStreamImpl(uint32 stream_id,
                                 const BalsaHeaders& headers) {
  SpdyHeaderBlock block;
  block["method"] = headers.request_method().as_string();
  if (!headers.HasHeader("status"))
    block["status"] = headers.response_code().as_string();
  if (!headers.HasHeader("version"))
    block["version"] =headers.response_version().as_string();
  if (headers.HasHeader("X-Original-Url")) {
    std::string original_url = headers.GetHeader("X-Original-Url").as_string();
    block["path"] = UrlUtilities::GetUrlPath(original_url);
  } else {
    block["path"] = headers.request_uri().as_string();
  }
  CopyHeaders(block, headers);

  SpdyFrame* fsrcf = buffered_spdy_framer_->CreateSynStream(
      stream_id, 0, 0, 0, CONTROL_FLAG_NONE, true, &block);
  size_t df_size = fsrcf->size();
  EnqueueDataFrame(new SpdyFrameDataFrame(fsrcf));

  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending SynStreamheader "
          << stream_id;
  return df_size;
}

size_t SpdySM::SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers) {
  SpdyHeaderBlock block;
  CopyHeaders(block, headers);
  block["status"] = headers.response_code().as_string() + " " +
                    headers.response_reason_phrase().as_string();
  block["version"] = headers.response_version().as_string();

  SpdyFrame* fsrcf = buffered_spdy_framer_->CreateSynReply(
      stream_id, CONTROL_FLAG_NONE, true, &block);
  size_t df_size = fsrcf->size();
  EnqueueDataFrame(new SpdyFrameDataFrame(fsrcf));

  VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending SynReplyheader "
          << stream_id;
  return df_size;
}

void SpdySM::SendDataFrameImpl(uint32 stream_id, const char* data, int64 len,
                       SpdyDataFlags flags, bool compress) {
  // TODO(mbelshe):  We can't compress here - before going into the
  //                 priority queue.  Compression needs to be done
  //                 with late binding.
  if (len == 0) {
    SpdyFrame* fdf = buffered_spdy_framer_->CreateDataFrame(
        stream_id, data, len, flags);
    EnqueueDataFrame(new SpdyFrameDataFrame(fdf));
    return;
  }

  // Chop data frames into chunks so that one stream can't monopolize the
  // output channel.
  while (len > 0) {
    int64 size = std::min(len, static_cast<int64>(kSpdySegmentSize));
    SpdyDataFlags chunk_flags = flags;

    // If we chunked this block, and the FIN flag was set, there is more
    // data coming.  So, remove the flag.
    if ((size < len) && (flags & DATA_FLAG_FIN))
      chunk_flags = static_cast<SpdyDataFlags>(chunk_flags & ~DATA_FLAG_FIN);

    SpdyFrame* fdf = buffered_spdy_framer_->CreateDataFrame(
        stream_id, data, size, chunk_flags);
    EnqueueDataFrame(new SpdyFrameDataFrame(fdf));

    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending data frame "
            << stream_id << " [" << size << "] shrunk to "
            << (fdf->size() - kSpdyOverhead) << ", flags=" << flags;

    data += size;
    len -= size;
  }
}

void SpdySM::EnqueueDataFrame(DataFrame* df) {
  connection_->EnqueueDataFrame(df);
}

void SpdySM::GetOutput() {
  while (client_output_list_->size() < 2) {
    MemCacheIter* mci = client_output_ordering_.GetIter();
    if (mci == NULL) {
      VLOG(2) << ACCEPTOR_CLIENT_IDENT
              << "SpdySM: GetOutput: nothing to output!?";
      return;
    }
    if (!mci->transformed_header) {
      mci->transformed_header = true;
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: GetOutput transformed "
              << "header stream_id: [" << mci->stream_id << "]";
      if ((mci->stream_id % 2) == 0) {
        // this is a server initiated stream.
        // Ideally, we'd do a 'syn-push' here, instead of a syn-reply.
        BalsaHeaders headers;
        headers.CopyFrom(*(mci->file_data->headers));
        headers.ReplaceOrAppendHeader("status", "200");
        headers.ReplaceOrAppendHeader("version", "http/1.1");
        headers.SetRequestFirstlineFromStringPieces("PUSH",
                                                    mci->file_data->filename,
                                                    "");
        mci->bytes_sent = SendSynStream(mci->stream_id, headers);
      } else {
        BalsaHeaders headers;
        headers.CopyFrom(*(mci->file_data->headers));
        mci->bytes_sent = SendSynReply(mci->stream_id, headers);
      }
      return;
    }
    if (mci->body_bytes_consumed >= mci->file_data->body.size()) {
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: GetOutput "
              << "remove_stream_id: [" << mci->stream_id << "]";
      SendEOF(mci->stream_id);
      return;
    }
    size_t num_to_write =
      mci->file_data->body.size() - mci->body_bytes_consumed;
    if (num_to_write > mci->max_segment_size)
      num_to_write = mci->max_segment_size;

    bool should_compress = false;
    if (!mci->file_data->headers->HasHeader("content-encoding")) {
      if (mci->file_data->headers->HasHeader("content-type")) {
        std::string content_type =
            mci->file_data->headers->GetHeader("content-type").as_string();
        if (content_type.find("image") == content_type.npos)
          should_compress = true;
      }
    }

    SendDataFrame(mci->stream_id,
                  mci->file_data->body.data() + mci->body_bytes_consumed,
                  num_to_write, 0, should_compress);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: GetOutput SendDataFrame["
            << mci->stream_id << "]: " << num_to_write;
    mci->body_bytes_consumed += num_to_write;
    mci->bytes_sent += num_to_write;
  }
}

}  // namespace net
