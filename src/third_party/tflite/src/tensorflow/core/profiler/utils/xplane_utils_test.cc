/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/core/profiler/utils/xplane_utils.h"

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "tensorflow/core/platform/env_time.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/profiler/protobuf/xplane.pb.h"
#include "tensorflow/core/profiler/utils/xplane_builder.h"
#include "tensorflow/core/profiler/utils/xplane_visitor.h"

namespace tensorflow {
namespace profiler {
namespace {

XEvent CreateEvent(int64 offset_ps, int64 duration_ps) {
  XEvent event;
  event.set_offset_ps(offset_ps);
  event.set_duration_ps(duration_ps);
  return event;
}

// Tests IsNested.
TEST(XPlaneUtilsTest, IsNestedTest) {
  XEvent event = CreateEvent(100, 100);
  XEvent parent = CreateEvent(50, 200);
  EXPECT_TRUE(IsNested(event, parent));
  // Returns false if there is no overlap.
  XEvent not_parent = CreateEvent(30, 50);
  EXPECT_FALSE(IsNested(event, not_parent));
  // Returns false if they overlap only partially.
  not_parent = CreateEvent(50, 100);
  EXPECT_FALSE(IsNested(event, not_parent));
}

TEST(XPlaneUtilsTest, RemovePlaneWithName) {
  XSpace space;
  RemovePlaneWithName(&space, "non-exist");
  EXPECT_EQ(space.planes_size(), 0);

  space.add_planes()->set_name("p1");
  space.add_planes()->set_name("p2");
  space.add_planes()->set_name("p3");
  RemovePlaneWithName(&space, "non-exist");
  EXPECT_EQ(space.planes_size(), 3);
  RemovePlaneWithName(&space, "p2");
  EXPECT_EQ(space.planes_size(), 2);
  RemovePlaneWithName(&space, "p1");
  EXPECT_EQ(space.planes_size(), 1);
  RemovePlaneWithName(&space, "p1");
  EXPECT_EQ(space.planes_size(), 1);
  RemovePlaneWithName(&space, "p3");
  EXPECT_EQ(space.planes_size(), 0);
}

TEST(XPlaneUtilsTest, RemoveEmptyPlanes) {
  XSpace space;
  RemoveEmptyPlanes(&space);
  EXPECT_EQ(space.planes_size(), 0);

  auto* plane1 = space.add_planes();
  plane1->set_name("p1");
  plane1->add_lines()->set_name("p1l1");
  plane1->add_lines()->set_name("p1l2");

  auto* plane2 = space.add_planes();
  plane2->set_name("p2");

  auto* plane3 = space.add_planes();
  plane3->set_name("p3");
  plane3->add_lines()->set_name("p3l1");

  auto* plane4 = space.add_planes();
  plane4->set_name("p4");

  RemoveEmptyPlanes(&space);
  ASSERT_EQ(space.planes_size(), 2);
  EXPECT_EQ(space.planes(0).name(), "p1");
  EXPECT_EQ(space.planes(1).name(), "p3");
}

TEST(XPlaneUtilsTest, RemoveEmptyLines) {
  XPlane plane;
  RemoveEmptyLines(&plane);
  EXPECT_EQ(plane.lines_size(), 0);

  auto* line1 = plane.add_lines();
  line1->set_name("l1");
  line1->add_events();
  line1->add_events();

  auto* line2 = plane.add_lines();
  line2->set_name("l2");

  auto* line3 = plane.add_lines();
  line3->set_name("l3");
  line3->add_events();

  auto* line4 = plane.add_lines();
  line4->set_name("l4");

  RemoveEmptyLines(&plane);
  ASSERT_EQ(plane.lines_size(), 2);
  EXPECT_EQ(plane.lines(0).name(), "l1");
  EXPECT_EQ(plane.lines(1).name(), "l3");
}

TEST(XPlaneUtilsTest, SortXPlaneTest) {
  XPlane plane;
  XLine* line = plane.add_lines();
  *line->add_events() = CreateEvent(200, 100);
  *line->add_events() = CreateEvent(100, 100);
  *line->add_events() = CreateEvent(120, 50);
  *line->add_events() = CreateEvent(120, 30);
  SortXPlane(&plane);
  ASSERT_EQ(plane.lines_size(), 1);
  ASSERT_EQ(plane.lines(0).events_size(), 4);
  EXPECT_EQ(plane.lines(0).events(0).offset_ps(), 100);
  EXPECT_EQ(plane.lines(0).events(0).duration_ps(), 100);
  EXPECT_EQ(plane.lines(0).events(1).offset_ps(), 120);
  EXPECT_EQ(plane.lines(0).events(1).duration_ps(), 50);
  EXPECT_EQ(plane.lines(0).events(2).offset_ps(), 120);
  EXPECT_EQ(plane.lines(0).events(2).duration_ps(), 30);
  EXPECT_EQ(plane.lines(0).events(3).offset_ps(), 200);
  EXPECT_EQ(plane.lines(0).events(3).duration_ps(), 100);
}

namespace {

XLineBuilder CreateXLine(XPlaneBuilder* plane, absl::string_view name,
                         absl::string_view display, int64 id,
                         int64 timestamp_ns) {
  XLineBuilder line = plane->GetOrCreateLine(id);
  line.SetName(name);
  line.SetTimestampNs(timestamp_ns);
  line.SetDisplayNameIfEmpty(display);
  return line;
}

XEventBuilder CreateXEvent(XPlaneBuilder* plane, XLineBuilder line,
                           absl::string_view event_name,
                           absl::optional<absl::string_view> display,
                           int64 offset_ns, int64 duration_ns) {
  XEventMetadata* event_metadata = plane->GetOrCreateEventMetadata(event_name);
  if (display) event_metadata->set_display_name(std::string(*display));
  XEventBuilder event = line.AddEvent(*event_metadata);
  event.SetOffsetNs(offset_ns);
  event.SetDurationNs(duration_ns);
  return event;
}

template <typename T, typename V>
void CreateXStats(XPlaneBuilder* plane, T* stats_owner,
                  absl::string_view stats_name, V stats_value) {
  stats_owner->AddStatValue(*plane->GetOrCreateStatMetadata(stats_name),
                            stats_value);
}

void CheckXLine(const XLine& line, absl::string_view name,
                absl::string_view display, int64 start_time_ns,
                int64 events_size) {
  EXPECT_EQ(line.name(), name);
  EXPECT_EQ(line.display_name(), display);
  EXPECT_EQ(line.timestamp_ns(), start_time_ns);
  EXPECT_EQ(line.events_size(), events_size);
}

void CheckXEvent(const XEvent& event, const XPlane& plane,
                 absl::string_view name, absl::string_view display,
                 int64 offset_ns, int64 duration_ns, int64 stats_size) {
  const XEventMetadata& event_metadata =
      plane.event_metadata().at(event.metadata_id());
  EXPECT_EQ(event_metadata.name(), name);
  EXPECT_EQ(event_metadata.display_name(), display);
  EXPECT_EQ(event.offset_ps(), offset_ns * EnvTime::kNanosToPicos);
  EXPECT_EQ(event.duration_ps(), duration_ns * EnvTime::kNanosToPicos);
  EXPECT_EQ(event.stats_size(), stats_size);
}
}  // namespace

TEST(XPlaneUtilsTest, MergeXPlaneTest) {
  XPlane src_plane, dst_plane;
  constexpr int64 kLineIdOnlyInSrcPlane = 1LL;
  constexpr int64 kLineIdOnlyInDstPlane = 2LL;
  constexpr int64 kLineIdInBothPlanes = 3LL;   // src start ts < dst start ts
  constexpr int64 kLineIdInBothPlanes2 = 4LL;  // src start ts > dst start ts

  {  // Populate the source plane.
    XPlaneBuilder src(&src_plane);
    CreateXStats(&src, &src, "plane_stat1", 1);    // only in source.
    CreateXStats(&src, &src, "plane_stat3", 3.0);  // shared by source/dest.

    auto l1 = CreateXLine(&src, "l1", "d1", kLineIdOnlyInSrcPlane, 100);
    auto e1 = CreateXEvent(&src, l1, "event1", "display1", 1, 2);
    CreateXStats(&src, &e1, "event_stat1", 2.0);
    auto e2 = CreateXEvent(&src, l1, "event2", absl::nullopt, 3, 4);
    CreateXStats(&src, &e2, "event_stat2", 3);

    auto l2 = CreateXLine(&src, "l2", "d2", kLineIdInBothPlanes, 200);
    auto e3 = CreateXEvent(&src, l2, "event3", absl::nullopt, 5, 7);
    CreateXStats(&src, &e3, "event_stat3", 2.0);
    auto e4 = CreateXEvent(&src, l2, "event4", absl::nullopt, 6, 8);
    CreateXStats(&src, &e4, "event_stat4", 3);
    CreateXStats(&src, &e4, "event_stat5", 3);

    auto l5 = CreateXLine(&src, "l5", "d5", kLineIdInBothPlanes2, 700);
    CreateXEvent(&src, l5, "event51", absl::nullopt, 9, 10);
    CreateXEvent(&src, l5, "event52", absl::nullopt, 11, 12);
  }

  {  // Populate the destination plane.
    XPlaneBuilder dst(&dst_plane);
    CreateXStats(&dst, &dst, "plane_stat2", 2);  // only in dest
    CreateXStats(&dst, &dst, "plane_stat3", 4);  // shared but different.

    auto l3 = CreateXLine(&dst, "l3", "d3", kLineIdOnlyInDstPlane, 300);
    auto e5 = CreateXEvent(&dst, l3, "event5", absl::nullopt, 11, 2);
    CreateXStats(&dst, &e5, "event_stat6", 2.0);
    auto e6 = CreateXEvent(&dst, l3, "event6", absl::nullopt, 13, 4);
    CreateXStats(&dst, &e6, "event_stat7", 3);

    auto l2 = CreateXLine(&dst, "l4", "d4", kLineIdInBothPlanes, 400);
    auto e7 = CreateXEvent(&dst, l2, "event7", absl::nullopt, 15, 7);
    CreateXStats(&dst, &e7, "event_stat8", 2.0);
    auto e8 = CreateXEvent(&dst, l2, "event8", "display8", 16, 8);
    CreateXStats(&dst, &e8, "event_stat9", 3);

    auto l6 = CreateXLine(&dst, "l6", "d6", kLineIdInBothPlanes2, 300);
    CreateXEvent(&dst, l6, "event61", absl::nullopt, 21, 10);
    CreateXEvent(&dst, l6, "event62", absl::nullopt, 22, 12);
  }

  MergePlanes(src_plane, &dst_plane);

  XPlaneVisitor plane(&dst_plane);
  EXPECT_EQ(dst_plane.lines_size(), 4);

  // Check plane level stats,
  EXPECT_EQ(dst_plane.stats_size(), 3);
  absl::flat_hash_map<absl::string_view, absl::string_view> plane_stats;
  plane.ForEachStat([&](const tensorflow::profiler::XStatVisitor& stat) {
    if (stat.Name() == "plane_stat1") {
      EXPECT_EQ(stat.IntValue(), 1);
    } else if (stat.Name() == "plane_stat2") {
      EXPECT_EQ(stat.IntValue(), 2);
    } else if (stat.Name() == "plane_stat3") {
      // XStat in src_plane overrides the counter-part in dst_plane.
      EXPECT_EQ(stat.DoubleValue(), 3.0);
    } else {
      EXPECT_TRUE(false);
    }
  });

  // 3 plane level stats, 9 event level stats.
  EXPECT_EQ(dst_plane.stat_metadata_size(), 12);

  {  // Old lines are untouched.
    const XLine& line = dst_plane.lines(0);
    CheckXLine(line, "l3", "d3", 300, 2);
    CheckXEvent(line.events(0), dst_plane, "event5", "", 11, 2, 1);
    CheckXEvent(line.events(1), dst_plane, "event6", "", 13, 4, 1);
  }
  {  // Lines with the same id are merged.
    // src plane start timestamp > dst plane start timestamp
    const XLine& line = dst_plane.lines(1);
    // NOTE: use minimum start time of src/dst.
    CheckXLine(line, "l4", "d4", 200, 4);
    CheckXEvent(line.events(0), dst_plane, "event7", "", 215, 7, 1);
    CheckXEvent(line.events(1), dst_plane, "event8", "display8", 216, 8, 1);
    CheckXEvent(line.events(2), dst_plane, "event3", "", 5, 7, 1);
    CheckXEvent(line.events(3), dst_plane, "event4", "", 6, 8, 2);
  }
  {  // Lines with the same id are merged.
    // src plane start timestamp < dst plane start timestamp
    const XLine& line = dst_plane.lines(2);
    CheckXLine(line, "l6", "d6", 300, 4);
    CheckXEvent(line.events(0), dst_plane, "event61", "", 21, 10, 0);
    CheckXEvent(line.events(1), dst_plane, "event62", "", 22, 12, 0);
    CheckXEvent(line.events(2), dst_plane, "event51", "", 409, 10, 0);
    CheckXEvent(line.events(3), dst_plane, "event52", "", 411, 12, 0);
  }
  {  // Lines only in source are "copied".
    const XLine& line = dst_plane.lines(3);
    CheckXLine(line, "l1", "d1", 100, 2);
    CheckXEvent(line.events(0), dst_plane, "event1", "display1", 1, 2, 1);
    CheckXEvent(line.events(1), dst_plane, "event2", "", 3, 4, 1);
  }
}

}  // namespace
}  // namespace profiler
}  // namespace tensorflow
