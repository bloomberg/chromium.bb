// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/shared_worker_instance.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class SharedWorkerInstanceTest : public testing::Test {
 protected:
  SharedWorkerInstanceTest() {}

  SharedWorkerInstance CreateInstance(const GURL& script_url,
                                      const std::string& name,
                                      const url::Origin& constructor_origin) {
    return SharedWorkerInstance(
        script_url, blink::mojom::ScriptType::kClassic,
        network::mojom::CredentialsMode::kSameOrigin, name, constructor_origin,
        std::string(), network::mojom::ContentSecurityPolicyType::kReport,
        network::mojom::IPAddressSpace::kPublic,
        blink::mojom::SharedWorkerCreationContextType::kNonsecure);
  }

  bool Matches(const SharedWorkerInstance& instance,
               const std::string& url,
               const base::StringPiece& name) {
    url::Origin constructor_origin;
    if (GURL(url).SchemeIs(url::kDataScheme))
      constructor_origin = url::Origin::Create(GURL("http://example.com/"));
    else
      constructor_origin = url::Origin::Create(GURL(url));
    return instance.Matches(GURL(url), name.as_string(), constructor_origin);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerInstanceTest);
};

TEST_F(SharedWorkerInstanceTest, MatchesTest) {
  const std::string kDataURL("data:text/javascript;base64,Ly8gSGVsbG8h");
  const std::string kFileURL("file:///w.js");

  // SharedWorker that doesn't have a name option.
  GURL script_url1("http://example.com/w.js");
  std::string name1("");
  url::Origin constructor_origin1 = url::Origin::Create(script_url1);
  SharedWorkerInstance instance1 =
      CreateInstance(script_url1, name1, constructor_origin1);

  EXPECT_TRUE(Matches(instance1, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", "name"));
  EXPECT_FALSE(Matches(instance1, kDataURL, ""));
  EXPECT_FALSE(Matches(instance1, kDataURL, "name"));
  EXPECT_FALSE(Matches(instance1, kFileURL, ""));
  EXPECT_FALSE(Matches(instance1, kFileURL, "name"));

  // SharedWorker that has a name option.
  GURL script_url2("http://example.com/w.js");
  std::string name2("name");
  url::Origin constructor_origin2 = url::Origin::Create(script_url2);
  SharedWorkerInstance instance2 =
      CreateInstance(script_url2, name2, constructor_origin2);

  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", ""));
  EXPECT_TRUE(Matches(instance2, "http://example.com/w.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance2, kDataURL, ""));
  EXPECT_FALSE(Matches(instance2, kDataURL, "name"));
  EXPECT_FALSE(Matches(instance2, kFileURL, ""));
  EXPECT_FALSE(Matches(instance2, kFileURL, "name"));
}

TEST_F(SharedWorkerInstanceTest, MatchesTest_DataURLWorker) {
  const std::string kDataURL("data:text/javascript;base64,Ly8gSGVsbG8h");
  const std::string kFileURL("file:///w.js");

  // SharedWorker created from a data: URL without a name option.
  GURL script_url1(kDataURL);
  std::string name1("");
  url::Origin constructor_origin1 =
      url::Origin::Create(GURL("http://example.com/"));
  SharedWorkerInstance instance1 =
      CreateInstance(script_url1, name1, constructor_origin1);

  EXPECT_FALSE(Matches(instance1, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w.js", "name2"));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", "name2"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", "name2"));
  // This should match because the instance has the same data: URL, name, and
  // constructor origin.
  EXPECT_TRUE(Matches(instance1, kDataURL, ""));
  EXPECT_FALSE(Matches(instance1, kDataURL, "name"));
  EXPECT_FALSE(Matches(instance1, kFileURL, ""));
  EXPECT_FALSE(Matches(instance1, kFileURL, "name"));

  // SharedWorker created from a data: URL with a name option.
  GURL script_url2(kDataURL);
  std::string name2("name");
  url::Origin constructor_origin2 =
      url::Origin::Create(GURL("http://example.com/"));
  SharedWorkerInstance instance2 =
      CreateInstance(script_url2, name2, constructor_origin2);

  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance2, kDataURL, ""));
  // This should match because the instance has the same data: URL, name, and
  // constructor origin.
  EXPECT_TRUE(Matches(instance2, kDataURL, "name"));
  EXPECT_FALSE(Matches(instance2, kFileURL, ""));
  EXPECT_FALSE(Matches(instance2, kFileURL, "name"));

  // SharedWorker created from a data: URL on a remote origin (i.e., example.net
  // opposed to example.com) without a name option.
  GURL script_url3(kDataURL);
  std::string name3("");
  url::Origin constructor_origin3 =
      url::Origin::Create(GURL("http://example.net/"));
  SharedWorkerInstance instance3 =
      CreateInstance(script_url3, name3, constructor_origin3);

  EXPECT_FALSE(Matches(instance3, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance3, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance3, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance3, "http://example.net/w2.js", ""));
  EXPECT_FALSE(Matches(instance3, "http://example.com/w.js", "name"));
  EXPECT_FALSE(Matches(instance3, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance3, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance3, "http://example.net/w2.js", "name"));
  EXPECT_FALSE(Matches(instance3, "http://example.com/w.js", "name2"));
  EXPECT_FALSE(Matches(instance3, "http://example.com/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance3, "http://example.net/w.js", "name2"));
  EXPECT_FALSE(Matches(instance3, "http://example.net/w2.js", "name2"));
  // This should not match because the instance has a different constructor
  // origin.
  EXPECT_FALSE(Matches(instance3, kDataURL, ""));
  EXPECT_FALSE(Matches(instance3, kDataURL, "name"));
  EXPECT_FALSE(Matches(instance3, kFileURL, ""));
  EXPECT_FALSE(Matches(instance3, kFileURL, "name"));

  // SharedWorker created from a data: URL on a remote origin (i.e., example.net
  // opposed to example.com) with a name option.
  GURL script_url4(kDataURL);
  std::string name4("");
  url::Origin constructor_origin4 =
      url::Origin::Create(GURL("http://example.net/"));
  SharedWorkerInstance instance4 =
      CreateInstance(script_url4, name4, constructor_origin4);

  EXPECT_FALSE(Matches(instance4, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance4, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance4, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance4, "http://example.net/w2.js", ""));
  EXPECT_FALSE(Matches(instance4, "http://example.com/w.js", "name"));
  EXPECT_FALSE(Matches(instance4, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance4, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance4, "http://example.net/w2.js", "name"));
  EXPECT_FALSE(Matches(instance4, "http://example.com/w.js", "name2"));
  EXPECT_FALSE(Matches(instance4, "http://example.com/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance4, "http://example.net/w.js", "name2"));
  EXPECT_FALSE(Matches(instance4, "http://example.net/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance4, kDataURL, ""));
  // This should not match because the instance has a different constructor
  // origin.
  EXPECT_FALSE(Matches(instance4, kDataURL, "name"));
  EXPECT_FALSE(Matches(instance4, kFileURL, ""));
  EXPECT_FALSE(Matches(instance4, kFileURL, "name"));
}

TEST_F(SharedWorkerInstanceTest, MatchesTest_FileURLWorker) {
  const std::string kDataURL("data:text/javascript;base64,Ly8gSGVsbG8h");
  const std::string kFileURL("file:///w.js");

  // SharedWorker created from a file:// URL without a name option.
  GURL script_url1(kFileURL);
  std::string name1("");
  url::Origin constructor_origin1 = url::Origin::Create(GURL(kFileURL));
  SharedWorkerInstance instance1 =
      CreateInstance(script_url1, name1, constructor_origin1);

  EXPECT_FALSE(Matches(instance1, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w.js", "name2"));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", "name2"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance1, kDataURL, ""));
  EXPECT_FALSE(Matches(instance1, kDataURL, "name"));
  // This should not match because file:// URL is treated as an opaque origin.
  EXPECT_FALSE(Matches(instance1, kFileURL, ""));
  EXPECT_FALSE(Matches(instance1, kFileURL, "name"));

  // SharedWorker created from a file:// URL with a name option.
  GURL script_url2(kFileURL);
  std::string name2("name");
  url::Origin constructor_origin2 = url::Origin::Create(GURL(kFileURL));
  SharedWorkerInstance instance2 =
      CreateInstance(script_url2, name2, constructor_origin2);

  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance2, kDataURL, ""));
  EXPECT_FALSE(Matches(instance2, kDataURL, "name"));
  EXPECT_FALSE(Matches(instance2, kFileURL, ""));
  // This should not match because file:// URL is treated as an opaque origin.
  EXPECT_FALSE(Matches(instance2, kFileURL, "name"));
}

TEST_F(SharedWorkerInstanceTest, AddressSpace) {
  const network::mojom::IPAddressSpace kAddressSpaces[] = {
      network::mojom::IPAddressSpace::kLocal,
      network::mojom::IPAddressSpace::kPrivate,
      network::mojom::IPAddressSpace::kPublic};
  for (auto address_space : kAddressSpaces) {
    SharedWorkerInstance instance(
        GURL("http://example.com/w.js"), blink::mojom::ScriptType::kClassic,
        network::mojom::CredentialsMode::kSameOrigin, "name",
        url::Origin::Create(GURL("http://example.com/")), std::string(),
        network::mojom::ContentSecurityPolicyType::kReport, address_space,
        blink::mojom::SharedWorkerCreationContextType::kNonsecure);
    EXPECT_EQ(address_space, instance.creation_address_space());
  }
}

}  // namespace content
