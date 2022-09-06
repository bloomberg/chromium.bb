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

#include <vector>

#include "dawn/tests/unittests/validation/ValidationTest.h"

namespace {

class ToggleValidationTest : public ValidationTest {
    void SetUp() override {
        ValidationTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());
    }
};

// Tests querying the detail of a toggle from dawn::native::InstanceBase works correctly.
TEST_F(ToggleValidationTest, QueryToggleInfo) {
    // Query with a valid toggle name
    {
        const char* kValidToggleName = "emulate_store_and_msaa_resolve";
        const dawn::native::ToggleInfo* toggleInfo = GetToggleInfo(kValidToggleName);
        ASSERT_NE(nullptr, toggleInfo);
        ASSERT_NE(nullptr, toggleInfo->name);
        ASSERT_NE(nullptr, toggleInfo->description);
        ASSERT_NE(nullptr, toggleInfo->url);
    }

    // Query with an invalid toggle name
    {
        const char* kInvalidToggleName = "!@#$%^&*";
        const dawn::native::ToggleInfo* toggleInfo = GetToggleInfo(kInvalidToggleName);
        ASSERT_EQ(nullptr, toggleInfo);
    }
}

// Tests overriding toggles when creating a device works correctly.
TEST_F(ToggleValidationTest, OverrideToggleUsage) {
    // Create device with a valid name of a toggle
    {
        const char* kValidToggleName = "emulate_store_and_msaa_resolve";
        wgpu::DeviceDescriptor descriptor;
        wgpu::DawnTogglesDeviceDescriptor togglesDesc;
        descriptor.nextInChain = &togglesDesc;
        togglesDesc.forceEnabledToggles = &kValidToggleName;
        togglesDesc.forceEnabledTogglesCount = 1;

        wgpu::Device deviceWithToggle =
            wgpu::Device::Acquire(GetBackendAdapter().CreateDevice(&descriptor));
        std::vector<const char*> toggleNames = dawn::native::GetTogglesUsed(deviceWithToggle.Get());
        bool validToggleExists = false;
        for (const char* toggle : toggleNames) {
            if (strcmp(toggle, kValidToggleName) == 0) {
                validToggleExists = true;
            }
        }
        ASSERT_EQ(validToggleExists, true);
    }

    // Create device with an invalid toggle name
    {
        const char* kInvalidToggleName = "!@#$%^&*";
        wgpu::DeviceDescriptor descriptor;
        wgpu::DawnTogglesDeviceDescriptor togglesDesc;
        descriptor.nextInChain = &togglesDesc;
        togglesDesc.forceEnabledToggles = &kInvalidToggleName;
        togglesDesc.forceEnabledTogglesCount = 1;

        wgpu::Device deviceWithToggle =
            wgpu::Device::Acquire(GetBackendAdapter().CreateDevice(&descriptor));
        std::vector<const char*> toggleNames = dawn::native::GetTogglesUsed(deviceWithToggle.Get());
        bool InvalidToggleExists = false;
        for (const char* toggle : toggleNames) {
            if (strcmp(toggle, kInvalidToggleName) == 0) {
                InvalidToggleExists = true;
            }
        }
        ASSERT_EQ(InvalidToggleExists, false);
    }
}

TEST_F(ToggleValidationTest, TurnOffVsyncWithToggle) {
    const char* kValidToggleName = "turn_off_vsync";
    wgpu::DeviceDescriptor descriptor;
    wgpu::DawnTogglesDeviceDescriptor togglesDesc;
    descriptor.nextInChain = &togglesDesc;
    togglesDesc.forceEnabledToggles = &kValidToggleName;
    togglesDesc.forceEnabledTogglesCount = 1;

    wgpu::Device deviceWithToggle =
        wgpu::Device::Acquire(GetBackendAdapter().CreateDevice(&descriptor));
    std::vector<const char*> toggleNames = dawn::native::GetTogglesUsed(deviceWithToggle.Get());
    bool validToggleExists = false;
    for (const char* toggle : toggleNames) {
        if (strcmp(toggle, kValidToggleName) == 0) {
            validToggleExists = true;
        }
    }
    ASSERT_EQ(validToggleExists, true);
}
}  // anonymous namespace
