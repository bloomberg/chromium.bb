// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_decoder.h"
#include "remoting/protocol/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

static const unsigned int kTestKey = 142;

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

  // Then append 10 update sequences to the data.
  for (int i = 0; i < 10; ++i) {
    EventMessage msg;
    msg.set_sequence_number(i);
    msg.mutable_key_event()->set_usb_keycode(kTestKey + i);
    msg.mutable_key_event()->set_pressed((i % 2) != 0);
    AppendMessage(msg, &encoded_data);
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
  for (int pos = 0; pos < size;) {
    SCOPED_TRACE("Input position: " + base::IntToString(pos));

    // First generate the amount to feed the decoder.
    int read = std::min(size - pos, read_sequence[pos % sequence_size]);

    // And then prepare an IOBuffer for feeding it.
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(read));
    memcpy(buffer->data(), test_data + pos, read);
    decoder.AddData(buffer, read);
    while (true) {
      scoped_ptr<CompoundBuffer> message(decoder.GetNextMessage());
      if (!message.get())
        break;

      EventMessage* event = new EventMessage();
      CompoundBufferInputStream stream(message.get());
      ASSERT_TRUE(event->ParseFromZeroCopyStream(&stream));
      message_list.push_back(event);
    }
    pos += read;
  }

  // Then verify the decoded messages.
  EXPECT_EQ(10u, message_list.size());

  unsigned int index = 0;
  for (std::list<EventMessage*>::iterator it =
           message_list.begin();
       it != message_list.end(); ++it) {
    SCOPED_TRACE("Message " + base::IntToString(index));

    EventMessage* message = *it;
    // Partial update stream.
    EXPECT_TRUE(message->has_key_event());

    // TODO(sergeyu): Don't use index here. Instead store the expected values
    // in an array.
    EXPECT_EQ(kTestKey + index, message->key_event().usb_keycode());
    EXPECT_EQ((index % 2) != 0, message->key_event().pressed());
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
