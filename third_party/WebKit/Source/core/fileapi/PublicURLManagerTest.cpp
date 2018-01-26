// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fileapi/PublicURLManager.h"

#include "core/fileapi/URLRegistry.h"
#include "core/testing/NullExecutionContext.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/blob/testing/FakeBlob.h"
#include "platform/blob/testing/FakeBlobURLStore.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using mojom::blink::Blob;
using mojom::blink::BlobPtr;
using mojom::blink::BlobRequest;
using mojom::blink::BlobURLStore;
using mojom::blink::BlobURLStoreAssociatedPtr;

class TestURLRegistrable : public URLRegistrable {
 public:
  TestURLRegistrable(URLRegistry* registry, BlobPtr blob = nullptr)
      : registry_(registry), blob_(std::move(blob)) {}

  URLRegistry& Registry() const override { return *registry_; }

  BlobPtr AsMojoBlob() override {
    if (!blob_)
      return nullptr;
    BlobPtr result;
    blob_->Clone(MakeRequest(&result));
    return result;
  }

 private:
  URLRegistry* const registry_;
  BlobPtr blob_;
};

class FakeURLRegistry : public URLRegistry {
 public:
  void RegisterURL(SecurityOrigin* origin,
                   const KURL& url,
                   URLRegistrable* registrable) override {
    registrations.push_back(Registration{origin, url, registrable});
  }
  void UnregisterURL(const KURL&) override {}

  struct Registration {
    SecurityOrigin* origin;
    KURL url;
    URLRegistrable* registrable;
  };
  Vector<Registration> registrations;
};

}  // namespace

class PublicURLManagerTest : public ::testing::Test {
 public:
  PublicURLManagerTest() : url_store_binding_(&url_store_) {}

  void SetUp() override {
    execution_context_ = new NullExecutionContext;
    // By default this creates a unique origin, which is exactly what this test
    // wants.
    execution_context_->SetUpSecurityContext();

    BlobURLStoreAssociatedPtr url_store_ptr;
    url_store_binding_.Bind(
        MakeRequestAssociatedWithDedicatedPipe(&url_store_ptr));
    url_manager().SetURLStoreForTesting(std::move(url_store_ptr));
  }

  PublicURLManager& url_manager() {
    return execution_context_->GetPublicURLManager();
  }

  BlobPtr CreateMojoBlob(const String& uuid) {
    BlobPtr result;
    mojo::MakeStrongBinding(std::make_unique<FakeBlob>(uuid),
                            MakeRequest(&result));
    return result;
  }

 protected:
  ScopedMojoBlobURLsForTest mojo_blob_urls_ = true;
  Persistent<NullExecutionContext> execution_context_;

  FakeBlobURLStore url_store_;
  mojo::AssociatedBinding<BlobURLStore> url_store_binding_;
};

TEST_F(PublicURLManagerTest, RegisterNonMojoBlob) {
  FakeURLRegistry registry;
  TestURLRegistrable registrable(&registry);
  String url = url_manager().RegisterURL(&registrable);
  ASSERT_EQ(1u, registry.registrations.size());
  EXPECT_EQ(0u, url_store_.registrations.size());
  EXPECT_EQ(execution_context_->GetSecurityOrigin(),
            registry.registrations[0].origin);
  EXPECT_EQ(url, registry.registrations[0].url);
  EXPECT_EQ(&registrable, registry.registrations[0].registrable);

  EXPECT_TRUE(SecurityOrigin::CreateFromString(url)->IsSameSchemeHostPort(
      execution_context_->GetSecurityOrigin()));
  EXPECT_EQ(execution_context_->GetSecurityOrigin(),
            SecurityOrigin::CreateFromString(url));

  url_manager().Revoke(KURL(url));
  EXPECT_FALSE(SecurityOrigin::CreateFromString(url)->IsSameSchemeHostPort(
      execution_context_->GetSecurityOrigin()));
  url_store_binding_.FlushForTesting();
  // Even though this was not a mojo blob, the PublicURLManager might not know
  // that, so still expect a revocation on the mojo interface.
  ASSERT_EQ(1u, url_store_.revocations.size());
  EXPECT_EQ(url, url_store_.revocations[0]);
}

TEST_F(PublicURLManagerTest, RegisterMojoBlob) {
  FakeURLRegistry registry;
  TestURLRegistrable registrable(&registry, CreateMojoBlob("id"));
  String url = url_manager().RegisterURL(&registrable);

  EXPECT_EQ(0u, registry.registrations.size());
  ASSERT_EQ(1u, url_store_.registrations.size());
  EXPECT_EQ(url, url_store_.registrations.begin()->key);

  EXPECT_TRUE(SecurityOrigin::CreateFromString(url)->IsSameSchemeHostPort(
      execution_context_->GetSecurityOrigin()));
  EXPECT_EQ(execution_context_->GetSecurityOrigin(),
            SecurityOrigin::CreateFromString(url));

  url_manager().Revoke(KURL(url));
  EXPECT_FALSE(SecurityOrigin::CreateFromString(url)->IsSameSchemeHostPort(
      execution_context_->GetSecurityOrigin()));
  url_store_binding_.FlushForTesting();
  ASSERT_EQ(1u, url_store_.revocations.size());
  EXPECT_EQ(url, url_store_.revocations[0]);
}

TEST_F(PublicURLManagerTest, RevokeValidNonRegisteredURL) {
  execution_context_->SetURL(KURL("http://example.com/foo/bar"));
  execution_context_->SetUpSecurityContext();

  KURL url = KURL("blob:http://example.com/id");
  url_manager().Revoke(url);
  url_store_binding_.FlushForTesting();
  ASSERT_EQ(1u, url_store_.revocations.size());
  EXPECT_EQ(url, url_store_.revocations[0]);
}

TEST_F(PublicURLManagerTest, RevokeInvalidURL) {
  execution_context_->SetURL(KURL("http://example.com/foo/bar"));
  execution_context_->SetUpSecurityContext();

  KURL invalid_scheme_url = KURL("blb:http://example.com/id");
  KURL fragment_url = KURL("blob:http://example.com/id#fragment");
  KURL invalid_origin_url = KURL("blob:http://foobar.com/id");
  url_manager().Revoke(invalid_scheme_url);
  url_manager().Revoke(fragment_url);
  url_manager().Revoke(invalid_origin_url);
  url_store_binding_.FlushForTesting();
  // Both should have been silently ignored.
  EXPECT_TRUE(url_store_.revocations.IsEmpty());
}

}  // namespace blink
