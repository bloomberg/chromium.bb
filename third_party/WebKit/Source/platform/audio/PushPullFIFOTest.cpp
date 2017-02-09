// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/audio/PushPullFIFO.h"

#include <memory>
#include <vector>
#include "platform/audio/AudioUtilities.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

// Check the basic contract of FIFO.
TEST(PushPullFIFOBasicTest, BasicTests) {
  // This suppresses the multi-thread warning for GTest. Potently it increases
  // the test execution time, but this specific test is very short and simple.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  // FIFO length exceeding the maximum length allowed will cause crash.
  // i.e.) m_fifoLength <= kMaxFIFOLength
  EXPECT_DEATH(new PushPullFIFO(2, PushPullFIFO::kMaxFIFOLength + 1), "");

  std::unique_ptr<PushPullFIFO> testFifo =
      WTF::wrapUnique(new PushPullFIFO(2, 1024));

  // The input bus length must be |AudioUtilities::kRenderQuantumFrames|.
  // i.e.) inputBus->length() == kRenderQuantumFrames
  RefPtr<AudioBus> inputBusOf129Frames =
      AudioBus::create(2, AudioUtilities::kRenderQuantumFrames + 1);
  EXPECT_DEATH(testFifo->push(inputBusOf129Frames.get()), "");
  RefPtr<AudioBus> inputBusOf127Frames =
      AudioBus::create(2, AudioUtilities::kRenderQuantumFrames - 1);
  EXPECT_DEATH(testFifo->push(inputBusOf127Frames.get()), "");

  // Pull request frames cannot exceed the length of output bus.
  // i.e.) framesRequested <= outputBus->length()
  RefPtr<AudioBus> outputBusOf512Frames = AudioBus::create(2, 512);
  EXPECT_DEATH(testFifo->pull(outputBusOf512Frames.get(), 513), "");

  // Pull request frames cannot exceed the length of FIFO.
  // i.e.) framesRequested <= m_fifoLength
  RefPtr<AudioBus> outputBusOf1025Frames = AudioBus::create(2, 1025);
  EXPECT_DEATH(testFifo->pull(outputBusOf1025Frames.get(), 1025), "");
}

// Fills each AudioChannel in an AudioBus with a series of linearly increasing
// values starting from |startingValue| and incrementing by 1. Then return value
// will be |startingValue| + |bus_length|.
size_t fillBusWithLinearRamp(AudioBus* targetBus, size_t startingValue) {
  for (unsigned c = 0; c < targetBus->numberOfChannels(); ++c) {
    float* busChannel = targetBus->channel(c)->mutableData();
    for (size_t i = 0; i < targetBus->channel(c)->length(); ++i) {
      busChannel[i] = static_cast<float>(startingValue + i);
    }
  }
  return startingValue + targetBus->length();
}

// Inspect the content of AudioBus with a given set of index and value across
// channels.
bool verifyBusValueAtIndex(AudioBus* targetBus,
                           int index,
                           float expectedValue) {
  for (unsigned c = 0; c < targetBus->numberOfChannels(); ++c) {
    float* busChannel = targetBus->channel(c)->mutableData();
    if (busChannel[index] != expectedValue) {
      LOG(ERROR) << ">> [FAIL] expected " << expectedValue << " at index "
                 << index << " but got " << busChannel[index] << ".";
      return false;
    }
  }
  return true;
}

struct FIFOAction {
  // The type of action; "PUSH" or "PULL".
  const char* action;
  // Number of frames for the operation.
  const size_t numberOfFrames;
};

struct AudioBusSample {
  // The frame index of a sample in the bus.
  const size_t index;
  // The value at the |index| above.
  const float value;
};

struct FIFOTestSetup {
  // Length of FIFO to be created for test case.
  const size_t fifoLength;
  // Channel count of FIFO to be created for test case.
  const unsigned numberOfChannels;
  // A list of |FIFOAction| entries to be performed in test case.
  const std::vector<FIFOAction> fifoActions;
};

struct FIFOTestExpectedState {
  // Expected read index in FIFO.
  const size_t indexRead;
  // Expected write index in FIFO.
  const size_t indexWrite;
  // Expected overflow count in FIFO.
  const unsigned overflowCount;
  // Expected underflow count in FIFO.
  const unsigned underflowCount;
  // A list of expected |AudioBusSample| entries for the FIFO bus.
  const std::vector<AudioBusSample> fifoSamples;
  // A list of expected |AudioBusSample| entries for the output bus.
  const std::vector<AudioBusSample> outputSamples;
};

// The data structure for the parameterized test cases.
struct FIFOTestParam {
  FIFOTestSetup setup;
  FIFOTestExpectedState expectedState;
};

std::ostream& operator<<(std::ostream& out, const FIFOTestParam& param) {
  out << "fifoLength=" << param.setup.fifoLength
      << " numberOfChannels=" << param.setup.numberOfChannels;
  return out;
}

class PushPullFIFOFeatureTest : public ::testing::TestWithParam<FIFOTestParam> {
};

TEST_P(PushPullFIFOFeatureTest, FeatureTests) {
  const FIFOTestSetup setup = GetParam().setup;
  const FIFOTestExpectedState expectedState = GetParam().expectedState;

  // Create a FIFO with a specified configuration.
  std::unique_ptr<PushPullFIFO> fifo = WTF::wrapUnique(
      new PushPullFIFO(setup.numberOfChannels, setup.fifoLength));

  RefPtr<AudioBus> outputBus;

  // Iterate all the scheduled push/pull actions.
  size_t frameCounter = 0;
  for (const auto& action : setup.fifoActions) {
    if (strcmp(action.action, "PUSH") == 0) {
      RefPtr<AudioBus> inputBus =
          AudioBus::create(setup.numberOfChannels, action.numberOfFrames);
      frameCounter = fillBusWithLinearRamp(inputBus.get(), frameCounter);
      fifo->push(inputBus.get());
      LOG(INFO) << "PUSH " << action.numberOfFrames
                << " frames (frameCounter=" << frameCounter << ")";
    } else {
      outputBus =
          AudioBus::create(setup.numberOfChannels, action.numberOfFrames);
      fifo->pull(outputBus.get(), action.numberOfFrames);
      LOG(INFO) << "PULL " << action.numberOfFrames << " frames";
    }
  }

  // Get FIFO config data.
  const PushPullFIFOStateForTest actualState = fifo->getStateForTest();

  // Verify the read/write indexes.
  EXPECT_EQ(expectedState.indexRead, actualState.indexRead);
  EXPECT_EQ(expectedState.indexWrite, actualState.indexWrite);
  EXPECT_EQ(expectedState.overflowCount, actualState.overflowCount);
  EXPECT_EQ(expectedState.underflowCount, actualState.underflowCount);

  // Verify in-FIFO samples.
  for (const auto& sample : expectedState.fifoSamples) {
    EXPECT_TRUE(verifyBusValueAtIndex(fifo->bus(), sample.index, sample.value));
  }

  // Verify samples from the most recent output bus.
  for (const auto& sample : expectedState.outputSamples) {
    EXPECT_TRUE(
        verifyBusValueAtIndex(outputBus.get(), sample.index, sample.value));
  }
}

FIFOTestParam featureTestParams[] = {
    // Test cases 0 ~ 3: Regular operation on various channel configuration.
    //  - Mono, Stereo, Quad, 5.1.
    //  - FIFO length and pull size are RQ-aligned.
    {{512, 1, {{"PUSH", 128}, {"PUSH", 128}, {"PULL", 256}}},
     {256, 256, 0, 0, {{0, 0}}, {{0, 0}, {255, 255}}}},

    {{512, 2, {{"PUSH", 128}, {"PUSH", 128}, {"PULL", 256}}},
     {256, 256, 0, 0, {{0, 0}}, {{0, 0}, {255, 255}}}},

    {{512, 4, {{"PUSH", 128}, {"PUSH", 128}, {"PULL", 256}}},
     {256, 256, 0, 0, {{0, 0}}, {{0, 0}, {255, 255}}}},

    {{512, 6, {{"PUSH", 128}, {"PUSH", 128}, {"PULL", 256}}},
     {256, 256, 0, 0, {{0, 0}}, {{0, 0}, {255, 255}}}},

    // Test case 4: Pull size less than or equal to 128.
    {{128, 2, {{"PUSH", 128}, {"PULL", 128}, {"PUSH", 128}, {"PULL", 64}}},
     {64, 0, 0, 0, {{64, 192}, {0, 128}}, {{0, 128}, {63, 191}}}},

    // Test case 5: Unusual FIFO and Pull length.
    //  - FIFO and pull length that are not aligned to render quantum.
    //  - Check if the indexes are wrapping around correctly.
    //  - Check if the output bus starts and ends with correct values.
    {{997,
      1,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 449},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 449},
      }},
     // - expectedIndexRead = 898, expectedIndexWrite = 27
     // - overflowCount = 0, underflowCount = 0
     // - FIFO samples (index, expectedValue) = (898, 898), (27, 27)
     // - Output bus samples (index, expectedValue) = (0, 499), (448, 897)
     {898, 27, 0, 0, {{898, 898}, {27, 27}}, {{0, 449}, {448, 897}}}},

    // Test case 6: Overflow
    //  - Check overflow counter.
    //  - After the overflow occurs, the read index must be moved to the write
    //    index. Thus pulled frames must not contain overwritten data.
    {{512,
      3,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 256},
      }},
     // - expectedIndexRead = 384, expectedIndexWrite = 128
     // - overflowCount = 1, underflowCount = 0
     // - FIFO samples (index, expectedValue) = (384, 384), (128, 128)
     // - Output bus samples (index, expectedValue) = (0, 128), (255, 383)
     {384, 128, 1, 0, {{384, 384}, {128, 128}}, {{0, 128}, {255, 383}}}},

    // Test case 7: Overflow in unusual FIFO and pull length.
    //  - Check overflow counter.
    //  - After the overflow occurs, the read index must be moved to the write
    //    index. Thus pulled frames must not contain overwritten data.
    {{577,
      5,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 227},
      }},
     // - expectedIndexRead = 290, expectedIndexWrite = 63
     // - overflowCount = 1, underflowCount = 0
     // - FIFO samples (index, expectedValue) = (63, 63), (290, 290)
     // - Output bus samples (index, expectedValue) = (0, 63), (226, 289)
     {290, 63, 1, 0, {{63, 63}, {290, 290}}, {{0, 63}, {226, 289}}}},

    // Test case 8: Underflow
    //  - Check underflow counter.
    //  - After the underflow occurs, the write index must be moved to the read
    //    index. Frames pulled after FIFO underflows must be zeroed.
    {{512,
      7,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 384},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 384},
      }},
     // - expectedIndexRead = 128, expectedIndexWrite = 128
     // - overflowCount = 0, underflowCount = 1
     // - FIFO samples (index, expectedValue) = (128, 128)
     // - Output bus samples (index, expectedValue) = (0, 384), (255, 639)
     //                                               (256, 0), (383, 0)
     {128,
      128,
      0,
      1,
      {{128, 128}},
      {{0, 384}, {255, 639}, {256, 0}, {383, 0}}}},

    // Test case 9: Underflow in unusual FIFO and pull length.
    //  - Check underflow counter.
    //  - After the underflow occurs, the write index must be moved to the read
    //    index. Frames pulled after FIFO underflows must be zeroed.
    {{523,
      11,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 383},
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 383},
      }},
     // - expectedIndexRead = 117, expectedIndexWrite = 117
     // - overflowCount = 0, underflowCount = 1
     // - FIFO samples (index, expectedValue) = (117, 117)
     // - Output bus samples (index, expectedValue) = (0, 383), (256, 639)
     //                                               (257, 0), (382, 0)
     {117,
      117,
      0,
      1,
      {{117, 117}},
      {{0, 383}, {256, 639}, {257, 0}, {382, 0}}}},

    // Test case 10: Multiple pull from an empty FIFO.
    //  - Check underflow counter.
    //  - After the underflow occurs, the write index must be moved to the read
    //    index. Frames pulled after FIFO underflows must be zeroed.
    {{1024,
      11,
      {
          {"PUSH", 128},
          {"PUSH", 128},
          {"PULL", 440},
          {"PULL", 440},
          {"PULL", 440},
          {"PULL", 440},
          {"PULL", 440},
      }},
     // - expectedIndexRead = 117, expectedIndexWrite = 117
     // - overflowCount = 0, underflowCount = 1
     // - FIFO samples (index, expectedValue) = (117, 117)
     // - Output bus samples (index, expectedValue) = (0, 383), (256, 639)
     //                                               (257, 0), (382, 0)
     {256, 256, 0, 5, {{256, 0}}, {{0, 0}, {439, 0}}}},

    // Test case 11: Multiple pull from an empty FIFO. (zero push)
    {{1024,
      11,
      {
          {"PULL", 144},
          {"PULL", 144},
          {"PULL", 144},
          {"PULL", 144},
      }},
     // - expectedIndexRead = 0, expectedIndexWrite = 0
     // - overflowCount = 0, underflowCount = 4
     // - FIFO samples (index, expectedValue) = (0, 0), (1023, 0)
     // - Output bus samples (index, expectedValue) = (0, 0), (143, 0)
     {0, 0, 0, 4, {{0, 0}, {1023, 0}}, {{0, 0}, {143, 0}}}}};

INSTANTIATE_TEST_CASE_P(PushPullFIFOFeatureTest,
                        PushPullFIFOFeatureTest,
                        ::testing::ValuesIn(featureTestParams));

}  // namespace

}  // namespace blink
