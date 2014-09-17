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
        b_(setup_.settings(), Label(SourceDir("//b/"), "b")),
        c_(setup_.settings(), Label(SourceDir("//c/"), "c")),
        d_(setup_.settings(), Label(SourceDir("//d/"), "d")) {
    a_.set_output_type(Target::SOURCE_SET);
    b_.set_output_type(Target::SOURCE_SET);
    c_.set_output_type(Target::SOURCE_SET);
    d_.set_output_type(Target::SOURCE_SET);

    Err err;
    a_.SetToolchain(setup_.toolchain(), &err);
    b_.SetToolchain(setup_.toolchain(), &err);
    c_.SetToolchain(setup_.toolchain(), &err);
    d_.SetToolchain(setup_.toolchain(), &err);

    a_.public_deps().push_back(LabelTargetPair(&b_));
    b_.public_deps().push_back(LabelTargetPair(&c_));

    // Start with all public visibility.
    a_.visibility().SetPublic();
    b_.visibility().SetPublic();
    c_.visibility().SetPublic();
    d_.visibility().SetPublic();

    d_.OnResolved(&err);
    c_.OnResolved(&err);
    b_.OnResolved(&err);
    a_.OnResolved(&err);

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

  // Add a target P ("private") that privately depends on C, and hook up the
  // chain so that A -> P -> C. A will depend on C via two different paths.
  Err err;
  Target p(setup_.settings(), Label(SourceDir("//p/"), "p"));
  p.set_output_type(Target::SOURCE_SET);
  p.SetToolchain(setup_.toolchain(), &err);
  EXPECT_FALSE(err.has_error());
  p.private_deps().push_back(LabelTargetPair(&c_));
  p.visibility().SetPublic();
  p.OnResolved(&err);

  a_.public_deps().push_back(LabelTargetPair(&p));

  // A does not depend on itself.
  bool is_public = false;
  std::vector<const Target*> chain;
  EXPECT_FALSE(checker->IsDependencyOf(&a_, &a_, &chain, &is_public));

  // A depends on B.
  chain.clear();
  is_public = false;
  EXPECT_TRUE(checker->IsDependencyOf(&b_, &a_, &chain, &is_public));
  ASSERT_EQ(2u, chain.size());
  EXPECT_EQ(&b_, chain[0]);
  EXPECT_EQ(&a_, chain[1]);
  EXPECT_TRUE(is_public);

  // A indirectly depends on C. The "public" dependency path through B should
  // be identified.
  chain.clear();
  is_public = false;
  EXPECT_TRUE(checker->IsDependencyOf(&c_, &a_, &chain, &is_public));
  ASSERT_EQ(3u, chain.size());
  EXPECT_EQ(&c_, chain[0]);
  EXPECT_EQ(&b_, chain[1]);
  EXPECT_EQ(&a_, chain[2]);
  EXPECT_TRUE(is_public);

  // C does not depend on A.
  chain.clear();
  is_public = false;
  EXPECT_FALSE(checker->IsDependencyOf(&a_, &c_, &chain, &is_public));
  EXPECT_TRUE(chain.empty());
  EXPECT_FALSE(is_public);

  // Add a private A -> C dependency. This should not be detected since it's
  // private, even though it's shorter than the A -> B -> C one.
  chain.clear();
  is_public = false;
  a_.private_deps().push_back(LabelTargetPair(&c_));
  EXPECT_TRUE(checker->IsDependencyOf(&c_, &a_, &chain, &is_public));
  EXPECT_EQ(&c_, chain[0]);
  EXPECT_EQ(&b_, chain[1]);
  EXPECT_EQ(&a_, chain[2]);
  EXPECT_TRUE(is_public);

  // Remove the B -> C public dependency, leaving A's private dep on C the only
  // path. This should now be found.
  chain.clear();
  EXPECT_EQ(&c_, b_.public_deps()[0].ptr); // Validate it's the right one.
  b_.public_deps().erase(b_.public_deps().begin());
  EXPECT_TRUE(checker->IsDependencyOf(&c_, &a_, &chain, &is_public));
  EXPECT_EQ(&c_, chain[0]);
  EXPECT_EQ(&a_, chain[1]);
  EXPECT_FALSE(is_public);
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
}

// Checks that the allow_circular_includes_from list works.
TEST_F(HeaderCheckerTest, CheckIncludeAllowCircular) {
  InputFile input_file(SourceFile("//some_file.cc"));
  input_file.SetContents(std::string());
  LocationRange range;  // Dummy value.

  // Add an include file to A.
  SourceFile a_public("//a_public.h");
  a_.sources().push_back(a_public);

  scoped_refptr<HeaderChecker> checker(
      new HeaderChecker(setup_.build_settings(), targets_));

  // A depends on B. So B normally can't include headers from A.
  Err err;
  EXPECT_FALSE(checker->CheckInclude(&b_, input_file, a_public, range, &err));
  EXPECT_TRUE(err.has_error());

  // Add an allow_circular_includes_from on A that lists B.
  a_.allow_circular_includes_from().insert(b_.label());

  // Now the include from B to A should be allowed.
  err = Err();
  EXPECT_TRUE(checker->CheckInclude(&b_, input_file, a_public, range, &err));
  EXPECT_FALSE(err.has_error());
}
