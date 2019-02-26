// Copyright 2017 The Dawn Authors
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

#ifndef DAWNNATIVE_SWAPCHAIN_H_
#define DAWNNATIVE_SWAPCHAIN_H_

#include "dawn_native/Builder.h"
#include "dawn_native/Forward.h"
#include "dawn_native/ObjectBase.h"

#include "dawn/dawn_wsi.h"
#include "dawn_native/dawn_platform.h"

namespace dawn_native {

    class SwapChainBase : public ObjectBase {
      public:
        SwapChainBase(SwapChainBuilder* builder);
        ~SwapChainBase();

        // Dawn API
        void Configure(dawn::TextureFormat format,
                       dawn::TextureUsageBit allowedUsage,
                       uint32_t width,
                       uint32_t height);
        TextureBase* GetNextTexture();
        void Present(TextureBase* texture);

      protected:
        const dawnSwapChainImplementation& GetImplementation();
        virtual TextureBase* GetNextTextureImpl(const TextureDescriptor*) = 0;
        virtual void OnBeforePresent(TextureBase* texture) = 0;

      private:
        dawnSwapChainImplementation mImplementation = {};
        dawn::TextureFormat mFormat = {};
        dawn::TextureUsageBit mAllowedUsage;
        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
        TextureBase* mLastNextTexture = nullptr;
    };

    class SwapChainBuilder : public Builder<SwapChainBase> {
      public:
        SwapChainBuilder(DeviceBase* device);

        // Dawn API
        SwapChainBase* GetResultImpl() override;
        void SetImplementation(uint64_t implementation);

      private:
        friend class SwapChainBase;

        dawnSwapChainImplementation mImplementation = {};
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_SWAPCHAIN_H_
