// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"

namespace blink {

// The MediaTrackConstraintsTest group tests the types declared in
// third_party/blink/renderer/platform/mediastream/media_constraints.h
TEST(MediaTrackConstraintsTest, LongConstraint) {
  LongConstraint range_constraint(nullptr);
  range_constraint.SetMin(5);
  range_constraint.SetMax(6);
  EXPECT_TRUE(range_constraint.Matches(5));
  EXPECT_TRUE(range_constraint.Matches(6));
  EXPECT_FALSE(range_constraint.Matches(4));
  EXPECT_FALSE(range_constraint.Matches(7));
  LongConstraint exact_constraint(nullptr);
  exact_constraint.SetExact(5);
  EXPECT_FALSE(exact_constraint.Matches(4));
  EXPECT_TRUE(exact_constraint.Matches(5));
  EXPECT_FALSE(exact_constraint.Matches(6));
}

TEST(MediaTrackConstraintsTest, DoubleConstraint) {
  DoubleConstraint range_constraint(nullptr);
  EXPECT_TRUE(range_constraint.IsEmpty());
  range_constraint.SetMin(5.0);
  range_constraint.SetMax(6.5);
  EXPECT_FALSE(range_constraint.IsEmpty());
  // Matching within epsilon
  EXPECT_TRUE(
      range_constraint.Matches(5.0 - DoubleConstraint::kConstraintEpsilon / 2));
  EXPECT_TRUE(
      range_constraint.Matches(6.5 + DoubleConstraint::kConstraintEpsilon / 2));
  DoubleConstraint exact_constraint(nullptr);
  exact_constraint.SetExact(5.0);
  EXPECT_FALSE(range_constraint.IsEmpty());
  EXPECT_FALSE(exact_constraint.Matches(4.9));
  EXPECT_TRUE(exact_constraint.Matches(5.0));
  EXPECT_TRUE(
      exact_constraint.Matches(5.0 - DoubleConstraint::kConstraintEpsilon / 2));
  EXPECT_TRUE(
      exact_constraint.Matches(5.0 + DoubleConstraint::kConstraintEpsilon / 2));
  EXPECT_FALSE(exact_constraint.Matches(5.1));
}

TEST(MediaTrackConstraintsTest, BooleanConstraint) {
  BooleanConstraint bool_constraint(nullptr);
  EXPECT_TRUE(bool_constraint.IsEmpty());
  EXPECT_TRUE(bool_constraint.Matches(false));
  EXPECT_TRUE(bool_constraint.Matches(true));
  bool_constraint.SetExact(false);
  EXPECT_FALSE(bool_constraint.IsEmpty());
  EXPECT_FALSE(bool_constraint.Matches(true));
  EXPECT_TRUE(bool_constraint.Matches(false));
  bool_constraint.SetExact(true);
  EXPECT_FALSE(bool_constraint.Matches(false));
  EXPECT_TRUE(bool_constraint.Matches(true));
}

TEST(MediaTrackConstraintsTest, ConstraintSetEmpty) {
  MediaTrackConstraintSetPlatform the_set;
  EXPECT_TRUE(the_set.IsEmpty());
  the_set.echo_cancellation.SetExact(false);
  EXPECT_FALSE(the_set.IsEmpty());
}

TEST(MediaTrackConstraintsTest, ConstraintName) {
  const char* the_name = "name";
  BooleanConstraint bool_constraint(the_name);
  EXPECT_EQ(the_name, bool_constraint.GetName());
}

TEST(MediaTrackConstraintsTest, MandatoryChecks) {
  MediaTrackConstraintSetPlatform the_set;
  String found_name;
  EXPECT_FALSE(the_set.HasMandatory());
  EXPECT_FALSE(the_set.HasMandatoryOutsideSet({"width"}, found_name));
  EXPECT_FALSE(the_set.width.HasMandatory());
  the_set.width.SetMax(240);
  EXPECT_TRUE(the_set.width.HasMandatory());
  EXPECT_TRUE(the_set.HasMandatory());
  EXPECT_FALSE(the_set.HasMandatoryOutsideSet({"width"}, found_name));
  EXPECT_TRUE(the_set.HasMandatoryOutsideSet({"height"}, found_name));
  EXPECT_EQ("width", found_name);
  the_set.goog_payload_padding.SetExact(true);
  EXPECT_TRUE(the_set.HasMandatoryOutsideSet({"width"}, found_name));
  EXPECT_EQ("googPayloadPadding", found_name);
}

TEST(MediaTrackConstraintsTest, SetToString) {
  MediaTrackConstraintSetPlatform the_set;
  EXPECT_EQ("", the_set.ToString());
  the_set.width.SetMax(240);
  EXPECT_EQ("width: {max: 240}", the_set.ToString().Utf8());
  the_set.echo_cancellation.SetIdeal(true);
  EXPECT_EQ("width: {max: 240}, echoCancellation: {ideal: true}",
            the_set.ToString().Utf8());
}

TEST(MediaTrackConstraintsTest, ConstraintsToString) {
  MediaConstraints the_constraints;
  MediaTrackConstraintSetPlatform basic;
  Vector<MediaTrackConstraintSetPlatform> advanced(static_cast<size_t>(1));
  basic.width.SetMax(240);
  advanced[0].echo_cancellation.SetExact(true);
  the_constraints.Initialize(basic, advanced);
  EXPECT_EQ(
      "{width: {max: 240}, advanced: [{echoCancellation: {exact: true}}]}",
      the_constraints.ToString().Utf8());

  MediaConstraints null_constraints;
  EXPECT_EQ("", null_constraints.ToString().Utf8());
}

TEST(MediaTrackConstraintsTest, ConvertWebConstraintsBasic) {
  MediaConstraints input;
  MediaTrackConstraints* output =
      media_constraints_impl::ConvertConstraints(input);
  ALLOW_UNUSED_LOCAL(output);
}

TEST(MediaTrackConstraintsTest, ConvertWebSingleStringConstraint) {
  MediaConstraints input;

  MediaTrackConstraintSetPlatform basic;
  Vector<MediaTrackConstraintSetPlatform> advanced;

  basic.facing_mode.SetIdeal(Vector<String>({"foo"}));
  input.Initialize(basic, advanced);
  MediaTrackConstraints* output =
      media_constraints_impl::ConvertConstraints(input);
  ASSERT_TRUE(output->hasFacingMode());
  ASSERT_TRUE(output->facingMode().IsString());
  EXPECT_EQ("foo", output->facingMode().GetAsString());
}

TEST(MediaTrackConstraintsTest, ConvertWebDoubleStringConstraint) {
  MediaConstraints input;

  Vector<String> buffer(static_cast<size_t>(2u));
  buffer[0] = "foo";
  buffer[1] = "bar";

  MediaTrackConstraintSetPlatform basic;
  Vector<MediaTrackConstraintSetPlatform> advanced;
  basic.facing_mode.SetIdeal(buffer);
  input.Initialize(basic, advanced);

  MediaTrackConstraints* output =
      media_constraints_impl::ConvertConstraints(input);
  ASSERT_TRUE(output->hasFacingMode());
  ASSERT_TRUE(output->facingMode().IsStringSequence());
  auto out_buffer = output->facingMode().GetAsStringSequence();
  EXPECT_EQ("foo", out_buffer[0]);
  EXPECT_EQ("bar", out_buffer[1]);
}

TEST(MediaTrackConstraintsTest, ConvertBlinkStringConstraint) {
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  MediaConstraints output;
  StringOrStringSequenceOrConstrainDOMStringParameters parameter;
  parameter.SetString("foo");
  input->setFacingMode(parameter);
  output =
      media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(input);
  ASSERT_TRUE(output.Basic().facing_mode.HasIdeal());
  ASSERT_EQ(1U, output.Basic().facing_mode.Ideal().size());
  ASSERT_EQ("foo", output.Basic().facing_mode.Ideal()[0]);
}

TEST(MediaTrackConstraintsTest, ConvertBlinkComplexStringConstraint) {
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  MediaConstraints output;
  StringOrStringSequenceOrConstrainDOMStringParameters parameter;
  ConstrainDOMStringParameters* subparameter =
      ConstrainDOMStringParameters::Create();
  StringOrStringSequence inner_string;
  inner_string.SetString("foo");
  subparameter->setIdeal(inner_string);
  parameter.SetConstrainDOMStringParameters(subparameter);
  input->setFacingMode(parameter);
  output =
      media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(input);
  ASSERT_TRUE(output.Basic().facing_mode.HasIdeal());
  ASSERT_EQ(1U, output.Basic().facing_mode.Ideal().size());
  ASSERT_EQ("foo", output.Basic().facing_mode.Ideal()[0]);

  // Convert this back, and see that it appears as a single string.
  MediaTrackConstraints* recycled =
      media_constraints_impl::ConvertConstraints(output);
  ASSERT_TRUE(recycled->hasFacingMode());
  ASSERT_TRUE(recycled->facingMode().IsString());
  ASSERT_EQ("foo", recycled->facingMode().GetAsString());
}

TEST(MediaTrackConstraintsTest, NakedIsExactInAdvanced) {
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  StringOrStringSequenceOrConstrainDOMStringParameters parameter;
  parameter.SetString("foo");
  input->setFacingMode(parameter);
  HeapVector<Member<MediaTrackConstraintSet>> advanced(
      1, MediaTrackConstraintSet::Create());
  advanced[0]->setFacingMode(parameter);
  input->setAdvanced(advanced);

  MediaConstraints output =
      media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(input);
  ASSERT_TRUE(output.Basic().facing_mode.HasIdeal());
  ASSERT_FALSE(output.Basic().facing_mode.HasExact());
  ASSERT_EQ(1U, output.Basic().facing_mode.Ideal().size());
  ASSERT_EQ("foo", output.Basic().facing_mode.Ideal()[0]);

  ASSERT_FALSE(output.Advanced()[0].facing_mode.HasIdeal());
  ASSERT_TRUE(output.Advanced()[0].facing_mode.HasExact());
  ASSERT_EQ(1U, output.Advanced()[0].facing_mode.Exact().size());
  ASSERT_EQ("foo", output.Advanced()[0].facing_mode.Exact()[0]);
}

TEST(MediaTrackConstraintsTest, IdealAndExactConvertToNaked) {
  MediaConstraints input;
  Vector<String> buffer(static_cast<size_t>(1u));

  MediaTrackConstraintSetPlatform basic;
  MediaTrackConstraintSetPlatform advanced_element1;
  MediaTrackConstraintSetPlatform advanced_element2;
  buffer[0] = "ideal";
  basic.facing_mode.SetIdeal(buffer);
  advanced_element1.facing_mode.SetIdeal(buffer);
  buffer[0] = "exact";
  advanced_element2.facing_mode.SetExact(buffer);
  Vector<MediaTrackConstraintSetPlatform> advanced;
  advanced.push_back(advanced_element1);
  advanced.push_back(advanced_element2);
  input.Initialize(basic, advanced);

  MediaTrackConstraints* output =
      media_constraints_impl::ConvertConstraints(input);
  // The first element should return a ConstrainDOMStringParameters
  // with an "ideal" value containing a String value of "ideal".
  // The second element should return a ConstrainDOMStringParameters
  // with a String value of "exact".
  ASSERT_TRUE(output->hasAdvanced());
  ASSERT_EQ(2U, output->advanced().size());
  MediaTrackConstraintSet* element1 = output->advanced()[0];
  MediaTrackConstraintSet* element2 = output->advanced()[1];

  ASSERT_TRUE(output->hasFacingMode());
  ASSERT_TRUE(output->facingMode().IsString());
  EXPECT_EQ("ideal", output->facingMode().GetAsString());

  ASSERT_TRUE(element1->hasFacingMode());
  ASSERT_TRUE(element1->facingMode().IsConstrainDOMStringParameters());
  EXPECT_EQ("ideal", element1->facingMode()
                         .GetAsConstrainDOMStringParameters()
                         ->ideal()
                         .GetAsString());

  ASSERT_TRUE(element2->hasFacingMode());
  ASSERT_TRUE(element2->facingMode().IsString());
  EXPECT_EQ("exact", element2->facingMode().GetAsString());
}

}  // namespace blink
