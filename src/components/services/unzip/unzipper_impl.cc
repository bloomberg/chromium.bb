// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/unzipper_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"
#include "third_party/zlib/google/zip.h"
#include "third_party/zlib/google/zip_reader.h"

namespace unzip {

namespace {

std::string PathToMojoString(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(path.value());
#else
  return path.value();
#endif
}

// Modifies output_dir to point to the final directory.
bool CreateDirectory(filesystem::mojom::Directory* output_dir,
                     const base::FilePath& path) {
  base::File::Error err = base::File::Error::FILE_OK;
  return output_dir->OpenDirectory(PathToMojoString(path), mojo::NullReceiver(),
                                   filesystem::mojom::kFlagOpenAlways, &err) &&
         err == base::File::Error::FILE_OK;
}

std::unique_ptr<zip::WriterDelegate> MakeFileWriterDelegateNoParent(
    filesystem::mojom::Directory* output_dir,
    const base::FilePath& path) {
  auto file = std::make_unique<base::File>();
  base::File::Error err;
  if (!output_dir->OpenFileHandle(PathToMojoString(path),
                                  filesystem::mojom::kFlagCreate |
                                      filesystem::mojom::kFlagWrite |
                                      filesystem::mojom::kFlagWriteAttributes,
                                  &err, file.get()) ||
      err != base::File::Error::FILE_OK) {
    return nullptr;
  }
  return std::make_unique<zip::FileWriterDelegate>(std::move(file));
}

std::unique_ptr<zip::WriterDelegate> MakeFileWriterDelegate(
    filesystem::mojom::Directory* output_dir,
    const base::FilePath& path) {
  if (path == path.BaseName())
    return MakeFileWriterDelegateNoParent(output_dir, path);
  mojo::Remote<filesystem::mojom::Directory> parent;
  base::File::Error err;
  if (!output_dir->OpenDirectory(PathToMojoString(path.DirName()),
                                 parent.BindNewPipeAndPassReceiver(),
                                 filesystem::mojom::kFlagOpenAlways, &err) ||
      err != base::File::Error::FILE_OK) {
    return nullptr;
  }
  return MakeFileWriterDelegateNoParent(parent.get(), path.BaseName());
}

bool Filter(const mojo::Remote<mojom::UnzipFilter>& filter,
            const base::FilePath& path) {
  bool result = false;
  filter->ShouldUnzipFile(path, &result);
  return result;
}

// Reads the given ZIP archive, and returns all the filenames concatenated
// together in one long string capped at ~100KB, without any separator, and in
// the encoding used by the ZIP archive itself. Returns an empty string if the
// ZIP cannot be read.
std::string GetRawFileNamesFromZip(const base::File& zip_file) {
  std::string result;

  // Open ZIP archive for reading.
  zip::ZipReader reader;
  if (!reader.OpenFromPlatformFile(zip_file.GetPlatformFile())) {
    LOG(ERROR) << "Cannot decode ZIP archive";
    return result;
  }

  // Reserve a ~100KB buffer.
  result.reserve(100000);

  // Iterate over file entries of the ZIP archive.
  while (const zip::ZipReader::Entry* const entry = reader.Next()) {
    const std::string& path = entry->path_in_original_encoding;

    // Stop if we have enough data in |result|.
    if (path.size() > (result.capacity() - result.size()))
      break;

    // Accumulate data in |result|.
    result += path;
  }

  LOG_IF(ERROR, result.empty()) << "Cannot extract filenames from ZIP archive";
  return result;
}

}  // namespace

UnzipperImpl::UnzipperImpl() = default;

UnzipperImpl::UnzipperImpl(mojo::PendingReceiver<mojom::Unzipper> receiver)
    : receiver_(this, std::move(receiver)) {}

UnzipperImpl::~UnzipperImpl() = default;

void UnzipperImpl::Unzip(
    base::File zip_file,
    mojo::PendingRemote<filesystem::mojom::Directory> output_dir_remote,
    mojo::PendingRemote<mojom::UnzipFilter> filter_remote,
    UnzipCallback callback) {
  DCHECK(zip_file.IsValid());
  mojo::Remote<filesystem::mojom::Directory> output_dir(
      std::move(output_dir_remote));
  zip::FilterCallback filter_cb;
  if (filter_remote) {
    filter_cb = base::BindRepeating(
        &Filter, mojo::Remote<mojom::UnzipFilter>(std::move(filter_remote)));
  }

  std::move(callback).Run(
      zip::Unzip(zip_file.GetPlatformFile(),
                 base::BindRepeating(&MakeFileWriterDelegate, output_dir.get()),
                 base::BindRepeating(&CreateDirectory, output_dir.get()),
                 {.filter = std::move(filter_cb)}));
}

void UnzipperImpl::DetectEncoding(base::File zip_file,
                                  DetectEncodingCallback callback) {
  DCHECK(zip_file.IsValid());

  // Accumulate raw filenames.
  const std::string all_names = GetRawFileNamesFromZip(zip_file);
  if (all_names.empty()) {
    std::move(callback).Run(UNKNOWN_ENCODING);
    return;
  }

  // Detect encoding.
  int consumed_bytes = 0;
  bool is_reliable = false;
  const Encoding encoding = CompactEncDet::DetectEncoding(
      all_names.data(), all_names.size(), nullptr, nullptr, nullptr,
      UNKNOWN_ENCODING, UNKNOWN_LANGUAGE,
      CompactEncDet::QUERY_CORPUS,  // Plain text
      true,                         // Exclude 7-bit encodings
      &consumed_bytes, &is_reliable);

  VLOG(1) << "Detected encoding: " << MimeEncodingName(encoding) << " ("
          << encoding << "), reliable: " << is_reliable
          << ", consumed bytes: " << consumed_bytes;

  LOG_IF(ERROR, encoding == UNKNOWN_ENCODING)
      << "Cannot detect encoding of filenames in ZIP archive";

  std::move(callback).Run(encoding);
}

}  // namespace unzip
