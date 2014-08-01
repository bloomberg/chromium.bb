// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/clipboard/DataObject.h"

#include "core/clipboard/DataObjectItem.h"
#include "public/platform/Platform.h"
#include "public/platform/WebBlobRegistry.h"
#include "public/platform/WebFileUtilities.h"
#include "public/platform/WebMimeRegistry.h"
#include "public/platform/WebUnitTestSupport.h"
#include <gtest/gtest.h>

namespace blink {

class MockMimeRegistry : public WebMimeRegistry {
public:
    virtual SupportsType supportsMIMEType(const WebString& mimeType) OVERRIDE
    {
        return SupportsType::MayBeSupported;
    }

    virtual SupportsType supportsImageMIMEType(const WebString& mimeType) OVERRIDE
    {
        return SupportsType::MayBeSupported;
    }

    virtual SupportsType supportsJavaScriptMIMEType(const WebString& mimeType) OVERRIDE
    {
        return SupportsType::MayBeSupported;
    }

    virtual SupportsType supportsMediaMIMEType(const WebString& mimeType, const WebString& codecs, const WebString& keySystem) OVERRIDE
    {
        return SupportsType::MayBeSupported;
    }

    virtual bool supportsMediaSourceMIMEType(const WebString& mimeType, const WebString& codecs) OVERRIDE
    {
        return false;
    }

    virtual bool supportsEncryptedMediaMIMEType(const WebString& keySystem, const WebString& mimeType, const WebString& codecs) OVERRIDE
    {
        return false;
    }

    virtual SupportsType supportsNonImageMIMEType(const WebString& mimeType) OVERRIDE
    {
        return SupportsType::MayBeSupported;
    }

    virtual WebString mimeTypeForExtension(const WebString& fileExtension) OVERRIDE
    {
        return WebString();
    }

    virtual WebString wellKnownMimeTypeForExtension(const WebString& fileExtension) OVERRIDE
    {
        return WebString();
    }

    virtual WebString mimeTypeFromFile(const WebString& filePath) OVERRIDE
    {
        return WebString();
    }
};

class MockFileUtilities : public WebFileUtilities {
public:
    ~MockFileUtilities() { }
};

class RegistryMockPlatform : public Platform {
public:
    RegistryMockPlatform(Platform* oldPlatform)
        : m_oldPlatform(oldPlatform)
    {
    }

    virtual ~RegistryMockPlatform() { }

    virtual WebBlobRegistry* blobRegistry() OVERRIDE
    {
        return &m_mockBlobRegistry;
    }

    virtual WebMimeRegistry* mimeRegistry() OVERRIDE
    {
        return &m_mockMimeRegistry;
    }

    virtual WebFileUtilities* fileUtilities() OVERRIDE
    {
        return &m_mockFileUtilities;
    }

    virtual WebUnitTestSupport* unitTestSupport() OVERRIDE
    {
        return m_oldPlatform->unitTestSupport();
    }

    virtual void cryptographicallyRandomValues(unsigned char* buffer, size_t length) OVERRIDE
    {
        m_oldPlatform->cryptographicallyRandomValues(buffer, length);
    }

    virtual const unsigned char* getTraceCategoryEnabledFlag(const char* categoryName) OVERRIDE
    {
        return m_oldPlatform->getTraceCategoryEnabledFlag(categoryName);
    }

protected:
    WebBlobRegistry m_mockBlobRegistry;
    MockMimeRegistry m_mockMimeRegistry;
    MockFileUtilities m_mockFileUtilities;
    Platform* m_oldPlatform;
};

class DataObjectTest : public ::testing::Test {
public:
    DataObjectTest() { }

protected:
    virtual void SetUp() OVERRIDE
    {
        m_oldPlatform = Platform::current();
        m_mockPlatform = adoptPtr(new RegistryMockPlatform(m_oldPlatform));
        Platform::initialize(m_mockPlatform.get());

        m_dataObject = DataObject::create();
    }
    virtual void TearDown() OVERRIDE
    {
        // clear() invokes the File destructor, which uses WebBlobRegistry, so
        // clear() must be called before restoring the original Platform
        m_dataObject.clear();
        Heap::collectAllGarbage();
        Platform::initialize(m_oldPlatform);
    }

    RefPtrWillBePersistent<DataObject> m_dataObject;
    OwnPtr<RegistryMockPlatform> m_mockPlatform;
    Platform* m_oldPlatform;
};

TEST_F(DataObjectTest, addItemWithFilename)
{
    String filePath = Platform::current()->unitTestSupport()->webKitRootDir();
    filePath.append("/Source/core/clipboard/DataObjectTest.cpp");

    m_dataObject->addFilename(filePath, String());
    EXPECT_EQ(1U, m_dataObject->length());

    RefPtrWillBeRawPtr<DataObjectItem> item = m_dataObject->item(0);
    EXPECT_EQ(DataObjectItem::FileKind, item->kind());

    RefPtrWillBeRawPtr<Blob> blob = item->getAsFile();
    ASSERT_TRUE(blob->isFile());
    RefPtrWillBeRawPtr<File> file = toFile(blob.get());
    EXPECT_TRUE(file->hasBackingFile());
    EXPECT_EQ(File::IsUserVisible, file->userVisibility());
    EXPECT_EQ(filePath, file->path());
}

} // namespace blink
