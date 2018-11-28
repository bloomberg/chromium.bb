// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/call_setup_state_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

template <typename StateType>
std::vector<StateType> GetAllCallSetupStates();

template <>
std::vector<OffererState> GetAllCallSetupStates() {
  std::vector<OffererState> states = {OffererState::kNotStarted,
                                      OffererState::kCreateOfferPending,
                                      OffererState::kCreateOfferRejected,
                                      OffererState::kCreateOfferResolved,
                                      OffererState::kSetLocalOfferPending,
                                      OffererState::kSetLocalOfferRejected,
                                      OffererState::kSetLocalOfferResolved,
                                      OffererState::kSetRemoteAnswerPending,
                                      OffererState::kSetRemoteAnswerRejected,
                                      OffererState::kSetRemoteAnswerResolved};
  EXPECT_EQ(static_cast<size_t>(OffererState::kMaxValue) + 1u, states.size());
  return states;
}

template <>
std::vector<AnswererState> GetAllCallSetupStates() {
  std::vector<AnswererState> states = {AnswererState::kNotStarted,
                                       AnswererState::kSetRemoteOfferPending,
                                       AnswererState::kSetRemoteOfferRejected,
                                       AnswererState::kSetRemoteOfferResolved,
                                       AnswererState::kCreateAnswerPending,
                                       AnswererState::kCreateAnswerRejected,
                                       AnswererState::kCreateAnswerResolved,
                                       AnswererState::kSetLocalAnswerPending,
                                       AnswererState::kSetLocalAnswerRejected,
                                       AnswererState::kSetLocalAnswerResolved};
  EXPECT_EQ(static_cast<size_t>(AnswererState::kMaxValue) + 1u, states.size());
  return states;
}

}  // namespace

class CallSetupStateTrackerTest : public testing::Test {
 public:
  enum class Reachability {
    kReachable,
    kUnreachable,
  };

  template <typename StateType>
  StateType current_state() const;

  template <>
  OffererState current_state() const {
    return tracker_.offerer_state();
  }

  template <>
  AnswererState current_state() const {
    return tracker_.answerer_state();
  }

  template <typename StateType>
  bool NoteStateEvent(CallSetupStateTracker* tracker, StateType event) const;

  template <>
  bool NoteStateEvent(CallSetupStateTracker* tracker,
                      OffererState event) const {
    return tracker->NoteOffererStateEvent(event);
  }

  template <>
  bool NoteStateEvent(CallSetupStateTracker* tracker,
                      AnswererState event) const {
    return tracker->NoteAnswererStateEvent(event);
  }

  template <typename StateType>
  bool VerifyReachability(Reachability reachability,
                          std::vector<StateType> states) const {
    bool expected_state_reached = (reachability == Reachability::kReachable);
    for (const auto& state : states) {
      bool did_reach_state;
      if (state == current_state<StateType>()) {
        // The current state always counts as reachable.
        did_reach_state = true;
      } else {
        // Perform the test on a copy to avoid mutating |tracker_|.
        CallSetupStateTracker tracker_copy = tracker_;
        did_reach_state = NoteStateEvent<StateType>(&tracker_copy, state);
      }
      if (did_reach_state != expected_state_reached)
        return false;
    }
    return true;
  }

  template <typename StateType>
  bool VerifyOnlyReachableStates(std::vector<StateType> reachable_states,
                                 bool include_current = true) const {
    if (include_current)
      reachable_states.push_back(current_state<StateType>());
    std::vector<StateType> unreachable_states =
        GetAllCallSetupStates<StateType>();
    for (const auto& reachable_state : reachable_states) {
      unreachable_states.erase(std::find(unreachable_states.begin(),
                                         unreachable_states.end(),
                                         reachable_state));
    }
    return VerifyReachability<StateType>(Reachability::kReachable,
                                         reachable_states) &&
           VerifyReachability<StateType>(Reachability::kUnreachable,
                                         unreachable_states);
  }

 protected:
  CallSetupStateTracker tracker_;
};

TEST_F(CallSetupStateTrackerTest, InitialState) {
  EXPECT_EQ(OffererState::kNotStarted, tracker_.offerer_state());
  EXPECT_EQ(AnswererState::kNotStarted, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kNotStarted, tracker_.GetCallSetupState());
}

TEST_F(CallSetupStateTrackerTest, OffererSuccessfulNegotiation) {
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kCreateOfferPending}));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending));
  EXPECT_EQ(OffererState::kCreateOfferPending, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kCreateOfferResolved,
       OffererState::kCreateOfferRejected}));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved));
  EXPECT_EQ(OffererState::kCreateOfferResolved, tracker_.offerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetLocalOfferPending}));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferPending));
  EXPECT_EQ(OffererState::kSetLocalOfferPending, tracker_.offerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetLocalOfferResolved,
       OffererState::kSetLocalOfferRejected}));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferResolved));
  EXPECT_EQ(OffererState::kSetLocalOfferResolved, tracker_.offerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetRemoteAnswerPending}));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerPending));
  EXPECT_EQ(OffererState::kSetRemoteAnswerPending, tracker_.offerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetRemoteAnswerResolved,
       OffererState::kSetRemoteAnswerRejected}));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerResolved));
  EXPECT_EQ(OffererState::kSetRemoteAnswerResolved, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>({}));
}

TEST_F(CallSetupStateTrackerTest, OffererCreateOfferRejected) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferRejected));
  EXPECT_EQ(OffererState::kCreateOfferRejected, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kCreateOfferResolved}));
}

TEST_F(CallSetupStateTrackerTest, OffererSetLocalOfferRejected) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferPending));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferRejected));
  EXPECT_EQ(OffererState::kSetLocalOfferRejected, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetLocalOfferResolved}));
}

TEST_F(CallSetupStateTrackerTest, OffererSetRemoteAnswerRejected) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferPending));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerPending));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerRejected));
  EXPECT_EQ(OffererState::kSetRemoteAnswerRejected, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetRemoteAnswerResolved}));
}

TEST_F(CallSetupStateTrackerTest, OffererRejectThenSucceed) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferPending));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerPending));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerRejected));
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  // Pending another operation should not revert the states to "pending" or
  // "started".
  EXPECT_FALSE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerPending));
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerResolved));
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
}

TEST_F(CallSetupStateTrackerTest, AnswererSuccessfulNegotiation) {
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetRemoteOfferPending}));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferPending));
  EXPECT_EQ(AnswererState::kSetRemoteOfferPending, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetRemoteOfferResolved,
       AnswererState::kSetRemoteOfferRejected}));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferResolved));
  EXPECT_EQ(AnswererState::kSetRemoteOfferResolved, tracker_.answerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kCreateAnswerPending}));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kCreateAnswerPending));
  EXPECT_EQ(AnswererState::kCreateAnswerPending, tracker_.answerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kCreateAnswerResolved,
       AnswererState::kCreateAnswerRejected}));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kCreateAnswerResolved));
  EXPECT_EQ(AnswererState::kCreateAnswerResolved, tracker_.answerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetLocalAnswerPending}));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetLocalAnswerPending));
  EXPECT_EQ(AnswererState::kSetLocalAnswerPending, tracker_.answerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetLocalAnswerResolved,
       AnswererState::kSetLocalAnswerRejected}));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetLocalAnswerResolved));
  EXPECT_EQ(AnswererState::kSetLocalAnswerResolved, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>({}));
}

TEST_F(CallSetupStateTrackerTest, AnswererSetRemoteOfferRejected) {
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferPending));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferRejected));
  EXPECT_EQ(AnswererState::kSetRemoteOfferRejected, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetRemoteOfferResolved}));
}

TEST_F(CallSetupStateTrackerTest, AnswererCreateAnswerRejected) {
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferPending));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kCreateAnswerPending));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kCreateAnswerRejected));
  EXPECT_EQ(AnswererState::kCreateAnswerRejected, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kCreateAnswerResolved}));
}

TEST_F(CallSetupStateTrackerTest, AnswererSetLocalAnswerRejected) {
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferPending));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kCreateAnswerPending));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kCreateAnswerResolved));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetLocalAnswerPending));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetLocalAnswerRejected));
  EXPECT_EQ(AnswererState::kSetLocalAnswerRejected, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetLocalAnswerResolved}));
}

TEST_F(CallSetupStateTrackerTest, AnswererRejectThenSucceed) {
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferPending));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kCreateAnswerPending));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kCreateAnswerResolved));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetLocalAnswerPending));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetLocalAnswerRejected));
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  // Pending another operation should not revert the states to "pending" or
  // "started".
  EXPECT_FALSE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetLocalAnswerPending));
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetLocalAnswerResolved));
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
}

// Succeeding in one role and subsequently failing in another should not revert
// the call setup state from kSucceeded; the most succeessful attempt would
// still have been successful.
TEST_F(CallSetupStateTrackerTest, OffererSucceedAnswererFail) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferPending));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferResolved));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerPending));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kSetRemoteAnswerResolved));
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferPending));
  EXPECT_TRUE(
      tracker_.NoteAnswererStateEvent(AnswererState::kSetRemoteOfferRejected));
  // Still succeeded.
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
}

}  // namespace blink
