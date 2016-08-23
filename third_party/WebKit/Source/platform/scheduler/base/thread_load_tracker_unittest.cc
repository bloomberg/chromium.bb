#include "platform/scheduler/base/thread_load_tracker.h"

#include "base/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

namespace {

void AddToVector(std::vector<std::pair<base::TimeTicks, double>>* vector,
                 base::TimeTicks time,
                 double load) {
  vector->push_back({time, load});
}

base::TimeTicks SecondsToTime(int seconds) {
  return base::TimeTicks() + base::TimeDelta::FromSeconds(seconds);
}

double Divide(int a, int b) {
  return static_cast<double>(a) / b;
}

}  // namespace

TEST(ThreadLoadTrackerTest, RecordTasks) {
  std::vector<std::pair<base::TimeTicks, double>> result;

  ThreadLoadTracker thread_load_tracker(
      SecondsToTime(1), base::Bind(&AddToVector, base::Unretained(&result)));

  thread_load_tracker.RecordTaskTime(SecondsToTime(3), SecondsToTime(5));
  thread_load_tracker.RecordTaskTime(SecondsToTime(7), SecondsToTime(9));
  thread_load_tracker.RecordTaskTime(SecondsToTime(10), SecondsToTime(13));
  thread_load_tracker.RecordTaskTime(SecondsToTime(15), SecondsToTime(18));

  // Because of 10-second delay we're getting information starting with 11s.
  std::vector<std::pair<base::TimeTicks, double>> expected_result{
      {SecondsToTime(11), Divide(5, 10)}, {SecondsToTime(12), Divide(6, 11)},
      {SecondsToTime(13), Divide(7, 12)}, {SecondsToTime(14), Divide(7, 13)},
      {SecondsToTime(15), Divide(7, 14)}, {SecondsToTime(16), Divide(8, 15)},
      {SecondsToTime(17), Divide(9, 16)}, {SecondsToTime(18), Divide(10, 17)}};

  EXPECT_EQ(result, expected_result);
}

TEST(ThreadLoadTrackerTest, PauseAndResume) {
  std::vector<std::pair<base::TimeTicks, double>> result;

  ThreadLoadTracker thread_load_tracker(
      SecondsToTime(1), base::Bind(&AddToVector, base::Unretained(&result)));

  thread_load_tracker.RecordTaskTime(SecondsToTime(3), SecondsToTime(4));
  thread_load_tracker.Pause(SecondsToTime(5));
  // This task should be ignored.
  thread_load_tracker.RecordTaskTime(SecondsToTime(10), SecondsToTime(11));
  thread_load_tracker.Resume(SecondsToTime(15));
  // We're counting only last second.
  thread_load_tracker.RecordTaskTime(SecondsToTime(14), SecondsToTime(16));
  thread_load_tracker.RecordTaskTime(SecondsToTime(22), SecondsToTime(23));

  std::vector<std::pair<base::TimeTicks, double>> expected_result{
      {SecondsToTime(21), Divide(2, 10)},
      {SecondsToTime(22), Divide(2, 11)},
      {SecondsToTime(23), Divide(3, 12)}};

  EXPECT_EQ(result, expected_result);
}

}  // namespace scheduler
}  // namespace blink
