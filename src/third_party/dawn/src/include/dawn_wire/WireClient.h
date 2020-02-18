// Copyright 2019 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DAWNWIRE_WIRECLIENT_H_
#define DAWNWIRE_WIRECLIENT_H_

#include <memory>

#include "dawn_wire/Wire.h"

namespace dawn_wire {

    namespace client {
        class Client;
        class MemoryTransferService;
    }

    struct ReservedTexture {
        DawnTexture texture;
        uint32_t id;
        uint32_t generation;
    };

    struct DAWN_WIRE_EXPORT WireClientDescriptor {
        CommandSerializer* serializer;
        client::MemoryTransferService* memoryTransferService = nullptr;
    };

    class DAWN_WIRE_EXPORT WireClient : public CommandHandler {
      public:
        WireClient(const WireClientDescriptor& descriptor);
        ~WireClient();

        DawnDevice GetDevice() const;
        DawnProcTable GetProcs() const;
        const char* HandleCommands(const char* commands, size_t size) override final;

        ReservedTexture ReserveTexture(DawnDevice device);

      private:
        std::unique_ptr<client::Client> mImpl;
    };

    namespace client {
        class DAWN_WIRE_EXPORT MemoryTransferService {
          public:
            class ReadHandle;
            class WriteHandle;

            virtual ~MemoryTransferService();

            // Create a handle for reading server data.
            // This may fail and return nullptr.
            virtual ReadHandle* CreateReadHandle(size_t) = 0;

            // Create a handle for writing server data.
            // This may fail and return nullptr.
            virtual WriteHandle* CreateWriteHandle(size_t) = 0;

            // Imported memory implementation needs to override these to create Read/Write
            // handles associated with a particular buffer. The client should receive a file
            // descriptor for the buffer out-of-band.
            virtual ReadHandle* CreateReadHandle(DawnBuffer, uint64_t offset, size_t size);
            virtual WriteHandle* CreateWriteHandle(DawnBuffer, uint64_t offset, size_t size);

            class DAWN_WIRE_EXPORT ReadHandle {
              public:
                // Serialize the handle into |serializePointer| so it can be received by the server.
                // If |serializePointer| is nullptr, this returns the required serialization space.
                virtual size_t SerializeCreate(void* serializePointer = nullptr) = 0;

                // Load initial data and open the handle for reading.
                // This function takes in the serialized result of
                // server::MemoryTransferService::ReadHandle::SerializeInitialData.
                // This function should write to |data| and |dataLength| the pointer and size of the
                // mapped data for reading. It must live at least until the ReadHandle is
                // destructed.
                virtual bool DeserializeInitialData(const void* deserializePointer,
                                                    size_t deserializeSize,
                                                    const void** data,
                                                    size_t* dataLength) = 0;
                virtual ~ReadHandle();
            };

            class DAWN_WIRE_EXPORT WriteHandle {
              public:
                // Serialize the handle into |serializePointer| so it can be received by the server.
                // If |serializePointer| is nullptr, this returns the required serialization space.
                virtual size_t SerializeCreate(void* serializePointer = nullptr) = 0;

                // Open the handle for reading. The data returned should be zero-initialized.
                // The data returned must live at least until the WriteHandle is destructed.
                // On failure, the pointer returned should be null.
                virtual std::pair<void*, size_t> Open() = 0;

                // Flush writes to the handle. This should serialize info to send updates to the
                // server.
                // If |serializePointer| is nullptr, this returns the required serialization space.
                virtual size_t SerializeFlush(void* serializePointer = nullptr) = 0;
                virtual ~WriteHandle();
            };
        };
    }  // namespace client

}  // namespace dawn_wire

#endif  // DAWNWIRE_WIRECLIENT_H_
