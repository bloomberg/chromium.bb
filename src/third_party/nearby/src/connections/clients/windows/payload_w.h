// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_PAYLOAD_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_PAYLOAD_W_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "connections/clients/windows/dll_config.h"
#include "connections/payload_type.h"
#include "internal/platform/payload_id.h"

namespace location::nearby {
// Forward declarations
namespace connections {
class Payload;
struct PayloadDeleter {
  void operator()(Payload* p);
};
}  // namespace connections
class InputFile;
class InputStream;
namespace windows {

// Payload is default-constructible, and moveable, but not copyable container
// that holds at most one instance of one of:
// ByteArray, InputStream, or InputFile.
class DLL_API PayloadW {
 public:
  PayloadW(PayloadW&& other) noexcept;
  PayloadW& operator=(PayloadW&& other) noexcept;

  // Default (invalid) payload.
  PayloadW();
  ~PayloadW();

  // Constructors for outgoing payloads.
  explicit PayloadW(const char* bytes, const size_t size);

  explicit PayloadW(InputFile& file);
  explicit PayloadW(std::function<InputStream&()> stream);

  // Constructors for incoming payloads.
  PayloadW(PayloadId id, const char* bytes, const size_t size);
  PayloadW(PayloadId id, InputFile file);
  explicit PayloadW(const char* parent_folder, const char* file_name,
                    InputFile file);

  // TODO(jfcarroll): Convert std::function to function pointer
  PayloadW(PayloadId id, std::function<InputStream&()> stream);
  // Returns ByteArray payload, if it has
  // been defined, or empty ByteArray.
  bool AsBytes(const char* bytes, size_t& bytes_size) const&;
  bool AsBytes(const char* bytes, size_t& bytes_size) &&;
  // Returns InputStream* payload, if it has been defined, or nullptr.
  InputStream* AsStream();
  // Returns InputFile* payload, if it has been defined, or nullptr.
  const InputFile* AsFile() const;

  // Returns Payload unique ID.
  uint64_t GetId() const;

  // Returns Payload type.
  const connections::PayloadType GetType() const;

  // Sets the payload offset in bytes
  void SetOffset(size_t offset);

  size_t GetOffset();

  // Generate Payload Id; to be passed to outgoing file constructor.
  static PayloadId GenerateId();

  const char* GetFileName() const;
  const char* GetParentFolder() const;

  std::unique_ptr<connections::Payload, connections::PayloadDeleter> GetImpl();

 private:
  std::unique_ptr<connections::Payload, connections::PayloadDeleter> impl_;
};

}  // namespace windows
}  // namespace location::nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_PAYLOAD_W_H_
