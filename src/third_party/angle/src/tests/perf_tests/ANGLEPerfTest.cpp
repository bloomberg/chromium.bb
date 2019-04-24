//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLEPerfTests:
//   Base class for google test performance tests
//

#include "ANGLEPerfTest.h"

#include "common/platform.h"
#include "third_party/perf/perf_test.h"
#include "util/shader_utils.h"
#include "util/system_utils.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

#include <json/json.h>

#if defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
#    include "util/windows/WGLWindow.h"
#endif  // defined(ANGLE_USE_UTIL_LOADER) &&defined(ANGLE_PLATFORM_WINDOWS)

namespace
{
constexpr size_t kInitialTraceEventBufferSize = 50000;
constexpr double kMicroSecondsPerSecond       = 1e6;
constexpr double kNanoSecondsPerSecond        = 1e9;
constexpr double kCalibrationRunTimeSeconds   = 1.0;
constexpr double kMaximumRunTimeSeconds       = 10.0;
constexpr unsigned int kNumTrials             = 3;

bool gCalibration = false;
Optional<unsigned int> gStepsToRunOverride;
bool gEnableTrace      = false;
const char *gTraceFile = "ANGLETrace.json";

struct TraceCategory
{
    unsigned char enabled;
    const char *name;
};

constexpr TraceCategory gTraceCategories[2] = {
    {1, "gpu.angle"},
    {1, "gpu.angle.gpu"},
};

void EmptyPlatformMethod(angle::PlatformMethods *, const char *) {}

void OverrideWorkaroundsD3D(angle::PlatformMethods *platform, angle::WorkaroundsD3D *workaroundsD3D)
{
    auto *angleRenderTest = static_cast<ANGLERenderTest *>(platform->context);
    angleRenderTest->overrideWorkaroundsD3D(workaroundsD3D);
}

angle::TraceEventHandle AddTraceEvent(angle::PlatformMethods *platform,
                                      char phase,
                                      const unsigned char *categoryEnabledFlag,
                                      const char *name,
                                      unsigned long long id,
                                      double timestamp,
                                      int numArgs,
                                      const char **argNames,
                                      const unsigned char *argTypes,
                                      const unsigned long long *argValues,
                                      unsigned char flags)
{
    if (!gEnableTrace)
        return 0;

    // Discover the category name based on categoryEnabledFlag.  This flag comes from the first
    // parameter of TraceCategory, and corresponds to one of the entries in gTraceCategories.
    static_assert(offsetof(TraceCategory, enabled) == 0,
                  "|enabled| must be the first field of the TraceCategory class.");
    const TraceCategory *category = reinterpret_cast<const TraceCategory *>(categoryEnabledFlag);

    ANGLERenderTest *renderTest     = static_cast<ANGLERenderTest *>(platform->context);
    std::vector<TraceEvent> &buffer = renderTest->getTraceEventBuffer();
    buffer.emplace_back(phase, category->name, name, timestamp);
    return buffer.size();
}

const unsigned char *GetTraceCategoryEnabledFlag(angle::PlatformMethods *platform,
                                                 const char *categoryName)
{
    if (gEnableTrace)
    {
        for (const TraceCategory &category : gTraceCategories)
        {
            if (strcmp(category.name, categoryName) == 0)
            {
                return &category.enabled;
            }
        }
    }

    constexpr static unsigned char kZero = 0;
    return &kZero;
}

void UpdateTraceEventDuration(angle::PlatformMethods *platform,
                              const unsigned char *categoryEnabledFlag,
                              const char *name,
                              angle::TraceEventHandle eventHandle)
{
    // Not implemented.
}

double MonotonicallyIncreasingTime(angle::PlatformMethods *platform)
{
    ANGLERenderTest *renderTest = static_cast<ANGLERenderTest *>(platform->context);
    // Move the time origin to the first call to this function, to avoid generating unnecessarily
    // large timestamps.
    static double origin = renderTest->getTimer()->getAbsoluteTime();
    return renderTest->getTimer()->getAbsoluteTime() - origin;
}

void DumpTraceEventsToJSONFile(const std::vector<TraceEvent> &traceEvents,
                               const char *outputFileName)
{
    Json::Value eventsValue(Json::arrayValue);

    for (const TraceEvent &traceEvent : traceEvents)
    {
        Json::Value value(Json::objectValue);

        std::stringstream phaseName;
        phaseName << traceEvent.phase;

        unsigned long long microseconds =
            static_cast<unsigned long long>(traceEvent.timestamp * 1000.0 * 1000.0);

        value["name"] = traceEvent.name;
        value["cat"]  = traceEvent.categoryName;
        value["ph"]   = phaseName.str();
        value["ts"]   = microseconds;
        value["pid"]  = "ANGLE";
        value["tid"]  = strcmp(traceEvent.categoryName, "gpu.angle.gpu") == 0 ? "GPU" : "CPU";

        eventsValue.append(value);
    }

    Json::Value root(Json::objectValue);
    root["traceEvents"] = eventsValue;

    std::ofstream outFile;
    outFile.open(outputFileName);

    Json::StyledWriter styledWrite;
    outFile << styledWrite.write(root);

    outFile.close();
}

bool OneFrame()
{
    return gStepsToRunOverride.valid() && gStepsToRunOverride.value() == 1;
}
}  // anonymous namespace

ANGLEPerfTest::ANGLEPerfTest(const std::string &name,
                             const std::string &suffix,
                             unsigned int iterationsPerStep)
    : mName(name),
      mSuffix(suffix),
      mTimer(CreateTimer()),
      mGPUTimeNs(0),
      mSkipTest(false),
      mStepsToRun(std::numeric_limits<unsigned int>::max()),
      mNumStepsPerformed(0),
      mIterationsPerStep(iterationsPerStep),
      mRunning(true)
{}

ANGLEPerfTest::~ANGLEPerfTest()
{
    SafeDelete(mTimer);
}

void ANGLEPerfTest::run()
{
    if (mSkipTest)
    {
        return;
    }

    // Calibrate to a fixed number of steps during an initial set time.
    if (!gStepsToRunOverride.valid())
    {
        doRunLoop(kCalibrationRunTimeSeconds);

        // Scale steps down according to the time that exeeded one second.
        double scale = kCalibrationRunTimeSeconds / mTimer->getElapsedTime();
        mStepsToRun  = static_cast<size_t>(static_cast<double>(mNumStepsPerformed) * scale);

        // Calibration allows the perf test runner script to save some time.
        if (gCalibration)
        {
            printResult("steps", static_cast<size_t>(mStepsToRun), "count", false);
            return;
        }
    }
    else
    {
        mStepsToRun = gStepsToRunOverride.value();
    }

    // Do another warmup run. Seems to consistently improve results.
    doRunLoop(kMaximumRunTimeSeconds);

    double totalTime = 0.0;
    for (unsigned int trial = 0; trial < kNumTrials; ++trial)
    {
        doRunLoop(kMaximumRunTimeSeconds);
        totalTime += printResults();
    }
    double average = totalTime / kNumTrials;
    std::ostringstream averageString;
    averageString << "for " << kNumTrials << " runs";
    printResult("average", average, averageString.str(), false);
}

void ANGLEPerfTest::doRunLoop(double maxRunTime)
{
    mNumStepsPerformed = 0;
    mRunning           = true;
    mTimer->start();
    startTest();

    while (mRunning)
    {
        step();
        if (mRunning)
        {
            ++mNumStepsPerformed;
            if (mTimer->getElapsedTime() > maxRunTime)
            {
                mRunning = false;
            }
            else if (mNumStepsPerformed >= mStepsToRun)
            {
                mRunning = false;
            }
        }
    }
    finishTest();
    mTimer->stop();
}

void ANGLEPerfTest::printResult(const std::string &trace,
                                double value,
                                const std::string &units,
                                bool important) const
{
    perf_test::PrintResult(mName, mSuffix, trace, value, units, important);
}

void ANGLEPerfTest::printResult(const std::string &trace,
                                size_t value,
                                const std::string &units,
                                bool important) const
{
    perf_test::PrintResult(mName, mSuffix, trace, value, units, important);
}

void ANGLEPerfTest::SetUp() {}

void ANGLEPerfTest::TearDown() {}

double ANGLEPerfTest::printResults()
{
    double elapsedTimeSeconds[2] = {
        mTimer->getElapsedTime(),
        mGPUTimeNs * 1e-9,
    };

    const char *clockNames[2] = {
        "wall_time",
        "gpu_time",
    };

    // If measured gpu time is non-zero, print that too.
    size_t clocksToOutput = mGPUTimeNs > 0 ? 2 : 1;

    double retValue = 0.0;
    for (size_t i = 0; i < clocksToOutput; ++i)
    {
        double secondsPerStep = elapsedTimeSeconds[i] / static_cast<double>(mNumStepsPerformed);
        double secondsPerIteration = secondsPerStep / static_cast<double>(mIterationsPerStep);

        // Give the result a different name to ensure separate graphs if we transition.
        if (secondsPerIteration > 1e-3)
        {
            double microSecondsPerIteration = secondsPerIteration * kMicroSecondsPerSecond;
            retValue                        = microSecondsPerIteration;
            printResult(clockNames[i], microSecondsPerIteration, "us", true);
        }
        else
        {
            double nanoSecPerIteration = secondsPerIteration * kNanoSecondsPerSecond;
            retValue                   = nanoSecPerIteration;
            printResult(clockNames[i], nanoSecPerIteration, "ns", true);
        }
    }
    return retValue;
}

double ANGLEPerfTest::normalizedTime(size_t value) const
{
    return static_cast<double>(value) / static_cast<double>(mNumStepsPerformed);
}

std::string RenderTestParams::suffix() const
{
    switch (driver)
    {
        case angle::GLESDriverType::AngleEGL:
            break;
        case angle::GLESDriverType::SystemEGL:
            return "_native";
        case angle::GLESDriverType::SystemWGL:
            return "_wgl";
        default:
            assert(0);
            return "_unk";
    }

    switch (getRenderer())
    {
        case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
            return "_d3d11";
        case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
            return "_d3d9";
        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
            return "_gl";
        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
            return "_gles";
        case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
            return "_default";
        case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
            return "_vulkan";
        default:
            assert(0);
            return "_unk";
    }
}

ANGLERenderTest::ANGLERenderTest(const std::string &name, const RenderTestParams &testParams)
    : ANGLEPerfTest(name, testParams.suffix(), OneFrame() ? 1 : testParams.iterationsPerStep),
      mTestParams(testParams),
      mGLWindow(nullptr),
      mOSWindow(nullptr)
{
    // Force fast tests to make sure our slowest bots don't time out.
    if (OneFrame())
    {
        const_cast<RenderTestParams &>(testParams).iterationsPerStep = 1;
    }

    // Try to ensure we don't trigger allocation during execution.
    mTraceEventBuffer.reserve(kInitialTraceEventBufferSize);

    switch (testParams.driver)
    {
        case angle::GLESDriverType::AngleEGL:
            mGLWindow = createEGLWindow(testParams);
            mEntryPointsLib.reset(angle::OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME));
            break;
        case angle::GLESDriverType::SystemEGL:
            std::cerr << "Not implemented." << std::endl;
            mSkipTest = true;
            break;
        case angle::GLESDriverType::SystemWGL:
#if defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
            mGLWindow = WGLWindow::New(testParams.majorVersion, testParams.minorVersion);
            mEntryPointsLib.reset(angle::OpenSharedLibrary("opengl32"));
#else
            std::cout << "WGL driver not available. Skipping test." << std::endl;
            mSkipTest = true;
#endif  // defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
            break;
        default:
            std::cerr << "Error in switch." << std::endl;
            mSkipTest = true;
            break;
    }
}

ANGLERenderTest::~ANGLERenderTest()
{
    OSWindow::Delete(&mOSWindow);
    GLWindowBase::Delete(&mGLWindow);
}

void ANGLERenderTest::addExtensionPrerequisite(const char *extensionName)
{
    mExtensionPrerequisites.push_back(extensionName);
}

void ANGLERenderTest::SetUp()
{
    if (mSkipTest)
    {
        return;
    }

    ANGLEPerfTest::SetUp();

    // Set a consistent CPU core affinity and high priority.
    angle::StabilizeCPUForBenchmarking();

    mOSWindow = OSWindow::New();

    if (!mGLWindow)
    {
        mSkipTest = true;
        return;
    }

    mGLWindow->setSwapInterval(0);

    mPlatformMethods.overrideWorkaroundsD3D      = OverrideWorkaroundsD3D;
    mPlatformMethods.logError                    = EmptyPlatformMethod;
    mPlatformMethods.logWarning                  = EmptyPlatformMethod;
    mPlatformMethods.logInfo                     = EmptyPlatformMethod;
    mPlatformMethods.addTraceEvent               = AddTraceEvent;
    mPlatformMethods.getTraceCategoryEnabledFlag = GetTraceCategoryEnabledFlag;
    mPlatformMethods.updateTraceEventDuration    = UpdateTraceEventDuration;
    mPlatformMethods.monotonicallyIncreasingTime = MonotonicallyIncreasingTime;
    mPlatformMethods.context                     = this;
    mGLWindow->setPlatformMethods(&mPlatformMethods);

    if (!mOSWindow->initialize(mName, mTestParams.windowWidth, mTestParams.windowHeight))
    {
        mSkipTest = true;
        FAIL() << "Failed initializing OSWindow";
        // FAIL returns.
    }

    if (!mGLWindow->initializeGL(mOSWindow, mEntryPointsLib.get()))
    {
        mSkipTest = true;
        FAIL() << "Failed initializing GL Window";
        // FAIL returns.
    }

    if (!areExtensionPrerequisitesFulfilled())
    {
        mSkipTest = true;
    }

    if (mSkipTest)
    {
        return;
    }

    initializeBenchmark();

    if (mTestParams.iterationsPerStep == 0)
    {
        mSkipTest = true;
        FAIL() << "Please initialize 'iterationsPerStep'.";
        // FAIL returns.
    }
}

void ANGLERenderTest::TearDown()
{
    if (!mSkipTest)
    {
        destroyBenchmark();
    }

    if (mGLWindow)
    {
        mGLWindow->destroyGL();
        mGLWindow = nullptr;
    }

    if (mOSWindow)
    {
        mOSWindow->destroy();
        mOSWindow = nullptr;
    }

    // Dump trace events to json file.
    if (gEnableTrace)
    {
        DumpTraceEventsToJSONFile(mTraceEventBuffer, gTraceFile);
    }

    ANGLEPerfTest::TearDown();
}

void ANGLERenderTest::step()
{
    // Clear events that the application did not process from this frame
    Event event;
    bool closed = false;
    while (popEvent(&event))
    {
        // If the application did not catch a close event, close now
        if (event.Type == Event::EVENT_CLOSED)
        {
            closed = true;
        }
    }

    if (closed)
    {
        abortTest();
    }
    else
    {
        drawBenchmark();
        // Swap is needed so that the GPU driver will occasionally flush its
        // internal command queue to the GPU. This is enabled for null back-end
        // devices because some back-ends (e.g. Vulkan) also accumulate internal
        // command queues.
        mGLWindow->swap();
        mOSWindow->messageLoop();
    }
}

void ANGLERenderTest::startGpuTimer()
{
    if (mTestParams.trackGpuTime)
    {
        glBeginQueryEXT(GL_TIME_ELAPSED_EXT, mTimestampQuery);
    }
}

void ANGLERenderTest::stopGpuTimer()
{
    if (mTestParams.trackGpuTime)
    {
        glEndQueryEXT(GL_TIME_ELAPSED_EXT);
        uint64_t gpuTimeNs = 0;
        glGetQueryObjectui64vEXT(mTimestampQuery, GL_QUERY_RESULT_EXT, &gpuTimeNs);

        mGPUTimeNs += gpuTimeNs;
    }
}

void ANGLERenderTest::startTest()
{
    if (mTestParams.trackGpuTime)
    {
        glGenQueriesEXT(1, &mTimestampQuery);
        mGPUTimeNs = 0;
    }
}

void ANGLERenderTest::finishTest()
{
    if (mTestParams.trackGpuTime)
    {
        glDeleteQueriesEXT(1, &mTimestampQuery);
    }
    if (mTestParams.eglParameters.deviceType != EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
    {
        glFinish();
    }
}

bool ANGLERenderTest::popEvent(Event *event)
{
    return mOSWindow->popEvent(event);
}

OSWindow *ANGLERenderTest::getWindow()
{
    return mOSWindow;
}

bool ANGLERenderTest::areExtensionPrerequisitesFulfilled() const
{
    for (const char *extension : mExtensionPrerequisites)
    {
        if (!CheckExtensionExists(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                  extension))
        {
            std::cout << "Test skipped due to missing extension: " << extension << std::endl;
            return false;
        }
    }
    return true;
}

void ANGLERenderTest::setWebGLCompatibilityEnabled(bool webglCompatibility)
{
    mGLWindow->setWebGLCompatibilityEnabled(webglCompatibility);
}

void ANGLERenderTest::setRobustResourceInit(bool enabled)
{
    mGLWindow->setRobustResourceInit(enabled);
}

std::vector<TraceEvent> &ANGLERenderTest::getTraceEventBuffer()
{
    return mTraceEventBuffer;
}

// static
EGLWindow *ANGLERenderTest::createEGLWindow(const RenderTestParams &testParams)
{
    return EGLWindow::New(testParams.majorVersion, testParams.minorVersion,
                          testParams.eglParameters);
}

void ANGLEProcessPerfTestArgs(int *argc, char **argv)
{
    int argcOutCount = 0;

    for (int argIndex = 0; argIndex < *argc; argIndex++)
    {
        if (strcmp("--one-frame-only", argv[argIndex]) == 0)
        {
            gStepsToRunOverride = 1;
        }
        else if (strcmp("--enable-trace", argv[argIndex]) == 0)
        {
            gEnableTrace = true;
        }
        else if (strcmp("--trace-file", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            gTraceFile = argv[argIndex];
            // Skip an additional argument.
            argIndex++;
        }
        else if (strcmp("--calibration", argv[argIndex]) == 0)
        {
            gCalibration = true;
        }
        else if (strcmp("--steps", argv[argIndex]) == 0 && argIndex < *argc - 1)
        {
            unsigned int stepsToRun = 0;
            std::stringstream strstr;
            strstr << argv[argIndex + 1];
            strstr >> stepsToRun;
            gStepsToRunOverride = stepsToRun;
            // Skip an additional argument.
            argIndex++;
        }
        else
        {
            argv[argcOutCount++] = argv[argIndex];
        }
    }

    *argc = argcOutCount;
}
