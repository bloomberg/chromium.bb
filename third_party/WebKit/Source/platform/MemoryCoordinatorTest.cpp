// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/MemoryCoordinator.h"

#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MemoryCoordinatorTest : public ::testing::Test {};

TEST_F(MemoryCoordinatorTest, GetApproximatedDeviceMemory) {
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(128);  // 128MB
  EXPECT_EQ(0.125, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(256);  // 256MB
  EXPECT_EQ(0.25, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(510);  // <512MB
  EXPECT_EQ(0.5, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(512);  // 512MB
  EXPECT_EQ(0.5, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(640);  // 512+128MB
  EXPECT_EQ(0.5, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(768);  // 512+256MB
  EXPECT_EQ(0.75, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(1000);  // <1GB
  EXPECT_EQ(1, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(1024);  // 1GB
  EXPECT_EQ(1, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(1536);  // 1.5GB
  EXPECT_EQ(1.5, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(2000);  // <2GB
  EXPECT_EQ(2, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(2048);  // 2GB
  EXPECT_EQ(2, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(3000);  // <3GB
  EXPECT_EQ(3, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(5120);  // 5GB
  EXPECT_EQ(4, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(8192);  // 8GB
  EXPECT_EQ(8, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(16384);  // 16GB
  EXPECT_EQ(16, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(32768);  // 32GB
  EXPECT_EQ(32, MemoryCoordinator::GetApproximatedDeviceMemory());
  MemoryCoordinator::SetPhysicalMemoryMBForTesting(64385);  // <64GB
  EXPECT_EQ(64, MemoryCoordinator::GetApproximatedDeviceMemory());
}
}  // namespace blink
