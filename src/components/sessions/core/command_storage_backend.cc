// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_backend.h"

#include <stdint.h>
#include <algorithm>
#include <limits>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "crypto/aead.h"

namespace sessions {

namespace {

// File version number.
constexpr int32_t kFileCurrentVersion = 1;
constexpr int32_t kEncryptedFileCurrentVersion = 2;

// The signature at the beginning of the file = SSNS (Sessions).
constexpr int32_t kFileSignature = 0x53534E53;

// Length (in bytes) of the nonce (used when encrypting).
constexpr int kNonceLength = 12;

// The file header is the first bytes written to the file,
// and is used to identify the file as one written by us.
struct FileHeader {
  int32_t signature;
  int32_t version;
};

// SessionFileReader ----------------------------------------------------------

// SessionFileReader is responsible for reading the set of SessionCommands that
// describe a Session back from a file. SessionFileRead does minimal error
// checking on the file (pretty much only that the header is valid).

class SessionFileReader {
 public:
  typedef sessions::SessionCommand::id_type id_type;
  typedef sessions::SessionCommand::size_type size_type;

  SessionFileReader(const base::FilePath& path,
                    const std::vector<uint8_t>& crypto_key)
      : buffer_(CommandStorageBackend::kFileReadBufferSize, 0),
        crypto_key_(crypto_key) {
    if (!crypto_key.empty()) {
      aead_ = std::make_unique<crypto::Aead>(crypto::Aead::AES_256_GCM);
      aead_->Init(base::make_span(crypto_key_));
    }
    file_ = std::make_unique<base::File>(
        path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }
  // Reads the contents of the file specified in the constructor, returning
  // true on success, and filling up |commands| with commands.
  bool Read(std::vector<std::unique_ptr<sessions::SessionCommand>>* commands);

 private:
  // Reads a single command, returning it. A return value of null indicates
  // either there are no commands, or there was an error. Use errored_ to
  // distinguish the two. If null is returned, and there is no error, it means
  // the end of file was successfully reached.
  std::unique_ptr<sessions::SessionCommand> ReadCommand();

  // Decrypts a previously encrypted command. Returns the new command on
  // success.
  std::unique_ptr<sessions::SessionCommand> CreateCommandFromEncrypted(
      const char* data,
      size_type length);

  // Creates a command from the previously written value.
  std::unique_ptr<sessions::SessionCommand> CreateCommand(const char* data,
                                                          size_type length);

  // Shifts the unused portion of buffer_ to the beginning and fills the
  // remaining portion with data from the file. Returns false if the buffer
  // couldn't be filled. A return value of false only signals an error if
  // errored_ is set to true.
  bool FillBuffer();

  // Whether an error condition has been detected (
  bool errored_ = false;

  // As we read from the file, data goes here.
  std::string buffer_;

  const std::vector<uint8_t> crypto_key_;

  std::unique_ptr<crypto::Aead> aead_;

  // The file.
  std::unique_ptr<base::File> file_;

  // Position in buffer_ of the data.
  size_t buffer_position_ = 0;

  // Number of available bytes; relative to buffer_position_.
  size_t available_count_ = 0;

  // Count of the number of commands encountered.
  int command_counter_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SessionFileReader);
};

bool SessionFileReader::Read(
    std::vector<std::unique_ptr<sessions::SessionCommand>>* commands) {
  if (!file_->IsValid())
    return false;
  FileHeader header;
  int read_count;
  read_count =
      file_->ReadAtCurrentPos(reinterpret_cast<char*>(&header), sizeof(header));
  if (read_count != sizeof(header) || header.signature != kFileSignature) {
    const bool encrypt = aead_.get() != nullptr;
    if ((encrypt && header.version != kEncryptedFileCurrentVersion) ||
        (!encrypt && header.version != kFileCurrentVersion)) {
      return false;
    }
  }

  std::vector<std::unique_ptr<sessions::SessionCommand>> read_commands;
  for (std::unique_ptr<sessions::SessionCommand> command = ReadCommand();
       command && !errored_; command = ReadCommand())
    read_commands.push_back(std::move(command));
  if (!errored_)
    read_commands.swap(*commands);
  return !errored_;
}

std::unique_ptr<sessions::SessionCommand> SessionFileReader::ReadCommand() {
  // Make sure there is enough in the buffer for the size of the next command.
  if (available_count_ < sizeof(size_type)) {
    if (!FillBuffer())
      return nullptr;
    if (available_count_ < sizeof(size_type)) {
      VLOG(1) << "SessionFileReader::ReadCommand, file incomplete";
      // Still couldn't read a valid size for the command, assume write was
      // incomplete and return null.
      return nullptr;
    }
  }
  // Get the size of the command.
  size_type command_size;
  memcpy(&command_size, &(buffer_[buffer_position_]), sizeof(command_size));
  buffer_position_ += sizeof(command_size);
  available_count_ -= sizeof(command_size);

  if (command_size == 0) {
    VLOG(1) << "SessionFileReader::ReadCommand, empty command";
    // Empty command. Shouldn't happen if write was successful, fail.
    return nullptr;
  }

  // Make sure buffer has the complete contents of the command.
  if (command_size > available_count_) {
    if (command_size > buffer_.size())
      buffer_.resize((command_size / 1024 + 1) * 1024, 0);
    if (!FillBuffer() || command_size > available_count_) {
      // Again, assume the file was ok, and just the last chunk was lost.
      VLOG(1) << "SessionFileReader::ReadCommand, last chunk lost";
      return nullptr;
    }
  }
  std::unique_ptr<SessionCommand> command;
  if (aead_) {
    command = CreateCommandFromEncrypted(buffer_.c_str() + buffer_position_,
                                         command_size);
  } else {
    command = CreateCommand(buffer_.c_str() + buffer_position_, command_size);
  }
  ++command_counter_;
  buffer_position_ += command_size;
  available_count_ -= command_size;
  return command;
}

std::unique_ptr<sessions::SessionCommand>
SessionFileReader::CreateCommandFromEncrypted(const char* data,
                                              size_type length) {
  // This means the nonce overflowed and we're reusing a nonce.
  // CommandStorageBackend should never write enough commands to trigger this,
  // so assume we should stop.
  if (command_counter_ < 0)
    return nullptr;

  char nonce[kNonceLength];
  memset(nonce, 0, kNonceLength);
  memcpy(nonce, &command_counter_, sizeof(command_counter_));
  std::string plain_text;
  if (!aead_->Open(base::StringPiece(data, length),
                   base::StringPiece(nonce, kNonceLength), base::StringPiece(),
                   &plain_text)) {
    DVLOG(1) << "SessionFileReader::ReadCommand, decryption failed";
    return nullptr;
  }
  if (plain_text.size() < sizeof(id_type)) {
    DVLOG(1) << "SessionFileReader::ReadCommand, size too small";
    return nullptr;
  }
  return CreateCommand(plain_text.c_str(), plain_text.size());
}

std::unique_ptr<sessions::SessionCommand> SessionFileReader::CreateCommand(
    const char* data,
    size_type length) {
  // Callers should have checked the size.
  DCHECK_GE(length, sizeof(id_type));
  const id_type command_id = data[0];
  // NOTE: |length| includes the size of the id, which is not part of the
  // contents of the SessionCommand.
  std::unique_ptr<sessions::SessionCommand> command =
      std::make_unique<sessions::SessionCommand>(command_id,
                                                 length - sizeof(id_type));
  if (length > sizeof(id_type)) {
    memcpy(command->contents(), &(data[sizeof(id_type)]),
           length - sizeof(id_type));
  }
  return command;
}

bool SessionFileReader::FillBuffer() {
  if (available_count_ > 0 && buffer_position_ > 0) {
    // Shift buffer to beginning.
    memmove(&(buffer_[0]), &(buffer_[buffer_position_]), available_count_);
  }
  buffer_position_ = 0;
  DCHECK(buffer_position_ + available_count_ < buffer_.size());
  int to_read = static_cast<int>(buffer_.size() - available_count_);
  int read_count =
      file_->ReadAtCurrentPos(&(buffer_[available_count_]), to_read);
  if (read_count < 0) {
    errored_ = true;
    return false;
  }
  if (read_count == 0)
    return false;
  available_count_ += read_count;
  return true;
}

}  // namespace

// CommandStorageBackend
// -------------------------------------------------------------

// static
const int CommandStorageBackend::kFileReadBufferSize = 1024;

// static
const SessionCommand::size_type
    CommandStorageBackend::kEncryptionOverheadInBytes = 16;

CommandStorageBackend::CommandStorageBackend(
    scoped_refptr<base::SequencedTaskRunner> owning_task_runner,
    const base::FilePath& path)
    : RefCountedDeleteOnSequence(owning_task_runner), path_(path) {}

void CommandStorageBackend::AppendCommands(
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands,
    bool truncate,
    const std::vector<uint8_t>& crypto_key) {
  InitIfNecessary();

  if (truncate) {
    const bool was_encrypted = IsEncrypted();
    const bool encrypt = !crypto_key.empty();
    if (was_encrypted != encrypt) {
      // The header is different when encrypting, so the file needs to be
      // recreated.
      CloseFile();
    }
    if (encrypt) {
      aead_ = std::make_unique<crypto::Aead>(crypto::Aead::AES_256_GCM);
      crypto_key_ = crypto_key;
      aead_->Init(base::make_span(crypto_key_));
    } else {
      aead_.reset();
    }
  } else {
    // |crypto_key| is only used when |truncate| is true.
    DCHECK(crypto_key.empty());
  }

  // Make sure and check |file_|, if opening the file failed |file_| will be
  // null.
  if (truncate || !file_ || !file_->IsValid())
    TruncateFile();

  // Check |file_| again as TruncateFile() may fail.
  if (file_ && file_->IsValid() &&
      !AppendCommandsToFile(file_.get(), commands)) {
    file_.reset();
  }
}

void CommandStorageBackend::ReadCurrentSessionCommands(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    const std::vector<uint8_t>& crypto_key,
    GetCommandsCallback callback) {
  if (is_canceled.Run())
    return;

  InitIfNecessary();

  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
  ReadCommandsFromFile(path_, crypto_key, &commands);
  std::move(callback).Run(std::move(commands));
}

bool CommandStorageBackend::AppendCommandsToFile(
    base::File* file,
    const std::vector<std::unique_ptr<sessions::SessionCommand>>& commands) {
  for (auto& command : commands) {
    if (IsEncrypted()) {
      if (!AppendEncryptedCommandToFile(file, *(command.get())))
        return false;
    } else if (!AppendCommandToFile(file, *(command.get()))) {
      return false;
    }
    commands_written_++;
  }
#if defined(OS_CHROMEOS)
  file->Flush();
#endif
  return true;
}

CommandStorageBackend::~CommandStorageBackend() = default;

void CommandStorageBackend::InitIfNecessary() {
  if (inited_)
    return;

  inited_ = true;
  base::CreateDirectory(path_.DirName());
  DoInit();
}

bool CommandStorageBackend::ReadCommandsFromFile(
    const base::FilePath& path,
    const std::vector<uint8_t>& crypto_key,
    std::vector<std::unique_ptr<sessions::SessionCommand>>* commands) {
  SessionFileReader file_reader(path, crypto_key);
  return file_reader.Read(commands);
}

void CommandStorageBackend::CloseFile() {
  file_.reset();
}

void CommandStorageBackend::TruncateFile() {
  DCHECK(inited_);
  if (file_) {
    // File is already open, truncate it. We truncate instead of closing and
    // reopening to avoid the possibility of scanners locking the file out
    // from under us once we close it. If truncation fails, we'll try to
    // recreate.
    const int header_size = static_cast<int>(sizeof(FileHeader));
    if (file_->Seek(base::File::FROM_BEGIN, header_size) != header_size ||
        !file_->SetLength(header_size))
      file_.reset();
  }
  if (!file_)
    file_ = OpenAndWriteHeader(path_);
  commands_written_ = 0;
}

std::unique_ptr<base::File> CommandStorageBackend::OpenAndWriteHeader(
    const base::FilePath& path) {
  DCHECK(!path.empty());
  std::unique_ptr<base::File> file = std::make_unique<base::File>(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                base::File::FLAG_EXCLUSIVE_WRITE |
                base::File::FLAG_EXCLUSIVE_READ);
  if (!file->IsValid())
    return nullptr;
  FileHeader header;
  header.signature = kFileSignature;
  header.version =
      IsEncrypted() ? kEncryptedFileCurrentVersion : kFileCurrentVersion;
  if (file->WriteAtCurrentPos(reinterpret_cast<char*>(&header),
                              sizeof(header)) != sizeof(header)) {
    return nullptr;
  }
  commands_written_ = 0;
  return file;
}

bool CommandStorageBackend::AppendCommandToFile(
    base::File* file,
    const sessions::SessionCommand& command) {
  const size_type total_size = command.GetSerializedSize();
  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&total_size),
                              sizeof(total_size)) != sizeof(total_size)) {
    DVLOG(1) << "error writing";
    return false;
  }
  id_type command_id = command.id();
  if (file->WriteAtCurrentPos(reinterpret_cast<char*>(&command_id),
                              sizeof(command_id)) != sizeof(command_id)) {
    DVLOG(1) << "error writing";
    return false;
  }

  const size_type content_size = total_size - sizeof(id_type);
  if (content_size == 0)
    return true;

  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(command.contents()),
                              content_size) != content_size) {
    DVLOG(1) << "error writing";
    return false;
  }
  return true;
}

bool CommandStorageBackend::AppendEncryptedCommandToFile(
    base::File* file,
    const sessions::SessionCommand& command) {
  // This means the nonce overflowed and we're reusing a nonce. This class
  // should never write enough commands to trigger this, so assume we should
  // stop.
  if (commands_written_ < 0)
    return false;
  DCHECK(IsEncrypted());
  char nonce[kNonceLength];
  memset(nonce, 0, kNonceLength);
  memcpy(nonce, &commands_written_, sizeof(commands_written_));

  // Encryption adds overhead, resulting in a slight reduction in the available
  // space for each command. Chop any contents beyond the available size.
  const size_type command_size = std::min(
      command.size(),
      static_cast<size_type>(std::numeric_limits<size_type>::max() -
                             sizeof(id_type) - kEncryptionOverheadInBytes));
  std::vector<char> command_and_id(command_size + sizeof(id_type));
  const id_type command_id = command.id();
  memcpy(&command_and_id.front(), reinterpret_cast<const char*>(&command_id),
         sizeof(id_type));
  memcpy(&(command_and_id.front()) + sizeof(id_type), command.contents(),
         command_size);

  std::string cipher_text;
  aead_->Seal(base::StringPiece(&command_and_id.front(), command_and_id.size()),
              base::StringPiece(nonce, kNonceLength), base::StringPiece(),
              &cipher_text);
  DCHECK_LE(cipher_text.size(), std::numeric_limits<size_type>::max());
  const size_type command_and_id_size =
      static_cast<size_type>(cipher_text.size());

  int wrote = file->WriteAtCurrentPos(
      reinterpret_cast<const char*>(&command_and_id_size),
      sizeof(command_and_id_size));
  if (wrote != sizeof(command_and_id_size)) {
    DVLOG(1) << "error writing";
    return false;
  }
  wrote = file->WriteAtCurrentPos(cipher_text.c_str(), cipher_text.size());
  if (wrote != static_cast<int>(cipher_text.size())) {
    DVLOG(1) << "error writing";
    return false;
  }
  return true;
}

}  // namespace sessions
