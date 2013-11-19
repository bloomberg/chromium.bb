/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"

#include "URLTestHelpers.h"
#include "platform/SerializedResource.h"
#include "platform/SharedBuffer.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"
#include <gtest/gtest.h>

using namespace WebCore;
using namespace blink;

using blink::URLTestHelpers::toKURL;

namespace {

class LineReader {
public:
    LineReader(const std::string& text) : m_text(text), m_index(0) { }
    bool getNextLine(std::string* line)
    {
        line->clear();
        if (m_index >= m_text.length())
            return false;

        size_t endOfLineIndex = m_text.find("\r\n", m_index);
        if (endOfLineIndex == std::string::npos) {
            *line = m_text.substr(m_index);
            m_index = m_text.length();
        } else {
            *line = m_text.substr(m_index, endOfLineIndex - m_index);
            m_index = endOfLineIndex + 2;
        }
        return true;
    }

private:
    std::string m_text;
    size_t m_index;
};


class MHTMLTest : public testing::Test {
public:
    MHTMLTest()
    {
        m_filePath = Platform::current()->unitTestSupport()->webKitRootDir();
        m_filePath.append("/Source/web/tests/data/mhtml/");
    }

protected:
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }

    void addResource(const char* url, const char* mime, PassRefPtr<SharedBuffer> data)
    {
        SerializedResource resource(toKURL(url), mime, data);
        m_resources.append(resource);
    }

    void addResource(const char* url, const char* mime, const char* fileName)
    {
        addResource(url, mime, readFile(fileName));
    }

    void addTestResources()
    {
        addResource("http://www.test.com", "text/html", "css_test_page.html");
        addResource("http://www.test.com/link_styles.css", "text/css", "link_styles.css");
        addResource("http://www.test.com/import_style_from_link.css", "text/css", "import_style_from_link.css");
        addResource("http://www.test.com/import_styles.css", "text/css", "import_styles.css");
        addResource("http://www.test.com/red_background.png", "image/png", "red_background.png");
        addResource("http://www.test.com/orange_background.png", "image/png", "orange_background.png");
        addResource("http://www.test.com/yellow_background.png", "image/png", "yellow_background.png");
        addResource("http://www.test.com/green_background.png", "image/png", "green_background.png");
        addResource("http://www.test.com/blue_background.png", "image/png", "blue_background.png");
        addResource("http://www.test.com/purple_background.png", "image/png", "purple_background.png");
        addResource("http://www.test.com/ul-dot.png", "image/png", "ul-dot.png");
        addResource("http://www.test.com/ol-dot.png", "image/png", "ol-dot.png");
    }

    PassRefPtr<SharedBuffer> serialize(const char *title, const char *mime,  MHTMLArchive::EncodingPolicy encodingPolicy)
    {
        return MHTMLArchive::generateMHTMLData(m_resources, encodingPolicy, title, mime);
    }


private:

    PassRefPtr<SharedBuffer> readFile(const char* fileName)
    {
        String filePath = m_filePath + fileName;
        return Platform::current()->unitTestSupport()->readFromFile(filePath);
    }

    String m_filePath;
    Vector<SerializedResource> m_resources;
};

TEST_F(MHTMLTest, FAILS_TestMHTMLEncoding)
{
    addTestResources();
    RefPtr<SharedBuffer> data  = serialize("Test Serialization", "text/html", MHTMLArchive::UseDefaultEncoding);

    // Read the MHTML data line per line and do some pseudo-parsing to make sure the right encoding is used for the different sections.
    LineReader lineReader(std::string(data->data()));
    int sectionCheckedCount = 0;
    const char* expectedEncoding = 0;
    std::string line;
    while (lineReader.getNextLine(&line)) {
        if (!line.find("Content-Type:")) {
            ASSERT_FALSE(expectedEncoding);
            if (line.find("multipart/related;") != std::string::npos) {
                // Skip this one, it's part of the MHTML header.
                continue;
            }
            if (line.find("text/") != std::string::npos)
                expectedEncoding = "quoted-printable";
            else if (line.find("image/") != std::string::npos)
                expectedEncoding = "base64";
            else
                FAIL() << "Unexpected Content-Type: " << line;
            continue;
        }
        if (!line.find("Content-Transfer-Encoding:")) {
            ASSERT_TRUE(expectedEncoding);
            EXPECT_TRUE(line.find(expectedEncoding) != std::string::npos);
            expectedEncoding = 0;
            sectionCheckedCount++;
        }
    }
    EXPECT_EQ(12, sectionCheckedCount);
}
}
