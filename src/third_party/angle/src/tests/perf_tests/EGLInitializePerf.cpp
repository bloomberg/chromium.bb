//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLInitializePerfTest:
//   Performance test for device creation.
//

#include "ANGLEPerfTest.h"
#include "Timer.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/angle_test_instantiate.h"
#include "platform/Platform.h"

using namespace testing;

namespace
{
// Only applies to D3D11
struct Captures final : private angle::NonCopyable
{
    Timer *timer           = CreateTimer();
    size_t loadDLLsMS      = 0;
    size_t createDeviceMS  = 0;
    size_t initResourcesMS = 0;
};

double CapturePlatform_currentTime(angle::PlatformMethods *platformMethods)
{
    Captures *captures = static_cast<Captures *>(platformMethods->context);
    return captures->timer->getElapsedTime();
}

void CapturePlatform_histogramCustomCounts(angle::PlatformMethods *platformMethods,
                                           const char *name,
                                           int sample,
                                           int /*min*/,
                                           int /*max*/,
                                           int /*bucketCount*/)
{
    Captures *captures = static_cast<Captures *>(platformMethods->context);

    // These must match the names of the histograms.
    if (strcmp(name, "GPU.ANGLE.Renderer11InitializeDLLsMS") == 0)
    {
        captures->loadDLLsMS += static_cast<size_t>(sample);
    }
    // Note: not captured in debug, due to creating a debug device
    else if (strcmp(name, "GPU.ANGLE.D3D11CreateDeviceMS") == 0)
    {
        captures->createDeviceMS += static_cast<size_t>(sample);
    }
    else if (strcmp(name, "GPU.ANGLE.Renderer11InitializeDeviceMS") == 0)
    {
        captures->initResourcesMS += static_cast<size_t>(sample);
    }
}

class EGLInitializePerfTest : public ANGLEPerfTest,
                              public WithParamInterface<angle::PlatformParameters>
{
  public:
    EGLInitializePerfTest();
    ~EGLInitializePerfTest();

    void step() override;
    void SetUp() override;
    void TearDown() override;

  private:
    OSWindow *mOSWindow;
    EGLDisplay mDisplay;
    Captures mCaptures;
};

EGLInitializePerfTest::EGLInitializePerfTest()
    : ANGLEPerfTest("EGLInitialize", "_run"),
      mOSWindow(nullptr),
      mDisplay(EGL_NO_DISPLAY)
{
    auto platform = GetParam().eglParameters;

    std::vector<EGLint> displayAttributes;
    displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
    displayAttributes.push_back(platform.renderer);
    displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
    displayAttributes.push_back(platform.majorVersion);
    displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
    displayAttributes.push_back(platform.minorVersion);

    if (platform.renderer == EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE ||
        platform.renderer == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        displayAttributes.push_back(platform.deviceType);
    }
    displayAttributes.push_back(EGL_NONE);

    mOSWindow = CreateOSWindow();
    mOSWindow->initialize("EGLInitialize Test", 64, 64);

    auto eglGetPlatformDisplayEXT =
        reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (eglGetPlatformDisplayEXT == nullptr)
    {
        std::cerr << "Error getting platform display!" << std::endl;
        return;
    }

    mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                        reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                        &displayAttributes[0]);
}

void EGLInitializePerfTest::SetUp()
{
    ANGLEPerfTest::SetUp();

    angle::PlatformMethods *platformMethods = nullptr;
    ASSERT_TRUE(ANGLEGetDisplayPlatform(mDisplay, angle::g_PlatformMethodNames,
                                        angle::g_NumPlatformMethods, &mCaptures, &platformMethods));

    platformMethods->currentTime           = CapturePlatform_currentTime;
    platformMethods->histogramCustomCounts = CapturePlatform_histogramCustomCounts;
}

EGLInitializePerfTest::~EGLInitializePerfTest()
{
    SafeDelete(mOSWindow);
}

void EGLInitializePerfTest::step()
{
    ASSERT_NE(EGL_NO_DISPLAY, mDisplay);

    EGLint majorVersion, minorVersion;
    ASSERT_EQ(static_cast<EGLBoolean>(EGL_TRUE), eglInitialize(mDisplay, &majorVersion, &minorVersion));
    ASSERT_EQ(static_cast<EGLBoolean>(EGL_TRUE), eglTerminate(mDisplay));
}

void EGLInitializePerfTest::TearDown()
{
    ANGLEPerfTest::TearDown();
    printResult("LoadDLLs", normalizedTime(mCaptures.loadDLLsMS), "ms", true);
    printResult("D3D11CreateDevice", normalizedTime(mCaptures.createDeviceMS), "ms", true);
    printResult("InitResources", normalizedTime(mCaptures.initResourcesMS), "ms", true);

    ANGLEResetDisplayPlatform(mDisplay);
}

TEST_P(EGLInitializePerfTest, Run)
{
    run();
}

ANGLE_INSTANTIATE_TEST(EGLInitializePerfTest, angle::ES2_D3D11(), angle::ES2_VULKAN());

} // namespace
