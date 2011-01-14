// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/message_decoder.h"
#include "remoting/protocol/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

static const int kTestKey = 142;

static void AppendMessage(const EventMessage& msg,
                          std::string* buffer) {
  // Contains one encoded message.
  scoped_refptr<net::IOBufferWithSize> encoded_msg;
  encoded_msg = SerializeAndFrameMessage(msg);
  buffer->append(encoded_msg->data(), encoded_msg->size());
}

// Construct and prepare data in the |output_stream|.
static void PrepareData(uint8** buffer, int* size) {
  // Contains all encoded messages.
  std::string encoded_data;

  EventMessage msg;

  // Then append 10 update sequences to the data.
  for (int i = 0; i < 10; ++i) {
    Event* event = msg.add_event();
    event->set_timestamp(i);
    event->mutable_key()->set_keycode(kTestKey + i);
    event->mutable_key()->set_pressed((i % 2) != 0);
    AppendMessage(msg, &encoded_data);
    msg.Clear();
  }

  *size = encoded_data.length();
  *buffer = new uint8[*size];
  memcpy(*buffer, encoded_data.c_str(), *size);
}

void SimulateReadSequence(const int read_sequence[], int sequence_size) {
  // Prepare encoded data for testing.
  int size;
  uint8* test_data;
  PrepareData(&test_data, &size);
  scoped_array<uint8> memory_deleter(test_data);

  // Then simulate using MessageDecoder to decode variable
  // size of encoded data.
  // The first thing to do is to generate a variable size of data. This is done
  // by iterating the following array for read sizes.
  MessageDecoder decoder;

  // Then feed the protocol decoder using the above generated data and the
  // read pattern.
  std::list<EventMessage*> message_list;
  for (int i = 0; i < size;) {
    // First generate the amount to feed the decoder.
    int read = std::min(size - i, read_sequence[i % sequence_size]);

    // And then prepare an IOBuffer for feeding it.
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(read));
    memcpy(buffer->data(), test_data + i, read);
    decoder.AddData(buffer, read);
    while (true) {
      CompoundBuffer message;
      if (!decoder.GetNextMessage(&message))
        break;

      EventMessage* event = new EventMessage();
      CompoundBufferInputStream stream(&message);
      ASSERT_TRUE(event->ParseFromZeroCopyStream(&stream));
      message_list.push_back(event);
    }
    i += read;
  }

  // Then verify the decoded messages.
  EXPECT_EQ(10u, message_list.size());

  int index = 0;
  for (std::list<EventMessage*>::iterator it =
           message_list.begin();
       it != message_list.end(); ++it) {
    EventMessage* message = *it;
    // Partial update stream.
    EXPECT_EQ(message->event_size(), 1);
    EXPECT_TRUE(message->event(0).has_key());

    // TODO(sergeyu): Don't use index here. Instead store the expected values
    // in an array.
    EXPECT_EQ(kTestKey + index, message->event(0).key().keycode());
    EXPECT_EQ((index % 2) != 0, message->event(0).key().pressed());
    ++index;
  }
  STLDeleteElements(&message_list);
}

TEST(MessageDecoderTest, SmallReads) {
  const int kReads[] = {1, 2, 3, 1};
  SimulateReadSequence(kReads, arraysize(kReads));
}

TEST(MessageDecoderTest, LargeReads) {
  const int kReads[] = {50, 50, 5};
  SimulateReadSequence(kReads, arraysize(kReads));
}

TEST(MessageDecoderTest, EmptyReads) {
  const int kReads[] = {4, 0, 50, 0};
  SimulateReadSequence(kReads, arraysize(kReads));
}

}  // namespace protocol
}  // namespace remoting
