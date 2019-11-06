// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_system_stat_collector.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using ArcSystemStatCollectorTest = testing::Test;

namespace {

base::FilePath GetPath(const std::string& name) {
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  return base_path.Append("arc_graphics_tracing").Append(name);
}

}  // namespace

TEST_F(ArcSystemStatCollectorTest, Parse) {
  int64_t zram_values[3];
  EXPECT_TRUE(ParseStatFile(GetPath("zram_stat"),
                            ArcSystemStatCollector::kZramStatColumns,
                            zram_values));
  EXPECT_EQ(2384, zram_values[0]);
  EXPECT_EQ(56696, zram_values[1]);
  EXPECT_EQ(79, zram_values[2]);

  int64_t mem_values[2];
  EXPECT_TRUE(ParseStatFile(GetPath("proc_meminfo"),
                            ArcSystemStatCollector::kMemInfoColumns,
                            mem_values));
  EXPECT_EQ(8058940, mem_values[0]);
  EXPECT_EQ(2714260, mem_values[1]);

  int64_t gem_values[2];
  EXPECT_TRUE(ParseStatFile(GetPath("gem_objects"),
                            ArcSystemStatCollector::kGemInfoColumns,
                            gem_values));
  EXPECT_EQ(853, gem_values[0]);
  EXPECT_EQ(458256384, gem_values[1]);

  int64_t cpu_temp;
  EXPECT_TRUE(ParseStatFile(GetPath("temp1_input"),
                            ArcSystemStatCollector::kCpuTempInfoColumns,
                            &cpu_temp));
  EXPECT_EQ(52000, cpu_temp);
}

}  // namespace arc
