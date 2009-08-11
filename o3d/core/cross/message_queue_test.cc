/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// Tests the functionality defined in MessageQueue.cc/h

#include "core/cross/message_queue.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"
#include "core/cross/service_dependency.h"
#include "core/cross/texture.h"
#include "core/cross/types.h"
#include "tests/common/win/testing_common.h"
#include "base/condition_variable.h"
#include "base/lock.h"
#include "base/platform_thread.h"
#include "base/time.h"

using ::base::Time;
using ::base::TimeDelta;

namespace o3d {

namespace {
//----------------------------------------------------------------------
// These are helper classes for the little multithreaded test harness
// below.

class TimeSource {
 public:
  virtual ~TimeSource() {}
  virtual TimeDelta TimeSinceConstruction() = 0;
};

class WallClockTimeSource : public TimeSource {
 private:
  Time construction_time_;

 public:
  WallClockTimeSource() {
    construction_time_ = Time::Now();
  }

  virtual TimeDelta TimeSinceConstruction() {
    return Time::Now() - construction_time_;
  }
};

// The TestWatchdog expects to be signalled a certain number of times
// within a certain period of time. If it is not signalled this number
// of times, this indicates one failure mode of the test.
class TestWatchdog {
 private:
  Lock lock_;
  ConditionVariable condition_;
  int expected_num_signals_;
  TimeDelta time_to_run_;
  TimeSource* time_source_;

 public:
  TestWatchdog(int expected_num_signals,
               TimeDelta time_to_run,
               TimeSource* time_source)
      : lock_(),
        condition_(&lock_),
        expected_num_signals_(expected_num_signals),
        time_to_run_(time_to_run),
        time_source_(time_source) {}

  void Signal() {
    AutoLock locker(lock_);
    ASSERT_GE(expected_num_signals_, 0);
    --expected_num_signals_;
    condition_.Broadcast();
  }

  // Pause the current thread briefly waiting for a signal so we don't
  // consume all CPU
  void WaitBrieflyForSignal() {
    AutoLock locker(lock_);
    condition_.TimedWait(TimeDelta::FromMilliseconds(5));
  }

  bool Expired() {
    return time_source_->TimeSinceConstruction() > time_to_run_;
  }

  bool Succeeded() {
    return expected_num_signals_ == 0;
  }

  bool Done() {
    return Succeeded() || Expired();
  }
};

// This is the base class for the multithreaded tests which are
// executed via MessageQueueTest::RunTests(). Each instance is run in
// its own thread. Override the Run() method with the body of the
// test.
class PerThreadConnectedTest : public PlatformThread::Delegate {
 private:
  MessageQueue* queue_;
  nacl::Handle socket_handle_;
  TestWatchdog* watchdog_;
  volatile bool completed_;
  volatile bool passed_;
  String file_;
  int line_;
  String failure_message_;

 protected:
  void Pass() {
    completed_ = true;
    passed_ = true;
    watchdog_->Signal();
  }

  void Fail(String file,
            int line,
            String failure_message) {
    completed_ = true;
    passed_ = false;
    file_ = file;
    line_ = line;
    failure_message_ = failure_message;
    watchdog_->Signal();
  }

 public:
  PerThreadConnectedTest()
      : queue_(NULL),
        socket_handle_(nacl::kInvalidHandle),
        watchdog_(NULL),
        completed_(false),
        passed_(false),
        line_(0) {}

  // Override this with the particular test's functionality
  virtual void Run(MessageQueue* queue,
                   nacl::Handle socket_handle) = 0;

  void Configure(MessageQueue* queue,
                 nacl::Handle socket_handle,
                 TestWatchdog* watchdog) {
    queue_ = queue;
    watchdog_ = watchdog;
    socket_handle_ = socket_handle;
  }

  // Indicates whether or not the PerThreadTest should be deleted;
  // if it is hanging then to avoid crashes we do not delete it
  bool Completed() {
    return completed_;
  }

  bool Passed() {
    return passed_;
  }

  const String FailureMessage() {
    std::ostringstream oss;
    oss << file_ << ", line " << line_ << ": " + failure_message_;
    return oss.str();
  }

  // This overrides the functionality in PlatformThread::Delegate;
  // don't override this in subclasses.
  virtual void ThreadMain() {
    Run(queue_, socket_handle_);
  }
};

#define FAIL_TEST(message) Fail(__FILE__, __LINE__, message); return

class TestProvider {
 public:
  virtual ~TestProvider() {}
  virtual PerThreadConnectedTest* CreateTest() = 0;
};

//----------------------------------------------------------------------
// This is a helper class that handles connecting to the MessageQueue
// and issuing commands to it.
class TextureUpdateHelper {
 public:
  TextureUpdateHelper() : o3d_handle_(nacl::kInvalidHandle) {}

  // Connects to the MessageQueue.
  bool ConnectToO3D(const char* o3d_address,
                    nacl::Handle my_socket_handle);

  // Makes a request for a shared memory buffer.
  bool RequestSharedMemory(size_t requested_size,
                           int* shared_mem_id,
                           void** shared_mem_address);

  // Makes a request to update a texture.
  bool RequestTextureUpdate(unsigned int texture_id,
                            int level,
                            int shared_memory_id,
                            size_t offset,
                            size_t number_of_bytes);

  // Makes a request to update a portion of a texture.
  bool RequestTextureRectUpdate(unsigned int texture_id,
                                int level,
                                int x,
                                int y,
                                int width,
                                int height,
                                int shared_memory_id,
                                size_t offset,
                                size_t number_of_bytes);

  // Registers a client-allocated shared memory segment with O3D,
  // returning O3D's shared memory ID for later updating. Returns -1
  // upon failure.
  int RegisterSharedMemory(nacl::Handle shared_memory,
                           size_t shared_memory_size);

  // Unregisters a previously-registered client-allocated shared
  // memory segment.
  bool UnregisterSharedMemory(int shared_memory_id);

 private:
  // Handle of the socket that's connected to o3d.
  nacl::Handle o3d_handle_;

  bool ReceiveBooleanResponse();
};  // TextureUpdateHelper

// Waits for a message with a single integer value.  If the value is 0 it
// returns false otherwise returns true.
bool TextureUpdateHelper::ReceiveBooleanResponse() {
  int response = 0;
  nacl::IOVec vec;
  vec.base = &response;
  vec.length = sizeof(response);

  nacl::MessageHeader header;
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = 0;
  header.handle_count = 0;

  int result = nacl::ReceiveDatagram(o3d_handle_, &header, 0);

  EXPECT_EQ(sizeof(response), static_cast<unsigned>(result));
  if (result != sizeof(response)) {
    return false;
  }

  return (response ? true : false);
}


// Send the initial handshake message to O3D.
bool TextureUpdateHelper::ConnectToO3D(const char* o3d_address,
                                       nacl::Handle my_socket_handle) {
  nacl::Handle pair[2];

  if (nacl::SocketPair(pair) != 0) {
    return false;
  }

  MessageHello msg;
  nacl::MessageHeader header;
  nacl::IOVec vec;
  vec.base = &msg;
  vec.length = sizeof(msg);

  nacl::SocketAddress socket_address;
  ::base::snprintf(socket_address.path,
                   sizeof(socket_address.path),
                   "%s", o3d_address);

  header.iov = &vec;
  header.iov_length = 1;
  header.handles = &pair[1];
  header.handle_count = 1;
  int result = nacl::SendDatagramTo(my_socket_handle,
                                    &header,
                                    0,
                                    &socket_address);

  EXPECT_EQ(sizeof(msg), static_cast<size_t>(result));
  if (static_cast<size_t>(result) != sizeof(msg)) {
    return false;
  }

  // The socket handle we established the connection with o3d with.
  o3d_handle_ = pair[0];

  bool response = ReceiveBooleanResponse();

  EXPECT_TRUE(response);

  // We don't need to have that handle open anymore since the server has it now.
  result = nacl::Close(pair[1]);

  return response;
}

// Sends the server a request to allocate shared memory.  It received back
// from the server a shared memory handle which it then uses to map the shared
// memory into this process' address space.  The server also returns a unique
// id for the shared memory which can be used to identify the buffer in
// subsequent communcations with the server.
bool TextureUpdateHelper::RequestSharedMemory(size_t requested_size,
                                              int* shared_mem_id,
                                              void** shared_mem_address) {
  if (o3d_handle_ == nacl::kInvalidHandle) {
    return false;
  }

  MessageAllocateSharedMemory msg(requested_size);
  nacl::MessageHeader header;
  nacl::IOVec vec;

  vec.base = &msg;
  vec.length = sizeof(msg);

  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;

  // Send message.
  int result = nacl::SendDatagram(o3d_handle_, &header, 0);
  EXPECT_EQ(vec.length, static_cast<unsigned>(result));
  if (static_cast<unsigned>(result) != vec.length) {
    return false;
  }

  // Wait for a message back from the server containing the handle to the
  // shared memory object.
  nacl::Handle shared_memory;
  int shared_memory_id = -1;
  nacl::IOVec shared_memory_vec;
  shared_memory_vec.base = &shared_memory_id;
  shared_memory_vec.length = sizeof(shared_memory_id);
  header.iov = &shared_memory_vec;
  header.iov_length = 1;
  header.handles = &shared_memory;
  header.handle_count = 1;
  result = nacl::ReceiveDatagram(o3d_handle_, &header, 0);

  EXPECT_LT(0, result);
  EXPECT_EQ(0, header.flags & nacl::kMessageTruncated);
  EXPECT_EQ(1U, header.handle_count);
  EXPECT_EQ(1U, header.iov_length);

  if (result < 0 ||
      header.flags & nacl::kMessageTruncated ||
      header.handle_count != 1 ||
      header.iov_length != 1) {
    return false;
  }

  // Map the shared memory object to our address space.
  void* shared_region = nacl::Map(0,
                                  requested_size,
                                  nacl::kProtRead | nacl::kProtWrite,
                                  nacl::kMapShared,
                                  shared_memory,
                                  0);

  EXPECT_TRUE(shared_region != NULL);

  if (shared_region == NULL) {
    return false;
  }

  *shared_mem_address = shared_region;
  *shared_mem_id = shared_memory_id;

  return true;
}

// Sends a message to O3D to update the contents of the texture bitmap
// using the data stored in shared memory.  We pass in the shared memory handle
// returned by the server as well as an offset from the start of the shared
// memory buffer where the new texture data is.
bool TextureUpdateHelper::RequestTextureUpdate(unsigned int texture_id,
                                               int level,
                                               int shared_memory_id,
                                               size_t offset,
                                               size_t number_of_bytes) {
  MessageUpdateTexture2D msg(
      texture_id, level, shared_memory_id, offset, number_of_bytes);

  nacl::MessageHeader header;
  nacl::IOVec vec;

  vec.base = &msg;
  vec.length = sizeof(msg);

  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;

  // Send message.
  int result = nacl::SendDatagram(o3d_handle_, &header, 0);

  EXPECT_EQ(vec.length, static_cast<unsigned>(result));

  if (static_cast<unsigned>(result) != vec.length) {
    return false;
  }

  // Wait for a response from the server.  If the server returns true then
  // the texture update was successfully procesed.
  bool texture_updated = ReceiveBooleanResponse();

  EXPECT_TRUE(texture_updated);

  return texture_updated;
}

// Sends a message to O3D to update the a potion of the contents of a texture
// using the data stored in shared memory.
bool TextureUpdateHelper::RequestTextureRectUpdate(unsigned int texture_id,
                                                   int level,
                                                   int x,
                                                   int y,
                                                   int width,
                                                   int height,
                                                   int shared_memory_id,
                                                   size_t offset,
                                                   size_t number_of_bytes) {
  MessageUpdateTexture2DRect msg(
      texture_id, level, x, y, width, height,
      shared_memory_id, offset, number_of_bytes);

  nacl::MessageHeader header;
  nacl::IOVec vec;

  vec.base = &msg;
  vec.length = sizeof(msg);

  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;

  // Send message.
  int result = nacl::SendDatagram(o3d_handle_, &header, 0);

  EXPECT_EQ(vec.length, static_cast<unsigned>(result));

  if (static_cast<unsigned>(result) != vec.length) {
    return false;
  }

  // Wait for a response from the server.  If the server returns true then
  // the texture update was successfully procesed.
  bool texture_updated = ReceiveBooleanResponse();

  EXPECT_TRUE(texture_updated);

  return texture_updated;
}

// Registers a client-allocated shared memory segment with O3D. It
// receives back a shared memory ID for later texture updating.
// Returns -1 upon failure.
int TextureUpdateHelper::RegisterSharedMemory(nacl::Handle shared_memory,
                                              size_t shared_memory_size) {
  if (o3d_handle_ == nacl::kInvalidHandle) {
    return false;
  }

  MessageRegisterSharedMemory msg(static_cast<int32>(shared_memory_size));

  nacl::MessageHeader header;
  nacl::IOVec vec;

  vec.base = &msg;
  vec.length = sizeof(msg);

  header.iov = &vec;
  header.iov_length = 1;
  header.handles = &shared_memory;
  header.handle_count = 1;

  // Send message.
  int result = nacl::SendDatagram(o3d_handle_, &header, 0);
  EXPECT_EQ(vec.length, static_cast<unsigned>(result));
  if (static_cast<unsigned>(result) != vec.length) {
    return -1;
  }

  // Wait for a message back from the server containing the ID of the
  // shared memory object.
  nacl::MessageHeader reply_header;
  int shared_memory_id = -1;
  nacl::IOVec shared_memory_vec;
  shared_memory_vec.base = &shared_memory_id;
  shared_memory_vec.length = sizeof(shared_memory_id);
  reply_header.iov = &shared_memory_vec;
  reply_header.iov_length = 1;
  reply_header.handles = NULL;
  reply_header.handle_count = 0;

  result = nacl::ReceiveDatagram(o3d_handle_, &reply_header, 0);
  EXPECT_EQ(shared_memory_vec.length, static_cast<unsigned>(result));
  EXPECT_EQ(0, reply_header.flags & nacl::kMessageTruncated);
  EXPECT_EQ(0U, reply_header.handle_count);
  EXPECT_EQ(1U, reply_header.iov_length);

  return shared_memory_id;
}

// Unregisters a previously-registered client-allocated shared
// memory segment.
bool TextureUpdateHelper::UnregisterSharedMemory(int shared_memory_id) {
  MessageUnregisterSharedMemory msg(shared_memory_id);
  nacl::MessageHeader header;
  nacl::IOVec vec;

  vec.base = &msg;
  vec.length = sizeof(msg);
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;

  // Send message.
  int result = nacl::SendDatagram(o3d_handle_, &header, 0);
  EXPECT_EQ(static_cast<int>(vec.length), result);
  // Read back the boolean reply from the O3D plugin
  bool reply = ReceiveBooleanResponse();
  EXPECT_TRUE(reply);
  return reply;
}


}  // anonymous namespace.

//----------------------------------------------------------------------
// This is the main class containing all of the other ones. It knows
// how to run multiple concurrent PerThreadConnectedTests.
class MessageQueueTest : public testing::Test {
 protected:
  MessageQueueTest()
      : object_manager_(g_service_locator),
        socket_handles_(NULL),
        num_socket_handles_(0) {}

  virtual void SetUp();
  virtual void TearDown();

  // This is the entry point for test cases that need to be run in one
  // or more threads.
  void RunTests(int num_threads,
                TimeDelta timeout,
                TestProvider* test_provider);

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
  nacl::Handle* socket_handles_;
  int num_socket_handles_;

  // This can't be part of SetUp since it needs to be called from each
  // individual test.
  void ConfigureSockets(int number_of_clients);

  nacl::Handle GetSocketHandle(int i) {
    // We would use ASSERT_ here, but that doesn't seem to work from
    // within methods that have non-void return types.
    EXPECT_TRUE(socket_handles_ != NULL);
    EXPECT_LT(i, num_socket_handles_);
    return socket_handles_[i];
  }
};

void MessageQueueTest::SetUp() {
  pack_ = object_manager_->CreatePack();
  pack_->set_name("MessageQueueTest pack");
}

void MessageQueueTest::ConfigureSockets(int number_of_clients) {
  ASSERT_GT(number_of_clients, 0);
  num_socket_handles_ = number_of_clients;
  socket_handles_ = new nacl::Handle[num_socket_handles_];
  ASSERT_TRUE(socket_handles_ != NULL);
  for (int i = 0; i < num_socket_handles_; i++) {
    nacl::SocketAddress socket_address;
    ::base::snprintf(socket_address.path,
                     sizeof(socket_address.path),
                     "%s%d",
                     "test-client",
                     i);
    socket_handles_[i] = nacl::BoundSocket(&socket_address);
    ASSERT_NE(nacl::kInvalidHandle, socket_handles_[i]);
  }
}

void MessageQueueTest::TearDown() {
  if (socket_handles_ != NULL) {
    for (int i = 0; i < num_socket_handles_; i++) {
      nacl::Close(socket_handles_[i]);
    }
    delete[] socket_handles_;
    socket_handles_ = NULL;
    num_socket_handles_ = 0;
  }

  object_manager_->DestroyPack(pack_);
}

void MessageQueueTest::RunTests(int num_threads,
                                TimeDelta timeout,
                                TestProvider* provider) {
  MessageQueue* message_queue = new MessageQueue(g_service_locator);
  message_queue->Initialize();

  TimeSource* time_source = new WallClockTimeSource();
  TestWatchdog* watchdog = new TestWatchdog(num_threads,
                                            timeout,
                                            time_source);
  PerThreadConnectedTest** tests = new PerThreadConnectedTest*[num_threads];
  ASSERT_TRUE(tests != NULL);
  PlatformThreadHandle* thread_handles = new PlatformThreadHandle[num_threads];
  ASSERT_TRUE(thread_handles != NULL);
  ConfigureSockets(num_threads);
  for (int i = 0; i < num_threads; i++) {
    tests[i] = provider->CreateTest();
    ASSERT_TRUE(tests[i] != NULL);
    tests[i]->Configure(message_queue, GetSocketHandle(i), watchdog);
  }
  // Now that all tests are created, start them up
  for (int i = 0; i < num_threads; i++) {
    ASSERT_TRUE(PlatformThread::Create(0, tests[i], &thread_handles[i]));
  }
  // Wait for completion
  while (!watchdog->Done()) {
    ASSERT_TRUE(message_queue->CheckForNewMessages());
    watchdog->WaitBrieflyForSignal();
  }
  ASSERT_FALSE(watchdog->Expired());
  ASSERT_TRUE(watchdog->Succeeded());
  for (int i = 0; i < num_threads; i++) {
    PerThreadConnectedTest* test = tests[i];
    ASSERT_TRUE(test->Passed()) << test->FailureMessage();
    // Only join the thread and delete the test if it completed
    if (test->Completed()) {
      PlatformThread::Join(thread_handles[i]);
      delete test;
    }
  }
  delete[] thread_handles;
  delete[] tests;
  delete watchdog;
  delete time_source;
  delete message_queue;
}

//----------------------------------------------------------------------
// Test cases follow.

// Tests that the message queue socket is properly initialized.
TEST_F(MessageQueueTest, Initialize) {
  MessageQueue* message_queue = new MessageQueue(g_service_locator);

  EXPECT_TRUE(message_queue->Initialize());

  String socket_addr = message_queue->GetSocketAddress();

  // Make sure the name starts with the expected value
  EXPECT_EQ(0U, socket_addr.find("o3d"));

  delete message_queue;
}

// Tests that the a client can actually establish a connection to the
// MessageQueue.
TEST_F(MessageQueueTest, TestConnection) {
  class ConnectionTest : public PerThreadConnectedTest {
   public:
    void Run(MessageQueue* queue,
             nacl::Handle socket_handle) {
      String socket_addr = queue->GetSocketAddress();
      TextureUpdateHelper helper;
      if (helper.ConnectToO3D(socket_addr.c_str(),
                              socket_handle)) {
        Pass();
      } else {
        FAIL_TEST("Failed to connect to O3D");
      }
    }
  };

  class Provider : public TestProvider {
   public:
    virtual PerThreadConnectedTest* CreateTest() {
      return new ConnectionTest();
    }
  };

  Provider provider;
  RunTests(1, TimeDelta::FromSeconds(1), &provider);
}

// Tests a request for shared memory.
TEST_F(MessageQueueTest, GetSharedMemory) {
  class SharedMemoryTest : public PerThreadConnectedTest {
   public:
    void Run(MessageQueue* queue,
             nacl::Handle socket_handle) {
      String socket_addr = queue->GetSocketAddress();
      TextureUpdateHelper helper;
      if (!helper.ConnectToO3D(socket_addr.c_str(),
                               socket_handle)) {
        FAIL_TEST("Failed to connect to O3D");
      }

      void *shared_mem_address = NULL;
      int shared_mem_id = -1;
      bool memory_ok = helper.RequestSharedMemory(65536,
                                                  &shared_mem_id,
                                                  &shared_mem_address);
      if (shared_mem_id == -1) {
        FAIL_TEST("Shared memory id was -1");
      }

      if (shared_mem_address == NULL) {
        FAIL_TEST("Shared memory address was NULL");
      }

      if (!memory_ok) {
        FAIL_TEST("Memory request failed");
      }

      Pass();
    }
  };

  class Provider : public TestProvider {
   public:
    virtual PerThreadConnectedTest* CreateTest() {
      return new SharedMemoryTest();
    }
  };

  Provider provider;
  RunTests(1, TimeDelta::FromSeconds(1), &provider);
}

// Tests a request to update a texture.
TEST_F(MessageQueueTest, UpdateTexture2D) {
  class UpdateTexture2DTest : public PerThreadConnectedTest {
   private:
    int texture_id_;

   public:
    explicit UpdateTexture2DTest(int texture_id) {
      texture_id_ = texture_id;
    }

    void Run(MessageQueue* queue,
             nacl::Handle socket_handle) {
      String socket_addr = queue->GetSocketAddress();
      TextureUpdateHelper helper;
      if (!helper.ConnectToO3D(socket_addr.c_str(),
                               socket_handle)) {
        FAIL_TEST("Failed to connect to O3D");
      }

      void *shared_mem_address = NULL;
      int shared_mem_id = -1;
      bool memory_ok = helper.RequestSharedMemory(65536,
                                                  &shared_mem_id,
                                                  &shared_mem_address);
      if (shared_mem_id == -1) {
        FAIL_TEST("Shared memory id was -1");
      }

      if (shared_mem_address == NULL) {
        FAIL_TEST("Shared memory address was NULL");
      }

      if (!memory_ok) {
        FAIL_TEST("Memory request failed");
      }

      int texture_buffer_size = 128 * 128 * 4;

      if (!helper.RequestTextureUpdate(texture_id_,
                                       0,
                                       shared_mem_id,
                                       0,
                                       texture_buffer_size)) {
        FAIL_TEST("RequestTextureUpdate failed");
      }

      Pass();
    }
  };

  class Provider : public TestProvider {
   private:
    int texture_id_;

   public:
    explicit Provider(int texture_id) : texture_id_(texture_id) {}

    virtual PerThreadConnectedTest* CreateTest() {
      return new UpdateTexture2DTest(texture_id_);
    }
  };

  Texture2D* texture = pack()->CreateTexture2D(128,
                                               128,
                                               Texture::ARGB8,
                                               0,
                                               false);

  ASSERT_TRUE(texture != NULL);

  Provider provider(texture->id());
  RunTests(1, TimeDelta::FromSeconds(1), &provider);
}

// Tests a request to update a texture.
TEST_F(MessageQueueTest, UpdateTexture2DRect) {
  class UpdateTexture2DRectTest : public PerThreadConnectedTest {
   private:
    int texture_id_;

   public:
    explicit UpdateTexture2DRectTest(int texture_id) {
      texture_id_ = texture_id;
    }

    void Run(MessageQueue* queue,
             nacl::Handle socket_handle) {
      String socket_addr = queue->GetSocketAddress();
      TextureUpdateHelper helper;
      if (!helper.ConnectToO3D(socket_addr.c_str(),
                               socket_handle)) {
        FAIL_TEST("Failed to connect to O3D");
      }

      void *shared_mem_address = NULL;
      int shared_mem_id = -1;
      bool memory_ok = helper.RequestSharedMemory(65536,
                                                  &shared_mem_id,
                                                  &shared_mem_address);
      if (shared_mem_id == -1) {
        FAIL_TEST("Shared memory id was -1");
      }

      if (shared_mem_address == NULL) {
        FAIL_TEST("Shared memory address was NULL");
      }

      if (!memory_ok) {
        FAIL_TEST("Memory request failed");
      }

      const int kLevel = 0;
      const int kX = 10;
      const int kY = 11;
      const int kWidth = 12;
      const int kHeight = 13;
      const int kTextureRectSize = kWidth * kHeight * 4;

      if (!helper.RequestTextureRectUpdate(texture_id_,
                                           kLevel,
                                           kX,
                                           kY,
                                           kWidth,
                                           kHeight,
                                           shared_mem_id,
                                           0,
                                           kTextureRectSize)) {
        FAIL_TEST("RequestTextureRectUpdate failed");
      }

      Pass();
    }
  };

  class Provider : public TestProvider {
   private:
    int texture_id_;

   public:
    explicit Provider(int texture_id) : texture_id_(texture_id) {}

    virtual PerThreadConnectedTest* CreateTest() {
      return new UpdateTexture2DRectTest(texture_id_);
    }
  };

  Texture2D* texture = pack()->CreateTexture2D(128,
                                               128,
                                               Texture::ARGB8,
                                               0,
                                               false);

  ASSERT_TRUE(texture != NULL);

  Provider provider(texture->id());
  RunTests(1, TimeDelta::FromSeconds(1), &provider);
}

namespace {

// This helper class is used for both single-threaded and concurrent
// shared memory registration / unregistration tests.
class SharedMemoryRegisterUnregisterTest : public PerThreadConnectedTest {
 private:
  int num_iterations_;

 public:
  explicit SharedMemoryRegisterUnregisterTest(int num_iterations) {
    num_iterations_ = num_iterations;
  }

  void Run(MessageQueue* queue,
           nacl::Handle socket_handle) {
    String socket_addr = queue->GetSocketAddress();
    TextureUpdateHelper helper;
    if (!helper.ConnectToO3D(socket_addr.c_str(),
                             socket_handle)) {
      FAIL_TEST("Failed to connect to O3D");
    }

    // Allocate a shared memory segment
    size_t mem_size = nacl::kMapPageSize;
    nacl::Handle shared_memory = nacl::CreateMemoryObject(mem_size);
    if (shared_memory == nacl::kInvalidHandle) {
      FAIL_TEST("Failed to allocate shared memory object");
    }

    // Note that we don't actually have to map it in our process in
    // order to test the failure mode (corrupted messages) this test
    // exercises

    for (int i = 0; i < num_iterations_; i++) {
      int shared_mem_id =
        helper.RegisterSharedMemory(shared_memory, mem_size);
      if (shared_mem_id < 0) {
        FAIL_TEST("Failed to register shared memory with server");
      }
      bool result = helper.UnregisterSharedMemory(shared_mem_id);
      if (!result) {
        FAIL_TEST("Failed to unregister shared memory from server");
      }
    }

    nacl::Close(shared_memory);

    Pass();
  }
};

}  // anonymous namespace.

// Tests that a simple shared memory registration and unregistration
// pair appear to work.
TEST_F(MessageQueueTest, RegisterAndUnregisterSharedMemory) {
  class Provider : public TestProvider {
   public:
    virtual PerThreadConnectedTest* CreateTest() {
      return new SharedMemoryRegisterUnregisterTest(1);
    }
  };

  Provider provider;
  RunTests(1, TimeDelta::FromSeconds(1), &provider);
}

// Tests that multiple concurrent clients of the MessageQueue don't
// break its deserialization operations.
TEST_F(MessageQueueTest, ConcurrentSharedMemoryOperations) {
  class Provider : public TestProvider {
   public:
    virtual PerThreadConnectedTest* CreateTest() {
      return new SharedMemoryRegisterUnregisterTest(100);
    }
  };

  Provider provider;
  RunTests(2, TimeDelta::FromSeconds(6), &provider);
}

}  // namespace o3d
