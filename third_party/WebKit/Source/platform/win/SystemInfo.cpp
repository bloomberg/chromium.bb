/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
#include "platform/win/SystemInfo.h"

#include <windows.h>

namespace WebCore {

WindowsVersion windowsVersion(int* major, int* minor)
{
    static bool initialized = false;
    static WindowsVersion version;
    static int majorVersion, minorVersion;

    if (!initialized) {
        initialized = true;
        OSVERSIONINFOEX versionInfo;
        ZeroMemory(&versionInfo, sizeof(versionInfo));
        versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
        GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&versionInfo));
        majorVersion = versionInfo.dwMajorVersion;
        minorVersion = versionInfo.dwMinorVersion;

        if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32s) {
            version = Windows3_1;
        } else if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
            if (!minorVersion)
                version = Windows95;
            else
                version = (minorVersion == 10) ? Windows98 : WindowsME;
        } else {
            if (majorVersion == 5) {
                if (!minorVersion)
                    version = Windows2000;
                else
                    version = (minorVersion == 1) ? WindowsXP : WindowsServer2003;
            } else if (majorVersion >= 6) {
                if (versionInfo.wProductType == VER_NT_WORKSTATION)
                    version = (majorVersion == 6 && !minorVersion) ? WindowsVista : Windows7;
                else
                    version = WindowsServer2008;
            } else {
                version = (majorVersion == 4) ? WindowsNT4 : WindowsNT3;
            }
        }
    }

    if (major)
        *major = majorVersion;
    if (minor)
        *minor = minorVersion;
    return version;
}

} // namespace WebCore
