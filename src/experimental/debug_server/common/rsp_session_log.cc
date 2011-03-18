#include "rsp_session_log.h"

#pragma warning(disable : 4996)

namespace {
void* BlobToCBuff(const debug::Blob& blob);
}  // namespace

namespace rsp {
SessionLog::SessionLog()
    : file_(NULL) {
}

SessionLog::~SessionLog() {
  Close();
}

bool SessionLog::OpenToWrite(const std::string& file_name) {
  Close();
  file_ = fopen(file_name.c_str(), "wb");
  return (NULL != file_);
}

bool SessionLog::OpenToRead(const std::string& file_name) {
  Close();
  file_ = fopen(file_name.c_str(), "rb");
  return (NULL != file_);
  return false;
}

void SessionLog::Close() {
  if (NULL != file_) {
    fclose(file_);
    file_ = NULL;
  }
}

void SessionLog::Add(char record_type, const debug::Blob& record) {
  if (NULL == file_)
    return;

  void* buff = BlobToCBuff(record);
  if (NULL != buff) {
    size_t length = record.Size() + sizeof(record_type);
    fwrite(&length, 1, sizeof(length), file_);
    fwrite(&record_type, 1, sizeof(record_type), file_);
    fwrite(buff, 1, length - sizeof(record_type), file_);
    free(buff);
  }  
  fflush(file_);
}

bool SessionLog::GetNextRecord(char* record_type, debug::Blob* record) {
  if (NULL == file_)
    return false;

  size_t length = 0;
  size_t rd1 = fread(&length, 1, sizeof(length), file_);
  size_t rd2 = fread(record_type, 1, sizeof(*record_type), file_);
  if ((sizeof(length) != rd1) || (sizeof(*record_type) != rd2))
    return false;

  length -= sizeof(*record_type);
  void* buff = malloc(length);
  if (NULL == buff) {
    fseek(file_, length, SEEK_CUR);
    return false;
  }

  fread(buff, 1, length, file_);
  *record = debug::Blob(buff, length);
  free(buff);
  return true;
}
}  // namespace rsp

namespace {
void* BlobToCBuff(const debug::Blob& blob) {
  size_t num = blob.Size();
  char* buff = static_cast<char*>(malloc(num));
  if (NULL != buff) {
    for (size_t i = 0; i < num; i++)
      buff[i] = blob[i];
  }
  return buff;
}
}  // namespace