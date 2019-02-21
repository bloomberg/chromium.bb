/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <windows.h>  // NOLINT
#include <blpwtk2_products.h>
#include <base/environment.h>
#include <content/public/app/sandbox_helper_win.h>  // for InitializeSandboxInfo
#include <sandbox/win/src/sandbox_types.h>  // for SandboxInterfaceInfo

std::string getSubProcessModuleName()
{
    char subProcessModuleEnvVar[64];
    sprintf_s(subProcessModuleEnvVar, sizeof(subProcessModuleEnvVar),
              "BLPWTK2_SUBPROCESS_%d_%d_%d_%d_%d",
              CHROMIUM_VERSION_MAJOR,
              CHROMIUM_VERSION_MINOR,
              CHROMIUM_VERSION_BUILD,
              CHROMIUM_VERSION_PATCH,
              BB_PATCH_NUMBER);
    std::string result;
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    if (!env->GetVar(subProcessModuleEnvVar, &result) || result.empty()) {
        return BLPWTK2_DLL_NAME;
    }
    return result;
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int)
{
    sandbox::SandboxInterfaceInfo sandboxInfo = {0};
    content::InitializeSandboxInfo(&sandboxInfo);
    {
        HMODULE subProcessModule = LoadLibraryA(getSubProcessModuleName().c_str());
        if (!subProcessModule) return -3456;
        typedef int (*MainFunc)(HINSTANCE hInstance,
                                sandbox::SandboxInterfaceInfo* sandboxInfo);
        MainFunc mainFunc = (MainFunc)GetProcAddress(subProcessModule, "SubProcessMain");
        if (!mainFunc) return -4567;

        return mainFunc(instance, &sandboxInfo);
    }
}


