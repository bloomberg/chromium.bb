//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils_winuwp.cpp: Implementation of OS-specific functions for Windows UWP

#include "system_utils.h"

#include <stdarg.h>
#include <windows.h>
#include <array>
#include <codecvt>
#include <locale>
#include <string>
#include <vector>

#include "common/debug.h"

namespace angle
{

bool SetEnvironmentVar(const char *variableName, const char *value)
{
    // Not supported for UWP
    return false;
}

std::string GetEnvironmentVar(const char *variableName)
{
    // Not supported for UWP
    return "";
}

const char *GetPathSeparatorForEnvironmentVar()
{
    return ";";
}

const char *GetSharedLibraryExtension()
{
    return "dll";
}

const char *GetExecutableExtension()
{
    return ".exe";
}

char GetPathSeparator()
{
    return '\\';
}

double GetCurrentTime()
{
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER curTime;
    QueryPerformanceCounter(&curTime);

    return static_cast<double>(curTime.QuadPart) / frequency.QuadPart;
}

class UwpLibrary : public Library
{
  public:
    UwpLibrary(const char *libraryName, SearchType searchType)
    {
        char buffer[MAX_PATH];
        int ret = snprintf(buffer, MAX_PATH, "%s.%s", libraryName, GetSharedLibraryExtension());

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wideBuffer = converter.from_bytes(buffer);

        if (ret > 0 && ret < MAX_PATH)
        {
            switch (searchType)
            {
                case SearchType::ApplicationDir:
                    mModule = LoadPackagedLibrary(wideBuffer.c_str(), 0);
                    break;
                case SearchType::SystemDir:
                    // Not supported in UWP
                    break;
            }
        }
    }

    ~UwpLibrary() override
    {
        if (mModule)
        {
            FreeLibrary(mModule);
        }
    }

    void *getSymbol(const char *symbolName) override
    {
        if (!mModule)
        {
            return nullptr;
        }

        return reinterpret_cast<void *>(GetProcAddress(mModule, symbolName));
    }

    void *getNative() const override { return reinterpret_cast<void *>(mModule); }

  private:
    HMODULE mModule = nullptr;
};

Library *OpenSharedLibrary(const char *libraryName, SearchType searchType)
{
    return new UwpLibrary(libraryName, searchType);
}

bool IsDebuggerAttached()
{
    return !!::IsDebuggerPresent();
}

void BreakDebugger()
{
    __debugbreak();
}
}  // namespace angle
