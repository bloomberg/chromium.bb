// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/scoped_ptr.h"
#include "media/base/data_buffer.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/base/protocol_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

static const int kWidth = 640;
static const int kHeight = 480;
static const std::string kTestData = "Chromoting rockz";

static void AppendMessage(const HostMessage& msg,
                          std::string* buffer) {
  // Contains one encoded message.
  scoped_refptr<media::DataBuffer> encoded_msg;
  encoded_msg = SerializeAndFrameMessage(msg);
  buffer->append(reinterpret_cast<const char*>(encoded_msg->GetData()),
                 encoded_msg->GetDataSize());
}

// Construct and prepare data in the |output_stream|.
static void PrepareData(uint8** buffer, int* size) {
  // Contains all encoded messages.
  std::string encoded_data;

  // The first message is InitClient.
  HostMessage msg;
  msg.mutable_init_client()->set_width(kWidth);
  msg.mutable_init_client()->set_height(kHeight);
  AppendMessage(msg, &encoded_data);
  msg.Clear();

  // Then append 10 update sequences to the data.
  for (int i = 0; i < 10; ++i) {
    msg.mutable_begin_update_stream();
    AppendMessage(msg, &encoded_data);
    msg.Clear();

    msg.mutable_update_stream_packet()->mutable_rect_data()->
        set_sequence_number(0);
    msg.mutable_update_stream_packet()->mutable_rect_data()->
        set_data(kTestData);
    AppendMessage(msg, &encoded_data);
    msg.Clear();

    msg.mutable_end_update_stream();
    AppendMessage(msg, &encoded_data);
    msg.Clear();
  }

  *size = encoded_data.length();
  *buffer = new uint8[*size];
  memcpy(*buffer, encoded_data.c_str(), *size);
}

TEST(ProtocolDecoderTest, BasicOperations) {
  // Prepare encoded data for testing.
  int size;
  uint8* test_data;
  PrepareData(&test_data, &size);
  scoped_array<uint8> memory_deleter(test_data);

  // Then simulate using ProtocolDecoder to decode variable
  // size of encoded data.
  // The first thing to do is to generate a variable size of data. This is done
  // by iterating the following array for read sizes.
  const int kReadSizes[] = {1, 2, 3, 1};

  ProtocolDecoder decoder;

  // Then feed the protocol decoder using the above generated data and the
  // read pattern.
  HostMessageList message_list;
  for (int i = 0; i < size;) {
    // First generate the amount to feed the decoder.
    int read = std::min(size - i, kReadSizes[i % arraysize(kReadSizes)]);

    // And then prepare a DataBuffer for feeding it.
    scoped_refptr<media::DataBuffer> buffer = new media::DataBuffer(read);
    memcpy(buffer->GetWritableData(), test_data + i, read);
    buffer->SetDataSize(read);
    decoder.ParseHostMessages(buffer, &message_list);
    i += read;
  }

  // Then verify the decoded messages.
  EXPECT_EQ(31u, message_list.size());
  ASSERT_TRUE(message_list.size() > 0);
  EXPECT_TRUE(message_list[0]->has_init_client());
  delete message_list[0];

  for (size_t i = 1; i < message_list.size(); ++i) {
    int type = (i - 1) % 3;
    if (type == 0) {
      // Begin update stream.
      EXPECT_TRUE(message_list[i]->has_begin_update_stream());
    } else if (type == 1) {
      // Partial update stream.
      EXPECT_TRUE(message_list[i]->has_update_stream_packet());
      EXPECT_EQ(kTestData,
                message_list[i]->update_stream_packet().rect_data().data());
    } else if (type == 2) {
      // End update stream.
      EXPECT_TRUE(message_list[i]->has_end_update_stream());
    }
    delete message_list[i];
  }
}

}  // namespace remoting
