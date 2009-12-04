// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <linux/tcp.h>  // For TCP_NODELAY
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <deque>
#include <iostream>
#include <limits>
#include <vector>
#include <list>

#include "base/google.h"
#include "base/logging.h"

// Used to get time. Can be replaced.
#include "base/timer.h"

// All "open-sourcable"
#include "net/flip/flip_frame_builder.h"
#include "net/flip/flip_framer.h"
#include "net/flip/flip_protocol.h"
#include "net/tools/flip_server/balsa_enums.h"
#include "net/tools/flip_server/balsa_frame.h"
#include "net/tools/flip_server/balsa_headers.h"
#include "net/tools/flip_server/balsa_visitor_interface.h"
#include "net/tools/flip_server/buffer_interface.h"
#include "net/tools/flip_server/create_listener.h"
#include "net/tools/flip_server/epoll_server.h"
#include "net/tools/flip_server/loadtime_measurement.h"
#include "net/tools/flip_server/ring_buffer.h"
#include "net/tools/flip_server/simple_buffer.h"
#include "net/tools/flip_server/url_to_filename_encoder.h"
#include "net/tools/flip_server/url_utilities.h"
#include "strings/split.h"
#include "thread/thread.h"
#include "third_party/openssl/ssl.h"

////////////////////////////////////////////////////////////////////////////////

DEFINE_bool(use_ssl, true,
            "If set to true, then the server will act as an SSL server for both"
            " HTTP and FLIP");
DEFINE_string(ssl_cert_name, "cert.pem", "The name of the cert .pem file");
DEFINE_string(ssl_key_name, "key.pem", "The name of the key .pem file");
DEFINE_int32(response_count_until_close, 1000 * 1000,
             "The number of responses given before the server closes the"
             " connection");
DEFINE_bool(no_nagle, true, "If true, then disables the nagle algorithm");
DEFINE_int32(accepts_per_wake, 0,
             "The number of times that accept() will be called when the "
             " alarm goes off when the accept_using_alarm flag is set to true."
             " If set to 0, accept() will be performed until the accept queue"
             " is completely drained and the accept() call returns an error");
DEFINE_int32(flip_port, 10040, "The port on which the flip server listens");
DEFINE_int32(port, 16002, "The port on which the http server listens");
DEFINE_int32(accept_backlog_size, 1024,
             "The size of the TCP accept backlog");
DEFINE_string(cache_base_dir, ".", "The directory where cache locates");
DEFINE_bool(need_to_encode_url, true, "If true, then encode url to filename");
DEFINE_bool(reuseport, false,
            "If set to false a single socket will be used. If set to true"
            " then a new socket will be created for each accept thread."
            " Note that this only works with kernels that support"
            " SO_REUSEPORT");

DEFINE_double(server_think_time_in_s, 0,
              " The amount of time the server delays before sending back the"
              "reply");
DEFINE_bool(use_xsub, false,
              "Does the server send X-Subresource headers");
DEFINE_bool(use_xac, false,
              "Does the server send X-Associated-Content headers");
DEFINE_bool(use_cwnd_opener, false,
              "Does the server advance cwnd by sending no-op packets");
DEFINE_bool(use_compression, false,
              "Does the server compress data frames");

DEFINE_string(urls_file, "experimental/users/fenix/flip/urls.txt",
              "The path to the urls file which includes the urls for testing");
DEFINE_string(pageload_html_file,
              "experimental/users/fenix/flip/loadtime_measurement.html",
              "The path to the html that does the pageload in iframe");
DEFINE_bool(record_mode, false,
            "If set to true, record requests in file named as fd used");
DEFINE_string(record_path, ".", "The path to save the record files");


////////////////////////////////////////////////////////////////////////////////

using gfe_flip::CONTROL_FLAG_NONE;
using gfe_flip::DATA_FLAG_COMPRESSED;
using gfe_flip::DATA_FLAG_FIN;
using gfe_flip::FIN_STREAM;
using gfe_flip::FlipControlFrame;
using gfe_flip::FlipFrame;
using gfe_flip::FlipDataFlags;
using gfe_flip::FlipDataFrame;
using gfe_flip::FlipFrameBuilder;
using gfe_flip::FlipFramer;
using gfe_flip::FlipFramerVisitorInterface;
using gfe_flip::FlipHeaderBlock;
using gfe_flip::FlipFinStreamControlFrame;
using gfe_flip::FlipStreamId;
using gfe_flip::FlipSynReplyControlFrame;
using gfe_flip::FlipSynStreamControlFrame;
using gfe_flip::SYN_REPLY;
using gfe_flip::SYN_STREAM;
using gfe2::BalsaFrame;
using gfe2::BalsaFrameEnums;
using gfe2::BalsaHeaders;
using gfe2::BalsaHeadersEnums;
using gfe2::BalsaVisitorInterface;
using gfe2::EpollAlarmCallbackInterface;
using gfe2::EpollCallbackInterface;
using gfe2::EpollEvent;
using gfe2::EpollServer;
using gfe2::RingBuffer;
using gfe2::SimpleBuffer;
using net::UrlUtilities;

////////////////////////////////////////////////////////////////////////////////

// Creates a socket with domain, type and protocol parameters.
// Assigns the return value of socket() to *fd.
// Returns errno if an error occurs, else returns zero.
int CreateSocket(int domain, int type, int protocol, int *fd) {
  CHECK(fd != NULL);
  *fd = ::socket(domain, type, protocol);
  return (*fd == -1) ? errno : 0;
}

////////////////////////////////////////////////////////////////////////////////

// Sets an FD to be nonblocking.
void SetNonBlocking(int fd) {
  DCHECK(fd >= 0);

  int fcntl_return = fcntl(fd, F_GETFL, 0);
  CHECK_NE(fcntl_return, -1)
    << "error doing fcntl(fd, F_GETFL, 0) fd: " << fd
    << " errno=" << errno;

  if (fcntl_return & O_NONBLOCK)
    return;

  fcntl_return = fcntl(fd, F_SETFL, fcntl_return | O_NONBLOCK);
  CHECK_NE(fcntl_return, -1)
    << "error doing fcntl(fd, F_SETFL, fcntl_return) fd: " << fd
    << " errno=" << errno;
}

////////////////////////////////////////////////////////////////////////////////

LoadtimeMeasurement global_loadtime_measurement(FLAGS_urls_file,
                                                FLAGS_pageload_html_file);

////////////////////////////////////////////////////////////////////////////////

struct GlobalSSLState {
  SSL_METHOD* ssl_method;
  SSL_CTX* ssl_ctx;
};

////////////////////////////////////////////////////////////////////////////////

GlobalSSLState* global_ssl_state = NULL;

////////////////////////////////////////////////////////////////////////////////

// SSL stuff
void flip_init_ssl(GlobalSSLState* state) {
  SSL_library_init();
  SSL_load_error_strings();

  state->ssl_method = TLSv1_server_method();
  state->ssl_ctx = SSL_CTX_new(state->ssl_method);
  if (!state->ssl_ctx) {
    LOG(FATAL) << "Unable to create SSL context";
  }
  if (SSL_CTX_use_certificate_file(state->ssl_ctx,
                                   FLAGS_ssl_cert_name.c_str(),
                                   SSL_FILETYPE_PEM) <= 0) {
    LOG(FATAL) << "Unable to use cert.pem as SSL cert.";
  }
  if (SSL_CTX_use_PrivateKey_file(state->ssl_ctx,
                                  FLAGS_ssl_key_name.c_str(),
                                  SSL_FILETYPE_PEM) <= 0) {
    LOG(FATAL) << "Unable to use key.pem as SSL key.";
  }
  if (!SSL_CTX_check_private_key(state->ssl_ctx)) {
    LOG(FATAL) << "The cert.pem and key.pem files don't match";
  }
}

SSL* flip_new_ssl(SSL_CTX* ssl_ctx) {
  SSL* ssl = SSL_new(ssl_ctx);
  SSL_set_accept_state(ssl);
  return ssl;
}

////////////////////////////////////////////////////////////////////////////////

const int kInitialDataSendersThreshold = 128;
const int kNormalSegmentSize = (4 * 1460) - FlipFrame::size();

////////////////////////////////////////////////////////////////////////////////

class DataFrame {
 public:
  const char* data;
  size_t size;
  bool delete_when_done;
  size_t index;
  DataFrame() : data(NULL), size(0), delete_when_done(false), index(0) {}
  void MaybeDelete() {
    if (delete_when_done) {
      delete[] data;
    }
  }
};

////////////////////////////////////////////////////////////////////////////////

class StoreBodyAndHeadersVisitor: public BalsaVisitorInterface {
 public:
  BalsaHeaders headers;
  string body;
  bool error_;

  virtual void ProcessBodyInput(const char *input, size_t size) {}
  virtual void ProcessBodyData(const char *input, size_t size) {
    body.append(input, size);
  }
  virtual void ProcessHeaderInput(const char *input, size_t size) {}
  virtual void ProcessTrailerInput(const char *input, size_t size) {}
  virtual void ProcessHeaders(const BalsaHeaders& headers) {
    // nothing to do here-- we're assuming that the BalsaFrame has
    // been handed our headers.
  }
  virtual void ProcessRequestFirstLine(const char* line_input,
                                       size_t line_length,
                                       const char* method_input,
                                       size_t method_length,
                                       const char* request_uri_input,
                                       size_t request_uri_length,
                                       const char* version_input,
                                       size_t version_length) {}
  virtual void ProcessResponseFirstLine(const char *line_input,
                                        size_t line_length,
                                        const char *version_input,
                                        size_t version_length,
                                        const char *status_input,
                                        size_t status_length,
                                        const char *reason_input,
                                        size_t reason_length) {}
  virtual void ProcessChunkLength(size_t chunk_length) {}
  virtual void ProcessChunkExtensions(const char *input, size_t size) {}
  virtual void HeaderDone() {}
  virtual void MessageDone() {}
  virtual void HandleHeaderError(BalsaFrame* framer) { HandleError(); }
  virtual void HandleHeaderWarning(BalsaFrame* framer) { HandleError(); }
  virtual void HandleChunkingError(BalsaFrame* framer) { HandleError(); }
  virtual void HandleBodyError(BalsaFrame* framer) { HandleError(); }

  void HandleError() { error_ = true; }
};

////////////////////////////////////////////////////////////////////////////////

struct FileData {
  void CopyFrom(const FileData& file_data) {
    headers = new BalsaHeaders;
    headers->CopyFrom(*(file_data.headers));
    filename = file_data.filename;
    related_files = file_data.related_files;
    body = file_data.body;
  }
  FileData(BalsaHeaders* h, const string& b) : headers(h), body(b) {}
  FileData() {}
  BalsaHeaders* headers;
  string filename;
  vector< pair<int, string> > related_files;   // priority, filename
  string body;
};

////////////////////////////////////////////////////////////////////////////////

class MemCacheIter {
 public:
  MemCacheIter() :
      file_data(NULL),
      priority(0),
      transformed_header(false),
      body_bytes_consumed(0),
      stream_id(0),
      max_segment_size(kInitialDataSendersThreshold),
      bytes_sent(0) {}
  explicit MemCacheIter(FileData* fd) :
      file_data(fd),
      priority(0),
      transformed_header(false),
      body_bytes_consumed(0),
      stream_id(0),
      max_segment_size(kInitialDataSendersThreshold),
      bytes_sent(0) {}
  FileData* file_data;
  int priority;
  bool transformed_header;
  size_t body_bytes_consumed;
  uint32 stream_id;
  uint32 max_segment_size;
  size_t bytes_sent;
};

////////////////////////////////////////////////////////////////////////////////

class MemoryCache {
 public:
  typedef map<string, FileData> Files;

 public:
  Files files_;
  string cwd_;

  void CloneFrom(const MemoryCache& mc) {
    for (Files::const_iterator i = mc.files_.begin();
         i != mc.files_.end();
         ++i) {
      Files::iterator out_i =
        files_.insert(make_pair(i->first, FileData())).first;
      out_i->second.CopyFrom(i->second);
      cwd_ = mc.cwd_;
    }
  }

  void AddFiles() {
    LOG(INFO) << "Adding files!";
    deque<string> paths;
    cwd_ = FLAGS_cache_base_dir;
    paths.push_back(cwd_ + "/GET_");
    DIR* current_dir = NULL;
    while (!paths.empty()) {
      while (current_dir == NULL && !paths.empty()) {
        string current_dir_name = paths.front();
        VLOG(1) << "Attempting to open dir: \"" << current_dir_name << "\"";
        current_dir = opendir(current_dir_name.c_str());
        paths.pop_front();

        if (current_dir == NULL) {
          perror("Unable to open directory. ");
          current_dir_name.clear();
          continue;
        }

        if (current_dir) {
          VLOG(1) << "Succeeded opening";
          for (struct dirent* dir_data = readdir(current_dir);
               dir_data != NULL;
               dir_data = readdir(current_dir)) {
            string current_entry_name =
              current_dir_name + "/" + dir_data->d_name;
            if (dir_data->d_type == DT_REG) {
              VLOG(1) << "Found file: " << current_entry_name;
              ReadAndStoreFileContents(current_entry_name.c_str());
            } else if (dir_data->d_type == DT_DIR) {
              VLOG(1) << "Found subdir: " << current_entry_name;
              if (string(dir_data->d_name) != "." &&
                  string(dir_data->d_name) != "..") {
                VLOG(1) << "Adding to search path: " << current_entry_name;
                paths.push_front(current_entry_name);
              }
            }
          }
          VLOG(1) << "Oops, no data left. Closing dir.";
          closedir(current_dir);
          current_dir = NULL;
        }
      }
    }
  }

  void ReadToString(const char* filename, string* output) {
    output->clear();
    int fd = open(filename, 0, "r");
    if (fd == -1)
      return;
    char buffer[4096];
    ssize_t read_status = read(fd, buffer, sizeof(buffer));
    while (read_status > 0) {
      output->append(buffer, static_cast<size_t>(read_status));
      do {
        read_status = read(fd, buffer, sizeof(buffer));
      } while (read_status <= 0 && errno == EINTR);
    }
    close(fd);
  }

  void ReadAndStoreFileContents(const char* filename) {
    StoreBodyAndHeadersVisitor visitor;
    BalsaFrame framer;
    framer.set_balsa_visitor(&visitor);
    framer.set_balsa_headers(&(visitor.headers));
    string filename_contents;
    ReadToString(filename, &filename_contents);

    // Ugly hack to make everything look like 1.1.
    if (filename_contents.find("HTTP/1.0") == 0)
      filename_contents[7] = '1';

    size_t pos = 0;
    size_t old_pos = 0;
    while (true) {
      old_pos = pos;
      pos += framer.ProcessInput(filename_contents.data() + pos,
                                 filename_contents.size() - pos);
      if (framer.Error() || pos == old_pos) {
        LOG(ERROR) << "Unable to make forward progress, or error"
          " framing file: " << filename;
        if (framer.Error()) {
          LOG(INFO) << "********************************************ERROR!";
          return;
        }
        return;
      }
      if (framer.MessageFullyRead()) {
        // If no Content-Length or Transfer-Encoding was captured in the
        // file, then the rest of the data is the body.  Many of the captures
        // from within Chrome don't have content-lengths.
        if (!visitor.body.length())
          visitor.body = filename_contents.substr(pos);
        break;
      }
    }
    visitor.headers.RemoveAllOfHeader("content-length");
    visitor.headers.RemoveAllOfHeader("transfer-encoding");
    visitor.headers.RemoveAllOfHeader("connection");
    visitor.headers.AppendHeader("transfer-encoding", "chunked");
    visitor.headers.AppendHeader("connection", "keep-alive");

    // Experiment with changing headers for forcing use of cached
    // versions of content.
    // TODO(mbelshe) REMOVE ME
#if 0
    // TODO(mbelshe): append current date.
    visitor.headers.RemoveAllOfHeader("date");
    if (visitor.headers.HasHeader("expires")) {
      visitor.headers.RemoveAllOfHeader("expires");
      visitor.headers.AppendHeader("expires",
                                 "Fri, 30 Aug, 2019 12:00:00 GMT");
    }
#endif
    BalsaHeaders* headers = new BalsaHeaders;
    headers->CopyFrom(visitor.headers);
    string filename_stripped = string(filename).substr(cwd_.size() + 1);
//    LOG(INFO) << "Adding file (" << visitor.body.length() << " bytes): "
//              << filename_stripped;
    files_[filename_stripped] = FileData();
    FileData& fd = files_[filename_stripped];
    fd = FileData(headers, visitor.body);
    fd.filename = string(filename_stripped,
                         filename_stripped.find_first_of('/'));
    if (headers->HasHeader("X-Associated-Content")) {
      string content = headers->GetHeader("X-Associated-Content").ToString();
      vector<string> urls_and_priorities;
      SplitStringUsing(content, "||", &urls_and_priorities);
      VLOG(1) << "Examining X-Associated-Content header";
      for (int i = 0; i < urls_and_priorities.size(); ++i) {
        const string& url_and_priority_pair = urls_and_priorities[i];
        vector<string> url_and_priority;
        SplitStringUsing(url_and_priority_pair, "??", &url_and_priority);
        if (url_and_priority.size() >= 2) {
          string& priority_string = url_and_priority[0];
          string& filename_string = url_and_priority[1];
          int priority;
          char* last_eaten_char;
          priority = strtol(priority_string.c_str(), &last_eaten_char, 0);
          if (last_eaten_char ==
              priority_string.c_str() + priority_string.size()) {
            pair<int, string> entry(priority, filename_string);
            VLOG(1) << "Adding associated content: " << filename_string;
            fd.related_files.push_back(entry);
          }
        }
      }
    }
  }

  // Called at runtime to update learned headers
  // |url| is a url which contains a referrer header.
  // |referrer| is the referring URL
  // Adds an X-Subresource or X-Associated-Content to |referer| for |url|
  void UpdateHeaders(string referrer, string file_url) {
    if (!FLAGS_use_xac && !FLAGS_use_xsub)
      return;

    string referrer_host_path =
      net::UrlToFilenameEncoder::Encode(referrer, "GET_/");

    FileData* fd1 = GetFileData(string("GET_") + file_url);
    if (!fd1) {
      LOG(ERROR) << "Updating headers for unknown url: " << file_url;
      return;
    }
    string url = fd1->headers->GetHeader("X-Original-Url").as_string();
    string content_type = fd1->headers->GetHeader("Content-Type").as_string();
    if (content_type.length() == 0) {
      LOG(ERROR) << "Skipping subresource with unknown content-type";
      return;
    }

    // Now, lets see if this is the same host or not
    bool same_host = (UrlUtilities::GetUrlHost(referrer) ==
                      UrlUtilities::GetUrlHost(url));

    // This is a hacked algorithm for figuring out what priority
    // to use with pushed content.
    int priority = 4;
    if (content_type.find("css") != std::string::npos)
      priority = 1;
    else if (content_type.find("cript") != std::string::npos)
      priority = 1;
    else if (content_type.find("html") != std::string::npos)
      priority = 2;

    LOG(ERROR) << "Attempting update for " << referrer_host_path;

    FileData* fd2 = GetFileData(referrer_host_path);
    if (fd2 != NULL) {
      // If they are on the same host, we'll use X-Associated-Content
      string header_name;
      string new_value;
      string delimiter;
      bool related_files = false;
      if (same_host && FLAGS_use_xac) {
        header_name = "X-Associated-Content";
        char pri_ch = priority + '0';
        new_value = pri_ch + string("??") + url;
        delimiter = "||";
        related_files = true;
      } else {
        if (!FLAGS_use_xsub)
          return;
        header_name = "X-Subresource";
        new_value = content_type + "!!" + url;
        delimiter = "!!";
      }

      if (fd2->headers->HasNonEmptyHeader(header_name)) {
        string existing_header =
            fd2->headers->GetHeader(header_name).as_string();
        if (existing_header.find(url) != string::npos)
          return;  // header already recorded

        // Don't let these lists grow too long for low pri stuff.
        // TODO(mbelshe) We need better algorithms for this.
        if (existing_header.length() > 256 && priority > 2)
          return;

        new_value = existing_header + delimiter + new_value;
      }

      LOG(INFO) << "Recording " << header_name << " for " << new_value;
      fd2->headers->ReplaceOrAppendHeader(header_name, new_value);

      // Add it to the related files so that it will actually get sent out.
      if (related_files) {
        pair<int, string> entry(4, file_url);
        fd2->related_files.push_back(entry);
      }
    } else {
      LOG(ERROR) << "Failed to update headers:";
      LOG(ERROR) << "FAIL url: " << url;
      LOG(ERROR) << "FAIL ref: " << referrer_host_path;
    }
  }

  FileData* GetFileData(const string& filename) {
    Files::iterator fi = files_.end();
    if (filename.compare(filename.length() - 5, 5, ".html", 5) == 0) {
      string new_filename(filename.data(), filename.size() - 5);
      new_filename += ".http";
      fi = files_.find(new_filename);
    }
    if (fi == files_.end())
      fi = files_.find(filename);

    if (fi == files_.end()) {
      return NULL;
    }
    return &(fi->second);
  }

  bool AssignFileData(const string& filename, MemCacheIter* mci) {
    mci->file_data = GetFileData(filename);
    if (mci->file_data == NULL) {
      LOG(ERROR) << "Could not find file data for " << filename;
      return false;
    }
    return true;
  }
};

////////////////////////////////////////////////////////////////////////////////

class NotifierInterface {
 public:
  virtual ~NotifierInterface() {}
  virtual void Notify() = 0;
};

////////////////////////////////////////////////////////////////////////////////

class SMInterface {
 public:
  virtual size_t ProcessInput(const char* data, size_t len) = 0;
  virtual bool MessageFullyRead() const = 0;
  virtual bool Error() const = 0;
  virtual const char* ErrorAsString() const = 0;
  virtual void Reset() = 0;
  virtual void ResetForNewConnection() = 0;

  virtual void PostAcceptHook() = 0;

  virtual void NewStream(uint32 stream_id, uint32 priority,
                         const string& filename) = 0;
  virtual void SendEOF(uint32 stream_id) = 0;
  virtual void SendErrorNotFound(uint32 stream_id) = 0;
  virtual size_t SendSynStream(uint32 stream_id,
                              const BalsaHeaders& headers) = 0;
  virtual size_t SendSynReply(uint32 stream_id,
                              const BalsaHeaders& headers) = 0;
  virtual void SendDataFrame(uint32 stream_id, const char* data, int64 len,
                             uint32 flags, bool compress) = 0;
  virtual void GetOutput() = 0;

  virtual ~SMInterface() {}
};

////////////////////////////////////////////////////////////////////////////////

class SMServerConnection;
typedef SMInterface*(SMInterfaceFactory)(SMServerConnection* conn);

////////////////////////////////////////////////////////////////////////////////

typedef list<DataFrame> OutputList;

////////////////////////////////////////////////////////////////////////////////

class SMServerConnection;

class SMServerConnectionPoolInterface {
 public:
  virtual ~SMServerConnectionPoolInterface() {}
  // SMServerConnections will use this:
  virtual void SMServerConnectionDone(SMServerConnection* conn) = 0;
};

////////////////////////////////////////////////////////////////////////////////

class SMServerConnection: public EpollCallbackInterface,
                          public NotifierInterface {
 private:
  SMServerConnection(SMInterfaceFactory* sm_interface_factory,
                     MemoryCache* memory_cache,
                     EpollServer* epoll_server) :
      fd_(-1),
      record_fd_(-1),
      events_(0),

      registered_in_epoll_server_(false),
      initialized_(false),

      connection_pool_(NULL),
      epoll_server_(epoll_server),

      read_buffer_(4096*10),
      memory_cache_(memory_cache),
      sm_interface_(sm_interface_factory(this)),

      max_bytes_sent_per_dowrite_(128),

      ssl_(NULL) {}

  int fd_;
  int record_fd_;
  int events_;

  bool registered_in_epoll_server_;
  bool initialized_;

  SMServerConnectionPoolInterface* connection_pool_;
  EpollServer* epoll_server_;

  RingBuffer read_buffer_;

  OutputList output_list_;
  MemoryCache* memory_cache_;
  SMInterface* sm_interface_;

  size_t max_bytes_sent_per_dowrite_;

  SSL* ssl_;
 public:
  EpollServer* epoll_server() { return epoll_server_; }
  OutputList* output_list() { return &output_list_; }
  MemoryCache* memory_cache() { return memory_cache_; }
  int record_fd() { return record_fd_; }
  void close_record_fd() {
    if (record_fd_ != -1) {
      close(record_fd_);
      record_fd_ = -1;
    }
  }
  void ReadyToSend() {
    epoll_server_->SetFDReady(fd_, EPOLLIN | EPOLLOUT);
  }
  void EnqueueDataFrame(const DataFrame& df) {
    output_list_.push_back(df);
    VLOG(2) << "EnqueueDataFrame. Setting FD ready.";
    ReadyToSend();
  }

 public:
  ~SMServerConnection() {
    if (initialized()) {
      Reset();
    }
  }
  static SMServerConnection* NewSMServerConnection(SMInterfaceFactory* smif,
                                                   MemoryCache* memory_cache,
                                                   EpollServer* epoll_server) {
    return new SMServerConnection(smif, memory_cache, epoll_server);
  }

  bool initialized() const { return initialized_; }

  void InitSMServerConnection(SMServerConnectionPoolInterface* connection_pool,
                              EpollServer* epoll_server,
                              int fd) {
    if (initialized_) {
      LOG(FATAL) << "Attempted to initialize already initialized server";
      return;
    }
    if (epoll_server_ && registered_in_epoll_server_ && fd_ != -1) {
      epoll_server_->UnregisterFD(fd_);
    }
    if (fd_ != -1) {
      VLOG(2) << "Closing pre-existing fd";
      close(fd_);
      fd_ = -1;
    }
    if (FLAGS_record_mode) {
      char record_file_name[1024];
      snprintf(record_file_name, 1024, "%s/%d_%lld",
              FLAGS_record_path.c_str(), fd, GetCurrentTimeMillis());
      record_fd_ = open(record_file_name, O_CREAT|O_APPEND|O_WRONLY, S_IRWXU);
      if (record_fd_ < 0) {
        LOG(ERROR) << "Open record file for fd " << fd << " failed";
        record_fd_ = -1;
      }
    }

    fd_ = fd;

    registered_in_epoll_server_ = false;
    initialized_ = true;

    connection_pool_ = connection_pool;
    epoll_server_ = epoll_server;

    sm_interface_->Reset();
    read_buffer_.Clear();

    epoll_server_->RegisterFD(fd_, this, EPOLLIN | EPOLLOUT | EPOLLET);

    if (global_ssl_state) {
      ssl_ = flip_new_ssl(global_ssl_state->ssl_ctx);
      SSL_set_fd(ssl_, fd_);
    }
    sm_interface_->PostAcceptHook();
  }

  int Send(const char* bytes, int len, int flags) {
    return send(fd_, bytes, len, flags);
  }

  // the following are from the EpollCallbackInterface
  virtual void OnRegistration(EpollServer* eps, int fd, int event_mask) {
    registered_in_epoll_server_ = true;
  }
  virtual void OnModification(int fd, int event_mask) { }
  virtual void OnEvent(int fd, EpollEvent* event) {
    events_ |= event->in_events;
    HandleEvents();
    if (events_) {
      event->out_ready_mask = events_;
      events_ = 0;
    }
  }
  virtual void OnUnregistration(int fd, bool replaced) {
    registered_in_epoll_server_ = false;
  }
  virtual void OnShutdown(EpollServer* eps, int fd) {
    Cleanup("OnShutdown");
    return;
  }

 private:
  void HandleEvents() {
    VLOG(1) << "Received: " << EpollServer::EventMaskToString(events_);
    if (events_ & EPOLLIN) {
      if (!DoRead())
        goto handle_close_or_error;
    }

    if (events_ & EPOLLOUT) {
      if (!DoWrite())
        goto handle_close_or_error;
    }

    if (events_ & (EPOLLHUP | EPOLLERR)) {
      VLOG(2) << "!!!! Got HUP or ERR";
      goto handle_close_or_error;
    }
    return;

 handle_close_or_error:
    Cleanup("HandleEvents");
  }

  bool DoRead() {
    VLOG(2) << "DoRead()";
    if (fd_ == -1) {
      VLOG(2) << "DoRead(): fd_ == -1. Invalid FD. Returning false";
      return false;
    }
    while (!read_buffer_.Full()) {
      char* bytes;
      int size;
      read_buffer_.GetWritablePtr(&bytes, &size);
      ssize_t bytes_read = 0;
      if (ssl_) {
        bytes_read = SSL_read(ssl_, bytes, size);
      } else {
        bytes_read = recv(fd_, bytes, size, MSG_DONTWAIT);
      }
      int stored_errno = errno;
      if (bytes_read == -1) {
        switch (stored_errno) {
          case EAGAIN:
            events_ &= ~EPOLLIN;
            VLOG(2) << "Got EAGAIN while reading";
            goto done;
          case EINTR:
            VLOG(2) << "Got EINTR while reading";
            continue;
          default:
            VLOG(2) << "While calling recv, got error: " << stored_errno
              << " " << strerror(stored_errno);
            goto error_or_close;
        }
      } else if (bytes_read > 0) {
        VLOG(2) << "Read: " << bytes_read << " bytes from fd: " << fd_;
        read_buffer_.AdvanceWritablePtr(bytes_read);
        if (!DoConsumeReadData()) {
          goto error_or_close;
        }
        continue;
      } else {  // bytes_read == 0
        VLOG(2) << "0 bytes read with recv call.";
      }
      goto error_or_close;
    }
   done:
    return true;

   error_or_close:
    VLOG(2) << "DoRead(): error_or_close. Cleaning up, then returning false";
    Cleanup("DoRead");
    return false;
  }

  bool DoConsumeReadData() {
    char* bytes;
    int size;
    read_buffer_.GetReadablePtr(&bytes, &size);
    while (size != 0) {
      size_t bytes_consumed = sm_interface_->ProcessInput(bytes, size);
      VLOG(2) << "consumed: " << bytes_consumed << " from socket fd: " << fd_;
      if (bytes_consumed == 0) {
        break;
      }
      read_buffer_.AdvanceReadablePtr(bytes_consumed);
      if (sm_interface_->MessageFullyRead()) {
        VLOG(2) << "HandleRequestFullyRead";
        HandleRequestFullyRead();
        sm_interface_->Reset();
        events_ |= EPOLLOUT;
      } else if (sm_interface_->Error()) {
        LOG(ERROR) << "Framer error detected: "
                   << sm_interface_->ErrorAsString();
        // this causes everything to be closed/cleaned up.
        events_ |= EPOLLOUT;
        return false;
      }
      read_buffer_.GetReadablePtr(&bytes, &size);
    }
    return true;
  }

  void WriteResponse() {
    // this happens asynchronously from separate threads
    // feeding files into the output buffer.
  }

  void HandleRequestFullyRead() {
  }

  void Notify() {
  }

  bool DoWrite() {
    size_t bytes_sent = 0;
    int flags = MSG_NOSIGNAL | MSG_DONTWAIT;
    if (fd_ == -1) {
      VLOG(2) << "DoWrite: fd == -1. Returning false.";
      return false;
    }
    if (output_list_.empty()) {
      sm_interface_->GetOutput();
      if (output_list_.empty())
        events_ &= ~EPOLLOUT;
    }
    while (!output_list_.empty()) {
      if (bytes_sent >= max_bytes_sent_per_dowrite_) {
        events_ |= EPOLLOUT;
        break;
      }
      if (output_list_.size() < 2) {
        sm_interface_->GetOutput();
      }
      DataFrame& data_frame = output_list_.front();
      const char*  bytes = data_frame.data;
      int size = data_frame.size;
      bytes += data_frame.index;
      size -= data_frame.index;
      DCHECK_GE(size, 0);
      if (size <= 0) {
        data_frame.MaybeDelete();
        output_list_.pop_front();
        continue;
      }

      flags = MSG_NOSIGNAL | MSG_DONTWAIT;
      if (output_list_.size() > 1) {
        flags |= MSG_MORE;
      }
      ssize_t bytes_written = 0;
      if (ssl_) {
        bytes_written = SSL_write(ssl_, bytes, size);
      } else {
        bytes_written = send(fd_, bytes, size, flags);
      }
      int stored_errno = errno;
      if (bytes_written == -1) {
        switch (stored_errno) {
          case EAGAIN:
            events_ &= ~EPOLLOUT;
            VLOG(2) << " Got EAGAIN while writing";
            goto done;
          case EINTR:
            VLOG(2) << " Got EINTR while writing";
            continue;
          default:
            VLOG(2) << "While calling send, got error: " << stored_errno
              << " " << strerror(stored_errno);
            goto error_or_close;
        }
      } else if (bytes_written > 0) {
        VLOG(1) << "Wrote: " << bytes_written  << " bytes to socket fd: "
          << fd_;
        data_frame.index += bytes_written;
        bytes_sent += bytes_written;
        continue;
      }
      VLOG(2) << "0 bytes written to socket " << fd_ << " with send call.";
      goto error_or_close;
    }
   done:
    return true;

   error_or_close:
    VLOG(2) << "DoWrite: error_or_close. Returning false after cleaning up";
    Cleanup("DoWrite");
    return false;
  }

  friend ostream& operator<<(ostream& os, const SMServerConnection& c) {
    os << &c << "\n";
    return os;
  }

  void Reset() {
    VLOG(2) << "Resetting";
    if (ssl_) {
      SSL_shutdown(ssl_);
      SSL_free(ssl_);
    }
    if (registered_in_epoll_server_) {
      epoll_server_->UnregisterFD(fd_);
      registered_in_epoll_server_ = false;
    }
    if (fd_ >= 0) {
      VLOG(2) << "Closing connection";
      close(fd_);
      fd_ = -1;
    }
    sm_interface_->ResetForNewConnection();
    read_buffer_.Clear();
    initialized_ = false;
    events_ = 0;
    output_list_.clear();
  }

  void Cleanup(const char* cleanup) {
    VLOG(2) << "Cleaning up: " << cleanup;
    if (!initialized_) {
      return;
    }
    Reset();
    connection_pool_->SMServerConnectionDone(this);
  }
};

////////////////////////////////////////////////////////////////////////////////

class OutputOrdering {
 public:
  typedef list<MemCacheIter> PriorityRing;

  typedef map<uint32, PriorityRing> PriorityMap;

  struct PriorityMapPointer {
    PriorityMapPointer(): ring(NULL), alarm_enabled(false) {}
    PriorityRing* ring;
    PriorityRing::iterator it;
    bool alarm_enabled;
    EpollServer::AlarmRegToken alarm_token;
  };
  typedef map<uint32, PriorityMapPointer> StreamIdToPriorityMap;

  StreamIdToPriorityMap stream_ids_;
  PriorityMap priority_map_;
  PriorityRing first_data_senders_;
  uint32 first_data_senders_threshold_;  // when you've passed this, you're no
                                         // longer a first_data_sender...
  SMServerConnection* connection_;
  EpollServer* epoll_server_;

  explicit OutputOrdering(SMServerConnection* connection) :
      first_data_senders_threshold_(kInitialDataSendersThreshold),
      connection_(connection),
      epoll_server_(connection->epoll_server()) {
  }

  void Reset() {
    while (!stream_ids_.empty()) {
      StreamIdToPriorityMap::iterator sitpmi = stream_ids_.begin();
      PriorityMapPointer& pmp = sitpmi->second;
      if (pmp.alarm_enabled) {
        epoll_server_->UnregisterAlarm(pmp.alarm_token);
      }
      stream_ids_.erase(sitpmi);
    }
    priority_map_.clear();
    first_data_senders_.clear();
  }

  bool ExistsInPriorityMaps(uint32 stream_id) {
    StreamIdToPriorityMap::iterator sitpmi = stream_ids_.find(stream_id);
    return sitpmi != stream_ids_.end();
  }

  struct BeginOutputtingAlarm : public EpollAlarmCallbackInterface {
   public:
    BeginOutputtingAlarm(OutputOrdering* oo,
                         OutputOrdering::PriorityMapPointer* pmp,
                         const MemCacheIter& mci) :
        output_ordering_(oo), pmp_(pmp), mci_(mci), epoll_server_(NULL) {}

    int64 OnAlarm() {
      OnUnregistration();
      output_ordering_->MoveToActive(pmp_, mci_);
      VLOG(1) << "ON ALARM! Should now start to output...";
      delete this;
      return 0;
    }
    void OnRegistration(const EpollServer::AlarmRegToken& tok,
                        EpollServer* eps) {
      epoll_server_ = eps;
      pmp_->alarm_token = tok;
      pmp_->alarm_enabled = true;
    }
    void OnUnregistration() {
      pmp_->alarm_enabled = false;
    }
    void OnShutdown(EpollServer* eps) {
      OnUnregistration();
    }
    ~BeginOutputtingAlarm() {
      if (epoll_server_ && pmp_->alarm_enabled)
        epoll_server_->UnregisterAlarm(pmp_->alarm_token);
    }
   private:
    OutputOrdering* output_ordering_;
    OutputOrdering::PriorityMapPointer* pmp_;
    MemCacheIter mci_;
    EpollServer* epoll_server_;
  };

  void MoveToActive(PriorityMapPointer* pmp, MemCacheIter mci) {
    VLOG(1) <<"Moving to active!";
    first_data_senders_.push_back(mci);
    pmp->ring = &first_data_senders_;
    pmp->it = first_data_senders_.end();
    --pmp->it;
    connection_->ReadyToSend();
  }

  void AddToOutputOrder(const MemCacheIter& mci) {
    if (ExistsInPriorityMaps(mci.stream_id))
      LOG(FATAL) << "OOps, already was inserted here?!";

    StreamIdToPriorityMap::iterator sitpmi;
    sitpmi = stream_ids_.insert(
        pair<uint32, PriorityMapPointer>(mci.stream_id,
                                         PriorityMapPointer())).first;
    PriorityMapPointer& pmp = sitpmi->second;

    BeginOutputtingAlarm* boa = new BeginOutputtingAlarm(this, &pmp, mci);
    epoll_server_->RegisterAlarmApproximateDelta(
        FLAGS_server_think_time_in_s * 1000000, boa);
  }

  void SpliceToPriorityRing(PriorityRing::iterator pri) {
    MemCacheIter& mci = *pri;
    PriorityMap::iterator pmi = priority_map_.find(mci.priority);
    if (pmi == priority_map_.end()) {
      pmi = priority_map_.insert(
          pair<uint32, PriorityRing>(mci.priority, PriorityRing())).first;
    }

    pmi->second.splice(pmi->second.end(),
                       first_data_senders_,
                       pri);
    StreamIdToPriorityMap::iterator sitpmi = stream_ids_.find(mci.stream_id);
    sitpmi->second.ring = &(pmi->second);
  }

  MemCacheIter* GetIter() {
    while (!first_data_senders_.empty()) {
      MemCacheIter& mci = first_data_senders_.front();
      if (mci.bytes_sent >= first_data_senders_threshold_) {
        SpliceToPriorityRing(first_data_senders_.begin());
      } else {
        first_data_senders_.splice(first_data_senders_.end(),
                                  first_data_senders_,
                                  first_data_senders_.begin());
        mci.max_segment_size = kInitialDataSendersThreshold;
        return &mci;
      }
    }
    while (!priority_map_.empty()) {
      PriorityRing& first_ring = priority_map_.begin()->second;
      if (first_ring.empty()) {
        priority_map_.erase(priority_map_.begin());
        continue;
      }
      MemCacheIter& mci = first_ring.front();
      first_ring.splice(first_ring.end(),
                        first_ring,
                        first_ring.begin());
      mci.max_segment_size = kNormalSegmentSize;
      return &mci;
    }
    return NULL;
  }

  void RemoveStreamId(uint32 stream_id) {
    StreamIdToPriorityMap::iterator sitpmi = stream_ids_.find(stream_id);
    if (sitpmi == stream_ids_.end())
      return;
    PriorityMapPointer& pmp = sitpmi->second;
    if (pmp.alarm_enabled) {
      epoll_server_->UnregisterAlarm(pmp.alarm_token);
    } else {
      pmp.ring->erase(pmp.it);
    }

    stream_ids_.erase(sitpmi);
  }
};

////////////////////////////////////////////////////////////////////////////////

class FlipSM : public FlipFramerVisitorInterface, public SMInterface {
 private:
  uint64 seq_num_;
  FlipFramer* framer_;

  SMServerConnection* connection_;
  OutputList* output_list_;
  OutputOrdering output_ordering_;
  MemoryCache* memory_cache_;
  uint32 next_outgoing_stream_id_;
 public:
  explicit FlipSM(SMServerConnection* connection) :
      seq_num_(0),
      framer_(new FlipFramer),
      connection_(connection),
      output_list_(connection->output_list()),
      output_ordering_(connection),
      memory_cache_(connection->memory_cache()),
      next_outgoing_stream_id_(2) {
    framer_->set_visitor(this);
  }
 private:
  virtual void OnError(FlipFramer* framer) {
    /* do nothing with this right now */
  }

  virtual void OnControl(const FlipControlFrame* frame) {
    FlipHeaderBlock headers;
    bool parsed_headers = false;
    switch (frame->type()) {
      case SYN_STREAM:
        {
        parsed_headers = framer_->ParseHeaderBlock(frame, &headers);
        VLOG(2) << "OnSyn(" << frame->stream_id() << ")";
        VLOG(2) << "headers parsed?: " << (parsed_headers? "yes": "no");
        if (parsed_headers) {
          VLOG(2) << "# headers: " << headers.size();
        }
        int j = 0;
        for (FlipHeaderBlock::iterator i = headers.begin();
             i != headers.end();
             ++i) {
          VLOG(2) << i->first << ": " << i->second;
          if (FLAGS_record_mode && connection_->record_fd() > 0) {
            // If record mode is enabled and corresponding server connection
            // has file opened, then save the request headers into the file.
            // All the requests from the same connection is save in one file.
            // This file will be used to replay and generate FLIP requests
            // load.
            string header = i->first + ": " + i->second + "\n";
            ++j;
            if (j == headers.size()) {
              header += "\n";  // add an additional empty lime
            }
            int r = write(
                connection_->record_fd(), header.c_str(), header.size());
            if (r < 0) {
              perror("unable to write to record file:");
            }
          }
        }

        FlipHeaderBlock::iterator method = headers.find("method");
        FlipHeaderBlock::iterator url = headers.find("url");
        if (url == headers.end() || method == headers.end()) {
          VLOG(2) << "didn't find method or url or method. Not creating stream";
          break;
        }

        FlipHeaderBlock::iterator referer = headers.find("referer");
        if (referer != headers.end() && method->second == "GET") {
          memory_cache_->UpdateHeaders(referer->second, url->second);
        }
        string uri = UrlUtilities::GetUrlPath(url->second);
        string host = UrlUtilities::GetUrlHost(url->second);
        // requests started with /testing are loadtime measurement related
        // urls, use LoadtimeMeasurement class to handle them.
        if (uri.find("/testing") == 0) {
          string output;
          global_loadtime_measurement.ProcessRequest(uri, output);
          SendOKResponse(frame->stream_id(), &output);
        } else {
          string filename;
          if (FLAGS_need_to_encode_url) {
            filename = net::UrlToFilenameEncoder::Encode(
                "http://" + host + uri, method->second + "_/");
          } else {
            filename = string(method->second + "_" + url->second);
          }

          NewStream(frame->stream_id(),
                    reinterpret_cast<const FlipSynStreamControlFrame*>(frame)->
                      priority(),
                    filename);
          }
        }
        break;

      case SYN_REPLY:
        parsed_headers = framer_->ParseHeaderBlock(frame, &headers);
        VLOG(2) << "OnSynReply(" << frame->stream_id() << ")";
        break;
      case FIN_STREAM:
        VLOG(2) << "OnFin(" << frame->stream_id() << ")";
        output_ordering_.RemoveStreamId(frame->stream_id());

        break;
      default:
        LOG(DFATAL) << "Unknown control frame type";
    }
  }
  virtual void OnStreamFrameData(
    FlipStreamId stream_id,
    const char* data, size_t len) {
    VLOG(2) << "StreamData(" << stream_id << ", [" << len << "])";
    /* do nothing with this right now */
  }
  virtual void OnLameDuck() {
    /* do nothing with this right now */
  }

 public:
  ~FlipSM() {
    Reset();
  }
  size_t ProcessInput(const char* data, size_t len) {
    return framer_->ProcessInput(data, len);
  }

  bool MessageFullyRead() const {
    return framer_->MessageFullyRead();
  }

  bool Error() const {
    return framer_->HasError();
  }

  const char* ErrorAsString() const {
    return FlipFramer::ErrorCodeToString(framer_->error_code());
  }

  void Reset() {}
  void ResetForNewConnection() {
    // seq_num is not cleared, intentionally.
    delete framer_;
    framer_ = new FlipFramer;
    framer_->set_visitor(this);
    output_ordering_.Reset();
    next_outgoing_stream_id_ = 2;
  }

  // Send a couple of NOOP packets to force opening of cwnd.
  void PostAcceptHook() {
    if (!FLAGS_use_cwnd_opener)
      return;

    // We send 2 because that is the initial cwnd, and also because
    // we have to in order to get an ACK back from the client due to
    // delayed ACK.
    const int kPkts = 2;

    LOG(ERROR) << "Sending NOP FRAMES";

    scoped_ptr<FlipControlFrame> frame(FlipFramer::CreateNopFrame());
    for (int i = 0; i < kPkts; ++i) {
      char* bytes = frame->data();
      size_t size = FlipFrame::size();
      ssize_t bytes_written = connection_->Send(bytes, size, MSG_DONTWAIT);
      if (bytes_written != size) {
        LOG(ERROR) << "Trouble sending Nop packet! (" << errno << ")";
        if (errno == EAGAIN)
          break;
      }
    }
  }

  void AddAssociatedContent(FileData* file_data) {
    for (int i = 0; i < file_data->related_files.size(); ++i) {
      pair<int, string>& related_file = file_data->related_files[i];
      MemCacheIter mci;
      string filename  = "GET_";
      filename += related_file.second;
      if (!memory_cache_->AssignFileData(filename, &mci)) {
        VLOG(1) << "Unable to find associated content for: " << filename;
        continue;
      }
      VLOG(1) << "Adding associated content: " << filename;
      mci.stream_id = next_outgoing_stream_id_;
      next_outgoing_stream_id_ += 2;
      mci.priority =  related_file.first;
      AddToOutputOrder(mci);
    }
  }

  void NewStream(uint32 stream_id, uint32 priority, const string& filename) {
    MemCacheIter mci;
    mci.stream_id = stream_id;
    mci.priority = priority;
    if (!memory_cache_->AssignFileData(filename, &mci)) {
      // error creating new stream.
      VLOG(2) << "Sending ErrorNotFound";
      SendErrorNotFound(stream_id);
    } else {
      AddToOutputOrder(mci);
      if (FLAGS_use_xac) {
        AddAssociatedContent(mci.file_data);
      }
    }
  }

  void AddToOutputOrder(const MemCacheIter& mci) {
    output_ordering_.AddToOutputOrder(mci);
  }

  void SendEOF(uint32 stream_id) {
    SendEOFImpl(stream_id);
  }

  void SendErrorNotFound(uint32 stream_id) {
    SendErrorNotFoundImpl(stream_id);
  }

  void SendOKResponse(uint32 stream_id, string* output) {
    SendOKResponseImpl(stream_id, output);
  }

  size_t SendSynStream(uint32 stream_id, const BalsaHeaders& headers) {
    return SendSynStreamImpl(stream_id, headers);
  }

  size_t SendSynReply(uint32 stream_id, const BalsaHeaders& headers) {
    return SendSynReplyImpl(stream_id, headers);
  }

  void SendDataFrame(uint32 stream_id, const char* data, int64 len,
                     uint32 flags, bool compress) {
    FlipDataFlags flip_flags = static_cast<FlipDataFlags>(flags);
    SendDataFrameImpl(stream_id, data, len, flip_flags, compress);
  }

  FlipFramer* flip_framer() { return framer_; }

 private:
  void SendEOFImpl(uint32 stream_id) {
    SendDataFrame(stream_id, NULL, 0, DATA_FLAG_FIN, false);
    VLOG(2) << "Sending EOF: " << stream_id;
    KillStream(stream_id);
  }

  void SendErrorNotFoundImpl(uint32 stream_id) {
    BalsaHeaders my_headers;
    my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "404", "Not Found");
    SendSynReplyImpl(stream_id, my_headers);
    SendDataFrame(stream_id, "wtf?", 4, DATA_FLAG_FIN, false);
    output_ordering_.RemoveStreamId(stream_id);
  }

  void SendOKResponseImpl(uint32 stream_id, string* output) {
    BalsaHeaders my_headers;
    my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "200", "OK");
    SendSynReplyImpl(stream_id, my_headers);
    SendDataFrame(
        stream_id, output->c_str(), output->size(), DATA_FLAG_FIN, false);
    output_ordering_.RemoveStreamId(stream_id);
  }

  void KillStream(uint32 stream_id) {
    output_ordering_.RemoveStreamId(stream_id);
  }

  void CopyHeaders(FlipHeaderBlock& dest, const BalsaHeaders& headers) {
    for (BalsaHeaders::const_header_lines_iterator hi =
         headers.header_lines_begin();
         hi != headers.header_lines_end();
         ++hi) {
      FlipHeaderBlock::iterator fhi = dest.find(hi->first.ToString());
      if (fhi == dest.end()) {
        dest[hi->first.ToString()] = hi->second.ToString();
      } else {
        dest[hi->first.ToString()] = (
            string(fhi->second.data(), fhi->second.size()) + "," +
            string(hi->second.data(), hi->second.size()));
      }
    }

    // These headers have no value
    dest.erase("X-Associated-Content");  // TODO(mbelshe): case-sensitive
    dest.erase("X-Original-Url");  // TODO(mbelshe): case-sensitive
  }

  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers) {
    FlipHeaderBlock block;
    block["method"] = headers.request_method().ToString();
    if (!headers.HasHeader("status"))
      block["status"] = headers.response_code().ToString();
    if (!headers.HasHeader("version"))
      block["version"] =headers.response_version().ToString();
    if (headers.HasHeader("X-Original-Url")) {
      string original_url = headers.GetHeader("X-Original-Url").as_string();
      block["path"] = UrlUtilities::GetUrlPath(original_url);
    } else {
      block["path"] = headers.request_uri().ToString();
    }
    CopyHeaders(block, headers);

    FlipSynStreamControlFrame* fsrcf =
      framer_->CreateSynStream(stream_id, 0, CONTROL_FLAG_NONE, true, &block);
    DataFrame df;
    df.size = fsrcf->length() + FlipFrame::size();
    size_t df_size = df.size;
    df.data = fsrcf->data();
    df.delete_when_done = true;
    EnqueueDataFrame(df);

    VLOG(2) << "Sending SynStreamheader " << stream_id;
    return df_size;
  }

  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers) {
    FlipHeaderBlock block;
    CopyHeaders(block, headers);
    block["status"] = headers.response_code().ToString() + " " +
                      headers.response_reason_phrase().ToString();
    block["version"] = headers.response_version().ToString();

    FlipSynReplyControlFrame* fsrcf =
      framer_->CreateSynReply(stream_id, CONTROL_FLAG_NONE, true, &block);
    DataFrame df;
    df.size = fsrcf->length() + FlipFrame::size();
    size_t df_size = df.size;
    df.data = fsrcf->data();
    df.delete_when_done = true;
    EnqueueDataFrame(df);

    VLOG(2) << "Sending SynReplyheader " << stream_id;
    return df_size;
  }

  void SendDataFrameImpl(uint32 stream_id, const char* data, int64 len,
                         FlipDataFlags flags, bool compress) {
    // Force compression off if disabled via command line.
    if (!FLAGS_use_compression)
      flags = static_cast<FlipDataFlags>(flags & ~DATA_FLAG_COMPRESSED);

    // TODO(mbelshe):  We can't compress here - before going into the
    //                 priority queue.  Compression needs to be done
    //                 with late binding.
    FlipDataFrame* fdf = framer_->CreateDataFrame(stream_id, data, len,
                                                  flags);
    DataFrame df;
    df.size = fdf->length() + FlipFrame::size();
    df.data = fdf->data();
    df.delete_when_done = true;
    EnqueueDataFrame(df);

    VLOG(2) << "Sending data frame" << stream_id << " [" << len << "]"
            << " shrunk to " << fdf->length();
  }

  void EnqueueDataFrame(const DataFrame& df) {
    connection_->EnqueueDataFrame(df);
  }

  void GetOutput() {
    while (output_list_->size() < 2) {
      MemCacheIter* mci = output_ordering_.GetIter();
      if (mci == NULL) {
        VLOG(2) << "GetOutput: nothing to output!?";
        return;
      }
      if (!mci->transformed_header) {
        mci->transformed_header = true;
        VLOG(2) << "GetOutput transformed header stream_id: ["
          << mci->stream_id << "]";
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
        VLOG(2) << "GetOutput remove_stream_id: [" << mci->stream_id << "]";
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
          string content_type =
              mci->file_data->headers->GetHeader("content-type").ToString();
          if (content_type.find("image") == content_type.npos)
            should_compress = true;
        }
      }

      SendDataFrame(mci->stream_id,
                    mci->file_data->body.data() + mci->body_bytes_consumed,
                    num_to_write, 0, should_compress);
      VLOG(2) << "GetOutput SendDataFrame[" << mci->stream_id
        << "]: " << num_to_write;
      mci->body_bytes_consumed += num_to_write;
      mci->bytes_sent += num_to_write;
    }
  }
};

////////////////////////////////////////////////////////////////////////////////

class HTTPSM : public BalsaVisitorInterface, public SMInterface {
 private:
  uint64 seq_num_;
  BalsaFrame* framer_;
  BalsaHeaders headers_;
  uint32 stream_id_;

  SMServerConnection* connection_;
  OutputList* output_list_;
  OutputOrdering output_ordering_;
  MemoryCache* memory_cache_;
 public:
  explicit HTTPSM(SMServerConnection* connection) :
      seq_num_(0),
      framer_(new BalsaFrame),
      stream_id_(1),
      connection_(connection),
      output_list_(connection->output_list()),
      output_ordering_(connection),
      memory_cache_(connection->memory_cache()) {
    framer_->set_balsa_visitor(this);
    framer_->set_balsa_headers(&headers_);
  }
 private:
  typedef map<string, uint32> ClientTokenMap;
 private:
    virtual void ProcessBodyInput(const char *input, size_t size) {
    }
    virtual void ProcessBodyData(const char *input, size_t size) {
      // ignoring this.
    }
    virtual void ProcessHeaderInput(const char *input, size_t size) {
    }
    virtual void ProcessTrailerInput(const char *input, size_t size) {}
    virtual void ProcessHeaders(const BalsaHeaders& headers) {
      VLOG(2) << "Got new request!";
      // requests started with /testing are loadtime measurement related
      // urls, use LoadtimeMeasurement class to handle them.
      if (headers.request_uri().ToString().find("/testing") == 0) {
        string output;
        global_loadtime_measurement.ProcessRequest(
            headers.request_uri().ToString(), output);
        SendOKResponse(stream_id_, &output);
        stream_id_ += 2;
      } else {
        string filename;
        if (FLAGS_need_to_encode_url) {
          filename = net::UrlToFilenameEncoder::Encode(
              headers.GetHeader("Host").ToString() +
              headers.request_uri().ToString(),
              headers.request_method().ToString() + "_/");
        } else {
         filename = headers.request_method().ToString() + "_" +
                    headers.request_uri().ToString();
        }
        NewStream(stream_id_, 0, filename);
        stream_id_ += 2;
      }
    }
    virtual void ProcessRequestFirstLine(const char* line_input,
                                         size_t line_length,
                                         const char* method_input,
                                         size_t method_length,
                                         const char* request_uri_input,
                                         size_t request_uri_length,
                                         const char* version_input,
                                         size_t version_length) {}
    virtual void ProcessResponseFirstLine(const char *line_input,
                                          size_t line_length,
                                          const char *version_input,
                                          size_t version_length,
                                          const char *status_input,
                                          size_t status_length,
                                          const char *reason_input,
                                          size_t reason_length) {}
    virtual void ProcessChunkLength(size_t chunk_length) {}
    virtual void ProcessChunkExtensions(const char *input, size_t size) {}
    virtual void HeaderDone() {}
    virtual void MessageDone() {
      VLOG(2) << "MessageDone!";
    }
    virtual void HandleHeaderError(BalsaFrame* framer) {
      HandleError();
    }
    virtual void HandleHeaderWarning(BalsaFrame* framer) {}
    virtual void HandleChunkingError(BalsaFrame* framer) {
      HandleError();
    }
    virtual void HandleBodyError(BalsaFrame* framer) {
      HandleError();
    }

    void HandleError() {
      VLOG(2) << "Error detected";
    }

 public:
  ~HTTPSM() {
    Reset();
  }
  size_t ProcessInput(const char* data, size_t len) {
    return framer_->ProcessInput(data, len);
  }

  bool MessageFullyRead() const {
    return framer_->MessageFullyRead();
  }

  bool Error() const {
    return framer_->Error();
  }

  const char* ErrorAsString() const {
    return BalsaFrameEnums::ErrorCodeToString(framer_->ErrorCode());
  }

  void Reset() {
    framer_->Reset();
  }

  void ResetForNewConnection() {
    seq_num_ = 0;
    output_ordering_.Reset();
    framer_->Reset();
  }

  void PostAcceptHook() {
  }

  void NewStream(uint32 stream_id, uint32 priority, const string& filename) {
    MemCacheIter mci;
    mci.stream_id = stream_id;
    mci.priority = priority;
    if (!memory_cache_->AssignFileData(filename, &mci)) {
      SendErrorNotFound(stream_id);
    } else {
      AddToOutputOrder(mci);
    }
  }

  void AddToOutputOrder(const MemCacheIter& mci) {
    output_ordering_.AddToOutputOrder(mci);
  }

  void SendEOF(uint32 stream_id) {
    SendEOFImpl(stream_id);
  }

  void SendErrorNotFound(uint32 stream_id) {
    SendErrorNotFoundImpl(stream_id);
  }

  void SendOKResponse(uint32 stream_id, string* output) {
    SendOKResponseImpl(stream_id, output);
  }

  size_t SendSynStream(uint32 stream_id, const BalsaHeaders& headers) {
    return 0;
  }

  size_t SendSynReply(uint32 stream_id, const BalsaHeaders& headers) {
    return SendSynReplyImpl(stream_id, headers);
  }

  void SendDataFrame(uint32 stream_id, const char* data, int64 len,
                     uint32 flags, bool compress) {
    SendDataFrameImpl(stream_id, data, len, flags, compress);
  }

  BalsaFrame* flip_framer() { return framer_; }

 private:
  void SendEOFImpl(uint32 stream_id) {
    DataFrame df;
    df.data = "0\r\n\r\n";
    df.size = 5;
    df.delete_when_done = false;
    EnqueueDataFrame(df);
  }

  void SendErrorNotFoundImpl(uint32 stream_id) {
    BalsaHeaders my_headers;
    my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "404", "Not Found");
    my_headers.RemoveAllOfHeader("content-length");
    my_headers.HackHeader("transfer-encoding", "chunked");
    SendSynReplyImpl(stream_id, my_headers);
    SendDataFrame(stream_id, "wtf?", 4, 0, false);
    SendEOFImpl(stream_id);
    output_ordering_.RemoveStreamId(stream_id);
  }

  void SendOKResponseImpl(uint32 stream_id, string* output) {
    BalsaHeaders my_headers;
    my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "200", "OK");
    my_headers.RemoveAllOfHeader("content-length");
    my_headers.HackHeader("transfer-encoding", "chunked");
    SendSynReplyImpl(stream_id, my_headers);
    SendDataFrame(stream_id, output->c_str(), output->size(), 0, false);
    SendEOFImpl(stream_id);
    output_ordering_.RemoveStreamId(stream_id);
  }

  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers) {
    SimpleBuffer sb;
    headers.WriteHeaderAndEndingToBuffer(&sb);
    DataFrame df;
    df.size = sb.ReadableBytes();
    char* buffer = new char[df.size];
    df.data = buffer;
    df.delete_when_done = true;
    sb.Read(buffer, df.size);
    VLOG(2) << "******************Sending HTTP Reply header " << stream_id;
    size_t df_size = df.size;
    EnqueueDataFrame(df);
    return df_size;
  }

  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers) {
    SimpleBuffer sb;
    headers.WriteHeaderAndEndingToBuffer(&sb);
    DataFrame df;
    df.size = sb.ReadableBytes();
    char* buffer = new char[df.size];
    df.data = buffer;
    df.delete_when_done = true;
    sb.Read(buffer, df.size);
    VLOG(2) << "******************Sending HTTP Reply header " << stream_id;
    size_t df_size = df.size;
    EnqueueDataFrame(df);
    return df_size;
  }

  void SendDataFrameImpl(uint32 stream_id, const char* data, int64 len,
                         uint32 flags, bool compress) {
    char chunk_buf[128];
    snprintf(chunk_buf, sizeof(chunk_buf), "%x\r\n", (unsigned int)len);
    string chunk_description(chunk_buf);
    DataFrame df;
    df.size = chunk_description.size() + len + 2;
    char* buffer = new char[df.size];
    df.data = buffer;
    df.delete_when_done = true;
    memcpy(buffer, chunk_description.data(), chunk_description.size());
    memcpy(buffer + chunk_description.size(), data, len);
    memcpy(buffer + chunk_description.size() + len, "\r\n", 2);
    EnqueueDataFrame(df);
  }

  void EnqueueDataFrame(const DataFrame& df) {
    connection_->EnqueueDataFrame(df);
  }

  void GetOutput() {
    MemCacheIter* mci = output_ordering_.GetIter();
    if (mci == NULL) {
      VLOG(2) << "GetOutput: nothing to output!?";
      return;
    }
    if (!mci->transformed_header) {
      mci->bytes_sent = SendSynReply(mci->stream_id,
                                     *(mci->file_data->headers));
      mci->transformed_header = true;
      VLOG(2) << "GetOutput transformed header stream_id: ["
        << mci->stream_id << "]";
      return;
    }
    if (mci->body_bytes_consumed >= mci->file_data->body.size()) {
      SendEOF(mci->stream_id);
      output_ordering_.RemoveStreamId(mci->stream_id);
      VLOG(2) << "GetOutput remove_stream_id: [" << mci->stream_id << "]";
      return;
    }
    size_t num_to_write =
      mci->file_data->body.size() - mci->body_bytes_consumed;
    if (num_to_write > mci->max_segment_size)
      num_to_write = mci->max_segment_size;
    SendDataFrame(mci->stream_id,
                  mci->file_data->body.data() + mci->body_bytes_consumed,
                  num_to_write, 0, true);
    VLOG(2) << "GetOutput SendDataFrame[" << mci->stream_id
      << "]: " << num_to_write;
    mci->body_bytes_consumed += num_to_write;
    mci->bytes_sent += num_to_write;
  }
};

////////////////////////////////////////////////////////////////////////////////

class SMAcceptorThread : public Thread,
                         public EpollCallbackInterface,
                         public SMServerConnectionPoolInterface {
  EpollServer epoll_server_;
  int listen_fd_;
  int accepts_per_wake_;

  vector<SMServerConnection*> unused_server_connections_;
  vector<SMServerConnection*> tmp_unused_server_connections_;
  vector<SMServerConnection*> allocated_server_connections_;
  Notification quitting_;
  SMInterfaceFactory* sm_interface_factory_;
  MemoryCache* memory_cache_;
  Mutex m_;
 public:

  SMAcceptorThread(int listen_fd,
                   int accepts_per_wake,
                   SMInterfaceFactory* smif,
                   MemoryCache* memory_cache) :
      listen_fd_(listen_fd),
      accepts_per_wake_(accepts_per_wake),
      quitting_(false),
      sm_interface_factory_(smif),
      memory_cache_(memory_cache) {
    Thread::SetJoinable(true);
  }

  ~SMAcceptorThread() {
    for (vector<SMServerConnection*>::iterator i =
           allocated_server_connections_.begin();
         i != allocated_server_connections_.end();
         ++i) {
      delete *i;
    }
  }

  SMServerConnection* NewConnection() {
    SMServerConnection* server =
      SMServerConnection::NewSMServerConnection(sm_interface_factory_,
                                                memory_cache_,
                                                &epoll_server_);
    allocated_server_connections_.push_back(server);
    VLOG(3) << "Making new server: " << server;
    return server;
  }

  SMServerConnection* FindOrMakeNewSMServerConnection() {
    if (unused_server_connections_.empty()) {
      return NewConnection();
    }
    SMServerConnection* retval = unused_server_connections_.back();
    unused_server_connections_.pop_back();
    return retval;
  }


  void InitWorker() {
    epoll_server_.RegisterFD(listen_fd_, this, EPOLLIN | EPOLLET);
  }

  void HandleConnection(int client_fd) {
    SMServerConnection* server_connection = FindOrMakeNewSMServerConnection();
    if (server_connection == NULL) {
      VLOG(2) << "Closing " << client_fd;
      close(client_fd);
      return;
    }
    server_connection->InitSMServerConnection(this,
                                            &epoll_server_,
                                            client_fd);
  }

  void AcceptFromListenFD() {
    if (accepts_per_wake_ > 0) {
      for (int i = 0; i < accepts_per_wake_; ++i) {
        struct sockaddr address;
        socklen_t socklen = sizeof(address);
        int fd = accept(listen_fd_, &address, &socklen);
        if (fd == -1) {
          VLOG(2) << "accept fail(" << listen_fd_ << "): " << errno;
          break;
        }
        VLOG(2) << "********************Accepted fd: " << fd << "\n\n\n";
        HandleConnection(fd);
      }
    } else {
      while (true) {
        struct sockaddr address;
        socklen_t socklen = sizeof(address);
        int fd = accept(listen_fd_, &address, &socklen);
        if (fd == -1) {
          VLOG(2) << "accept fail(" << listen_fd_ << "): " << errno;
          break;
        }
        VLOG(2) << "********************Accepted fd: " << fd << "\n\n\n";
        HandleConnection(fd);
      }
    }
  }

  // EpollCallbackInteface virtual functions.
  virtual void OnRegistration(EpollServer* eps, int fd, int event_mask) { }
  virtual void OnModification(int fd, int event_mask) { }
  virtual void OnEvent(int fd, EpollEvent* event) {
    if (event->in_events | EPOLLIN) {
      VLOG(2) << "Accepting based upon epoll events";
      AcceptFromListenFD();
    }
  }
  virtual void OnUnregistration(int fd, bool replaced) { }
  virtual void OnShutdown(EpollServer* eps, int fd) { }

  void Quit() {
    quitting_.Notify();
  }

  void Run() {
    while (!quitting_.HasBeenNotified()) {
      epoll_server_.set_timeout_in_us(10 * 1000);  // 10 ms
      epoll_server_.WaitForEventsAndExecuteCallbacks();
      unused_server_connections_.insert(unused_server_connections_.end(),
                                        tmp_unused_server_connections_.begin(),
                                        tmp_unused_server_connections_.end());
      tmp_unused_server_connections_.clear();
    }
  }

  // SMServerConnections will use this:
  virtual void SMServerConnectionDone(SMServerConnection* sc) {
    VLOG(3) << "Done with server connection: " << sc;
    sc->close_record_fd();
    tmp_unused_server_connections_.push_back(sc);
  }
};

////////////////////////////////////////////////////////////////////////////////

SMInterface* NewFlipSM(SMServerConnection* connection) {
  return new FlipSM(connection);
}

SMInterface* NewHTTPSM(SMServerConnection* connection) {
  return new HTTPSM(connection);
}

////////////////////////////////////////////////////////////////////////////////

int CreateListeningSocket(int port, int backlog_size,
                          bool reuseport, bool no_nagle) {
  int listening_socket = 0;
  char port_buf[256];
  snprintf(port_buf, sizeof(port_buf), "%d", port);
  cerr <<" Attempting to listen on port: " << port_buf << "\n";
  cerr <<" input port: " << port << "\n";
  gfe2::CreateListeningSocket("",
                              port_buf,
                              true,
                              backlog_size,
                              &listening_socket,
                              true,
                              reuseport,
                              &cerr);
  SetNonBlocking(listening_socket);
  if (no_nagle) {
    // set SO_REUSEADDR on the listening socket.
    int on = 1;
    int rc;
    rc = setsockopt(listening_socket, IPPROTO_TCP,  TCP_NODELAY,
                    reinterpret_cast<char *>(&on), sizeof(on));
    if (rc < 0) {
      close(listening_socket);
      LOG(FATAL) << "setsockopt() failed fd=" << listening_socket << "\n";
    }
  }
  return listening_socket;
}

////////////////////////////////////////////////////////////////////////////////

bool GotQuitFromStdin() {
  // Make stdin nonblocking. Yes this is done each time. Oh well.
  fcntl(0, F_SETFL, O_NONBLOCK);
  char c;
  string maybequit;
  while (read(0, &c, 1) > 0) {
    maybequit += c;
  }
  if (maybequit.size()) {
    VLOG(2) << "scanning string: \"" << maybequit << "\"";
  }
  return (maybequit.size() > 1 &&
          (maybequit.c_str()[0] == 'q' ||
           maybequit.c_str()[0] == 'Q'));
}


////////////////////////////////////////////////////////////////////////////////

const char* BoolToStr(bool b) {
  if (b)
    return "true";
  return "false";
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char**argv) {
  InitGoogleExceptChangeRootAndUser(argv[0], &argc, &argv, true);

  bool use_ssl = FLAGS_use_ssl;
  int response_count_until_close = FLAGS_response_count_until_close;
  int flip_port = FLAGS_flip_port;
  int port = FLAGS_port;
  int backlog_size = FLAGS_accept_backlog_size;
  bool reuseport = FLAGS_reuseport;
  bool no_nagle = FLAGS_no_nagle;
  double server_think_time_in_s = FLAGS_server_think_time_in_s;
  int accepts_per_wake = FLAGS_accepts_per_wake;
  int num_threads = 1;

  MemoryCache flip_memory_cache;
  flip_memory_cache.AddFiles();

  MemoryCache http_memory_cache;
  http_memory_cache.CloneFrom(flip_memory_cache);

  LOG(INFO) <<
    "Starting up with the following state: \n"
    "                      use_ssl: " << use_ssl << "\n"
    "   response_count_until_close: " << response_count_until_close << "\n"
    "                         port: " << port << "\n"
    "                    flip_port: " << flip_port << "\n"
    "                 backlog_size: " << backlog_size << "\n"
    "                    reuseport: " << BoolToStr(reuseport) << "\n"
    "                     no_nagle: " << BoolToStr(no_nagle) << "\n"
    "       server_think_time_in_s: " << server_think_time_in_s << "\n"
    "             accepts_per_wake: " << accepts_per_wake << "\n"
    "                  num_threads: " << num_threads << "\n"
    "                     use_xsub: " << BoolToStr(FLAGS_use_xsub) << "\n"
    "                      use_xac: " << BoolToStr(FLAGS_use_xac) << "\n";

  if (use_ssl) {
    global_ssl_state = new GlobalSSLState;
    flip_init_ssl(global_ssl_state);
  } else {
    global_ssl_state = NULL;
  }
  EpollServer epoll_server;
  vector<SMAcceptorThread*> sm_worker_threads_;

  {
    // flip
    int listen_fd = -1;

    if (reuseport || listen_fd == -1) {
      listen_fd = CreateListeningSocket(flip_port, backlog_size,
                                        reuseport, no_nagle);
      if (listen_fd < 0) {
        LOG(FATAL) << "Unable to open listening socket on flip_port: "
          << flip_port;
      } else {
        LOG(INFO) << "Listening for flip on port: " << flip_port;
      }
    }
    sm_worker_threads_.push_back(
        new SMAcceptorThread(listen_fd,
                             accepts_per_wake,
                             &NewFlipSM,
                             &flip_memory_cache));
    // Note that flip_memory_cache is not threadsafe, it is merely
    // thread compatible. Thus, if ever we are to spawn multiple threads,
    // we either must make the MemoryCache threadsafe, or use
    // a separate MemoryCache for each thread.
    //
    // The latter is what is currently being done as we spawn
    // two threads (one for flip, one for http).
    sm_worker_threads_.back()->InitWorker();
    sm_worker_threads_.back()->Start();
  }

  {
    // http
    int listen_fd = -1;
    if (reuseport || listen_fd == -1) {
      listen_fd = CreateListeningSocket(port, backlog_size,
                                        reuseport, no_nagle);
      if (listen_fd < 0) {
        LOG(FATAL) << "Unable to open listening socket on port: " << port;
      } else {
        LOG(INFO) << "Listening for HTTP on port: " << port;
      }
    }
    sm_worker_threads_.push_back(
        new SMAcceptorThread(listen_fd,
                             accepts_per_wake,
                             &NewHTTPSM,
                             &http_memory_cache));
    // Note that flip_memory_cache is not threadsafe, it is merely
    // thread compatible. Thus, if ever we are to spawn multiple threads,
    // we either must make the MemoryCache threadsafe, or use
    // a separate MemoryCache for each thread.
    //
    // The latter is what is currently being done as we spawn
    // two threads (one for flip, one for http).
    sm_worker_threads_.back()->InitWorker();
    sm_worker_threads_.back()->Start();
  }

  while (true) {
    if (GotQuitFromStdin()) {
      for (int i = 0; i < sm_worker_threads_.size(); ++i) {
        sm_worker_threads_[i]->Quit();
      }
      for (int i = 0; i < sm_worker_threads_.size(); ++i) {
        sm_worker_threads_[i]->Join();
      }
      return 0;
    }
    usleep(1000*10);  // 10 ms
  }
  return 0;
}

