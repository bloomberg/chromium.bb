/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FileSystem_h
#define FileSystem_h

#include <time.h>
#include <wtf/Forward.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// PlatformFileHandle
#if OS(WINDOWS)
typedef void *HANDLE;
typedef HANDLE PlatformFileHandle;
// FIXME: -1 is INVALID_HANDLE_VALUE, defined in <winbase.h>. Chromium tries to
// avoid using Windows headers in headers.  We'd rather move this into the .cpp.
const PlatformFileHandle invalidPlatformFileHandle = reinterpret_cast<HANDLE>(-1);
#else
typedef int PlatformFileHandle;
const PlatformFileHandle invalidPlatformFileHandle = -1;
#endif

enum FileOpenMode {
    OpenForRead = 0,
    OpenForWrite
};

enum FileSeekOrigin {
    SeekFromBeginning = 0,
    SeekFromCurrent,
    SeekFromEnd
};

#if OS(WINDOWS)
static const char PlatformFilePathSeparator = '\\';
#else
static const char PlatformFilePathSeparator = '/';
#endif

struct FileMetadata;

bool fileExists(const String&);
bool deleteFile(const String&);
bool deleteEmptyDirectory(const String&);
bool getFileSize(const String&, long long& result);
bool getFileModificationTime(const String&, time_t& result);
bool getFileMetadata(const String&, FileMetadata&);
String pathByAppendingComponent(const String& path, const String& component);
bool makeAllDirectories(const String& path);
String pathGetFileName(const String&);
String directoryName(const String&);

Vector<String> listDirectory(const String& path, const String& filter = String());

inline bool isHandleValid(const PlatformFileHandle& handle) { return handle != invalidPlatformFileHandle; }

inline double invalidFileTime() { return std::numeric_limits<double>::quiet_NaN(); }
inline bool isValidFileTime(double time) { return std::isfinite(time); }

PlatformFileHandle openFile(const String& path, FileOpenMode);
void closeFile(PlatformFileHandle&);
// Returns the resulting offset from the beginning of the file if successful, -1 otherwise.
long long seekFile(PlatformFileHandle, long long offset, FileSeekOrigin);
bool truncateFile(PlatformFileHandle, long long offset);
// Returns number of bytes actually read if successful, -1 otherwise.
int writeToFile(PlatformFileHandle, const char* data, int length);
// Returns number of bytes actually written if successful, -1 otherwise.
int readFromFile(PlatformFileHandle, char* data, int length);

// Encode a string for use within a file name.
String encodeForFileName(const String&);

} // namespace WebCore

#endif // FileSystem_h
