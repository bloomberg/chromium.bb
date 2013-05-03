/*
 * Copyright (C) 2004, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "core/platform/KURL.h"

#include <stdio.h>
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/PlatformMemoryInstrumentation.h"
#include "core/platform/text/DecodeEscapeSequences.h"
#include "core/platform/text/TextEncoding.h"
#include <unicode/uidna.h>
#include <wtf/HashMap.h>
#include <wtf/HexNumber.h>
#include <wtf/MemoryInstrumentationString.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

// FIXME: This file makes too much use of the + operator on String.
// We either have to optimize that operator so it doesn't involve
// so many allocations, or change this to use StringBuffer instead.

using namespace std;
using namespace WTF;

namespace WebCore {

typedef Vector<char, 512> CharBuffer;
typedef Vector<UChar, 512> UCharBuffer;

static const unsigned maximumValidPortNumber = 0xFFFE;
static const unsigned invalidPortNumber = 0xFFFF;

static inline bool isLetterMatchIgnoringCase(UChar character, char lowercaseLetter)
{
    ASSERT(isASCIILower(lowercaseLetter));
    return (character | 0x20) == lowercaseLetter;
}

String KURL::strippedForUseAsReferrer() const
{
    KURL referrer(*this);
    referrer.setUser(String());
    referrer.setPass(String());
    referrer.removeFragmentIdentifier();
    return referrer.string();
}

bool KURL::isLocalFile() const
{
    // Including feed here might be a bad idea since drag and drop uses this check
    // and including feed would allow feeds to potentially let someone's blog
    // read the contents of the clipboard on a drag, even without a drop.
    // Likewise with using the FrameLoader::shouldTreatURLAsLocal() function.
    return protocolIs("file");
}

bool protocolIsJavaScript(const String& url)
{
    return protocolIs(url, "javascript");
}

const KURL& blankURL()
{
    DEFINE_STATIC_LOCAL(KURL, staticBlankURL, (ParsedURLString, "about:blank"));
    return staticBlankURL;
}

bool KURL::isBlankURL() const
{
    return protocolIs("about");
}

bool isDefaultPortForProtocol(unsigned short port, const String& protocol)
{
    if (protocol.isEmpty())
        return false;

    typedef HashMap<String, unsigned, CaseFoldingHash> DefaultPortsMap;
    DEFINE_STATIC_LOCAL(DefaultPortsMap, defaultPorts, ());
    if (defaultPorts.isEmpty()) {
        defaultPorts.set("http", 80);
        defaultPorts.set("https", 443);
        defaultPorts.set("ftp", 21);
        defaultPorts.set("ftps", 990);
    }
    return defaultPorts.get(protocol) == port;
}

bool portAllowed(const KURL& url)
{
    unsigned short port = url.port();

    // Since most URLs don't have a port, return early for the "no port" case.
    if (!port)
        return true;

    // This blocked port list matches the port blocking that Mozilla implements.
    // See http://www.mozilla.org/projects/netlib/PortBanning.html for more information.
    static const unsigned short blockedPortList[] = {
        1,    // tcpmux
        7,    // echo
        9,    // discard
        11,   // systat
        13,   // daytime
        15,   // netstat
        17,   // qotd
        19,   // chargen
        20,   // FTP-data
        21,   // FTP-control
        22,   // SSH
        23,   // telnet
        25,   // SMTP
        37,   // time
        42,   // name
        43,   // nicname
        53,   // domain
        77,   // priv-rjs
        79,   // finger
        87,   // ttylink
        95,   // supdup
        101,  // hostriame
        102,  // iso-tsap
        103,  // gppitnp
        104,  // acr-nema
        109,  // POP2
        110,  // POP3
        111,  // sunrpc
        113,  // auth
        115,  // SFTP
        117,  // uucp-path
        119,  // nntp
        123,  // NTP
        135,  // loc-srv / epmap
        139,  // netbios
        143,  // IMAP2
        179,  // BGP
        389,  // LDAP
        465,  // SMTP+SSL
        512,  // print / exec
        513,  // login
        514,  // shell
        515,  // printer
        526,  // tempo
        530,  // courier
        531,  // Chat
        532,  // netnews
        540,  // UUCP
        556,  // remotefs
        563,  // NNTP+SSL
        587,  // ESMTP
        601,  // syslog-conn
        636,  // LDAP+SSL
        993,  // IMAP+SSL
        995,  // POP3+SSL
        2049, // NFS
        3659, // apple-sasl / PasswordServer [Apple addition]
        4045, // lockd
        6000, // X11
        6665, // Alternate IRC [Apple addition]
        6666, // Alternate IRC [Apple addition]
        6667, // Standard IRC [Apple addition]
        6668, // Alternate IRC [Apple addition]
        6669, // Alternate IRC [Apple addition]
        invalidPortNumber, // Used to block all invalid port numbers
    };
    const unsigned short* const blockedPortListEnd = blockedPortList + WTF_ARRAY_LENGTH(blockedPortList);

#ifndef NDEBUG
    // The port list must be sorted for binary_search to work.
    static bool checkedPortList = false;
    if (!checkedPortList) {
        for (const unsigned short* p = blockedPortList; p != blockedPortListEnd - 1; ++p)
            ASSERT(*p < *(p + 1));
        checkedPortList = true;
    }
#endif

    // If the port is not in the blocked port list, allow it.
    if (!binary_search(blockedPortList, blockedPortListEnd, port))
        return true;

    // Allow ports 21 and 22 for FTP URLs, as Mozilla does.
    if ((port == 21 || port == 22) && url.protocolIs("ftp"))
        return true;

    // Allow any port number in a file URL, since the port number is ignored.
    if (url.protocolIs("file"))
        return true;

    return false;
}

String mimeTypeFromDataURL(const String& url)
{
    ASSERT(protocolIs(url, "data"));
    size_t index = url.find(';');
    if (index == notFound)
        index = url.find(',');
    if (index != notFound) {
        if (index > 5)
            return url.substring(5, index - 5).lower();
        return "text/plain"; // Data URLs with no MIME type are considered text/plain.
    }
    return "";
}

String mimeTypeFromURL(const KURL& url)
{
    String decodedPath = decodeURLEscapeSequences(url.path());
    String extension = decodedPath.substring(decodedPath.reverseFind('.') + 1);

    // We don't use MIMETypeRegistry::getMIMETypeForPath() because it returns "application/octet-stream" upon failure
    return MIMETypeRegistry::getMIMETypeForExtension(extension);
}

void KURL::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this);
    info.addMember(m_url, "url");
}

bool KURL::isSafeToSendToAnotherThread() const
{
    return m_url.isSafeToSendToAnotherThread();
}

String KURL::elidedString() const
{
    if (string().length() <= 1024)
        return string();

    return string().left(511) + "..." + string().right(510);
}

}
