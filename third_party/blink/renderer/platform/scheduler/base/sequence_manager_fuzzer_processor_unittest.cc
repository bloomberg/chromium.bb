// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/base/sequence_manager_fuzzer_processor.h"

#include <memory>

#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/base/proto/sequence_manager_test_description.pb.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"
#include "third_party/protobuf/src/google/protobuf/util/message_differencer.h"

namespace base {
namespace sequence_manager {

using testing::ContainerEq;
using testing::IsEmpty;
using testing::UnorderedElementsAreArray;

class SequenceManagerFuzzerProcessorForTest
    : public SequenceManagerFuzzerProcessor {
 public:
  SequenceManagerFuzzerProcessorForTest()
      : SequenceManagerFuzzerProcessor(true) {}

  static void ParseAndRun(
      std::string test_description,
      std::vector<std::vector<TaskForTest>>* executed_tasks,
      std::vector<std::vector<ActionForTest>>* executed_actions) {
    SequenceManagerTestDescription proto_description;
    google::protobuf::TextFormat::ParseFromString(test_description,
                                                  &proto_description);

    SequenceManagerFuzzerProcessorForTest processor;
    processor.RunTest(proto_description);

    if (executed_tasks)
      *executed_tasks = processor.ordered_tasks();
    if (executed_actions)
      *executed_actions = processor.ordered_actions();
  }

  static void ParseAndRunSingleThread(
      std::string test_description,
      std::vector<TaskForTest>* executed_tasks,
      std::vector<ActionForTest>* executed_actions) {
    SequenceManagerTestDescription proto_description;

    google::protobuf::TextFormat::ParseFromString(
        base::StrCat(
            {"main_thread_actions { create_thread {", test_description, "}}"}),
        &proto_description);

    SequenceManagerFuzzerProcessorForTest processor;
    processor.RunTest(proto_description);

    if (executed_tasks)
      *executed_tasks = processor.ordered_tasks()[1];
    if (executed_actions)
      *executed_actions = processor.ordered_actions()[1];
  }

  using SequenceManagerFuzzerProcessor::ordered_actions;
  using SequenceManagerFuzzerProcessor::ordered_tasks;

  using SequenceManagerFuzzerProcessor::ActionForTest;
  using SequenceManagerFuzzerProcessor::TaskForTest;
};

using ActionForTest = SequenceManagerFuzzerProcessorForTest::ActionForTest;
using TaskForTest = SequenceManagerFuzzerProcessorForTest::TaskForTest;

TEST(SequenceManagerFuzzerProcessorTest, CreateTaskQueue) {
  std::vector<ActionForTest> executed_actions;

  // Describes a test that creates a task queue and posts a task to create a
  // task queue.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
       initial_thread_actions {
         action_id : 1
         create_task_queue {
         }
       }
       initial_thread_actions {
         action_id : 2
         post_delayed_task {
           task {
             task_id : 1
             actions {
               action_id : 3
               create_task_queue {
               }
             }
           }
         }
       })",
      nullptr, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kCreateTaskQueue,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kCreateTaskQueue,
                                0);
  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest, CreateQueueVoter) {
  std::vector<ActionForTest> executed_actions;

  // Describes a test that creates a voter and posts a task to create a queue
  // voter.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
       initial_thread_actions {
         action_id : 1
         create_queue_voter {
           task_queue_id : 1
         }
       }
       initial_thread_actions {
         action_id : 2
         post_delayed_task {
           task {
             task_id : 1
             actions {
               action_id : 3
               create_queue_voter {
               }
             }
           }
         }
       })",
      nullptr, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kCreateQueueVoter,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kCreateQueueVoter,
                                0);
  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest, PostDelayedTaskWithDuration) {
  std::vector<TaskForTest> executed_tasks;
  std::vector<ActionForTest> executed_actions;

  // Posts an 10 ms delayed task of duration 20 ms.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
      initial_thread_actions {
        action_id : 1
        post_delayed_task {
          task_queue_id : 1
          delay_ms : 10
          task {
            task_id : 1
            duration_ms : 20
          }
        }
      })",
      &executed_tasks, &executed_actions);

  std::vector<TaskForTest> expected_tasks;
  expected_tasks.emplace_back(1, 10, 30);
  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest, SetQueuePriority) {
  std::vector<ActionForTest> executed_actions;

  // Describes a test that sets the priority of queue and posts a task to set
  // the priority of a queue.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
       initial_thread_actions {
          action_id : 1
          set_queue_priority {
            task_queue_id: 2
            priority: CONTROL
          }
       }
       initial_thread_actions {
         action_id : 2
         post_delayed_task {
           task {
             task_id : 1
             actions {
               action_id : 3
               set_queue_priority {
                 task_queue_id : 1
                 priority : LOW
               }
             }
           }
         }
       })",
      nullptr, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kSetQueuePriority,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kSetQueuePriority,
                                0);

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest, SetQueueEnabled) {
  std::vector<ActionForTest> executed_actions;
  std::vector<TaskForTest> executed_tasks;

  // Describes a test that posts a number of tasks to a certain queue, disable
  // that queue, and post some more tasks to the same queue.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
       initial_thread_actions {
         action_id : 1
         post_delayed_task {
           task_queue_id: 1
           task {
             task_id : 1
           }
         }
       }
       initial_thread_actions {
         action_id : 2
         post_delayed_task {
           delay_ms : 10
           task_queue_id: 1
           task {
             task_id : 2
           }
         }
       }
       initial_thread_actions {
         action_id : 3
         set_queue_enabled {
           task_queue_id: 1
           enabled: false
         }
       }
      initial_thread_actions {
        action_id : 4
        post_delayed_task {
          task_queue_id: 1
          task {
            task_id : 3
          }
        }
      })",
      &executed_tasks, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kSetQueueEnabled,
                                0);
  expected_actions.emplace_back(4, ActionForTest::ActionType::kPostDelayedTask,
                                0);

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));

  // All the tasks posted to the task queue with id 1 do not get executed since
  // this task queue is disabled.
  EXPECT_THAT(executed_tasks, IsEmpty());
}

TEST(SequenceManagerFuzzerProcessorTest, SetQueueEnabledWithDelays) {
  std::vector<TaskForTest> executed_tasks;

  // Describes a test that posts two tasks to disable and enable a queue after
  // 10ms and 20ms, respectively; and other no-op tasks in the different
  // intervals to verify that the queue is indeed being disabled/enabled
  // properly.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
       initial_thread_actions {
         action_id : 1
         create_task_queue {
         }
       }
       initial_thread_actions {
         action_id : 2
         post_delayed_task {
           task_queue_id: 1
           task {
             task_id : 1
           }
         }
       }
       initial_thread_actions {
         action_id : 3
         post_delayed_task {
           delay_ms : 15
           task_queue_id: 1
           task {
             task_id : 2
           }
         }
       }
       initial_thread_actions {
         action_id : 4
         post_delayed_task {
           delay_ms : 10
           task_queue_id: 1
           task {
             task_id : 3
             actions {
               action_id : 5
               set_queue_enabled {
                 task_queue_id: 1
                 enabled: false
               }
             }
           }
         }
       }
       initial_thread_actions {
         action_id : 6
         post_delayed_task {
           delay_ms : 20
           task_queue_id: 0
           task {
             task_id : 4
             actions {
               action_id : 7
               set_queue_enabled {
                 task_queue_id: 1
                 enabled: true
               }
             }
           }
         }
       }
       initial_thread_actions {
         action_id : 8
         post_delayed_task {
           task_queue_id: 1
           delay_ms : 20
           task {
             task_id : 5
           }
         }
       })",
      &executed_tasks, nullptr);

  std::vector<TaskForTest> expected_tasks;

  expected_tasks.emplace_back(1, 0, 0);

  // Task that disables the queue.
  expected_tasks.emplace_back(3, 10, 10);

  // Task that enable the queue.
  expected_tasks.emplace_back(4, 20, 20);

  // Task couldn't execute at scheduled time i.e. 15ms since its queue was
  // disabled at that time.
  expected_tasks.emplace_back(2, 20, 20);
  expected_tasks.emplace_back(5, 20, 20);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest, MultipleVoters) {
  std::vector<ActionForTest> executed_actions;
  std::vector<TaskForTest> executed_tasks;

  // Describes a test that creates two voters for a queue, where one voter
  // enables the queue, and the other disables it.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
      initial_thread_actions {
         action_id : 1
         create_queue_voter {
           task_queue_id : 1
         }
       }
       initial_thread_actions {
         action_id : 2
         create_queue_voter {
           task_queue_id : 1
         }
       }
       initial_thread_actions {
         action_id : 3
         set_queue_enabled {
           voter_id : 1
           task_queue_id : 1
           enabled : true
         }
       }
       initial_thread_actions {
         action_id : 4
         set_queue_enabled {
           voter_id : 2
           task_queue_id : 1
           enabled : false
         }
       }
       initial_thread_actions {
         action_id : 5
         post_delayed_task {
           task_queue_id: 1
           task {
             task_id : 1
           }
         }
       })",
      &executed_tasks, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kCreateQueueVoter,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kCreateQueueVoter,
                                0);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kSetQueueEnabled,
                                0);
  expected_actions.emplace_back(4, ActionForTest::ActionType::kSetQueueEnabled,
                                0);
  expected_actions.emplace_back(5, ActionForTest::ActionType::kPostDelayedTask,
                                0);

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));

  std::vector<TaskForTest> expected_tasks;

  // Queue is enabled only if all voters enable it.
  EXPECT_THAT(executed_tasks, IsEmpty());
}

TEST(SequenceManagerFuzzerProcessorTest, ShutdownTaskQueue) {
  std::vector<ActionForTest> executed_actions;
  std::vector<TaskForTest> executed_tasks;

  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
       initial_thread_actions {
         action_id : 1
         create_task_queue {
         }
       }
       initial_thread_actions {
         action_id : 2
         post_delayed_task {
           task_queue_id: 1
           task {
             task_id : 1
           }
         }
        }
        initial_thread_actions {
          action_id :3
          post_delayed_task {
            delay_ms : 10
            task_queue_id: 1
            task {
              task_id : 2
            }
          }
        }
        initial_thread_actions {
          action_id : 4
          post_delayed_task {
            task_queue_id: 0
            delay_ms : 10
            task {
              task_id : 3
            }
          }
        }
        initial_thread_actions {
          action_id : 5
          shutdown_task_queue {
            task_queue_id: 1
          }
        }
        initial_thread_actions {
          action_id : 6
          post_delayed_task {
            task_queue_id: 1
            task {
              task_id : 4
            }
          }
        })",
      &executed_tasks, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kCreateTaskQueue,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(4, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(
      5, ActionForTest::ActionType::kShutdownTaskQueue, 0);
  expected_actions.emplace_back(6, ActionForTest::ActionType::kPostDelayedTask,
                                0);

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));

  std::vector<TaskForTest> expected_tasks;

  // Note that the task with id 4 isn't posted to the queue that was shutdown,
  // since that was posted to the first available queue (Check
  // sequence_manager_test_description.proto for more details).
  expected_tasks.emplace_back(4, 0, 0);
  expected_tasks.emplace_back(3, 10, 10);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest,
     ShutdownTaskQueueWhenOneQueueAvailable) {
  std::vector<TaskForTest> executed_tasks;
  std::vector<ActionForTest> executed_actions;
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
        initial_thread_actions {
          action_id : 1
          post_delayed_task {
            task {
              task_id : 1
            }
          }
        }
        initial_thread_actions {
          action_id : 2
          shutdown_task_queue {
            task_queue_id: 1
          }
        })",
      &executed_tasks, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(
      2, ActionForTest::ActionType::kShutdownTaskQueue, 0);

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));

  std::vector<TaskForTest> expected_tasks;

  // We always want to have a default task queue in every thread. So, if
  // we have only one queue, the shutdown action is effectively a no-op.
  expected_tasks.emplace_back(1, 0, 0);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest, ShutdownPostingTaskQueue) {
  std::vector<TaskForTest> executed_tasks;
  std::vector<ActionForTest> executed_actions;
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
        initial_thread_actions {
          action_id : 1
          create_task_queue {
          }
        }
        initial_thread_actions {
          action_id : 2
          post_delayed_task {
            task_queue_id : 0
            task{
              task_id : 1
              actions {
                action_id : 3
                shutdown_task_queue {
                  task_queue_id : 0
                }
              }
            }
          }
        })",
      &executed_tasks, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kCreateTaskQueue,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(
      3, ActionForTest::ActionType::kShutdownTaskQueue, 0);

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));

  std::vector<TaskForTest> expected_tasks;
  expected_tasks.emplace_back(1, 0, 0);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest, CancelParentTask) {
  std::vector<ActionForTest> executed_actions;
  std::vector<TaskForTest> executed_tasks;

  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
    initial_thread_actions {
      action_id : 1
      post_delayed_task {
        task {
          task_id : 0
          actions {
            action_id : 2
            post_delayed_task {
              task {
                task_id : 1
              }
            }
          }
          actions {
            action_id : 3
            cancel_task {
              task_id : 0
            }
          }
          actions {
            action_id : 4
            post_delayed_task {
              task {
                task_id : 2
              }
            }
          }
        }
      }
    })",
      &executed_tasks, &executed_actions);

  std::vector<ActionForTest> expected_actions;

  expected_actions.emplace_back(1, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kCancelTask, 0);
  expected_actions.emplace_back(4, ActionForTest::ActionType::kPostDelayedTask,
                                0);

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));

  std::vector<TaskForTest> expected_tasks;

  expected_tasks.emplace_back(0, 0, 0);
  expected_tasks.emplace_back(1, 0, 0);
  expected_tasks.emplace_back(2, 0, 0);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest, CancelTask) {
  std::vector<TaskForTest> executed_tasks;
  std::vector<ActionForTest> executed_actions;

  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
    initial_thread_actions {
      action_id : 1
      post_delayed_task {
        task {
          task_id : 1
        }
      }
    }
    initial_thread_actions {
      action_id : 2
      cancel_task {
        task_id : 1
      }
    }
  )",
      &executed_tasks, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kCancelTask, 0);
  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));

  EXPECT_THAT(executed_tasks, IsEmpty());
}

TEST(SequenceManagerFuzzerProcessorTest, CancelTaskWhenNoneArePending) {
  std::vector<ActionForTest> executed_actions;

  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
    initial_thread_actions {
      action_id : 1
      cancel_task {
        task_id : 1
      }
    }
  )",
      nullptr, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kCancelTask, 0);
  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest,
     TaskDurationBlocksOtherPendingTasksPostedFromOutsideOfTask) {
  std::vector<TaskForTest> executed_tasks;
  std::vector<ActionForTest> executed_actions;

  // Posts a task of duration 40 ms and a 10 ms delayed task of duration 20 ms.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
        initial_thread_actions {
          action_id : 1
          post_delayed_task {
            delay_ms : 10
            task {
              task_id : 1
              duration_ms : 20
            }
          }
        }
        initial_thread_actions {
          action_id :2
          post_delayed_task {
            delay_ms : 0
            task {
              task_id : 2
              duration_ms : 40
            }
          }
        })",
      &executed_tasks, &executed_actions);

  std::vector<TaskForTest> expected_tasks;

  // Task with id 2 is expected to run first and block the other task until it
  // done.
  expected_tasks.emplace_back(2, 0, 40);
  expected_tasks.emplace_back(1, 40, 60);
  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest,
     TaskDurationBlocksOtherNonNestableTaskWhenPostedFromTheWithinTask) {
  std::vector<TaskForTest> executed_tasks;

  // Posts an instant task of duration 40 ms that posts another non-nested
  // instant task.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
        initial_thread_actions {
          post_delayed_task {
            task {
              task_id : 1
              duration_ms : 40
              actions {
                post_delayed_task {
                  task {
                    task_id : 2
                  }
                }
              }
            }
          }
        })",
      &executed_tasks, nullptr);

  std::vector<TaskForTest> expected_tasks;

  // Task with task id 1 is expected to run for 40 ms, and block the other
  // posted task from running until its done. Note that the task with id 2 is
  // blocked since it is non-nested, so it is not supposed to run from within
  // the posting task.
  expected_tasks.emplace_back(1, 0, 40);
  expected_tasks.emplace_back(2, 40, 40);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest, PostNonEmptyTask) {
  std::vector<TaskForTest> executed_tasks;
  std::vector<ActionForTest> executed_actions;

  // Posts a 5 ms delayed task of duration 40 ms that creates a task queue,
  // posts a 4 ms delayed task, posts an instant task, creates a task queue,
  // and then posts a 40 ms delayed task.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
      initial_thread_actions {
        action_id : 1
        post_delayed_task {
          delay_ms: 5
          task {
            task_id : 1
            duration_ms : 40
            actions {
              action_id : 2
              create_task_queue {
              }
            }
            actions {
              action_id : 3
              post_delayed_task {
                delay_ms : 4
                task {
                  task_id : 2
                }
              }
            }
            actions {
              action_id: 4
              post_delayed_task {
                task {
                  task_id : 3
                }
              }
            }
            actions {
              action_id : 5
              create_task_queue {
              }
            }
            actions {
              action_id : 6
              post_delayed_task {
               delay_ms : 40
               task {
                 task_id : 4
               }
              }
            }
          }
        }
      })",
      &executed_tasks, &executed_actions);

  std::vector<TaskForTest> expected_tasks;

  // Task with task id 1 is expected to run first, and block all other pending
  // tasks until its done. The remaining tasks will be executed in
  // non-decreasing order of the delay parameter with ties broken by
  // the post order.
  expected_tasks.emplace_back(1, 5, 45);
  expected_tasks.emplace_back(3, 45, 45);
  expected_tasks.emplace_back(2, 45, 45);
  expected_tasks.emplace_back(4, 45, 45);
  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kCreateTaskQueue,
                                5);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kPostDelayedTask,
                                5);
  expected_actions.emplace_back(4, ActionForTest::ActionType::kPostDelayedTask,
                                5);
  expected_actions.emplace_back(5, ActionForTest::ActionType::kCreateTaskQueue,
                                5);
  expected_actions.emplace_back(6, ActionForTest::ActionType::kPostDelayedTask,
                                5);
  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest, OrderOfSimpleUnnestedExecutedActions) {
  std::vector<TaskForTest> executed_tasks;
  std::vector<ActionForTest> executed_actions;

  // Creates a task queue, posts a task after 20 ms delay, posts a 10 ms
  // duration task after 15 ms of delay, and posts a task after 100 ms of delay.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
      initial_thread_actions {
        action_id : 1
        create_task_queue {
        }
      }
      initial_thread_actions {
        action_id : 2
        post_delayed_task {
          delay_ms : 10
          task {
            task_id : 1
            actions {
              action_id : 3
              create_task_queue {
              }
            }
          }
        }
      }
      initial_thread_actions {
        action_id : 4
        post_delayed_task {
          delay_ms : 20
          task {
            task_id : 2
          }
        }
      }
      initial_thread_actions {
        action_id : 5
        post_delayed_task {
          delay_ms : 15
          task {
            task_id : 3
            duration_ms : 10
          }
        }
      }
      initial_thread_actions {
        action_id : 6
        post_delayed_task {
          delay_ms : 100
          task {
            task_id : 4
          }
        }
      })",
      &executed_tasks, &executed_actions);

  std::vector<TaskForTest> expected_tasks;

  // Tasks are expected to run in order of non-decreasing delay with ties broken
  // by order of posting. Note that the task with id 3 will block the task with
  // id 2 from running at its scheduled time.
  expected_tasks.emplace_back(1, 10, 10);
  expected_tasks.emplace_back(3, 15, 25);
  expected_tasks.emplace_back(2, 25, 25);
  expected_tasks.emplace_back(4, 100, 100);
  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kCreateTaskQueue,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(4, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(5, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(6, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kCreateTaskQueue,
                                10);
  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest, InsertAndRemoveFence) {
  std::vector<ActionForTest> executed_actions;
  std::vector<TaskForTest> executed_tasks;

  // Describes a test that inserts a fence to a task queue after a delay of
  // 20ms, posts a task to it after a delay of 25ms, and removes the fence after
  // a delay of 30ms.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
       initial_thread_actions {
         action_id : 1
           create_task_queue{
           }
         }
       initial_thread_actions {
         action_id : 2
         post_delayed_task {
           delay_ms : 20
           task_queue_id : 2
           task {
             task_id : 1
             actions {
               action_id : 3
               insert_fence {
                 position: NOW
                 task_queue_id: 1
               }
             }
           }
         }
       }
       initial_thread_actions {
         action_id : 4
         post_delayed_task {
           delay_ms : 30
           task_queue_id : 2
           task {
             task_id : 2
             actions {
               action_id : 5
               remove_fence {
                 task_queue_id: 1
               }
             }
           }
         }
      }
      initial_thread_actions {
        action_id: 6
        post_delayed_task {
          delay_ms: 25
          task_queue_id: 1
          task {
            task_id: 3
          }
        }
      })",
      &executed_tasks, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kCreateTaskQueue,
                                0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(4, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(6, ActionForTest::ActionType::kPostDelayedTask,
                                0);
  expected_actions.emplace_back(3, ActionForTest::ActionType::kInsertFence, 20);
  expected_actions.emplace_back(5, ActionForTest::ActionType::kRemoveFence, 30);

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));

  std::vector<TaskForTest> expected_tasks;
  expected_tasks.emplace_back(1, 20, 20);
  expected_tasks.emplace_back(2, 30, 30);

  // Task with id 3 will not execute until the fence is removed from the task
  // queue it was posted to.
  expected_tasks.emplace_back(3, 30, 30);

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));
}

TEST(SequenceManagerFuzzerProcessorTest, ThrottleTaskQueue) {
  std::vector<ActionForTest> executed_actions;
  std::vector<TaskForTest> executed_tasks;

  // Describes a test that throttles a task queue, and posts a task to it.
  SequenceManagerFuzzerProcessorForTest::ParseAndRunSingleThread(
      R"(
       initial_thread_actions {
        action_id : 1
        insert_fence {
          position: BEGINNING_OF_TIME
          task_queue_id: 1
        }
       }
       initial_thread_actions {
         action_id: 2
         post_delayed_task {
           task_queue_id: 1
           task {
             task_id: 3
           }
         }
       })",
      &executed_tasks, &executed_actions);

  std::vector<ActionForTest> expected_actions;
  expected_actions.emplace_back(1, ActionForTest::ActionType::kInsertFence, 0);
  expected_actions.emplace_back(2, ActionForTest::ActionType::kPostDelayedTask,
                                0);

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));

  // Task queue with id 1 is throttled, so posted tasks will not get executed.
  EXPECT_THAT(executed_tasks, IsEmpty());
}

TEST(SequenceManagerFuzzerProcessorTest, MultipleThreadsButNotInteracting) {
  std::string thread_actions =
      R"(
      main_thread_actions {
        action_id : 1
        create_thread {
          initial_thread_actions {
            action_id : 1
            create_task_queue {
            }
          }
          initial_thread_actions {
            action_id : 2
            post_delayed_task {
              delay_ms : 10
              task {
                task_id : 1
                actions {
                  action_id : 3
                  create_task_queue {
                  }
                }
              }
            }
          }
          initial_thread_actions {
            action_id : 4
            post_delayed_task {
              delay_ms : 20
              task {
                task_id : 2
              }
            }
          }
          initial_thread_actions {
            action_id : 5
            post_delayed_task {
              delay_ms : 15
              task {
                task_id : 3
                duration_ms : 10
              }
            }
          }
          initial_thread_actions {
            action_id : 6
            post_delayed_task {
              delay_ms : 100
              task {
                task_id : 4
              }
            }
          }
        }
      })";

  // Threads initialized with same list of actions.
  std::vector<std::string> threads{thread_actions, thread_actions,
                                   thread_actions, thread_actions,
                                   thread_actions};

  std::vector<std::vector<ActionForTest>> executed_actions;
  std::vector<std::vector<TaskForTest>> executed_tasks;

  SequenceManagerFuzzerProcessorForTest::ParseAndRun(
      base::StrCat(threads), &executed_tasks, &executed_actions);

  // |expected_tasks[0]| is empty since the main thread doesn't execute any
  // task.
  std::vector<std::vector<TaskForTest>> expected_tasks(6);

  for (int i = 1; i <= 5; i++) {
    // Created thread tasks: tasks are expected to run in order of
    // non-decreasing delay with ties broken by order of posting. Note that the
    // task with id 3 will block the task with id 2 from running at its
    // scheduled time.
    expected_tasks[i].emplace_back(1, 10, 10);
    expected_tasks[i].emplace_back(3, 15, 25);
    expected_tasks[i].emplace_back(2, 25, 25);
    expected_tasks[i].emplace_back(4, 100, 100);
  }

  EXPECT_THAT(executed_tasks, ContainerEq(expected_tasks));

  std::vector<std::vector<ActionForTest>> expected_actions(6);

  for (int i = 1; i <= 5; i++) {
    // Main thread action: creating the Ith thread.
    expected_actions[0].emplace_back(
        1, ActionForTest::ActionType::kCreateThread, 0);

    // Actions of the Ith thread.
    expected_actions[i].emplace_back(
        1, ActionForTest::ActionType::kCreateTaskQueue, 0);
    expected_actions[i].emplace_back(
        2, ActionForTest::ActionType::kPostDelayedTask, 0);
    expected_actions[i].emplace_back(
        4, ActionForTest::ActionType::kPostDelayedTask, 0);
    expected_actions[i].emplace_back(
        5, ActionForTest::ActionType::kPostDelayedTask, 0);
    expected_actions[i].emplace_back(
        6, ActionForTest::ActionType::kPostDelayedTask, 0);
    expected_actions[i].emplace_back(
        3, ActionForTest::ActionType::kCreateTaskQueue, 10);
  }

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest, CreateThreadRecursively) {
  std::vector<std::vector<ActionForTest>> executed_actions;

  SequenceManagerFuzzerProcessorForTest::ParseAndRun(
      R"(
      main_thread_actions {
        action_id : 1
        create_thread {
          initial_thread_actions {
            action_id : 2
            create_thread {
              initial_thread_actions {
                action_id : 3
                create_thread {}
              }
            }
          }
        }
      }
      )",
      nullptr, &executed_actions);

  // Last thread has no actions, so |expected_actions[3]| is empty.
  std::vector<std::vector<ActionForTest>> expected_actions(4);

  for (int i = 0; i <= 2; i++) {
    // Actions of the Ith thread.
    expected_actions[i].emplace_back(
        i + 1, ActionForTest::ActionType::kCreateThread, 0);
  }

  EXPECT_THAT(executed_actions, ContainerEq(expected_actions));
}

TEST(SequenceManagerFuzzerProcessorTest, PostTaskToCreateThread) {
  std::vector<std::vector<ActionForTest>> executed_actions;
  std::vector<std::vector<TaskForTest>> executed_tasks;

  SequenceManagerFuzzerProcessorForTest::ParseAndRun(
      R"(
      main_thread_actions {
        action_id : 1
        create_thread {
          initial_thread_actions {
            action_id : 2
            post_delayed_task {
              task {
                task_id: 1
                actions {
                  action_id : 3
                  create_thread {
                  }
                }
              }
            }
          }
          initial_thread_actions {
            action_id : 4
            create_thread {
            }
          }
        }
      }
      main_thread_actions {
        action_id : 5
        create_thread {
          initial_thread_actions {
            action_id : 6
            post_delayed_task {
              delay_ms : 20
              task {
                task_id: 2
                duration_ms : 30
                actions {
                  action_id : 7
                  create_thread {
                  }
                }
              }
            }
          }
        }
      })",
      &executed_tasks, &executed_actions);

  // Third, Fourth and Fifth created threads execute no actions.
  std::vector<std::vector<ActionForTest>> expected_actions(6);

  expected_actions[0].emplace_back(1, ActionForTest::ActionType::kCreateThread,
                                   0);
  expected_actions[0].emplace_back(5, ActionForTest::ActionType::kCreateThread,
                                   0);

  expected_actions[1].emplace_back(
      2, ActionForTest::ActionType::kPostDelayedTask, 0);

  // Posted messages execute after instant actions.
  expected_actions[1].emplace_back(4, ActionForTest::ActionType::kCreateThread,
                                   0);
  expected_actions[1].emplace_back(3, ActionForTest::ActionType::kCreateThread,
                                   0);

  expected_actions[2].emplace_back(
      6, ActionForTest::ActionType::kPostDelayedTask, 0);
  expected_actions[2].emplace_back(7, ActionForTest::ActionType::kCreateThread,
                                   20);

  // Order isn't deterministic, since threads only start running once all the
  // initial threads are created, and as a result the logging order isn't
  // deterministic,
  EXPECT_THAT(executed_actions, UnorderedElementsAreArray(expected_actions));
}

}  // namespace sequence_manager
}  // namespace base
