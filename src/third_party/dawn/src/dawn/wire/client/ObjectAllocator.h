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

#ifndef SRC_DAWN_WIRE_CLIENT_OBJECTALLOCATOR_H_
#define SRC_DAWN_WIRE_CLIENT_OBJECTALLOCATOR_H_

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "dawn/common/Assert.h"
#include "dawn/common/Compiler.h"
#include "dawn/wire/WireCmd_autogen.h"

namespace dawn::wire::client {

template <typename T>
class ObjectAllocator {
  public:
    struct ObjectAndSerial {
        ObjectAndSerial(std::unique_ptr<T> object, uint32_t generation)
            : object(std::move(object)), generation(generation) {}
        std::unique_ptr<T> object;
        uint32_t generation;
    };

    ObjectAllocator() {
        // ID 0 is nullptr
        mObjects.emplace_back(nullptr, 0);
    }

    template <typename Client>
    ObjectAndSerial* New(Client* client) {
        uint32_t id = GetNewId();
        auto object = std::make_unique<T>(client, 1, id);
        client->TrackObject(object.get());

        if (id >= mObjects.size()) {
            ASSERT(id == mObjects.size());
            mObjects.emplace_back(std::move(object), 0);
        } else {
            ASSERT(mObjects[id].object == nullptr);

            mObjects[id].generation++;
            // The generation should never overflow. We don't recycle ObjectIds that would
            // overflow their next generation.
            ASSERT(mObjects[id].generation != 0);

            mObjects[id].object = std::move(object);
        }

        return &mObjects[id];
    }
    void Free(T* obj) {
        ASSERT(obj->IsInList());
        if (DAWN_LIKELY(mObjects[obj->id].generation != std::numeric_limits<uint32_t>::max())) {
            // Only recycle this ObjectId if the generation won't overflow on the next
            // allocation.
            FreeId(obj->id);
        }
        mObjects[obj->id].object = nullptr;
    }

    T* GetObject(uint32_t id) {
        if (id >= mObjects.size()) {
            return nullptr;
        }
        return mObjects[id].object.get();
    }

    uint32_t GetGeneration(uint32_t id) {
        if (id >= mObjects.size()) {
            return 0;
        }
        return mObjects[id].generation;
    }

  private:
    uint32_t GetNewId() {
        if (mFreeIds.empty()) {
            return mCurrentId++;
        }
        uint32_t id = mFreeIds.back();
        mFreeIds.pop_back();
        return id;
    }
    void FreeId(uint32_t id) { mFreeIds.push_back(id); }

    // 0 is an ID reserved to represent nullptr
    uint32_t mCurrentId = 1;
    std::vector<uint32_t> mFreeIds;
    std::vector<ObjectAndSerial> mObjects;
};
}  // namespace dawn::wire::client

#endif  // SRC_DAWN_WIRE_CLIENT_OBJECTALLOCATOR_H_
