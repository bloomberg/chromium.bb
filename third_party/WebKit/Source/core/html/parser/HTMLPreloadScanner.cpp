/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/parser/HTMLPreloadScanner.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/html/LinkRelAttribute.h"
#include "core/html/forms/InputTypeNames.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/HTMLSrcsetParser.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "core/platform/chromium/TraceEvent.h"
#include "wtf/MainThread.h"

namespace WebCore {

using namespace HTMLNames;

static bool match(const StringImpl* impl, const QualifiedName& qName)
{
    return impl == qName.localName().impl();
}

static bool match(const HTMLIdentifier& name, const QualifiedName& qName)
{
    return match(name.asStringImpl(), qName);
}

static bool match(const AtomicString& name, const QualifiedName& qName)
{
    ASSERT(isMainThread());
    return qName.localName() == name;
}

static const StringImpl* tagImplFor(const HTMLToken::DataVector& data)
{
    AtomicString tagName(data);
    const StringImpl* result = tagName.impl();
    if (result->isStatic())
        return result;
    return 0;
}

static const StringImpl* tagImplFor(const HTMLIdentifier& tagName)
{
    const StringImpl* result = tagName.asStringImpl();
    if (result->isStatic())
        return result;
    return 0;
}

static String initiatorFor(const StringImpl* tagImpl)
{
    ASSERT(tagImpl);
    if (match(tagImpl, imgTag))
        return imgTag.localName();
    if (match(tagImpl, inputTag))
        return inputTag.localName();
    if (match(tagImpl, linkTag))
        return linkTag.localName();
    if (match(tagImpl, scriptTag))
        return scriptTag.localName();
    ASSERT_NOT_REACHED();
    return emptyString();
}

class TokenPreloadScanner::StartTagScanner {
public:
    StartTagScanner(const StringImpl* tagImpl, float deviceScaleFactor)
        : m_tagImpl(tagImpl)
        , m_linkIsStyleSheet(false)
        , m_inputIsImage(false)
        , m_deviceScaleFactor(deviceScaleFactor)
        , m_encounteredImgSrc(false)
    {
        if (!match(m_tagImpl, imgTag)
            && !match(m_tagImpl, inputTag)
            && !match(m_tagImpl, linkTag)
            && !match(m_tagImpl, scriptTag))
            m_tagImpl = 0;
    }

    enum URLReplacement {
        AllowURLReplacement,
        DisallowURLReplacement
    };

    void processAttributes(const HTMLToken::AttributeList& attributes)
    {
        ASSERT(isMainThread());
        if (!m_tagImpl)
            return;
        for (HTMLToken::AttributeList::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter) {
            AtomicString attributeName(iter->name);
            String attributeValue = StringImpl::create8BitIfPossible(iter->value);
            processAttribute(attributeName, attributeValue);
        }
    }

    void processAttributes(const Vector<CompactHTMLToken::Attribute>& attributes)
    {
        if (!m_tagImpl)
            return;
        for (Vector<CompactHTMLToken::Attribute>::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
            processAttribute(iter->name, iter->value);
    }

    PassOwnPtr<PreloadRequest> createPreloadRequest(const KURL& predictedBaseURL, const SegmentedString& source)
    {
        if (!shouldPreload())
            return nullptr;

        TRACE_EVENT_INSTANT1("net", "PreloadRequest", "url", m_urlToLoad.ascii());
        TextPosition position = TextPosition(source.currentLine(), source.currentColumn());
        OwnPtr<PreloadRequest> request = PreloadRequest::create(initiatorFor(m_tagImpl), position, m_urlToLoad, predictedBaseURL, resourceType(), m_mediaAttribute);
        request->setCrossOriginModeAllowsCookies(crossOriginModeAllowsCookies());
        request->setCharset(charset());
        return request.release();
    }

private:
    template<typename NameType>
    void processAttribute(const NameType& attributeName, const String& attributeValue)
    {
        if (match(attributeName, charsetAttr))
            m_charset = attributeValue;

        if (match(m_tagImpl, scriptTag)) {
            if (match(attributeName, srcAttr))
                setUrlToLoad(attributeValue, DisallowURLReplacement);
            else if (match(attributeName, crossoriginAttr) && !attributeValue.isNull())
                m_crossOriginMode = stripLeadingAndTrailingHTMLSpaces(attributeValue);
        } else if (match(m_tagImpl, imgTag)) {
            if (match(attributeName, srcAttr) && !m_encounteredImgSrc) {
                m_encounteredImgSrc = true;
                setUrlToLoad(bestFitSourceForImageAttributes(m_deviceScaleFactor, attributeValue, m_srcsetImageCandidate), AllowURLReplacement);
            } else if (match(attributeName, crossoriginAttr) && !attributeValue.isNull()) {
                m_crossOriginMode = stripLeadingAndTrailingHTMLSpaces(attributeValue);
            } else if (RuntimeEnabledFeatures::srcsetEnabled()
                && match(attributeName, srcsetAttr)
                && m_srcsetImageCandidate.isEmpty()) {
                m_srcsetImageCandidate = bestFitSourceForSrcsetAttribute(m_deviceScaleFactor, attributeValue);
                setUrlToLoad(bestFitSourceForImageAttributes(m_deviceScaleFactor, m_urlToLoad, m_srcsetImageCandidate), AllowURLReplacement);
            }
        } else if (match(m_tagImpl, linkTag)) {
            if (match(attributeName, hrefAttr))
                setUrlToLoad(attributeValue, DisallowURLReplacement);
            else if (match(attributeName, relAttr))
                m_linkIsStyleSheet = relAttributeIsStyleSheet(attributeValue);
            else if (match(attributeName, mediaAttr))
                m_mediaAttribute = attributeValue;
        } else if (match(m_tagImpl, inputTag)) {
            if (match(attributeName, srcAttr))
                setUrlToLoad(attributeValue, DisallowURLReplacement);
            else if (match(attributeName, typeAttr))
                m_inputIsImage = equalIgnoringCase(attributeValue, InputTypeNames::image());
        }
    }

    static bool relAttributeIsStyleSheet(const String& attributeValue)
    {
        LinkRelAttribute rel(attributeValue);
        return rel.isStyleSheet() && !rel.isAlternate() && rel.iconType() == InvalidIcon && !rel.isDNSPrefetch();
    }

    void setUrlToLoad(const String& value, URLReplacement replacement)
    {
        // We only respect the first src/href, per HTML5:
        // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#attribute-name-state
        if (replacement == DisallowURLReplacement && !m_urlToLoad.isEmpty())
            return;
        String url = stripLeadingAndTrailingHTMLSpaces(value);
        if (url.isEmpty())
            return;
        m_urlToLoad = url;
    }

    const String& charset() const
    {
        // FIXME: Its not clear that this if is needed, the loader probably ignores charset for image requests anyway.
        if (match(m_tagImpl, imgTag))
            return emptyString();
        return m_charset;
    }

    Resource::Type resourceType() const
    {
        if (match(m_tagImpl, scriptTag))
            return Resource::Script;
        if (match(m_tagImpl, imgTag) || (match(m_tagImpl, inputTag) && m_inputIsImage))
            return Resource::Image;
        if (match(m_tagImpl, linkTag) && m_linkIsStyleSheet)
            return Resource::CSSStyleSheet;
        ASSERT_NOT_REACHED();
        return Resource::Raw;
    }

    bool shouldPreload()
    {
        if (m_urlToLoad.isEmpty())
            return false;
        if (match(m_tagImpl, linkTag) && !m_linkIsStyleSheet)
            return false;
        if (match(m_tagImpl, inputTag) && !m_inputIsImage)
            return false;
        return true;
    }

    bool crossOriginModeAllowsCookies()
    {
        return m_crossOriginMode.isNull() || equalIgnoringCase(m_crossOriginMode, "use-credentials");
    }

    const StringImpl* m_tagImpl;
    String m_urlToLoad;
    ImageCandidate m_srcsetImageCandidate;
    String m_charset;
    String m_crossOriginMode;
    bool m_linkIsStyleSheet;
    String m_mediaAttribute;
    bool m_inputIsImage;
    float m_deviceScaleFactor;
    bool m_encounteredImgSrc;
};

TokenPreloadScanner::TokenPreloadScanner(const KURL& documentURL, float deviceScaleFactor)
    : m_documentURL(documentURL)
    , m_inStyle(false)
    , m_deviceScaleFactor(deviceScaleFactor)
    , m_templateCount(0)
{
}

TokenPreloadScanner::~TokenPreloadScanner()
{
}

TokenPreloadScannerCheckpoint TokenPreloadScanner::createCheckpoint()
{
    TokenPreloadScannerCheckpoint checkpoint = m_checkpoints.size();
    m_checkpoints.append(Checkpoint(m_predictedBaseElementURL, m_inStyle, m_templateCount));
    return checkpoint;
}

void TokenPreloadScanner::rewindTo(TokenPreloadScannerCheckpoint checkpointIndex)
{
    ASSERT(checkpointIndex < m_checkpoints.size()); // If this ASSERT fires, checkpointIndex is invalid.
    const Checkpoint& checkpoint = m_checkpoints[checkpointIndex];
    m_predictedBaseElementURL = checkpoint.predictedBaseElementURL;
    m_inStyle = checkpoint.inStyle;
    m_templateCount = checkpoint.templateCount;
    m_cssScanner.reset();
    m_checkpoints.clear();
}

void TokenPreloadScanner::scan(const HTMLToken& token, const SegmentedString& source, PreloadRequestStream& requests)
{
    scanCommon(token, source, requests);
}

void TokenPreloadScanner::scan(const CompactHTMLToken& token, const SegmentedString& source, PreloadRequestStream& requests)
{
    scanCommon(token, source, requests);
}

template<typename Token>
void TokenPreloadScanner::scanCommon(const Token& token, const SegmentedString& source, PreloadRequestStream& requests)
{
    switch (token.type()) {
    case HTMLToken::Character: {
        if (!m_inStyle)
            return;
        m_cssScanner.scan(token.data(), source, requests);
        return;
    }
    case HTMLToken::EndTag: {
        const StringImpl* tagImpl = tagImplFor(token.data());
        if (match(tagImpl, templateTag)) {
            if (m_templateCount)
                --m_templateCount;
            return;
        }
        if (match(tagImpl, styleTag)) {
            if (m_inStyle)
                m_cssScanner.reset();
            m_inStyle = false;
        }
        return;
    }
    case HTMLToken::StartTag: {
        if (m_templateCount)
            return;
        const StringImpl* tagImpl = tagImplFor(token.data());
        if (match(tagImpl, templateTag)) {
            ++m_templateCount;
            return;
        }
        if (match(tagImpl, styleTag)) {
            m_inStyle = true;
            return;
        }
        if (match(tagImpl, baseTag)) {
            // The first <base> element is the one that wins.
            if (!m_predictedBaseElementURL.isEmpty())
                return;
            updatePredictedBaseURL(token);
            return;
        }

        StartTagScanner scanner(tagImpl, m_deviceScaleFactor);
        scanner.processAttributes(token.attributes());
        OwnPtr<PreloadRequest> request = scanner.createPreloadRequest(m_predictedBaseElementURL, source);
        if (request)
            requests.append(request.release());
        return;
    }
    default: {
        return;
    }
    }
}

template<typename Token>
void TokenPreloadScanner::updatePredictedBaseURL(const Token& token)
{
    ASSERT(m_predictedBaseElementURL.isEmpty());
    if (const typename Token::Attribute* hrefAttribute = token.getAttributeItem(hrefAttr))
        m_predictedBaseElementURL = KURL(m_documentURL, stripLeadingAndTrailingHTMLSpaces(hrefAttribute->value)).copy();
}

HTMLPreloadScanner::HTMLPreloadScanner(const HTMLParserOptions& options, const KURL& documentURL, float deviceScaleFactor)
    : m_scanner(documentURL, deviceScaleFactor)
    , m_tokenizer(HTMLTokenizer::create(options))
{
}

HTMLPreloadScanner::~HTMLPreloadScanner()
{
}

void HTMLPreloadScanner::appendToEnd(const SegmentedString& source)
{
    m_source.append(source);
}

void HTMLPreloadScanner::scan(HTMLResourcePreloader* preloader, const KURL& startingBaseElementURL)
{
    ASSERT(isMainThread()); // HTMLTokenizer::updateStateFor only works on the main thread.

    // When we start scanning, our best prediction of the baseElementURL is the real one!
    if (!startingBaseElementURL.isEmpty())
        m_scanner.setPredictedBaseElementURL(startingBaseElementURL);

    PreloadRequestStream requests;

    while (m_tokenizer->nextToken(m_source, m_token)) {
        if (m_token.type() == HTMLToken::StartTag)
            m_tokenizer->updateStateFor(AtomicString(m_token.name()));
        m_scanner.scan(m_token, m_source, requests);
        m_token.clear();
    }

    preloader->takeAndPreload(requests);
}

}
