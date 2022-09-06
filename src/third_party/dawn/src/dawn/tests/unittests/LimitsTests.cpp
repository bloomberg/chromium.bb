// Copyright 2021 The Dawn Authors
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

#include <gtest/gtest.h>

#include "dawn/native/Limits.h"

// Test |GetDefaultLimits| returns the default.
TEST(Limits, GetDefaultLimits) {
    dawn::native::Limits limits = {};
    EXPECT_NE(limits.maxBindGroups, 4u);

    dawn::native::GetDefaultLimits(&limits);

    EXPECT_EQ(limits.maxBindGroups, 4u);
}

// Test |ReifyDefaultLimits| populates the default if
// values are undefined.
TEST(Limits, ReifyDefaultLimits_PopulatesDefault) {
    dawn::native::Limits limits;
    limits.maxComputeWorkgroupStorageSize = wgpu::kLimitU32Undefined;
    limits.maxStorageBufferBindingSize = wgpu::kLimitU64Undefined;

    dawn::native::Limits reified = dawn::native::ReifyDefaultLimits(limits);
    EXPECT_EQ(reified.maxComputeWorkgroupStorageSize, 16384u);
    EXPECT_EQ(reified.maxStorageBufferBindingSize, 134217728ul);
}

// Test |ReifyDefaultLimits| clamps to the default if
// values are worse than the default.
TEST(Limits, ReifyDefaultLimits_Clamps) {
    dawn::native::Limits limits;
    limits.maxStorageBuffersPerShaderStage = 4;
    limits.minUniformBufferOffsetAlignment = 512;

    dawn::native::Limits reified = dawn::native::ReifyDefaultLimits(limits);
    EXPECT_EQ(reified.maxStorageBuffersPerShaderStage, 8u);
    EXPECT_EQ(reified.minUniformBufferOffsetAlignment, 256u);
}

// Test |ValidateLimits| works to validate limits are not better
// than supported.
TEST(Limits, ValidateLimits) {
    // Start with the default for supported.
    dawn::native::Limits defaults;
    dawn::native::GetDefaultLimits(&defaults);

    // Test supported == required is valid.
    {
        dawn::native::Limits required = defaults;
        EXPECT_TRUE(ValidateLimits(defaults, required).IsSuccess());
    }

    // Test supported == required is valid, when they are not default.
    {
        dawn::native::Limits supported = defaults;
        dawn::native::Limits required = defaults;
        supported.maxBindGroups += 1;
        required.maxBindGroups += 1;
        EXPECT_TRUE(ValidateLimits(supported, required).IsSuccess());
    }

    // Test that default-initialized (all undefined) is valid.
    {
        dawn::native::Limits required = {};
        EXPECT_TRUE(ValidateLimits(defaults, required).IsSuccess());
    }

    // Test that better than supported is invalid for "maximum" limits.
    {
        dawn::native::Limits required = {};
        required.maxTextureDimension3D = defaults.maxTextureDimension3D + 1;
        dawn::native::MaybeError err = ValidateLimits(defaults, required);
        EXPECT_TRUE(err.IsError());
        err.AcquireError();
    }

    // Test that worse than supported is valid for "maximum" limits.
    {
        dawn::native::Limits required = {};
        required.maxComputeWorkgroupSizeX = defaults.maxComputeWorkgroupSizeX - 1;
        EXPECT_TRUE(ValidateLimits(defaults, required).IsSuccess());
    }

    // Test that better than min is invalid for "alignment" limits.
    {
        dawn::native::Limits required = {};
        required.minUniformBufferOffsetAlignment = defaults.minUniformBufferOffsetAlignment / 2;
        dawn::native::MaybeError err = ValidateLimits(defaults, required);
        EXPECT_TRUE(err.IsError());
        err.AcquireError();
    }

    // Test that worse than min and a power of two is valid for "alignment" limits.
    {
        dawn::native::Limits required = {};
        required.minStorageBufferOffsetAlignment = defaults.minStorageBufferOffsetAlignment * 2;
        EXPECT_TRUE(ValidateLimits(defaults, required).IsSuccess());
    }

    // Test that worse than min and not a power of two is invalid for "alignment" limits.
    {
        dawn::native::Limits required = {};
        required.minStorageBufferOffsetAlignment = defaults.minStorageBufferOffsetAlignment * 3;
        dawn::native::MaybeError err = ValidateLimits(defaults, required);
        EXPECT_TRUE(err.IsError());
        err.AcquireError();
    }
}

// Test that |ApplyLimitTiers| degrades limits to the next best tier.
TEST(Limits, ApplyLimitTiers) {
    auto SetLimitsStorageBufferBindingSizeTier2 = [](dawn::native::Limits* limits) {
        limits->maxStorageBufferBindingSize = 1073741824;
    };
    dawn::native::Limits limitsStorageBufferBindingSizeTier2;
    dawn::native::GetDefaultLimits(&limitsStorageBufferBindingSizeTier2);
    SetLimitsStorageBufferBindingSizeTier2(&limitsStorageBufferBindingSizeTier2);

    auto SetLimitsStorageBufferBindingSizeTier3 = [](dawn::native::Limits* limits) {
        limits->maxStorageBufferBindingSize = 2147483647;
    };
    dawn::native::Limits limitsStorageBufferBindingSizeTier3;
    dawn::native::GetDefaultLimits(&limitsStorageBufferBindingSizeTier3);
    SetLimitsStorageBufferBindingSizeTier3(&limitsStorageBufferBindingSizeTier3);

    auto SetLimitsComputeWorkgroupStorageSizeTier1 = [](dawn::native::Limits* limits) {
        limits->maxComputeWorkgroupStorageSize = 16384;
    };
    dawn::native::Limits limitsComputeWorkgroupStorageSizeTier1;
    dawn::native::GetDefaultLimits(&limitsComputeWorkgroupStorageSizeTier1);
    SetLimitsComputeWorkgroupStorageSizeTier1(&limitsComputeWorkgroupStorageSizeTier1);

    auto SetLimitsComputeWorkgroupStorageSizeTier3 = [](dawn::native::Limits* limits) {
        limits->maxComputeWorkgroupStorageSize = 65536;
    };
    dawn::native::Limits limitsComputeWorkgroupStorageSizeTier3;
    dawn::native::GetDefaultLimits(&limitsComputeWorkgroupStorageSizeTier3);
    SetLimitsComputeWorkgroupStorageSizeTier3(&limitsComputeWorkgroupStorageSizeTier3);

    // Test that applying tiers to limits that are exactly
    // equal to a tier returns the same values.
    {
        dawn::native::Limits limits = limitsStorageBufferBindingSizeTier2;
        EXPECT_EQ(ApplyLimitTiers(limits), limits);

        limits = limitsStorageBufferBindingSizeTier3;
        EXPECT_EQ(ApplyLimitTiers(limits), limits);
    }

    // Test all limits slightly worse than tier 3.
    {
        dawn::native::Limits limits = limitsStorageBufferBindingSizeTier3;
        limits.maxStorageBufferBindingSize -= 1;
        EXPECT_EQ(ApplyLimitTiers(limits), limitsStorageBufferBindingSizeTier2);
    }

    // Test that limits may match one tier exactly and be degraded in another tier.
    // Degrading to one tier does not affect the other tier.
    {
        dawn::native::Limits limits = limitsComputeWorkgroupStorageSizeTier3;
        // Set tier 3 and change one limit to be insufficent.
        SetLimitsStorageBufferBindingSizeTier3(&limits);
        limits.maxStorageBufferBindingSize -= 1;

        dawn::native::Limits tiered = ApplyLimitTiers(limits);

        // Check that |tiered| has the limits of memorySize tier 2
        dawn::native::Limits tieredWithMemorySizeTier2 = tiered;
        SetLimitsStorageBufferBindingSizeTier2(&tieredWithMemorySizeTier2);
        EXPECT_EQ(tiered, tieredWithMemorySizeTier2);

        // Check that |tiered| has the limits of bindingSpace tier 3
        dawn::native::Limits tieredWithBindingSpaceTier3 = tiered;
        SetLimitsComputeWorkgroupStorageSizeTier3(&tieredWithBindingSpaceTier3);
        EXPECT_EQ(tiered, tieredWithBindingSpaceTier3);
    }

    // Test that limits may be simultaneously degraded in two tiers independently.
    {
        dawn::native::Limits limits;
        dawn::native::GetDefaultLimits(&limits);
        SetLimitsComputeWorkgroupStorageSizeTier3(&limits);
        SetLimitsStorageBufferBindingSizeTier3(&limits);
        limits.maxComputeWorkgroupStorageSize =
            limitsComputeWorkgroupStorageSizeTier1.maxComputeWorkgroupStorageSize + 1;
        limits.maxStorageBufferBindingSize =
            limitsStorageBufferBindingSizeTier2.maxStorageBufferBindingSize + 1;

        dawn::native::Limits tiered = ApplyLimitTiers(limits);

        dawn::native::Limits expected = tiered;
        SetLimitsComputeWorkgroupStorageSizeTier1(&expected);
        SetLimitsStorageBufferBindingSizeTier2(&expected);
        EXPECT_EQ(tiered, expected);
    }
}
