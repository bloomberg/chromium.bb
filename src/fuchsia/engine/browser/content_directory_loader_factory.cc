// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/content_directory_loader_factory.h"

#include <lib/fdio/directory.h>
#include <lib/fdio/fdio.h>

#include <map>
#include <memory>
#include <utility>

#include "base/files/memory_mapped_file.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "fuchsia/engine/common.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/parse_number.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace {

using ContentDirectoriesMap =
    std::map<std::string, fidl::InterfaceHandle<fuchsia::io::Directory>>;

ContentDirectoriesMap ParseContentDirectoriesFromCommandLine() {
  ContentDirectoriesMap directories;

  // Parse the list of content directories from the command line.
  std::string content_directories_unsplit =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kContentDirectories);
  if (content_directories_unsplit.empty())
    return {};

  base::StringPairs named_handle_ids;
  if (!base::SplitStringIntoKeyValuePairs(content_directories_unsplit, '=', ',',
                                          &named_handle_ids)) {
    LOG(WARNING) << "Couldn't parse --" << kContentDirectories
                 << " into KV pairs: " << content_directories_unsplit;
    return {};
  }

  for (const auto& named_handle_id : named_handle_ids) {
    uint32_t handle_id = 0;
    if (!net::ParseUint32(named_handle_id.second, &handle_id)) {
      DLOG(FATAL) << "Couldn't parse handle ID as uint32: "
                  << named_handle_id.second;
      continue;
    }

    auto directory_channel = zx::channel(zx_take_startup_handle(handle_id));
    if (directory_channel == ZX_HANDLE_INVALID) {
      DLOG(FATAL) << "Couldn't take startup handle: " << handle_id;
      continue;
    }

    directories.emplace(named_handle_id.first,
                        fidl::InterfaceHandle<fuchsia::io::Directory>(
                            std::move(directory_channel)));
  }
  return directories;
}

// Gets the process-global list of content directory channels.
ContentDirectoriesMap* GetContentDirectories() {
  static base::NoDestructor<ContentDirectoriesMap> directories(
      ParseContentDirectoriesFromCommandLine());

  return directories.get();
}

// Returns a list of populated response HTTP headers.
// |mime_type|: The MIME type of the resource.
// |charset|: The resource's character set. Optional. If omitted, the browser
//            will assume the charset to be "text/plain" by default.
scoped_refptr<net::HttpResponseHeaders> CreateHeaders(
    base::StringPiece mime_type,
    const base::Optional<std::string>& charset) {
  constexpr char kXFrameOptionsHeader[] = "X-Frame-Options: DENY";
  constexpr char kCacheHeader[] = "Cache-Control: no-cache";
  constexpr char kContentTypePrefix[] = "Content-Type: ";
  constexpr char kCharsetSeparator[] = "; charset=";

  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
  headers->AddHeader(kXFrameOptionsHeader);
  headers->AddHeader(kCacheHeader);

  if (charset) {
    headers->AddHeader(base::StrCat(
        {kContentTypePrefix, mime_type, kCharsetSeparator, *charset}));
  } else {
    headers->AddHeader(base::StrCat({kContentTypePrefix, mime_type}));
  }

  return headers;
}

// Copies data from a fuchsia.io.Node file into a URL response stream.
class ContentDirectoryURLLoader : public network::mojom::URLLoader {
 public:
  ContentDirectoryURLLoader() = default;
  ~ContentDirectoryURLLoader() final = default;

  // Creates a read-only MemoryMappedFile view to |file|.
  bool MapFile(fidl::InterfaceHandle<fuchsia::io::Node> file,
               base::MemoryMappedFile* mmap) {
    // Bind the file channel to a FDIO entry and then a file descriptor so that
    // we can use it for reading.
    fdio_t* fdio = nullptr;
    zx_status_t status = fdio_create(file.TakeChannel().release(), &fdio);
    if (status == ZX_ERR_PEER_CLOSED) {
      // File-not-found errors are expected in some cases, so handle this result
      // w/o logging error text.
      return false;
    } else if (status != ZX_OK) {
      ZX_DLOG_IF(WARNING, status != ZX_OK, status) << "fdio_create";
      return false;
    }

    base::ScopedFD fd(fdio_bind_to_fd(fdio, -1, 0));
    if (!fd.is_valid()) {
      LOG(ERROR) << "fdio_bind_to_fd returned an invalid FD.";
      return false;
    }

    // Map the file into memory.
    if (!mmap->Initialize(base::File(fd.release()),
                          base::MemoryMappedFile::READ_ONLY)) {
      return false;
    }

    return true;
  }

  // Initiates data transfer from |file_channel| to |client_info|.
  // |metadata_channel|, if it is connected to a file, is accessed to get the
  // MIME type and charset of the file.
  static void CreateAndStart(
      network::mojom::URLLoaderRequest url_loader_request,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderClientPtrInfo client_info,
      fidl::InterfaceHandle<fuchsia::io::Node> file_channel,
      fidl::InterfaceHandle<fuchsia::io::Node> metadata_channel) {
    std::unique_ptr<ContentDirectoryURLLoader> loader =
        std::make_unique<ContentDirectoryURLLoader>();
    loader->Start(request, std::move(client_info), std::move(file_channel),
                  std::move(metadata_channel));

    // |loader|'s lifetime is bound to the lifetime of the URLLoader Mojo
    // client endpoint.
    mojo::MakeStrongBinding(std::move(loader), std::move(url_loader_request));
  }

  void Start(const network::ResourceRequest& request,
             network::mojom::URLLoaderClientPtrInfo client_info,
             fidl::InterfaceHandle<fuchsia::io::Node> file_channel,
             fidl::InterfaceHandle<fuchsia::io::Node> metadata_channel) {
    client_.Bind(std::move(client_info));

    if (!MapFile(std::move(file_channel), &mmap_)) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    // Construct and deliver the HTTP response header.
    network::ResourceResponseHead response;

    // Read the charset and MIME type from the optional _metadata file.
    base::Optional<std::string> charset;
    base::Optional<std::string> mime_type;
    base::MemoryMappedFile metadata_mmap;
    if (MapFile(std::move(metadata_channel), &metadata_mmap)) {
      base::Optional<base::Value> metadata_parsed = base::JSONReader::Read(
          base::StringPiece(reinterpret_cast<char*>(metadata_mmap.data()),
                            metadata_mmap.length()));

      if (metadata_parsed && metadata_parsed->is_dict()) {
        if (metadata_parsed->FindStringKey("charset"))
          charset = *metadata_parsed->FindStringKey("charset");

        if (metadata_parsed->FindStringKey("mime"))
          mime_type = *metadata_parsed->FindStringKey("mime");
      }
    }

    // If a MIME type wasn't specified, then fall back on inferring the type
    // from the file's contents.
    if (!mime_type) {
      mime_type.emplace();
      net::SniffMimeType(
          reinterpret_cast<char*>(mmap_.data()), mmap_.length(), request.url,
          "", net::ForceSniffFileUrlsForHtml::kDisabled, &*mime_type);
    }
    response.mime_type = *mime_type;
    response.headers = CreateHeaders(response.mime_type, charset);
    response.content_length = mmap_.length();
    client_->OnReceiveResponse(response);

    // Set up the Mojo DataPipe used for streaming the response payload to the
    // client.
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoResult rv = mojo::CreateDataPipe(0, &producer_handle, &consumer_handle);
    if (rv != MOJO_RESULT_OK) {
      client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }

    client_->OnStartLoadingResponseBody(std::move(consumer_handle));

    // Start reading the contents of |mmap_| into the response DataPipe.
    body_writer_ =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    body_writer_->Write(
        std::make_unique<mojo::StringDataSource>(
            base::StringPiece(reinterpret_cast<char*>(mmap_.data()),
                              mmap_.length()),
            mojo::StringDataSource::AsyncWritingMode::
                STRING_STAYS_VALID_UNTIL_COMPLETION),
        base::BindOnce(&ContentDirectoryURLLoader::OnWriteComplete,
                       base::Unretained(this)));
  }

  // network::mojom::URLLoader implementation:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_request_headers,
                      const base::Optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  // Called when body_writer_->Write() has completed asynchronously.
  void OnWriteComplete(MojoResult result) {
    // Signal stream EOF to the client.
    body_writer_.reset();

    if (result != MOJO_RESULT_OK) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = mmap_.length();
    status.encoded_body_length = mmap_.length();
    status.decoded_body_length = mmap_.length();
    client_->OnComplete(std::move(status));
  }

  // Used for sending status codes and response payloads to the client.
  network::mojom::URLLoaderClientPtr client_;

  // A read-only, memory mapped view of the file being loaded.
  base::MemoryMappedFile mmap_;

  // Manages chunked data transfer over the response DataPipe.
  std::unique_ptr<mojo::DataPipeProducer> body_writer_;

  DISALLOW_COPY_AND_ASSIGN(ContentDirectoryURLLoader);
};

}  // namespace

ContentDirectoryLoaderFactory::ContentDirectoryLoaderFactory()
    : task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

ContentDirectoryLoaderFactory::~ContentDirectoryLoaderFactory() {}

net::Error ContentDirectoryLoaderFactory::OpenFileFromDirectory(
    const std::string& directory_name,
    base::FilePath path,
    fidl::InterfaceRequest<fuchsia::io::Node> file_request) {
  DCHECK(file_request);

  ContentDirectoriesMap* content_directories = GetContentDirectories();
  if (content_directories->find(directory_name) == content_directories->end())
    return net::ERR_FILE_NOT_FOUND;

  const fidl::InterfaceHandle<fuchsia::io::Directory>& directory =
      content_directories->at(directory_name);
  if (!directory)
    return net::ERR_INVALID_HANDLE;

  zx_status_t status = fdio_open_at(
      directory.channel().get(), path.value().c_str(),
      fuchsia::io::OPEN_RIGHT_READABLE, file_request.TakeChannel().release());
  if (status != ZX_OK) {
    ZX_DLOG(WARNING, status) << "fdio_open_at";
    return net::ERR_FILE_NOT_FOUND;
  }

  return net::OK;
}

void ContentDirectoryLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!request.url.SchemeIs(kFuchsiaContentDirectoryScheme) ||
      !request.url.is_valid()) {
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }

  if (request.method != "GET") {
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_METHOD_NOT_SUPPORTED));
    return;
  }

  fidl::InterfaceHandle<fuchsia::io::Node> file_handle;
  net::Error open_result = OpenFileFromDirectory(
      request.url.GetOrigin().host(), base::FilePath(request.url.path()),
      file_handle.NewRequest());
  if (open_result != net::OK) {
    client->OnComplete(network::URLLoaderCompletionStatus(open_result));
    return;
  }
  DCHECK(file_handle);

  // Metadata files are optional. In the event that the file is absent,
  // |metadata_channel| will produce a ZX_CHANNEL_PEER_CLOSED status inside
  // ContentDirectoryURLLoader::Start().
  fidl::InterfaceHandle<fuchsia::io::Node> metadata_handle;
  open_result =
      OpenFileFromDirectory(request.url.GetOrigin().host(),
                            base::FilePath(request.url.path() + "._metadata"),
                            metadata_handle.NewRequest());
  if (open_result != net::OK) {
    client->OnComplete(network::URLLoaderCompletionStatus(open_result));
    return;
  }

  // Load the resource on a blocking-capable TaskRunner.
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&ContentDirectoryURLLoader::CreateAndStart,
                                    base::Passed(std::move(loader)), request,
                                    base::Passed(client.PassInterface()),
                                    base::Passed(std::move(file_handle)),
                                    base::Passed(std::move(metadata_handle))));
}

void ContentDirectoryLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader) {
  bindings_.AddBinding(this, std::move(loader));
}

void ContentDirectoryLoaderFactory::SetContentDirectoriesForTest(
    std::vector<fuchsia::web::ContentDirectoryProvider> directories) {
  ContentDirectoriesMap* current_process_directories = GetContentDirectories();

  current_process_directories->clear();
  for (fuchsia::web::ContentDirectoryProvider& directory : directories) {
    current_process_directories->emplace(directory.name(),
                                         directory.mutable_directory()->Bind());
  }
}
