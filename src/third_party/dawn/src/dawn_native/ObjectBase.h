// Copyright 2018 The Dawn Authors
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

#ifndef DAWNNATIVE_OBJECTBASE_H_
#define DAWNNATIVE_OBJECTBASE_H_

#include "common/LinkedList.h"
#include "common/RefCounted.h"
#include "dawn_native/Forward.h"

#include <string>

namespace dawn_native {

    class DeviceBase;

    class ObjectBase : public RefCounted {
      public:
        struct ErrorTag {};
        static constexpr ErrorTag kError = {};

        explicit ObjectBase(DeviceBase* device);
        ObjectBase(DeviceBase* device, ErrorTag tag);

        DeviceBase* GetDevice() const;
        bool IsError() const;

      private:
        // Pointer to owning device.
        DeviceBase* mDevice;
    };

    class ApiObjectBase : public ObjectBase, public LinkNode<ApiObjectBase> {
      public:
        struct LabelNotImplementedTag {};
        static constexpr LabelNotImplementedTag kLabelNotImplemented = {};
        struct UntrackedByDeviceTag {};
        static constexpr UntrackedByDeviceTag kUntrackedByDevice = {};

        ApiObjectBase(DeviceBase* device, LabelNotImplementedTag tag);
        ApiObjectBase(DeviceBase* device, const char* label);
        ApiObjectBase(DeviceBase* device, ErrorTag tag);
        ~ApiObjectBase() override;

        virtual ObjectType GetType() const = 0;
        const std::string& GetLabel() const;

        // The ApiObjectBase is considered alive if it is tracked in a respective linked list owned
        // by the owning device.
        bool IsAlive() const;

        // Allow virtual overriding of actual destroy call in order to allow for re-using of base
        // destruction oerations. Classes that override this function should almost always call this
        // class's implementation in the override. This needs to be public because it can be called
        // from the device owning the object. Returns true iff destruction occurs. Upon any re-calls
        // of the function it will return false to indicate no further operations should be taken.
        virtual bool DestroyApiObject();

        // Dawn API
        void APISetLabel(const char* label);

      protected:
        // Overriding of the RefCounted's DeleteThis function ensures that instances of objects
        // always call their derived class implementation of DestroyApiObject prior to the derived
        // class being destroyed. This guarantees that when ApiObjects' reference counts drop to 0,
        // then the underlying backend's Destroy calls are executed. We cannot naively put the call
        // to DestroyApiObject in the destructor of this class because it calls DestroyApiObjectImpl
        // which is a virtual function often implemented in the Derived class which would already
        // have been destroyed by the time ApiObject's destructor is called by C++'s destruction
        // order. Note that some classes like BindGroup may override the DeleteThis function again,
        // and they should ensure that their overriding versions call this underlying version
        // somewhere.
        void DeleteThis() override;
        void TrackInDevice();

        // Thread-safe manner to mark an object as destroyed. Returns true iff the call was the
        // "winning" attempt to destroy the object. This is useful when sub-classes may have extra
        // pre-destruction steps that need to occur only once, i.e. Buffer needs to be unmapped
        // before being destroyed.
        bool MarkDestroyed();
        virtual void DestroyApiObjectImpl();

      private:
        virtual void SetLabelImpl();

        std::string mLabel;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_OBJECTBASE_H_
