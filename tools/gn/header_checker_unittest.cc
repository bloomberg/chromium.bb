// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/config.h"
#include "tools/gn/header_checker.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"

namespace {

class HeaderCheckerTest : public testing::Test {
 public:
  HeaderCheckerTest()
      : a_(setup_.settings(), Label(SourceDir("//a/"), "a")),
        b_(setup_.settings(), Label(SourceDir("//b/"), "a")),
        c_(setup_.settings(), Label(SourceDir("//c/"), "c")),
        d_(setup_.settings(), Label(SourceDir("//d/"), "d")) {
    a_.deps().push_back(LabelTargetPair(&b_));
    b_.deps().push_back(LabelTargetPair(&c_));

    // Start with all public visibility.
    a_.visibility().SetPublic();
    b_.visibility().SetPublic();
    c_.visibility().SetPublic();
    d_.visibility().SetPublic();

    targets_.push_back(&a_);
    targets_.push_back(&b_);
    targets_.push_back(&c_);
    targets_.push_back(&d_);
  }

 protected:
  Scheduler scheduler_;

  TestWithScope setup_;

  // Some headers that are automatically set up with a dependency chain.
  // a -> b -> c
  Target a_;
  Target b_;
  Target c_;
  Target d_;

  std::vector<const Target*> targets_;
};

}  // namespace

TEST_F(HeaderCheckerTest, IsDependencyOf) {
  scoped_refptr<HeaderChecker> checker(
      new HeaderChecker(setup_.build_settings(), targets_));

  std::vector<const Target*> chain;
  EXPECT_FALSE(checker->IsDependencyOf(&a_, &a_, false, &chain, NULL));

  chain.clear();
  EXPECT_TRUE(checker->IsDependencyOf(&b_, &a_, false, &chain, NULL));
  ASSERT_EQ(2u, chain.size());
  EXPECT_EQ(&b_, chain[0]);
  EXPECT_EQ(&a_, chain[1]);

  chain.clear();
  EXPECT_TRUE(checker->IsDependencyOf(&c_, &a_, false, &chain, NULL));
  ASSERT_EQ(3u, chain.size());
  EXPECT_EQ(&c_, chain[0]);
  EXPECT_EQ(&b_, chain[1]);
  EXPECT_EQ(&a_, chain[2]);

  chain.clear();
  EXPECT_FALSE(checker->IsDependencyOf(&a_, &c_, false, &chain, NULL));
  EXPECT_TRUE(chain.empty());

  // If an a -> c dependency exists, this should be chosen for the chain.
  chain.clear();
  a_.deps().push_back(LabelTargetPair(&c_));
  EXPECT_TRUE(checker->IsDependencyOf(&c_, &a_, false, &chain, NULL));
  EXPECT_EQ(&c_, chain[0]);
  EXPECT_EQ(&a_, chain[1]);
}

TEST_F(HeaderCheckerTest, IsDependencyOf_ForwardsDirectDependentConfigs) {
  scoped_refptr<HeaderChecker> checker(
      new HeaderChecker(setup_.build_settings(), targets_));

  // The a -> b -> c chain is found, since no chains that forward direct-
  // dependent configs exist.
  std::vector<const Target*> chain;
  bool direct_dependent_configs_apply = false;
  EXPECT_TRUE(checker->IsDependencyOf(
      &c_, &a_, true, &chain, &direct_dependent_configs_apply));
  EXPECT_FALSE(direct_dependent_configs_apply);
  EXPECT_EQ(3u, chain.size());
  EXPECT_EQ(&c_, chain[0]);
  EXPECT_EQ(&b_, chain[1]);
  EXPECT_EQ(&a_, chain[2]);

  // Create a chain a -> d -> c where d forwards direct-dependent configs.
  // This path should be preferred when dependency chains which forward
  // direct-dependent configs are preferred.
  chain.clear();
  direct_dependent_configs_apply = false;
  d_.deps().push_back(LabelTargetPair(&c_));
  d_.forward_dependent_configs().push_back(LabelTargetPair(&c_));
  a_.deps().push_back(LabelTargetPair(&d_));
  EXPECT_TRUE(checker->IsDependencyOf(
      &c_, &a_, true, &chain, &direct_dependent_configs_apply));
  EXPECT_TRUE(direct_dependent_configs_apply);
  EXPECT_EQ(3u, chain.size());
  EXPECT_EQ(&c_, chain[0]);
  EXPECT_EQ(&d_, chain[1]);
  EXPECT_EQ(&a_, chain[2]);

  // d also forwards direct-dependent configs if it is a group.
  chain.clear();
  direct_dependent_configs_apply = false;
  d_.set_output_type(Target::GROUP);
  d_.forward_dependent_configs().clear();
  EXPECT_TRUE(checker->IsDependencyOf(
      &c_, &a_, true, &chain, &direct_dependent_configs_apply));
  EXPECT_TRUE(direct_dependent_configs_apply);
  EXPECT_EQ(3u, chain.size());
  EXPECT_EQ(&c_, chain[0]);
  EXPECT_EQ(&d_, chain[1]);
  EXPECT_EQ(&a_, chain[2]);

  // A direct dependency a -> c carries direct-dependent configs.
  chain.clear();
  direct_dependent_configs_apply = false;
  a_.deps().push_back(LabelTargetPair(&c_));
  EXPECT_TRUE(checker->IsDependencyOf(
      &c_, &a_, true, &chain, &direct_dependent_configs_apply));
  EXPECT_TRUE(direct_dependent_configs_apply);
  EXPECT_EQ(2u, chain.size());
  EXPECT_EQ(&c_, chain[0]);
  EXPECT_EQ(&a_, chain[1]);
}

TEST_F(HeaderCheckerTest, CheckInclude) {
  InputFile input_file(SourceFile("//some_file.cc"));
  input_file.SetContents(std::string());
  LocationRange range;  // Dummy value.

  // Add a disconnected target d with a header to check that you have to have
  // to depend on a target listing a header.
  SourceFile d_header("//d_header.h");
  d_.sources().push_back(SourceFile(d_header));

  // Add a header on B and say everything in B is public.
  SourceFile b_public("//b_public.h");
  b_.sources().push_back(b_public);
  c_.set_all_headers_public(true);

  // Add a public and private header on C.
  SourceFile c_public("//c_public.h");
  SourceFile c_private("//c_private.h");
  c_.sources().push_back(c_private);
  c_.public_headers().push_back(c_public);
  c_.set_all_headers_public(false);

  targets_.push_back(&d_);
  scoped_refptr<HeaderChecker> checker(
      new HeaderChecker(setup_.build_settings(), targets_));

  // A file in target A can't include a header from D because A has no
  // dependency on D.
  Err err;
  EXPECT_FALSE(checker->CheckInclude(&a_, input_file, d_header, range, &err));
  EXPECT_TRUE(err.has_error());

  // A can include the public header in B.
  err = Err();
  EXPECT_TRUE(checker->CheckInclude(&a_, input_file, b_public, range, &err));
  EXPECT_FALSE(err.has_error());

  // Check A depending on the public and private headers in C.
  err = Err();
  EXPECT_TRUE(checker->CheckInclude(&a_, input_file, c_public, range, &err));
  EXPECT_FALSE(err.has_error());
  EXPECT_FALSE(checker->CheckInclude(&a_, input_file, c_private, range, &err));
  EXPECT_TRUE(err.has_error());

  // A can depend on a random file unknown to the build.
  err = Err();
  EXPECT_TRUE(checker->CheckInclude(&a_, input_file, SourceFile("//random.h"),
                                    range, &err));
  EXPECT_FALSE(err.has_error());

  // If C is not visible from A, A can't include public headers even if there
  // is a dependency path.
  c_.visibility().SetPrivate(c_.label().dir());
  err = Err();
  EXPECT_FALSE(checker->CheckInclude(&a_, input_file, c_public, range, &err));
  EXPECT_TRUE(err.has_error());
  c_.visibility().SetPublic();

  // If C has direct-dependent configs, then B must forward them to A.
  // If B is a group, that suffices to forward direct-dependent configs.
  {
    Config direct(setup_.settings(), Label(SourceDir("//c/"), "config"));
    direct.config_values().cflags().push_back("-DSOME_DEFINE");

    c_.direct_dependent_configs().push_back(LabelConfigPair(&direct));
    err = Err();
    EXPECT_FALSE(checker->CheckInclude(&a_, input_file, c_public, range, &err));
    EXPECT_TRUE(err.has_error());

    b_.forward_dependent_configs().push_back(LabelTargetPair(&c_));
    err = Err();
    EXPECT_TRUE(checker->CheckInclude(&a_, input_file, c_public, range, &err));
    EXPECT_FALSE(err.has_error());

    b_.forward_dependent_configs().clear();
    b_.set_output_type(Target::GROUP);
    err = Err();
    EXPECT_TRUE(checker->CheckInclude(&a_, input_file, c_public, range, &err));
    EXPECT_FALSE(err.has_error());

    b_.set_output_type(Target::UNKNOWN);
    c_.direct_dependent_configs().clear();
  }
}

TEST_F(HeaderCheckerTest, GetDependentConfigChainProblemIndex) {
  // Assume we have a chain A -> B -> C -> D.
  Target target_a(setup_.settings(), Label(SourceDir("//a/"), "a"));
  Target target_b(setup_.settings(), Label(SourceDir("//b/"), "b"));
  Target target_c(setup_.settings(), Label(SourceDir("//c/"), "c"));
  Target target_d(setup_.settings(), Label(SourceDir("//d/"), "d"));

  // C is a group, and B forwards deps from C, so A should get configs from D.
  target_a.set_output_type(Target::SOURCE_SET);
  target_b.set_output_type(Target::SOURCE_SET);
  target_c.set_output_type(Target::GROUP);
  target_d.set_output_type(Target::SOURCE_SET);
  target_b.forward_dependent_configs().push_back(
      LabelTargetPair(&target_c));

  // Dependency chain goes from bottom to top.
  std::vector<const Target*> chain;
  chain.push_back(&target_d);
  chain.push_back(&target_c);
  chain.push_back(&target_b);
  chain.push_back(&target_a);

  // If C is not a group, it shouldn't work anymore.
  target_c.set_output_type(Target::SOURCE_SET);
  EXPECT_EQ(1u, HeaderChecker::GetDependentConfigChainProblemIndex(chain));

  // Or if B stops forwarding from C, it shouldn't work anymore.
  target_c.set_output_type(Target::GROUP);
  target_b.forward_dependent_configs().clear();
  EXPECT_EQ(2u, HeaderChecker::GetDependentConfigChainProblemIndex(chain));
}
