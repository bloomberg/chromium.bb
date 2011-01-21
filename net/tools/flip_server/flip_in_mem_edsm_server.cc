// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <netinet/tcp.h>  // For TCP_NODELAY
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <signal.h>

#include <deque>
#include <iostream>
#include <limits>
#include <vector>
#include <list>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/threading/simple_thread.h"
#include "base/timer.h"
#include "base/lock.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/dump_cache/url_to_filename_encoder.h"
#include "net/tools/dump_cache/url_utilities.h"
#include "net/tools/flip_server/balsa_enums.h"
#include "net/tools/flip_server/balsa_frame.h"
#include "net/tools/flip_server/balsa_headers.h"
#include "net/tools/flip_server/balsa_visitor_interface.h"
#include "net/tools/flip_server/buffer_interface.h"
#include "net/tools/flip_server/epoll_server.h"
#include "net/tools/flip_server/ring_buffer.h"
#include "net/tools/flip_server/simple_buffer.h"
#include "net/tools/flip_server/split.h"
#include "net/tools/flip_server/flip_config.h"

////////////////////////////////////////////////////////////////////////////////

using std::deque;
using std::list;
using std::map;
using std::ostream;
using std::pair;
using std::string;
using std::vector;
using std::cout;
using std::cerr;

////////////////////////////////////////////////////////////////////////////////

#define IPV4_PRINTABLE_FORMAT(IP) (((IP)>>0)&0xff),(((IP)>>8)&0xff), \
                                 (((IP)>>16)&0xff),(((IP)>>24)&0xff)

#define ACCEPTOR_CLIENT_IDENT acceptor_->listen_ip_ << ":" \
                               << acceptor_->listen_port_ << " "

#define NEXT_PROTO_STRING "\x06spdy/2\x08http/1.1\x08http/1.0"

#define SSL_CTX_DEFAULT_CIPHER_LIST "!aNULL:!ADH:!eNull:!LOW:!EXP:RC4+RSA:MEDIUM:HIGH"

#define PIDFILE "/var/run/flip-server.pid"

// If true, then disables the nagle algorithm);
bool FLAGS_disable_nagle = true;

// The number of times that accept() will be called when the
//  alarm goes off when the accept_using_alarm flag is set to true.
//  If set to 0, accept() will be performed until the accept queue
//  is completely drained and the accept() call returns an error);
int32 FLAGS_accepts_per_wake = 0;

// The size of the TCP accept backlog);
int32 FLAGS_accept_backlog_size = 1024;

// The directory where cache locates);
string FLAGS_cache_base_dir = ".";

// If true, then encode url to filename);
bool FLAGS_need_to_encode_url = false;

// If set to false a single socket will be used. If set to true
//  then a new socket will be created for each accept thread.
//  Note that this only works with kernels that support
//  SO_REUSEPORT);
bool FLAGS_reuseport = false;

// Flag to force spdy, even if NPN is not negotiated.
bool FLAGS_force_spdy = false;

// The amount of time the server delays before sending back the
//  reply);
double FLAGS_server_think_time_in_s = 0;

// Does the server send X-Subresource headers);
bool FLAGS_use_xsub = false;

// Does the server send X-Associated-Content headers);
bool FLAGS_use_xac = false;

// Does the server compress data frames);
bool FLAGS_use_compression = false;

////////////////////////////////////////////////////////////////////////////////

using base::StringPiece;
using base::SimpleThread;
using net::BalsaFrame;
using net::BalsaFrameEnums;
using net::BalsaHeaders;
using net::BalsaHeadersEnums;
using net::BalsaVisitorInterface;
using net::EpollAlarmCallbackInterface;
using net::EpollCallbackInterface;
using net::EpollEvent;
using net::EpollServer;
using net::RingBuffer;
using net::SimpleBuffer;
using net::SplitStringPieceToVector;
using net::UrlUtilities;
using spdy::CONTROL_FLAG_NONE;
using spdy::DATA_FLAG_COMPRESSED;
using spdy::DATA_FLAG_FIN;
using spdy::RST_STREAM;
using spdy::SYN_REPLY;
using spdy::SYN_STREAM;
using spdy::SpdyControlFrame;
using spdy::SpdySettingsControlFrame;
using spdy::SpdyDataFlags;
using spdy::SpdyDataFrame;
using spdy::SpdyRstStreamControlFrame;
using spdy::SpdyFrame;
using spdy::SpdyFrameBuilder;
using spdy::SpdyFramer;
using spdy::SpdyFramerVisitorInterface;
using spdy::SpdyHeaderBlock;
using spdy::SpdyStreamId;
using spdy::SpdySynReplyControlFrame;
using spdy::SpdySynStreamControlFrame;

FlipConfig g_proxy_config;

////////////////////////////////////////////////////////////////////////////////

void PrintSslError() {
  char buf[128];  // this buffer must be at least 120 chars long.
  int error_num = ERR_get_error();
  while (error_num != 0) {
    LOG(ERROR) << ERR_error_string(error_num, buf);
    error_num = ERR_get_error();
  }
}

static int ssl_set_npn_callback(SSL *s,
                                const unsigned char **data,
                                unsigned int *len,
                                void *arg)
{
  VLOG(1) <<  "SSL NPN callback: advertising protocols.";
  *data = (const unsigned char *) NEXT_PROTO_STRING;
  *len = strlen(NEXT_PROTO_STRING);
  return SSL_TLSEXT_ERR_OK;
}
////////////////////////////////////////////////////////////////////////////////

// Creates a socket with domain, type and protocol parameters.
// Assigns the return value of socket() to *fd.
// Returns errno if an error occurs, else returns zero.
int CreateSocket(int domain, int type, int protocol, int *fd) {
  CHECK(fd != NULL);
  *fd = ::socket(domain, type, protocol);
  return (*fd == -1) ? errno : 0;
}

// Encode the URL.
string EncodeURL(string uri, string host, string method) {
  if (!FLAGS_need_to_encode_url) {
    // TODO(mbelshe): if uri is fully qualified, need to strip protocol/host.
    return string(method + "_" + uri);
  }

  string filename;
  if (uri[0] == '/') {
    // uri is not fully qualified.
    filename = net::UrlToFilenameEncoder::Encode(
        "http://" + host + uri, method + "_/", false);
  } else {
    filename = net::UrlToFilenameEncoder::Encode(uri, method + "_/", false);
  }
  return filename;
}

////////////////////////////////////////////////////////////////////////////////

struct SSLState {
  SSL_METHOD* ssl_method;
  SSL_CTX* ssl_ctx;
};

// SSL stuff
void spdy_init_ssl(SSLState* state,
                   string ssl_cert_name,
                   string ssl_key_name,
                   bool use_npn) {
  SSL_library_init();
  PrintSslError();

  SSL_load_error_strings();
  PrintSslError();

  state->ssl_method = SSLv23_method();
  state->ssl_ctx = SSL_CTX_new(state->ssl_method);
  if (!state->ssl_ctx) {
    PrintSslError();
    LOG(FATAL) << "Unable to create SSL context";
  }
  // Disable SSLv2 support.
  SSL_CTX_set_options(state->ssl_ctx,
                      SSL_OP_NO_SSLv2 | SSL_OP_CIPHER_SERVER_PREFERENCE);
  if (SSL_CTX_use_certificate_file(state->ssl_ctx,
                                   ssl_cert_name.c_str(),
                                   SSL_FILETYPE_PEM) <= 0) {
    PrintSslError();
    LOG(FATAL) << "Unable to use cert.pem as SSL cert.";
  }
  if (SSL_CTX_use_PrivateKey_file(state->ssl_ctx,
                                  ssl_key_name.c_str(),
                                  SSL_FILETYPE_PEM) <= 0) {
    PrintSslError();
    LOG(FATAL) << "Unable to use key.pem as SSL key.";
  }
  if (!SSL_CTX_check_private_key(state->ssl_ctx)) {
    PrintSslError();
    LOG(FATAL) << "The cert.pem and key.pem files don't match";
  }
  if (use_npn) {
    SSL_CTX_set_next_protos_advertised_cb(state->ssl_ctx,
                                          ssl_set_npn_callback, NULL);
  }
  VLOG(1) << "SSL CTX default cipher list: " << SSL_CTX_DEFAULT_CIPHER_LIST;
  SSL_CTX_set_cipher_list(state->ssl_ctx, SSL_CTX_DEFAULT_CIPHER_LIST);

  VLOG(1) << "SSL CTX session expiry: " << g_proxy_config.ssl_session_expiry_
          << " seconds";
  SSL_CTX_set_timeout(state->ssl_ctx, g_proxy_config.ssl_session_expiry_);

#ifdef SSL_MODE_RELEASE_BUFFERS
  VLOG(1) << "SSL CTX: Setting Release Buffers mode.";
  SSL_CTX_set_mode(state->ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif

  // Proper methods to disable compression don't exist until 0.9.9+. For now
  // we must manipulate the stack of compression methods directly.
  if (g_proxy_config.ssl_disable_compression_) {
    STACK_OF(SSL_COMP) *ssl_comp_methods = SSL_COMP_get_compression_methods();
    int num_methods = sk_SSL_COMP_num(ssl_comp_methods);
    int i;
    for (i = 0; i < num_methods; i++) {
      static_cast<void>(sk_SSL_COMP_delete(ssl_comp_methods, i));
    }
  }
}

SSL* spdy_new_ssl(SSL_CTX* ssl_ctx) {
  SSL* ssl = SSL_new(ssl_ctx);
  PrintSslError();

  SSL_set_accept_state(ssl);
  PrintSslError();

  return ssl;
}

////////////////////////////////////////////////////////////////////////////////

const int kMSS = 1400;  // Linux default
const int kSSLOverhead = 33;
const int kSpdyOverhead = SpdyFrame::size();
const int kInitialDataSendersThreshold = (2 * kMSS) - kSpdyOverhead;
const int kSSLSegmentSize = (1 * kMSS) - kSSLOverhead;
const int kSpdySegmentSize = kSSLSegmentSize - kSpdyOverhead;

////////////////////////////////////////////////////////////////////////////////

class DataFrame {
 public:
  const char* data;
  size_t size;
  bool delete_when_done;
  size_t index;
  DataFrame() : data(NULL), size(0), delete_when_done(false), index(0) {}
  virtual void MaybeDelete() {
    if (delete_when_done) {
      delete[] data;
    }
  }
  virtual ~DataFrame() {
    MaybeDelete();
  }
};

class SpdyFrameDataFrame : public DataFrame {
 public:
  SpdyFrame* frame;
  virtual void MaybeDelete() {
    if (delete_when_done) {
      delete frame;
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
    // TODO(mbelshe) append current date.
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
      string content = headers->GetHeader("X-Associated-Content").as_string();
      vector<StringPiece> urls_and_priorities;
      SplitStringPieceToVector(content, "||", &urls_and_priorities, true);
      VLOG(1) << "Examining X-Associated-Content header";
      for (unsigned int i = 0; i < urls_and_priorities.size(); ++i) {
        const StringPiece& url_and_priority_pair = urls_and_priorities[i];
        vector<StringPiece> url_and_priority;
        SplitStringPieceToVector(url_and_priority_pair, "??",
                                 &url_and_priority, true);
        if (url_and_priority.size() >= 2) {
          string priority_string(url_and_priority[0].data(),
                                 url_and_priority[0].size());
          string filename_string(url_and_priority[1].data(),
                                 url_and_priority[1].size());
          long priority;
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
      net::UrlToFilenameEncoder::Encode(referrer, "GET_/", false);

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
    if (content_type.find("css") != string::npos)
      priority = 1;
    else if (content_type.find("cript") != string::npos)
      priority = 1;
    else if (content_type.find("html") != string::npos)
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

class NotifierInterface {
 public:
  virtual ~NotifierInterface() {}
  virtual void Notify() = 0;
};

////////////////////////////////////////////////////////////////////////////////

class SMConnectionPoolInterface;

class SMInterface {
 public:
  virtual void InitSMInterface(SMInterface* sm_other_interface,
                               int32 server_idx) = 0;
  virtual void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                SMInterface* sm_interface,
                                EpollServer* epoll_server,
                                int fd,
                                string server_ip,
                                string server_port,
                                string remote_ip,
                                bool use_ssl)  = 0;
  virtual size_t ProcessReadInput(const char* data, size_t len) = 0;
  virtual size_t ProcessWriteInput(const char* data, size_t len) = 0;
  virtual void SetStreamID(uint32 stream_id) = 0;
  virtual bool MessageFullyRead() const = 0;
  virtual bool Error() const = 0;
  virtual const char* ErrorAsString() const = 0;
  virtual void Reset() = 0;
  virtual void ResetForNewInterface(int32 server_idx) = 0;
  // ResetForNewConnection is used for interfaces which control SMConnection
  // objects. When called an interface may put its connection object into
  // a reusable instance pool. Currently this is what the HttpSM interface
  // does.
  virtual void ResetForNewConnection() = 0;
  virtual void Cleanup() = 0;

  virtual int PostAcceptHook() = 0;

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

class SMConnectionInterface {
 public:
   virtual ~SMConnectionInterface() {}
   virtual void ReadyToSend() = 0;
   virtual EpollServer* epoll_server() = 0;
};

class HttpSM;
class SMConnection;

typedef list<DataFrame*> OutputList;

class SMConnectionPoolInterface {
 public:
  virtual ~SMConnectionPoolInterface() {}
  // SMConnections will use this:
  virtual void SMConnectionDone(SMConnection* connection) = 0;
};

SMInterface* NewStreamerSM(SMConnection* connection,
                           SMInterface* sm_interface,
                           EpollServer* epoll_server,
                           FlipAcceptor* acceptor);

SMInterface* NewSpdySM(SMConnection* connection,
                       SMInterface* sm_interface,
                       EpollServer* epoll_server,
                       MemoryCache* memory_cache,
                       FlipAcceptor* acceptor);

SMInterface* NewHttpSM(SMConnection* connection,
                       SMInterface* sm_interface,
                       EpollServer* epoll_server,
                       MemoryCache* memory_cache,
                       FlipAcceptor* acceptor);

////////////////////////////////////////////////////////////////////////////////

class SMConnection:  public SMConnectionInterface,
                     public EpollCallbackInterface,
                     public NotifierInterface {
 private:
  SMConnection(EpollServer* epoll_server,
               SSLState* ssl_state,
               MemoryCache* memory_cache,
               FlipAcceptor* acceptor,
               string log_prefix)
      : fd_(-1),
        events_(0),
        registered_in_epoll_server_(false),
        initialized_(false),
        protocol_detected_(false),
        connection_complete_(false),
        connection_pool_(NULL),
        epoll_server_(epoll_server),
        ssl_state_(ssl_state),
        memory_cache_(memory_cache),
        acceptor_(acceptor),
        read_buffer_(4096*10),
        sm_spdy_interface_(NULL),
        sm_http_interface_(NULL),
        sm_streamer_interface_(NULL),
        sm_interface_(NULL),
        log_prefix_(log_prefix),
        max_bytes_sent_per_dowrite_(4096),
        ssl_(NULL),
        last_read_time_(0)
        {}

  int fd_;
  int events_;

  bool registered_in_epoll_server_;
  bool initialized_;
  bool protocol_detected_;
  bool connection_complete_;

  SMConnectionPoolInterface* connection_pool_;

  EpollServer *epoll_server_;
  SSLState *ssl_state_;
  MemoryCache* memory_cache_;
  FlipAcceptor *acceptor_;
  string client_ip_;

  RingBuffer read_buffer_;

  OutputList output_list_;
  SMInterface* sm_spdy_interface_;
  SMInterface* sm_http_interface_;
  SMInterface* sm_streamer_interface_;
  SMInterface* sm_interface_;
  string log_prefix_;

  size_t max_bytes_sent_per_dowrite_;

  SSL* ssl_;
 public:
  time_t last_read_time_;
  string server_ip_;
  string server_port_;

  EpollServer* epoll_server() { return epoll_server_; }
  OutputList* output_list() { return &output_list_; }
  MemoryCache* memory_cache() { return memory_cache_; }
  void ReadyToSend() {
    VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
            << "Setting ready to send: EPOLLIN | EPOLLOUT";
    epoll_server_->SetFDReady(fd_, EPOLLIN | EPOLLOUT);
  }
  void EnqueueDataFrame(DataFrame* df) {
    output_list_.push_back(df);
    VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "EnqueueDataFrame: "
            << "size = " << df->size << ": Setting FD ready.";
    ReadyToSend();
  }
  int fd() { return fd_; }

 public:
  ~SMConnection() {
    if (initialized()) {
      Reset();
    }
  }
  static SMConnection* NewSMConnection(EpollServer* epoll_server,
                                       SSLState *ssl_state,
                                       MemoryCache* memory_cache,
                                       FlipAcceptor *acceptor,
                                       string log_prefix) {
    return new SMConnection(epoll_server, ssl_state, memory_cache,
                            acceptor, log_prefix);
  }

  bool initialized() const { return initialized_; }
  string client_ip() const { return client_ip_; }

  void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                        SMInterface* sm_interface,
                        EpollServer* epoll_server,
                        int fd,
                        string server_ip,
                        string server_port,
                        string remote_ip,
                        bool use_ssl) {
    if (initialized_) {
      LOG(FATAL) << "Attempted to initialize already initialized server";
      return;
    }

    client_ip_ = remote_ip;

    if (fd == -1) {
      // If fd == -1, then we are initializing a new connection that will
      // connect to the backend.
      //
      // ret:  -1 == error
      //        0 == connection in progress
      //        1 == connection complete
      // TODO: is_numeric_host_address value needs to be detected
      server_ip_ = server_ip;
      server_port_ = server_port;
      int ret = net::CreateConnectedSocket(&fd_,
                                           server_ip,
                                           server_port,
                                           true,
                                           acceptor_->disable_nagle_);

      if (ret < 0) {
        LOG(ERROR) << "-1 Could not create connected socket";
        return;
      } else if (ret == 1) {
        DCHECK_NE(-1, fd_);
        connection_complete_ = true;
        VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                << "Connection complete to: " << server_ip_ << ":"
                << server_port_ << " ";
      }
      VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "Connecting to server: " << server_ip_ << ":"
                << server_port_ << " ";
    } else {
      // If fd != -1 then we are initializing a connection that has just been
      // accepted from the listen socket.
      connection_complete_ = true;
      if (epoll_server_ && registered_in_epoll_server_ && fd_ != -1) {
        epoll_server_->UnregisterFD(fd_);
      }
      if (fd_ != -1) {
        VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                 << "Closing pre-existing fd";
        close(fd_);
        fd_ = -1;
      }

      fd_ = fd;
    }

    registered_in_epoll_server_ = false;
    // Set the last read time here as the idle checker will start from
    // now.
    last_read_time_ = time(NULL);
    initialized_ = true;

    connection_pool_ = connection_pool;
    epoll_server_ = epoll_server;

    if (sm_interface) {
      sm_interface_ = sm_interface;
      protocol_detected_ = true;
    }

    read_buffer_.Clear();

    epoll_server_->RegisterFD(fd_, this, EPOLLIN | EPOLLOUT | EPOLLET);

    if (use_ssl) {
      ssl_ = spdy_new_ssl(ssl_state_->ssl_ctx);
      SSL_set_fd(ssl_, fd_);
      PrintSslError();
    }
  }

  int Send(const char* data, int len, int flags) {
    ssize_t bytes_written = 0;
    if (ssl_) {
      // Write smallish chunks to SSL so that we don't have large
      // multi-packet TLS records to receive before being able to handle
      // the data.
      while(len > 0) {
        const int kMaxTLSRecordSize = 1460;
        const char* ptr = &(data[bytes_written]);
        int chunksize = std::min(len, kMaxTLSRecordSize);
        int rv = SSL_write(ssl_, ptr, chunksize);
        if (rv <= 0) {
          switch(SSL_get_error(ssl_, rv)) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_CONNECT:
              rv = -2;
              break;
            default:
              PrintSslError();
              break;
          }
          // If we wrote some data, return that count.  Otherwise
          // return the stall error.
          return bytes_written > 0 ? bytes_written : rv;
        }
        bytes_written += rv;
        len -= rv;
        if (rv != chunksize)
          break;  // If we couldn't write everything, we're implicitly stalled
      }
      if (!(flags & MSG_MORE)) {
        int state = 0;
        setsockopt( fd_, IPPROTO_TCP, TCP_CORK, &state, sizeof( state ) );
        state = 1;
        setsockopt( fd_, IPPROTO_TCP, TCP_CORK, &state, sizeof( state ) );
      }
    } else {
      bytes_written = send(fd_, data, len, flags);
    }
    return bytes_written;
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

  void Cleanup(const char* cleanup) {
    VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "Cleanup: " << cleanup;
    if (!initialized_) {
      return;
    }
    Reset();
    if (connection_pool_) {
      connection_pool_->SMConnectionDone(this);
    }
    if (sm_interface_) {
      sm_interface_->ResetForNewConnection();
    }
    last_read_time_ = 0;
  }

 private:
  void HandleEvents() {
    VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "Received: "
            << EpollServer::EventMaskToString(events_).c_str();

    if (events_ & EPOLLIN) {
      if (!DoRead())
        goto handle_close_or_error;
    }

    if (events_ & EPOLLOUT) {
      // Check if we have connected or not
      if (connection_complete_ == false) {
        int sock_error;
        socklen_t sock_error_len = sizeof(sock_error);
        int ret = getsockopt(fd_, SOL_SOCKET, SO_ERROR, &sock_error,
                              &sock_error_len);
        if (ret != 0) {
          VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                  << "getsockopt error: " << errno << ": " << strerror(errno);
          goto handle_close_or_error;
        }
        if (sock_error == 0) {
          connection_complete_ = true;
          VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                  << "Connection complete to " << server_ip_ << ":"
                << server_port_ << " ";
        } else if (sock_error == EINPROGRESS) {
          return;
        } else {
          VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                  << "error connecting to server";
          goto handle_close_or_error;
        }
      }
      if (!DoWrite())
        goto handle_close_or_error;
    }

    if (events_ & (EPOLLHUP | EPOLLERR)) {
      VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "!!! Got HUP or ERR";
      goto handle_close_or_error;
    }
    return;

    handle_close_or_error:
    Cleanup("HandleEvents");
  }

  bool DoRead() {
    VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "DoRead()";
    while (!read_buffer_.Full()) {
      char* bytes;
      int size;
      if (fd_ == -1) {
        VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                << "DoRead(): fd_ == -1. Invalid FD. Returning false";
        return false;
      }
      read_buffer_.GetWritablePtr(&bytes, &size);
      ssize_t bytes_read = 0;
      if (ssl_) {
        bytes_read = SSL_read(ssl_, bytes, size);
        if (bytes_read < 0) {
          switch(SSL_get_error(ssl_, bytes_read)) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_CONNECT:
              events_ &= ~EPOLLIN;
              goto done;
            default:
              PrintSslError();
              break;
          }
        }
      } else {
        bytes_read = recv(fd_, bytes, size, MSG_DONTWAIT);
      }
      int stored_errno = errno;
      if (bytes_read == -1) {
        switch (stored_errno) {
          case EAGAIN:
            events_ &= ~EPOLLIN;
            VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                    << "Got EAGAIN while reading";
            goto done;
          case EINTR:
            VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                    << "Got EINTR while reading";
            continue;
          default:
            VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                    << "While calling recv, got error: "
                    << (ssl_?"(ssl error)":strerror(stored_errno));
            goto error_or_close;
        }
      } else if (bytes_read > 0) {
        VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "read " << bytes_read
                 << " bytes";
        last_read_time_ = time(NULL);
        if (!protocol_detected_) {
          if (acceptor_->flip_handler_type_ == FLIP_HANDLER_HTTP_SERVER) {
            // Http Server
            protocol_detected_ = true;
            if (!sm_http_interface_) {
              VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                      << "Created HTTP interface.";
              sm_http_interface_ = NewHttpSM(this, NULL, epoll_server_,
                                             memory_cache_, acceptor_);
            } else {
              VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                      << "Reusing HTTP interface.";
            }
            sm_interface_ = sm_http_interface_;
          } else if (ssl_) {
            protocol_detected_ = true;
            if (SSL_session_reused(ssl_) == 0) {
              VLOG(1) << "Session status: renegotiated";
            } else {
              VLOG(1) << "Session status: resumed";
            }
            bool spdy_negotiated = FLAGS_force_spdy;
            if (!spdy_negotiated) {
              const unsigned char *npn_proto;
              unsigned int npn_proto_len;
              SSL_get0_next_proto_negotiated(ssl_, &npn_proto, &npn_proto_len);
              if (npn_proto_len > 0) {
                string npn_proto_str((const char *)npn_proto, npn_proto_len);
                VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                        << "NPN protocol detected: " << npn_proto_str;
              } else {
                VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                        << "NPN protocol detected: none";
                if (acceptor_->flip_handler_type_ == FLIP_HANDLER_SPDY_SERVER) {
                  VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                          << "NPN protocol: Could not negotiate SPDY protocol.";
                  goto error_or_close;
                }
              }
              if (npn_proto_len > 0 &&
                  !strncmp(reinterpret_cast<const char*>(npn_proto),
                           "spdy/2", npn_proto_len)) {
                  spdy_negotiated = true;
              }
            }
            if (spdy_negotiated) {
              if (!sm_spdy_interface_) {
                sm_spdy_interface_ = NewSpdySM(this, NULL, epoll_server_,
                                               memory_cache_, acceptor_);
                VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                        << "Created SPDY interface.";
              } else {
                VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                        << "Reusing SPDY interface.";
              }
              sm_interface_ = sm_spdy_interface_;
            } else if (acceptor_->spdy_only_) {
              VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                      << "SPDY proxy only, closing HTTPS connection.";
              goto error_or_close;
            } else {
              if (!sm_streamer_interface_) {
                sm_streamer_interface_ = NewStreamerSM(this, NULL,
                                                       epoll_server_,
                                                       acceptor_);
                VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                        << "Created Streamer interface.";
              } else {
                VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                        << "Reusing Streamer interface: ";
              }
              sm_interface_ = sm_streamer_interface_;
            }
          }
          if (sm_interface_->PostAcceptHook() == 0) {
            goto error_or_close;
          }
        }
        read_buffer_.AdvanceWritablePtr(bytes_read);
        if (!DoConsumeReadData()) {
          goto error_or_close;
        }
        continue;
      } else {  // bytes_read == 0
        VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                << "0 bytes read with recv call.";
      }
      goto error_or_close;
    }
   done:
    return true;

    error_or_close:
    VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
            << "DoRead(): error_or_close. "
            << "Cleaning up, then returning false";
    Cleanup("DoRead");
    return false;
  }

  bool DoConsumeReadData() {
    char* bytes;
    int size;
    read_buffer_.GetReadablePtr(&bytes, &size);
    while (size != 0) {
      size_t bytes_consumed = sm_interface_->ProcessReadInput(bytes, size);
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "consumed "
              << bytes_consumed << " bytes";
      if (bytes_consumed == 0) {
        break;
      }
      read_buffer_.AdvanceReadablePtr(bytes_consumed);
      if (sm_interface_->MessageFullyRead()) {
        VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                << "HandleRequestFullyRead: Setting EPOLLOUT";
        HandleResponseFullyRead();
        events_ |= EPOLLOUT;
      } else if (sm_interface_->Error()) {
        LOG(ERROR) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                   << "Framer error detected: Setting EPOLLOUT: "
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

  void HandleResponseFullyRead() {
    sm_interface_->Cleanup();
  }

  void Notify() {
  }

  bool DoWrite() {
    size_t bytes_sent = 0;
    int flags = MSG_NOSIGNAL | MSG_DONTWAIT;
    if (fd_ == -1) {
      VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "DoWrite: fd == -1. Returning false.";
      return false;
    }
    if (output_list_.empty()) {
      VLOG(2) << log_prefix_ << "DoWrite: Output list empty.";
      if (sm_interface_) {
        sm_interface_->GetOutput();
      }
      if (output_list_.empty()) {
        events_ &= ~EPOLLOUT;
      }
    }
    while (!output_list_.empty()) {
      VLOG(2) << log_prefix_ << "DoWrite: Items in output list: "
              << output_list_.size();
      if (bytes_sent >= max_bytes_sent_per_dowrite_) {
        VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                << " byte sent >= max bytes sent per write: Setting EPOLLOUT";
        events_ |= EPOLLOUT;
        break;
      }
      if (sm_interface_ && output_list_.size() < 2) {
        sm_interface_->GetOutput();
      }
      DataFrame* data_frame = output_list_.front();
      const char*  bytes = data_frame->data;
      int size = data_frame->size;
      bytes += data_frame->index;
      size -= data_frame->index;
      DCHECK_GE(size, 0);
      if (size <= 0) {
        // Empty data frame. Indicates end of data from client.
        // Uncork the socket.
        int state = 0;
        VLOG(2) << log_prefix_ << "Empty data frame, uncorking socket.";
        setsockopt( fd_, IPPROTO_TCP, TCP_CORK, &state, sizeof( state ) );
        output_list_.pop_front();
        delete data_frame;
        continue;
      }

      flags = MSG_NOSIGNAL | MSG_DONTWAIT;
      if (output_list_.size() > 1) {
        VLOG(2) << log_prefix_ << "Outlist size: " << output_list_.size()
                << ": Adding MSG_MORE flag";
        flags |= MSG_MORE;
      }
      VLOG(2) << log_prefix_ << "Attempting to send " << size << " bytes.";
      ssize_t bytes_written = Send(bytes, size, flags);
      int stored_errno = errno;
      if (bytes_written == -1) {
        switch (stored_errno) {
          case EAGAIN:
            events_ &= ~EPOLLOUT;
            VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                    << "Got EAGAIN while writing";
            goto done;
          case EINTR:
            VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                    << "Got EINTR while writing";
            continue;
          default:
            VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
                    << "While calling send, got error: " << stored_errno
                    << ": " << (ssl_?"":strerror(stored_errno));
            goto error_or_close;
        }
      } else if (bytes_written > 0) {
        VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "Wrote: "
                << bytes_written << " bytes";
        data_frame->index += bytes_written;
        bytes_sent += bytes_written;
        continue;
      } else if (bytes_written == -2) {
        // -2 handles SSL_ERROR_WANT_* errors
        events_ &= ~EPOLLOUT;
        goto done;
      }
      VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
              << "0 bytes written with send call.";
      goto error_or_close;
    }
   done:
    return true;

   error_or_close:
    VLOG(1) << log_prefix_ << ACCEPTOR_CLIENT_IDENT
            << "DoWrite: error_or_close. Returning false "
            << "after cleaning up";
    Cleanup("DoWrite");
    return false;
  }

  friend ostream& operator<<(ostream& os, const SMConnection& c) {
    os << &c << "\n";
    return os;
  }

  void Reset() {
    VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "Resetting";
    if (ssl_) {
      SSL_shutdown(ssl_);
      PrintSslError();
      SSL_free(ssl_);
      PrintSslError();
      ssl_ = NULL;
    }
    if (registered_in_epoll_server_) {
      epoll_server_->UnregisterFD(fd_);
      registered_in_epoll_server_ = false;
    }
    if (fd_ >= 0) {
      VLOG(2) << log_prefix_ << ACCEPTOR_CLIENT_IDENT << "Closing connection";
      close(fd_);
      fd_ = -1;
    }
    read_buffer_.Clear();
    initialized_ = false;
    protocol_detected_ = false;
    events_ = 0;
    for (list<DataFrame*>::iterator i =
         output_list_.begin();
         i != output_list_.end();
         ++i) {
      delete *i;
    }
    output_list_.clear();
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
  SMConnectionInterface* connection_;
  EpollServer* epoll_server_;

  explicit OutputOrdering(SMConnectionInterface* connection) :
        first_data_senders_threshold_(kInitialDataSendersThreshold),
        connection_(connection) {
    if (connection) {
      epoll_server_ = connection->epoll_server();
    }
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
      VLOG(2) << "ON ALARM! Should now start to output...";
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
    VLOG(2) << "Moving to active!";
    first_data_senders_.push_back(mci);
    pmp->ring = &first_data_senders_;
    pmp->it = first_data_senders_.end();
    --pmp->it;
    connection_->ReadyToSend();
  }

  void AddToOutputOrder(const MemCacheIter& mci) {
    if (ExistsInPriorityMaps(mci.stream_id))
      LOG(ERROR) << "OOps, already was inserted here?!";

    double think_time_in_s = g_proxy_config.server_think_time_in_s_;
    string x_server_latency =
      mci.file_data->headers->GetHeader("X-Server-Latency").as_string();
    if (x_server_latency.size() != 0) {
      char* endp;
      double tmp_think_time_in_s = strtod(x_server_latency.c_str(), &endp);
      if (endp != x_server_latency.c_str() + x_server_latency.size()) {
        LOG(ERROR) << "Unable to understand X-Server-Latency of: "
                   << x_server_latency << " for resource: "
                   <<  mci.file_data->filename.c_str();
      } else {
        think_time_in_s = tmp_think_time_in_s;
      }
    }
    StreamIdToPriorityMap::iterator sitpmi;
    sitpmi = stream_ids_.insert(
        pair<uint32, PriorityMapPointer>(mci.stream_id,
                                         PriorityMapPointer())).first;
    PriorityMapPointer& pmp = sitpmi->second;

    BeginOutputtingAlarm* boa = new BeginOutputtingAlarm(this, &pmp, mci);
    VLOG(1) << "Server think time: " << think_time_in_s;
    epoll_server_->RegisterAlarmApproximateDelta(
        think_time_in_s * 1000000, boa);
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
      mci.max_segment_size = kSpdySegmentSize;
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

class SpdySM : public SpdyFramerVisitorInterface, public SMInterface {
 private:
  uint64 seq_num_;
  SpdyFramer* spdy_framer_;

  SMConnection* connection_;
  OutputList* client_output_list_;
  OutputOrdering client_output_ordering_;
  uint32 next_outgoing_stream_id_;
  EpollServer* epoll_server_;
  FlipAcceptor* acceptor_;
  MemoryCache* memory_cache_;
  vector<SMInterface*> server_interface_list;
  vector<int32> unused_server_interface_list;
  typedef map<uint32,SMInterface*> StreamToSmif;
  StreamToSmif stream_to_smif_;
 public:
  SpdySM(SMConnection* connection,
         SMInterface* sm_http_interface,
         EpollServer* epoll_server,
         MemoryCache* memory_cache,
         FlipAcceptor* acceptor)
         : seq_num_(0),
           spdy_framer_(new SpdyFramer),
           connection_(connection),
           client_output_list_(connection->output_list()),
           client_output_ordering_(connection),
           next_outgoing_stream_id_(2),
           epoll_server_(epoll_server),
           acceptor_(acceptor),
           memory_cache_(memory_cache) {
    spdy_framer_->set_visitor(this);
  }

  ~SpdySM() {
    delete spdy_framer_;
  }

  void InitSMInterface(SMInterface* sm_http_interface,
                       int32 server_idx) { }

  void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                        SMInterface* sm_interface,
                        EpollServer* epoll_server,
                        int fd,
                        string server_ip,
                        string server_port,
                        string remote_ip,
                        bool use_ssl) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT
            << "SpdySM: Initializing server connection.";
    connection_->InitSMConnection(connection_pool, sm_interface,
                                  epoll_server, fd, server_ip, server_port,
                                  remote_ip, use_ssl);
  }

 private:
  virtual void OnError(SpdyFramer* framer) {
    /* do nothing with this right now */
  }

  SMInterface* NewConnectionInterface() {
    SMConnection* server_connection =
      SMConnection::NewSMConnection(epoll_server_, NULL,
                                    memory_cache_, acceptor_,
                                    "http_conn: ");
    if (server_connection == NULL) {
      LOG(ERROR) << "SpdySM: Could not create server connection";
      return NULL;
    }
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Creating new HTTP interface";
    SMInterface *sm_http_interface = NewHttpSM(server_connection, this,
                                               epoll_server_, memory_cache_,
                                               acceptor_);
    return sm_http_interface;
  }

  SMInterface* FindOrMakeNewSMConnectionInterface(string server_ip,
                                                  string server_port) {
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

  int SpdyHandleNewStream(const SpdyControlFrame* frame,
                          string &http_data,
                          bool *is_https_scheme)
  {
    bool parsed_headers = false;
    SpdyHeaderBlock headers;
    const SpdySynStreamControlFrame* syn_stream =
      reinterpret_cast<const SpdySynStreamControlFrame*>(frame);

    *is_https_scheme = false;
    parsed_headers = spdy_framer_->ParseHeaderBlock(frame, &headers);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnSyn("
            << syn_stream->stream_id() << ")";
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: headers parsed?: "
            << (parsed_headers? "yes": "no");
    if (parsed_headers) {
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: # headers: "
              << headers.size();
    }
    SpdyHeaderBlock::iterator url = headers.find("url");
    SpdyHeaderBlock::iterator method = headers.find("method");
    if (url == headers.end() || method == headers.end()) {
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: didn't find method or url "
              << "or method. Not creating stream";
      return 0;
    }

    SpdyHeaderBlock::iterator scheme = headers.find("scheme");
    if (scheme->second.compare("https") == 0) {
      *is_https_scheme = true;
    }

    string uri = UrlUtilities::GetUrlPath(url->second);
    if (acceptor_->flip_handler_type_ == FLIP_HANDLER_SPDY_SERVER) {
      SpdyHeaderBlock::iterator referer = headers.find("referer");
      if (referer != headers.end() && method->second == "GET") {
        memory_cache_->UpdateHeaders(referer->second, url->second);
      }
      string host = UrlUtilities::GetUrlHost(url->second);
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Request: " << method->second
              << " " << uri;
      string filename = EncodeURL(uri, host, method->second);
      NewStream(syn_stream->stream_id(),
                reinterpret_cast<const SpdySynStreamControlFrame*>(frame)->
                priority(),
                filename);
    } else {
      SpdyHeaderBlock::iterator version = headers.find("version");
      http_data += method->second + " " + uri + " " + version->second + "\r\n";
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Request: " << method->second << " "
              << uri << " " << version->second;
      for (SpdyHeaderBlock::iterator i = headers.begin();
           i != headers.end(); ++i) {
        http_data += i->first + ": " + i->second + "\r\n";
        VLOG(2) << ACCEPTOR_CLIENT_IDENT << i->first.c_str() << ":"
                << i->second.c_str();
      }
      if (g_proxy_config.forward_ip_header_enabled_) {
        // X-Client-Cluster-IP header
        http_data += g_proxy_config.forward_ip_header_ + ": " +
                      connection_->client_ip() + "\r\n";
      }
      http_data += "\r\n";
    }

    VLOG(3) << ACCEPTOR_CLIENT_IDENT << "SpdySM: HTTP Request:\n" << http_data;
    return 1;
  }

  virtual void OnControl(const SpdyControlFrame* frame) {
    SpdyHeaderBlock headers;
    bool parsed_headers = false;
    switch (frame->type()) {
      case SYN_STREAM:
        {
        const SpdySynStreamControlFrame* syn_stream =
            reinterpret_cast<const SpdySynStreamControlFrame*>(frame);

          string http_data;
          bool is_https_scheme;
          int ret = SpdyHandleNewStream(frame, http_data, &is_https_scheme);
          if (!ret) {
            LOG(ERROR) << "SpdySM: Could not convert spdy into http.";
            break;
          }

          if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
            string server_ip;
            string server_port;
            if (is_https_scheme) {
              server_ip = acceptor_->https_server_ip_;
              server_port = acceptor_->https_server_port_;
            } else {
              server_ip = acceptor_->http_server_ip_;
              server_port = acceptor_->http_server_port_;
            }
            SMInterface *sm_http_interface =
              FindOrMakeNewSMConnectionInterface(server_ip, server_port);
            stream_to_smif_[syn_stream->stream_id()] = sm_http_interface;
            sm_http_interface->SetStreamID(syn_stream->stream_id());
            sm_http_interface->ProcessWriteInput(http_data.c_str(),
                                                 http_data.size());
          }
        }
        break;

      case SYN_REPLY:
        parsed_headers = spdy_framer_->ParseHeaderBlock(frame, &headers);
        VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnSynReply(" <<
          reinterpret_cast<const SpdySynReplyControlFrame*>(frame)->stream_id()
          << ")";
        break;
      case RST_STREAM:
        {
        const SpdyRstStreamControlFrame* rst_stream =
            reinterpret_cast<const SpdyRstStreamControlFrame*>(frame);
          VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: OnRst("
                  << rst_stream->stream_id() << ")";
          client_output_ordering_.RemoveStreamId(rst_stream ->stream_id());
        }
        break;

      default:
        LOG(ERROR) << "SpdySM: Unknown control frame type";
    }
  }
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data, size_t len) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: StreamData(" << stream_id
            << ", [" << len << "])";
    if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
          stream_to_smif_[stream_id]->ProcessWriteInput(data, len);
    }
  }

 public:
  size_t ProcessReadInput(const char* data, size_t len) {
    return spdy_framer_->ProcessInput(data, len);
  }

  size_t ProcessWriteInput(const char* data, size_t len) {
    return 0;
  }

  bool MessageFullyRead() const {
    return spdy_framer_->MessageFullyRead();
  }

  void SetStreamID(uint32 stream_id) {}

  bool Error() const {
    return spdy_framer_->HasError();
  }

  const char* ErrorAsString() const {
    DCHECK(Error());
    return SpdyFramer::ErrorCodeToString(spdy_framer_->error_code());
  }

  void Reset() {
  }

  void ResetForNewInterface(int32 server_idx) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Reset for new interface: "
            << "server_idx: " << server_idx;
    unused_server_interface_list.push_back(server_idx);
  }

  void ResetForNewConnection() {
    // seq_num is not cleared, intentionally.
    delete spdy_framer_;
    spdy_framer_ = new SpdyFramer;
    spdy_framer_->set_visitor(this);
    client_output_ordering_.Reset();
    next_outgoing_stream_id_ = 2;
  }

  // SMInterface's Cleanup is currently only called by SMConnection after a
  // protocol message as been fully read. Spdy's SMInterface does not need
  // to do any cleanup at this time.
  // TODO (klindsay) This method is probably not being used properly and
  // some logic review and method renaming is probably in order.
  void Cleanup() {}

  // Send a settings frame
  int PostAcceptHook() {
    ssize_t bytes_written;
    spdy::SpdySettings settings;
    spdy::SettingsFlagsAndId settings_id(0);
    settings_id.set_id(spdy::SETTINGS_MAX_CONCURRENT_STREAMS);
    settings.push_back(spdy::SpdySetting(settings_id, 100));
    scoped_ptr<SpdySettingsControlFrame>
      settings_frame(spdy_framer_->CreateSettings(settings));

    char* bytes = settings_frame->data();
    size_t size = SpdyFrame::size() + settings_frame->length();
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Sending Settings Frame";
    bytes_written = connection_->Send(bytes, size,
                                      MSG_NOSIGNAL | MSG_DONTWAIT);
    if (static_cast<size_t>(bytes_written) != size) {
      LOG(ERROR) << "Trouble sending SETTINGS frame! (" << errno << ")";
      if (errno == EAGAIN) {
        return 0;
      }
    }
    return 1;
  }

  void AddAssociatedContent(FileData* file_data) {
    for (unsigned int i = 0; i < file_data->related_files.size(); ++i) {
      pair<int, string>& related_file = file_data->related_files[i];
      MemCacheIter mci;
      string filename  = "GET_";
      filename += related_file.second;
      if (!memory_cache_->AssignFileData(filename, &mci)) {
        VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Unable to find associated "
                << "content for: " << filename;
        continue;
      }
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Adding associated content: "
              << filename;
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
    if (acceptor_->flip_handler_type_ == FLIP_HANDLER_SPDY_SERVER) {
      if (!memory_cache_->AssignFileData(filename, &mci)) {
        // error creating new stream.
        VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Sending ErrorNotFound";
        SendErrorNotFound(stream_id);
      } else {
        AddToOutputOrder(mci);
        if (FLAGS_use_xac) {
          AddAssociatedContent(mci.file_data);
        }
      }
    } else {
      AddToOutputOrder(mci);
    }
  }

  void AddToOutputOrder(const MemCacheIter& mci) {
    client_output_ordering_.AddToOutputOrder(mci);
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
    SpdyDataFlags spdy_flags = static_cast<SpdyDataFlags>(flags);
    SendDataFrameImpl(stream_id, data, len, spdy_flags, compress);
  }

  SpdyFramer* spdy_framer() { return spdy_framer_; }

 private:
  void SendEOFImpl(uint32 stream_id) {
    SendDataFrame(stream_id, NULL, 0, DATA_FLAG_FIN, false);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending EOF: " << stream_id;
    KillStream(stream_id);
  }

  void SendErrorNotFoundImpl(uint32 stream_id) {
    BalsaHeaders my_headers;
    my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "404", "Not Found");
    SendSynReplyImpl(stream_id, my_headers);
    SendDataFrame(stream_id, "wtf?", 4, DATA_FLAG_FIN, false);
    client_output_ordering_.RemoveStreamId(stream_id);
  }

  void SendOKResponseImpl(uint32 stream_id, string* output) {
    BalsaHeaders my_headers;
    my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "200", "OK");
    SendSynReplyImpl(stream_id, my_headers);
    SendDataFrame(
        stream_id, output->c_str(), output->size(), DATA_FLAG_FIN, false);
    client_output_ordering_.RemoveStreamId(stream_id);
  }

  void KillStream(uint32 stream_id) {
    client_output_ordering_.RemoveStreamId(stream_id);
  }

  void CopyHeaders(SpdyHeaderBlock& dest, const BalsaHeaders& headers) {
    for (BalsaHeaders::const_header_lines_iterator hi =
         headers.header_lines_begin();
         hi != headers.header_lines_end();
         ++hi) {
      SpdyHeaderBlock::iterator fhi = dest.find(hi->first.as_string());
      if (fhi == dest.end()) {
        dest[hi->first.as_string()] = hi->second.as_string();
      } else {
        dest[hi->first.as_string()] = (
            string(fhi->second.data(), fhi->second.size()) + "," +
            string(hi->second.data(), hi->second.size()));
      }
    }

    // These headers have no value
    dest.erase("X-Associated-Content");  // TODO(mbelshe): case-sensitive
    dest.erase("X-Original-Url");  // TODO(mbelshe): case-sensitive
  }

  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers) {
    SpdyHeaderBlock block;
    block["method"] = headers.request_method().as_string();
    if (!headers.HasHeader("status"))
      block["status"] = headers.response_code().as_string();
    if (!headers.HasHeader("version"))
      block["version"] =headers.response_version().as_string();
    if (headers.HasHeader("X-Original-Url")) {
      string original_url = headers.GetHeader("X-Original-Url").as_string();
      block["path"] = UrlUtilities::GetUrlPath(original_url);
    } else {
      block["path"] = headers.request_uri().as_string();
    }
    CopyHeaders(block, headers);

    SpdySynStreamControlFrame* fsrcf =
      spdy_framer_->CreateSynStream(stream_id, 0, 0, CONTROL_FLAG_NONE, true,
                                    &block);
    SpdyFrameDataFrame* df = new SpdyFrameDataFrame;
    df->size = fsrcf->length() + SpdyFrame::size();
    size_t df_size = df->size;
    df->data = fsrcf->data();
    df->frame = fsrcf;
    df->delete_when_done = true;
    EnqueueDataFrame(df);

    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending SynStreamheader "
            << stream_id;
    return df_size;
  }

  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers) {
    SpdyHeaderBlock block;
    CopyHeaders(block, headers);
    block["status"] = headers.response_code().as_string() + " " +
                      headers.response_reason_phrase().as_string();
    block["version"] = headers.response_version().as_string();

    SpdySynReplyControlFrame* fsrcf =
      spdy_framer_->CreateSynReply(stream_id, CONTROL_FLAG_NONE, true, &block);
    SpdyFrameDataFrame* df = new SpdyFrameDataFrame;
    df->size = fsrcf->length() + SpdyFrame::size();
    size_t df_size = df->size;
    df->data = fsrcf->data();
    df->frame = fsrcf;
    df->delete_when_done = true;
    EnqueueDataFrame(df);

    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending SynReplyheader "
            << stream_id;
    return df_size;
  }

  void SendDataFrameImpl(uint32 stream_id, const char* data, int64 len,
                         SpdyDataFlags flags, bool compress) {
    // Force compression off if disabled via command line.
    if (!FLAGS_use_compression)
      flags = static_cast<SpdyDataFlags>(flags & ~DATA_FLAG_COMPRESSED);

    // TODO(mbelshe):  We can't compress here - before going into the
    //                 priority queue.  Compression needs to be done
    //                 with late binding.
    if (len == 0) {
      SpdyDataFrame* fdf = spdy_framer_->CreateDataFrame(stream_id, data, len,
                                                         flags);
      SpdyFrameDataFrame* df = new SpdyFrameDataFrame;
      df->size = fdf->length() + SpdyFrame::size();
      df->data = fdf->data();
      df->delete_when_done = true;
      EnqueueDataFrame(df);
      return;
    }

    // Chop data frames into chunks so that one stream can't monopolize the
    // output channel.
    while(len > 0) {
      int64 size = std::min(len, static_cast<int64>(kSpdySegmentSize));
      SpdyDataFlags chunk_flags = flags;

      // If we chunked this block, and the FIN flag was set, there is more
      // data coming.  So, remove the flag.
      if ((size < len) && (flags & DATA_FLAG_FIN))
        chunk_flags = static_cast<SpdyDataFlags>(chunk_flags & ~DATA_FLAG_FIN);

      SpdyDataFrame* fdf = spdy_framer_->CreateDataFrame(stream_id, data, size,
                                                         chunk_flags);
      SpdyFrameDataFrame* df = new SpdyFrameDataFrame;
      df->size = fdf->length() + SpdyFrame::size();
      df->data = fdf->data();
      df->delete_when_done = true;
      EnqueueDataFrame(df);

      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "SpdySM: Sending data frame "
              << stream_id << " [" << size << "] shrunk to " << fdf->length()
              << ", flags=" << flags;

      data += size;
      len -= size;
    }
  }

  void EnqueueDataFrame(DataFrame* df) {
    connection_->EnqueueDataFrame(df);
  }

  void GetOutput() {
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
          string content_type =
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
};

////////////////////////////////////////////////////////////////////////////////

class HttpSM : public BalsaVisitorInterface, public SMInterface {
 private:
  uint64 seq_num_;
  BalsaFrame* http_framer_;
  BalsaHeaders headers_;
  uint32 stream_id_;
  int32 server_idx_;

  SMConnection* connection_;
  SMInterface* sm_spdy_interface_;
  OutputList* output_list_;
  OutputOrdering output_ordering_;
  MemoryCache* memory_cache_;
  FlipAcceptor* acceptor_;
 public:
  explicit HttpSM(SMConnection* connection,
                  SMInterface* sm_spdy_interface,
                  EpollServer* epoll_server,
                  MemoryCache* memory_cache,
                  FlipAcceptor* acceptor) :
      seq_num_(0),
      http_framer_(new BalsaFrame),
      stream_id_(0),
      server_idx_(-1),
      connection_(connection),
      sm_spdy_interface_(sm_spdy_interface),
      output_list_(connection->output_list()),
      output_ordering_(connection),
      memory_cache_(connection->memory_cache()),
      acceptor_(acceptor) {
    http_framer_->set_balsa_visitor(this);
    http_framer_->set_balsa_headers(&headers_);
    if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
      http_framer_->set_is_request(false);
    }
  }
 private:
  typedef map<string, uint32> ClientTokenMap;
 private:
    virtual void ProcessBodyInput(const char *input, size_t size) {
    }
    virtual void ProcessBodyData(const char *input, size_t size) {
      if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
        VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Process Body Data: stream "
                << stream_id_ << ": size " << size;
        sm_spdy_interface_->SendDataFrame(stream_id_, input, size, 0, false);
      }
    }
    virtual void ProcessHeaderInput(const char *input, size_t size) {
    }
    virtual void ProcessTrailerInput(const char *input, size_t size) {}
    virtual void ProcessHeaders(const BalsaHeaders& headers) {
      if (acceptor_->flip_handler_type_ == FLIP_HANDLER_HTTP_SERVER) {
        string host =
          UrlUtilities::GetUrlHost(headers.GetHeader("Host").as_string());
        string method = headers.request_method().as_string();
        VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Received Request: "
                << headers.request_uri().as_string() << " " << method;
        string filename = EncodeURL(headers.request_uri().as_string(),
                                    host, method);
        NewStream(stream_id_, 0, filename);
        stream_id_ += 2;
      } else {
        VLOG(1) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Received Response from "
                << connection_->server_ip_ << ":"
                << connection_->server_port_ << " ";
        sm_spdy_interface_->SendSynReply(stream_id_, headers);
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
      if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
        VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: MessageDone. Sending EOF: "
                << "stream " << stream_id_;
        sm_spdy_interface_->SendEOF(stream_id_);
      } else {
        VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: MessageDone.";
      }
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
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Error detected";
    }

 public:
  ~HttpSM() {
    Reset();
    delete http_framer_;
  }

  void InitSMInterface(SMInterface* sm_spdy_interface,
                       int32 server_idx)
  {
    sm_spdy_interface_ = sm_spdy_interface;
    server_idx_ = server_idx;
  }

  void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                        SMInterface* sm_interface,
                        EpollServer* epoll_server,
                        int fd,
                        string server_ip,
                        string server_port,
                        string remote_ip,
                        bool use_ssl)
  {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Initializing server "
            << "connection.";
    connection_->InitSMConnection(connection_pool, sm_interface,
                                  epoll_server, fd, server_ip, server_port,
                                  remote_ip, use_ssl);
  }

  size_t ProcessReadInput(const char* data, size_t len) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Process read input: stream "
            << stream_id_;
    return http_framer_->ProcessInput(data, len);
  }

  size_t ProcessWriteInput(const char* data, size_t len) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Process write input: size "
            << len << ": stream " << stream_id_;
    char * dataPtr = new char[len];
    memcpy(dataPtr, data, len);
    DataFrame* data_frame = new DataFrame;
    data_frame->data = (const char *)dataPtr;
    data_frame->size = len;
    data_frame->delete_when_done = true;
    connection_->EnqueueDataFrame(data_frame);
    return len;
  }

  bool MessageFullyRead() const {
    return http_framer_->MessageFullyRead();
  }

  void SetStreamID(uint32 stream_id) {
    stream_id_ = stream_id;
  }

  bool Error() const {
    return http_framer_->Error();
  }

  const char* ErrorAsString() const {
    return BalsaFrameEnums::ErrorCodeToString(http_framer_->ErrorCode());
  }

  void Reset() {
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Reset: stream "
            << stream_id_;
    http_framer_->Reset();
  }

  void ResetForNewInterface(int32 server_idx) {
  }

  void ResetForNewConnection() {
    if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Server connection closing "
        << "to: " << connection_->server_ip_ << ":"
        << connection_->server_port_ << " ";
    }
    seq_num_ = 0;
    output_ordering_.Reset();
    http_framer_->Reset();
    if (sm_spdy_interface_) {
      sm_spdy_interface_->ResetForNewInterface(server_idx_);
    }
  }

  void Cleanup() {
    if (!(acceptor_->flip_handler_type_ == FLIP_HANDLER_HTTP_SERVER)) {
      connection_->Cleanup("HttpSM Request Fully Read: stream_id " +
                           stream_id_);
    }
  }

  int PostAcceptHook() {
    return 1;
  }

  void NewStream(uint32 stream_id, uint32 priority, const string& filename) {
    MemCacheIter mci;
    mci.stream_id = stream_id;
    mci.priority = priority;
    if (!memory_cache_->AssignFileData(filename, &mci)) {
      // error creating new stream.
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Sending ErrorNotFound";
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
    if (acceptor_->flip_handler_type_ == FLIP_HANDLER_PROXY) {
      sm_spdy_interface_->ResetForNewInterface(server_idx_);
    }
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

  BalsaFrame* spdy_framer() { return http_framer_; }

 private:
  void SendEOFImpl(uint32 stream_id) {
    DataFrame* df = new DataFrame;
    df->data = "0\r\n\r\n";
    df->size = 5;
    df->delete_when_done = false;
    EnqueueDataFrame(df);
    if (acceptor_->flip_handler_type_ == FLIP_HANDLER_HTTP_SERVER) {
      Reset();
    }
  }

  void SendErrorNotFoundImpl(uint32 stream_id) {
    BalsaHeaders my_headers;
    my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "404", "Not Found");
    my_headers.RemoveAllOfHeader("content-length");
    my_headers.AppendHeader("transfer-encoding", "chunked");
    SendSynReplyImpl(stream_id, my_headers);
    SendDataFrame(stream_id, "wtf?", 4, 0, false);
    SendEOFImpl(stream_id);
    output_ordering_.RemoveStreamId(stream_id);
  }

  void SendOKResponseImpl(uint32 stream_id, string* output) {
    BalsaHeaders my_headers;
    my_headers.SetFirstlineFromStringPieces("HTTP/1.1", "200", "OK");
    my_headers.RemoveAllOfHeader("content-length");
    my_headers.AppendHeader("transfer-encoding", "chunked");
    SendSynReplyImpl(stream_id, my_headers);
    SendDataFrame(stream_id, output->c_str(), output->size(), 0, false);
    SendEOFImpl(stream_id);
    output_ordering_.RemoveStreamId(stream_id);
  }

  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers) {
    SimpleBuffer sb;
    headers.WriteHeaderAndEndingToBuffer(&sb);
    DataFrame* df = new DataFrame;
    df->size = sb.ReadableBytes();
    char* buffer = new char[df->size];
    df->data = buffer;
    df->delete_when_done = true;
    sb.Read(buffer, df->size);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Sending HTTP Reply header "
            << stream_id_;
    size_t df_size = df->size;
    EnqueueDataFrame(df);
    return df_size;
  }

  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers) {
    SimpleBuffer sb;
    headers.WriteHeaderAndEndingToBuffer(&sb);
    DataFrame* df = new DataFrame;
    df->size = sb.ReadableBytes();
    char* buffer = new char[df->size];
    df->data = buffer;
    df->delete_when_done = true;
    sb.Read(buffer, df->size);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Sending HTTP Reply header "
            << stream_id_;
    size_t df_size = df->size;
    EnqueueDataFrame(df);
    return df_size;
  }

  void SendDataFrameImpl(uint32 stream_id, const char* data, int64 len,
                         uint32 flags, bool compress) {
    char chunk_buf[128];
    snprintf(chunk_buf, sizeof(chunk_buf), "%x\r\n", (unsigned int)len);
    string chunk_description(chunk_buf);
    DataFrame* df = new DataFrame;
    df->size = chunk_description.size() + len + 2;
    char* buffer = new char[df->size];
    df->data = buffer;
    df->delete_when_done = true;
    memcpy(buffer, chunk_description.data(), chunk_description.size());
    memcpy(buffer + chunk_description.size(), data, len);
    memcpy(buffer + chunk_description.size() + len, "\r\n", 2);
    EnqueueDataFrame(df);
  }

  void EnqueueDataFrame(DataFrame* df) {
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: Enqueue data frame: stream "
            << stream_id_;
    connection_->EnqueueDataFrame(df);
  }

  void GetOutput() {
    MemCacheIter* mci = output_ordering_.GetIter();
    if (mci == NULL) {
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: GetOutput: nothing to "
              << "output!?: stream " << stream_id_;
      return;
    }
    if (!mci->transformed_header) {
      mci->bytes_sent = SendSynReply(mci->stream_id,
                                     *(mci->file_data->headers));
      mci->transformed_header = true;
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: GetOutput transformed "
              << "header stream_id: [" << mci->stream_id << "]";
      return;
    }
    if (mci->body_bytes_consumed >= mci->file_data->body.size()) {
      SendEOF(mci->stream_id);
      output_ordering_.RemoveStreamId(mci->stream_id);
      VLOG(2) << ACCEPTOR_CLIENT_IDENT << "GetOutput remove_stream_id: ["
              << mci->stream_id << "]";
      return;
    }
    size_t num_to_write =
      mci->file_data->body.size() - mci->body_bytes_consumed;
    if (num_to_write > mci->max_segment_size)
      num_to_write = mci->max_segment_size;

    SendDataFrame(mci->stream_id,
                  mci->file_data->body.data() + mci->body_bytes_consumed,
                  num_to_write, 0, true);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "HttpSM: GetOutput SendDataFrame["
            << mci->stream_id << "]: " << num_to_write;
    mci->body_bytes_consumed += num_to_write;
    mci->bytes_sent += num_to_write;
  }
};

////////////////////////////////////////////////////////////////////////////////

class StreamerSM : public SMInterface {
 private:
   SMConnection* connection_;
   SMInterface* sm_other_interface_;
   EpollServer* epoll_server_;
   FlipAcceptor* acceptor_;
 public:
   explicit StreamerSM(SMConnection* connection,
                       SMInterface* sm_other_interface,
                       EpollServer* epoll_server,
                       FlipAcceptor* acceptor) :
   connection_(connection),
   sm_other_interface_(sm_other_interface),
   epoll_server_(epoll_server),
   acceptor_(acceptor)
   {
     VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Creating StreamerSM object";
   }
   ~StreamerSM() {
     VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Destroying StreamerSM object";
     Reset();
   }

   void InitSMInterface(SMInterface* sm_other_interface,
                        int32 server_idx)
   {
     sm_other_interface_ = sm_other_interface;
   }

   void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                         SMInterface* sm_interface,
                         EpollServer* epoll_server,
                         int fd,
                         string server_ip,
                         string server_port,
                         string remote_ip,
                         bool use_ssl)
   {
     VLOG(2) << ACCEPTOR_CLIENT_IDENT << "StreamerSM: Initializing server "
             << "connection.";
     connection_->InitSMConnection(connection_pool, sm_interface,
                                   epoll_server, fd, server_ip,
                                   server_port, remote_ip, use_ssl);
   }

   size_t ProcessReadInput(const char* data, size_t len) {
     return sm_other_interface_->ProcessWriteInput(data, len);
   }

   size_t ProcessWriteInput(const char* data, size_t len) {
     char * dataPtr = new char[len];
     memcpy(dataPtr, data, len);
     DataFrame* df = new DataFrame;
     df->data = (const char *)dataPtr;
     df->size = len;
     df->delete_when_done = true;
     connection_->EnqueueDataFrame(df);
     return len;
   }

   bool MessageFullyRead() const {
     return false;
   }

   void SetStreamID(uint32 stream_id) {}

   bool Error() const {
     return false;
   }

   const char* ErrorAsString() const {
     return "(none)";
   }

   void Reset() {
     VLOG(1) << ACCEPTOR_CLIENT_IDENT << "StreamerSM: Reset";
     connection_->Cleanup("Server Reset");
   }

   void ResetForNewInterface(int32 server_idx) {
   }

   void ResetForNewConnection() {
     sm_other_interface_->Reset();
   }

   void Cleanup() {
   }

   int PostAcceptHook() {
     if (!sm_other_interface_) {
       SMConnection *server_connection =
         SMConnection::NewSMConnection(epoll_server_, NULL, NULL,
                                       acceptor_, "server_conn: ");
       if (server_connection == NULL) {
         LOG(ERROR) << "StreamerSM: Could not create server conenction.";
         return 0;
       }
       VLOG(2) << ACCEPTOR_CLIENT_IDENT << "StreamerSM: Creating new server "
               << "connection.";
       sm_other_interface_ = new StreamerSM(server_connection, this,
                                            epoll_server_, acceptor_);
       sm_other_interface_->InitSMInterface(this, 0);
     }
     // The Streamer interface is used to stream HTTPS connections, so we
     // will always use the https_server_ip/port here.
     sm_other_interface_->InitSMConnection(NULL, sm_other_interface_,
                                           epoll_server_, -1,
                                           acceptor_->https_server_ip_,
                                           acceptor_->https_server_port_,
                                           "",
                                           false);

     return 1;
   }

   void NewStream(uint32 stream_id, uint32 priority, const string& filename) {
   }

   void AddToOutputOrder(const MemCacheIter& mci) {
   }

   void SendEOF(uint32 stream_id) {
   }

   void SendErrorNotFound(uint32 stream_id) {
   }

   void SendOKResponse(uint32 stream_id, string output) {
   }

   size_t SendSynStream(uint32 stream_id, const BalsaHeaders& headers) {
     return 0;
   }

   size_t SendSynReply(uint32 stream_id, const BalsaHeaders& headers) {
     return 0;
   }

   void SendDataFrame(uint32 stream_id, const char* data, int64 len,
                      uint32 flags, bool compress) {
   }

 private:
   void SendEOFImpl(uint32 stream_id) {
   }

   void SendErrorNotFoundImpl(uint32 stream_id) {
   }

   void SendOKResponseImpl(uint32 stream_id, string* output) {
   }

   size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers) {
     return 0;
   }

   size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers) {
     return 0;
   }

   void SendDataFrameImpl(uint32 stream_id, const char* data, int64 len,
                          uint32 flags, bool compress) {
   }

   void GetOutput() {
   }
};

////////////////////////////////////////////////////////////////////////////////

class Notification {
 public:
   explicit Notification(bool value) : value_(value) {}

   void Notify() {
     AutoLock al(lock_);
     value_ = true;
   }
   bool HasBeenNotified() {
     AutoLock al(lock_);
     return value_;
   }
   bool value_;
   Lock lock_;
};

////////////////////////////////////////////////////////////////////////////////

class SMAcceptorThread : public SimpleThread,
                         public EpollCallbackInterface,
                         public SMConnectionPoolInterface {
  EpollServer epoll_server_;
  FlipAcceptor *acceptor_;
  SSLState *ssl_state_;
  bool use_ssl_;

  vector<SMConnection*> unused_server_connections_;
  vector<SMConnection*> tmp_unused_server_connections_;
  vector<SMConnection*> allocated_server_connections_;
  list<SMConnection*> active_server_connections_;
  Notification quitting_;
  MemoryCache* memory_cache_;
 public:

  SMAcceptorThread(FlipAcceptor *acceptor,
                   MemoryCache* memory_cache) :
      SimpleThread("SMAcceptorThread"),
      acceptor_(acceptor),
      ssl_state_(NULL),
      use_ssl_(false),
      quitting_(false),
      memory_cache_(memory_cache)
  {
    if (!acceptor->ssl_cert_filename_.empty() &&
        !acceptor->ssl_cert_filename_.empty()) {
      ssl_state_ = new SSLState;
      bool use_npn = true;
      if (acceptor_->flip_handler_type_ == FLIP_HANDLER_HTTP_SERVER) {
        use_npn = false;
      }
      spdy_init_ssl(ssl_state_, acceptor_->ssl_cert_filename_,
                    acceptor_->ssl_key_filename_, use_npn);
      use_ssl_ = true;
    }
  }

  ~SMAcceptorThread() {
    for (vector<SMConnection*>::iterator i =
         allocated_server_connections_.begin();
         i != allocated_server_connections_.end();
         ++i) {
      delete *i;
    }
    delete ssl_state_;
  }

  SMConnection* NewConnection() {
    SMConnection* server =
      SMConnection::NewSMConnection(&epoll_server_, ssl_state_,
                                    memory_cache_, acceptor_,
                                    "client_conn: ");
    allocated_server_connections_.push_back(server);
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Acceptor: Making new server.";
    return server;
  }

  SMConnection* FindOrMakeNewSMConnection() {
    if (unused_server_connections_.empty()) {
      return NewConnection();
    }
    SMConnection* server = unused_server_connections_.back();
    unused_server_connections_.pop_back();
    VLOG(2) << ACCEPTOR_CLIENT_IDENT << "Acceptor: Reusing server.";
    return server;
  }

  void InitWorker() {
    epoll_server_.RegisterFD(acceptor_->listen_fd_, this, EPOLLIN | EPOLLET);
  }

  void HandleConnection(int server_fd, struct sockaddr_in *remote_addr) {
    int on = 1;
    int rc;
    if (acceptor_->disable_nagle_) {
      rc = setsockopt(server_fd, IPPROTO_TCP,  TCP_NODELAY,
                      reinterpret_cast<char*>(&on), sizeof(on));
      if (rc < 0) {
        close(server_fd);
        LOG(ERROR) << "setsockopt() failed fd=" + server_fd;
        return;
      }
    }

    SMConnection* server_connection = FindOrMakeNewSMConnection();
    if (server_connection == NULL) {
      VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Acceptor: Closing fd " << server_fd;
      close(server_fd);
      return;
    }
    string remote_ip = inet_ntoa(remote_addr->sin_addr);
    server_connection->InitSMConnection(this,
                                        NULL,
                                        &epoll_server_,
                                        server_fd,
                                        "", "", remote_ip,
                                        use_ssl_);
    if (server_connection->initialized())
      active_server_connections_.push_back(server_connection);
  }

  void AcceptFromListenFD() {
    if (acceptor_->accepts_per_wake_ > 0) {
      for (int i = 0; i < acceptor_->accepts_per_wake_; ++i) {
        struct sockaddr address;
        socklen_t socklen = sizeof(address);
        int fd = accept(acceptor_->listen_fd_, &address, &socklen);
        if (fd == -1) {
          if (errno != 11) {
            VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Acceptor: accept fail("
                    << acceptor_->listen_fd_ << "): " << errno << ": "
                    << strerror(errno);
          }
          break;
        }
        VLOG(1) << ACCEPTOR_CLIENT_IDENT << " Accepted connection";
        HandleConnection(fd, (struct sockaddr_in *)&address);
      }
    } else {
      while (true) {
        struct sockaddr address;
        socklen_t socklen = sizeof(address);
        int fd = accept(acceptor_->listen_fd_, &address, &socklen);
        if (fd == -1) {
          if (errno != 11) {
            VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Acceptor: accept fail("
                    << acceptor_->listen_fd_ << "): " << errno << ": "
                    << strerror(errno);
          }
          break;
        }
        VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Accepted connection";
        HandleConnection(fd, (struct sockaddr_in *)&address);
      }
    }
  }

  // EpollCallbackInteface virtual functions.
  virtual void OnRegistration(EpollServer* eps, int fd, int event_mask) { }
  virtual void OnModification(int fd, int event_mask) { }
  virtual void OnEvent(int fd, EpollEvent* event) {
    if (event->in_events | EPOLLIN) {
      VLOG(2) << ACCEPTOR_CLIENT_IDENT
              << "Acceptor: Accepting based upon epoll events";
      AcceptFromListenFD();
    }
  }
  virtual void OnUnregistration(int fd, bool replaced) { }
  virtual void OnShutdown(EpollServer* eps, int fd) { }

  void Quit() {
    quitting_.Notify();
  }

 // Iterates through a list of active connections expiring any that have been
 // idle longer than the configured timeout.
 void HandleConnectionIdleTimeout() {
   int cur_time = time(NULL);
   static time_t oldest_time = cur_time;
   // Only iterate the list if we speculate that a connection is ready to be
   // expired
   if ((cur_time - oldest_time) < g_proxy_config.idle_timeout_s_)
     return;
   list<SMConnection*>::iterator iter = active_server_connections_.begin();
   while (iter != active_server_connections_.end()) {
     SMConnection *conn = *iter;
     int elapsed_time = (cur_time - conn->last_read_time_);
     if (elapsed_time > g_proxy_config.idle_timeout_s_) {
       conn->Cleanup("Connection idle timeout reached.");
       iter = active_server_connections_.erase(iter);
       continue;
     }
     if (conn->last_read_time_ < oldest_time)
       oldest_time = conn->last_read_time_;
     iter++;
   }
   if ((cur_time - oldest_time) >= g_proxy_config.idle_timeout_s_)
     oldest_time = cur_time;
 }

  void Run() {
    while (!quitting_.HasBeenNotified()) {
      epoll_server_.set_timeout_in_us(10 * 1000);  // 10 ms
      epoll_server_.WaitForEventsAndExecuteCallbacks();
      unused_server_connections_.insert(unused_server_connections_.end(),
                                        tmp_unused_server_connections_.begin(),
                                        tmp_unused_server_connections_.end());
      tmp_unused_server_connections_.clear();
      HandleConnectionIdleTimeout();
    }
  }

  // SMConnections will use this:
  virtual void SMConnectionDone(SMConnection* sc) {
    VLOG(1) << ACCEPTOR_CLIENT_IDENT << "Done with connection.";
    tmp_unused_server_connections_.push_back(sc);
  }
};

////////////////////////////////////////////////////////////////////////////////

SMInterface* NewStreamerSM(SMConnection* connection,
                           SMInterface* sm_interface,
                           EpollServer* epoll_server,
                           FlipAcceptor* acceptor) {
  return new StreamerSM(connection, sm_interface, epoll_server, acceptor);
}


SMInterface* NewSpdySM(SMConnection* connection,
                       SMInterface* sm_interface,
                       EpollServer* epoll_server,
                       MemoryCache* memory_cache,
                       FlipAcceptor* acceptor) {
  return new SpdySM(connection, sm_interface, epoll_server,
                    memory_cache, acceptor);
}

SMInterface* NewHttpSM(SMConnection* connection,
                       SMInterface* sm_interface,
                       EpollServer* epoll_server,
                       MemoryCache* memory_cache,
                       FlipAcceptor* acceptor) {
  return new HttpSM(connection, sm_interface, epoll_server,
                    memory_cache, acceptor);
}

////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> &split(const std::string &s,
                                char delim,
                                std::vector<std::string> &elems) {
  std::stringstream ss(s);
  std::string item;
  while(std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  return split(s, delim, elems);
}

bool GotQuitFromStdin() {
  // Make stdin nonblocking. Yes this is done each time. Oh well.
  fcntl(0, F_SETFL, O_NONBLOCK);
  char c;
  string maybequit;
  while (read(0, &c, 1) > 0) {
    maybequit += c;
  }
  if (maybequit.size()) {
    VLOG(1) << "scanning string: \"" << maybequit << "\"";
  }
  return (maybequit.size() > 1 &&
          (maybequit.c_str()[0] == 'q' ||
           maybequit.c_str()[0] == 'Q'));
}

const char* BoolToStr(bool b) {
  if (b)
    return "true";
  return "false";
}

////////////////////////////////////////////////////////////////////////////////

static bool wantExit = false;
static bool wantLogClose = false;
void SignalHandler(int signum)
{
  switch(signum) {
    case SIGTERM:
    case SIGINT:
      wantExit = true;
      break;
    case SIGHUP:
      wantLogClose = true;
      break;
  }
}

static int OpenPidFile(const char *pidfile)
{
  int fd;
  struct stat pid_stat;
  int ret;

  fd = open(pidfile, O_RDWR | O_CREAT, 0600);
  if (fd == -1) {
      cerr << "Could not open pid file '" << pidfile << "' for reading.\n";
      exit(1);
  }

  ret = flock(fd, LOCK_EX | LOCK_NB);
  if (ret == -1) {
    if (errno == EWOULDBLOCK) {
      cerr << "Flip server is already running.\n";
    } else {
      cerr << "Error getting lock on pid file: " << strerror(errno) << "\n";
    }
    exit(1);
  }

  if (fstat(fd, &pid_stat) == -1) {
    cerr << "Could not stat pid file '" << pidfile << "': " << strerror(errno)
         << "\n";
  }
  if (pid_stat.st_size != 0) {
    if (ftruncate(fd, pid_stat.st_size) == -1) {
      cerr << "Could not truncate pid file '" << pidfile << "': "
           << strerror(errno) << "\n";
    }
  }

  char pid_str[8];
  snprintf(pid_str, sizeof(pid_str), "%d", getpid());
  int bytes = static_cast<int>(strlen(pid_str));
  if (write(fd, pid_str, strlen(pid_str)) != bytes) {
    cerr << "Could not write pid file: " << strerror(errno) << "\n";
    close(fd);
    exit(1);
  }

  return fd;
}

int main (int argc, char**argv)
{
  unsigned int i = 0;
  bool wait_for_iface = false;
  int pidfile_fd;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGTERM, SignalHandler);
  signal(SIGINT, SignalHandler);
  signal(SIGHUP, SignalHandler);

  CommandLine::Init(argc, argv);
  CommandLine cl(argc, argv);

  if (cl.HasSwitch("help") || argc < 2) {
    cout << argv[0] << " <options>\n";
    cout << "  Proxy options:\n";
    cout << "\t--proxy<1..n>=\"<listen ip>,<listen port>,"
         << "<ssl cert filename>,\n"
         << "\t               <ssl key filename>,<http server ip>,"
         << "<http server port>,\n"
         << "\t               [https server ip],[https server port],"
         << "<spdy only 0|1>\"\n";
    cout << "\t  * The https server ip and port may be left empty if they are"
         << " the same as\n"
         << "\t    the http server fields.\n";
    cout << "\t  * spdy only prevents non-spdy https connections from being"
         << " passed\n"
         << "\t    through the proxy listen ip:port.\n";
    cout << "\t--forward-ip-header=<header name>\n";
    cout << "\n  Server options:\n";
    cout << "\t--spdy-server=\"<listen ip>,<listen port>,[ssl cert filename],"
         << "\n\t               [ssl key filename]\"\n";
    cout << "\t--http-server=\"<listen ip>,<listen port>,[ssl cert filename],"
         << "\n\t               [ssl key filename]\"\n";
    cout << "\t  * Leaving the ssl cert and key fields empty will disable ssl"
         << " for the\n"
         << "\t    http and spdy flip servers\n";
    cout << "\n  Global options:\n";
    cout << "\t--logdest=<file|system|both>\n";
    cout << "\t--logfile=<logfile>\n";
    cout << "\t--wait-for-iface\n";
    cout << "\t  * The flip server will block until the listen ip has been"
         << " raised.\n";
    cout << "\t--ssl-session-expiry=<seconds> (default is 300)\n";
    cout << "\t--ssl-disable-compression\n";
    cout << "\t--idle-timeout=<seconds> (default is 300)\n";
    cout << "\t--pidfile=<filepath> (default /var/run/flip-server.pid)\n";
    cout << "\t--help\n";
    exit(0);
  }

  if (cl.HasSwitch("pidfile")) {
    pidfile_fd = OpenPidFile(cl.GetSwitchValueASCII("pidfile").c_str());
  } else {
    pidfile_fd = OpenPidFile(PIDFILE);
  }

  g_proxy_config.server_think_time_in_s_ = FLAGS_server_think_time_in_s;

  if (cl.HasSwitch("forward-ip-header")) {
    g_proxy_config.forward_ip_header_enabled_ = true;
    g_proxy_config.forward_ip_header_ =
      cl.GetSwitchValueASCII("forward-ip-header");
  } else {
    g_proxy_config.forward_ip_header_enabled_ = false;
  }

  if (cl.HasSwitch("logdest")) {
    string log_dest_value = cl.GetSwitchValueASCII("logdest");
    if (log_dest_value.compare("file") == 0) {
      g_proxy_config.log_destination_ = logging::LOG_ONLY_TO_FILE;
    } else if (log_dest_value.compare("system") == 0) {
      g_proxy_config.log_destination_ = logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG;
    } else if (log_dest_value.compare("both") == 0) {
      g_proxy_config.log_destination_ =
        logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
    } else {
      LOG(FATAL) << "Invalid logging destination value: " << log_dest_value;
    }
  } else {
    g_proxy_config.log_destination_ = logging::LOG_NONE;
  }

  if (cl.HasSwitch("logfile")) {
    g_proxy_config.log_filename_ = cl.GetSwitchValueASCII("logfile");
    if (g_proxy_config.log_destination_ == logging::LOG_NONE) {
      g_proxy_config.log_destination_ = logging::LOG_ONLY_TO_FILE;
    }
  } else if (g_proxy_config.log_destination_ == logging::LOG_ONLY_TO_FILE ||
             g_proxy_config.log_destination_ ==
             logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG) {
    LOG(FATAL) << "Logging destination requires a log file to be specified.";
  }

  if (cl.HasSwitch("wait-for-iface")) {
    wait_for_iface = true;
  }

  if (cl.HasSwitch("ssl-session-expiry")) {
    string session_expiry = cl.GetSwitchValueASCII("ssl-session-expiry");
    g_proxy_config.ssl_session_expiry_ = atoi(session_expiry.c_str());
  }

  if (cl.HasSwitch("ssl-disable-compression")) {
    g_proxy_config.ssl_disable_compression_ = true;
  }

  if (cl.HasSwitch("idle-timeout")) {
    g_proxy_config.idle_timeout_s_ =
      atoi(cl.GetSwitchValueASCII("idle-timeout").c_str());
  }

  if (cl.HasSwitch("force_spdy"))
    FLAGS_force_spdy = true;

  InitLogging(g_proxy_config.log_filename_.c_str(),
              g_proxy_config.log_destination_,
              logging::DONT_LOCK_LOG_FILE,
              logging::APPEND_TO_OLD_LOG_FILE,
              logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  LOG(INFO) << "Flip SPDY proxy started with configuration:";
  LOG(INFO) << "Logging destination     : " << g_proxy_config.log_destination_;
  LOG(INFO) << "Log file                : " << g_proxy_config.log_filename_;
  LOG(INFO) << "Forward IP Header       : "
            << (g_proxy_config.forward_ip_header_enabled_ ?
                g_proxy_config.forward_ip_header_ : "(disabled)");
  LOG(INFO) << "Wait for interfaces     : " << (wait_for_iface?"true":"false");
  LOG(INFO) << "Accept backlog size     : " << FLAGS_accept_backlog_size;
  LOG(INFO) << "Accepts per wake        : " << FLAGS_accepts_per_wake;
  LOG(INFO) << "Disable nagle           : "
            << (FLAGS_disable_nagle?"true":"false");
  LOG(INFO) << "Reuseport               : "
            << (FLAGS_reuseport?"true":"false");
  LOG(INFO) << "Force SPDY              : "
            << (FLAGS_force_spdy?"true":"false");
  LOG(INFO) << "SSL session expiry      : "
            << g_proxy_config.ssl_session_expiry_;
  LOG(INFO) << "SSL disable compression : "
            << g_proxy_config.ssl_disable_compression_;
  LOG(INFO) << "Connection idle timeout : " << g_proxy_config.idle_timeout_s_;

  // Proxy Acceptors
  while (true) {
    i += 1;
    std::stringstream name;
    name << "proxy" << i;
    if (!cl.HasSwitch(name.str())) {
      break;
    }
    string value = cl.GetSwitchValueASCII(name.str());
    vector<std::string> valueArgs = split(value, ',');
    CHECK_EQ((unsigned int)9, valueArgs.size());
    int spdy_only = atoi(valueArgs[8].c_str());
    // If wait_for_iface is enabled, then this call will block
    // indefinitely until the interface is raised.
    g_proxy_config.AddAcceptor(FLIP_HANDLER_PROXY,
                               valueArgs[0], valueArgs[1],
                               valueArgs[2], valueArgs[3],
                               valueArgs[4], valueArgs[5],
                               valueArgs[6], valueArgs[7],
                               spdy_only,
                               FLAGS_accept_backlog_size,
                               FLAGS_disable_nagle,
                               FLAGS_accepts_per_wake,
                               FLAGS_reuseport,
                               wait_for_iface,
                               NULL);
  }

  // Spdy Server Acceptor
  MemoryCache spdy_memory_cache;
  if (cl.HasSwitch("spdy-server")) {
    spdy_memory_cache.AddFiles();
    string value = cl.GetSwitchValueASCII("spdy-server");
    vector<std::string> valueArgs = split(value, ',');
    g_proxy_config.AddAcceptor(FLIP_HANDLER_SPDY_SERVER,
                               valueArgs[0], valueArgs[1],
                               valueArgs[2], valueArgs[3],
                               "", "", "", "",
                               0,
                               FLAGS_accept_backlog_size,
                               FLAGS_disable_nagle,
                               FLAGS_accepts_per_wake,
                               FLAGS_reuseport,
                               wait_for_iface,
                               &spdy_memory_cache);
  }

  // Spdy Server Acceptor
  MemoryCache http_memory_cache;
  if (cl.HasSwitch("http-server")) {
    http_memory_cache.AddFiles();
    string value = cl.GetSwitchValueASCII("http-server");
    vector<std::string> valueArgs = split(value, ',');
    g_proxy_config.AddAcceptor(FLIP_HANDLER_HTTP_SERVER,
                               valueArgs[0], valueArgs[1],
                               valueArgs[2], valueArgs[3],
                               "", "", "", "",
                               0,
                               FLAGS_accept_backlog_size,
                               FLAGS_disable_nagle,
                               FLAGS_accepts_per_wake,
                               FLAGS_reuseport,
                               wait_for_iface,
                               &http_memory_cache);
  }

  vector<SMAcceptorThread*> sm_worker_threads_;

  for (i = 0; i < g_proxy_config.acceptors_.size(); i++) {
    FlipAcceptor *acceptor = g_proxy_config.acceptors_[i];

    sm_worker_threads_.push_back(new SMAcceptorThread(acceptor,
                                     (MemoryCache *)acceptor->memory_cache_));
    // Note that spdy_memory_cache is not threadsafe, it is merely
    // thread compatible. Thus, if ever we are to spawn multiple threads,
    // we either must make the MemoryCache threadsafe, or use
    // a separate MemoryCache for each thread.
    //
    // The latter is what is currently being done as we spawn
    // a separate thread for each http and spdy server acceptor.

    sm_worker_threads_.back()->InitWorker();
    sm_worker_threads_.back()->Start();
  }

  while (!wantExit) {
    // Close logfile when HUP signal is received. Logging system will
    // automatically reopen on next log message.
    if ( wantLogClose ) {
      wantLogClose = false;
      VLOG(1) << "HUP received, reopening log file.";
      logging::CloseLogFile();
    }
    if (GotQuitFromStdin()) {
      for (unsigned int i = 0; i < sm_worker_threads_.size(); ++i) {
        sm_worker_threads_[i]->Quit();
      }
      for (unsigned int i = 0; i < sm_worker_threads_.size(); ++i) {
        sm_worker_threads_[i]->Join();
      }
      break;
    }
    usleep(1000*10);  // 10 ms
  }

  unlink(PIDFILE);
  close(pidfile_fd);
  return 0;
}
