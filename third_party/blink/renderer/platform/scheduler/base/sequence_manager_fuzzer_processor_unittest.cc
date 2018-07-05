// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/base/sequence_manager_fuzzer_processor.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/base/proto/sequence_manager_test_description.pb.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"
#include "third_party/protobuf/src/google/protobuf/util/message_differencer.h"

namespace base {
namespace sequence_manager {

using testing::ContainerEq;

class SequenceManagerFuzzerProcessorForTest
    : public SequenceManagerFuzzerProcessor {
 public:
  explicit SequenceManagerFuzzerProcessorForTest(
      const SequenceManagerTestDescription& description)
      : SequenceManagerFuzzerProcessor(description, true) {}

  static const std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
  ParseAndRun(std::string test_description) {
    SequenceManagerTestDescription proto_description;
    google::protobuf::TextFormat::ParseFromString(test_description,
                                                  &proto_description);

    SequenceManagerFuzzerProcessorForTest processor(proto_description);
    processor.RunTest();
    return processor.ordered_tasks();
  }

  using SequenceManagerFuzzerProcessor::ordered_tasks;
  using TaskForTest = SequenceManagerFuzzerProcessor::TaskForTest;
};

TEST(SequenceManagerFuzzerProcessorTest, CreateTaskQueue) {
  // Posts a instant task to create a task queue.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      executed_tasks = SequenceManagerFuzzerProcessorForTest::ParseAndRun(R"(
       initial_task {
        task {
         task_id : 1
         action {
          create_task_queue {
         }
        }
       }
      })");

  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      expected_tasks;
  expected_tasks.emplace_back(1, 0, 0);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest, PostDelayedTaskWithDuration) {
  // Posts an 10 ms delayed task of duration 20 ms.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      executed_tasks = SequenceManagerFuzzerProcessorForTest::ParseAndRun(R"(
      initial_task {
        task_queue_id : 1
          delay_ms : 10
          task {
            task_id : 1
            duration_ms : 20
          }
        })");

  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      expected_tasks;
  expected_tasks.emplace_back(1, 10, 30);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest,
     TaskDurationBlocksOtherPendingTasksPostedFromOutsideOfTask) {
  // Posts a 10 ms delayed task of duration 40 ms and a 20 ms delayed task of
  // duration 20 ms.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      executed_tasks = SequenceManagerFuzzerProcessorForTest::ParseAndRun(R"(
        initial_task {
          delay_ms : 20
          task {
            task_id : 1
            duration_ms : 20
         }
        }
        initial_task {
         delay_ms : 10
         task {
          task_id : 2
          duration_ms : 40
        }
       })");

  // Task with id 2 is expected to run first and block the other task until it
  // done.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      expected_tasks;
  expected_tasks.emplace_back(2, 10, 50);
  expected_tasks.emplace_back(1, 50, 70);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest,
     TaskDurationBlocksOtherNonNestableTaskWhenPostedFromTheWithinTask) {
  // Posts an instant task of duration 40 ms that posts another non-nested
  // instant task.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      executed_tasks = SequenceManagerFuzzerProcessorForTest::ParseAndRun(R"(
        initial_task {
          task {
           task_id : 1
           duration_ms : 40
           action {
            post_delayed_task {
              task {
                task_id : 2
              }
            }
          }
        }
      })");

  // Task with task id 1 is expected to run for 40 ms, and block the other
  // posted task from running until its done. Note that the task with id 2 is
  // blocked as it is non-nested, so it is not supposed to run from within the
  // posting task.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      expected_tasks;
  expected_tasks.emplace_back(1, 0, 40);
  expected_tasks.emplace_back(2, 40, 40);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest, OrderOfSimpleUnnestedExecutedActions) {
  // Creates a task after 5 ms of delay, creates a task after 10 ms of
  // delay, posts a task after 20 ms delay, posts a 10 ms duration task after 15
  // ms of delay, and posts a task after 100 ms of delay.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      executed_tasks = SequenceManagerFuzzerProcessorForTest::ParseAndRun(R"(
      initial_task {
          delay_ms : 5
          task {
            task_id : 1
            action {
             create_task_queue {
             }
            }
          }
        }
      initial_task {
        delay_ms : 20
        task {
         task_id : 2
        }
      }
      initial_task {
        delay_ms : 15
        task {
         task_id : 3
         duration_ms : 10
        }
      }
      initial_task {
       delay_ms : 10
       task {
         task_id : 4
         action {
          create_task_queue {
          }
        }
       }
      }
      initial_task {
       delay_ms : 100
        task {
        task_id : 5
       }
      })");

  // Tasks are expected to run in order of non-decreasing delay with ties broken
  // by order of posting. Note that the task with id 3 will block the task with
  // id 2 from running at its scheduled time.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      expected_tasks;
  expected_tasks.emplace_back(1, 5, 5);
  expected_tasks.emplace_back(4, 10, 10);
  expected_tasks.emplace_back(3, 15, 25);
  expected_tasks.emplace_back(2, 25, 25);
  expected_tasks.emplace_back(5, 100, 100);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest, PostNonEmptyTask) {
  // Posthes a 5 ms delayed task of duration 40 ms that creates a task queue,
  // posts a 4 ms delayed task, posts an instant task, creates a task queue,
  // and then posts a 40 ms delayed task.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      executed_tasks = SequenceManagerFuzzerProcessorForTest::ParseAndRun(R"(
      initial_task {
          delay_ms : 5
          task {
           task_id : 1
           duration_ms : 40
           action {
            create_task_queue {
            }
           }
           action {
            post_delayed_task {
             delay_ms : 4
             task {
              task_id : 2
             }
            }
           }
           action {
            post_delayed_task {
             task {
              task_id : 3
             }
            }
           }
           action {
            create_task_queue {
            }
           }
           action {
            post_delayed_task {
             delay_ms : 40
             task {
               task_id : 4
             }
            }
          }
        }
      })");

  // Task with task id 1 is expected to run first, and block all other pending
  // tasks until its done. The remaining tasks will be executed in
  // non-decreasing order of the delay parameter with ties broken by
  // the post order.
  std::vector<SequenceManagerFuzzerProcessorForTest::TaskForTest>
      expected_tasks;
  expected_tasks.emplace_back(1, 5, 45);
  expected_tasks.emplace_back(3, 45, 45);
  expected_tasks.emplace_back(2, 45, 45);
  expected_tasks.emplace_back(4, 45, 45);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

}  // namespace sequence_manager
}  // namespace base
