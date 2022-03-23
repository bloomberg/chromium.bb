/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skqp_DEFINED
#define skqp_DEFINED

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class SkData;
template <typename T> class sk_sp;

namespace skiagm {
class GM;
}  // namespace skiagm

namespace skiatest {
struct Test;
}  // namespace skiatest

class SkStreamAsset;

////////////////////////////////////////////////////////////////////////////////
class SkQPAssetManager {
public:
    SkQPAssetManager() {}
    virtual ~SkQPAssetManager() {}
    virtual sk_sp<SkData> open(const char* path) = 0;
private:
    SkQPAssetManager(const SkQPAssetManager&) = delete;
    SkQPAssetManager& operator=(const SkQPAssetManager&) = delete;
};

class SkQP {
public:
    enum class SkiaBackend {
        kGL,
        kGLES,
        kVulkan,
    };
    using UnitTest = const skiatest::Test*;

    ////////////////////////////////////////////////////////////////////////////

    /** These functions provide a descriptive name for the given value.*/
    static const char* GetUnitTestName(UnitTest);

    SkQP();
    ~SkQP();

    /**
        Initialize Skia and the SkQP.  Should be executed only once.

        @param assetManager - provides assets for the models.  Does not take ownership.
        @param reportDirectory - where to write out report.
    */
    void init(SkQPAssetManager* assetManager, const char* reportDirectory);

    /** @return a (hopefully empty) list of errors produced by this unit test.  */
    std::vector<std::string> executeTest(UnitTest);

    /** Call this after running all checks to write a report into the given
        report directory. */
    void makeReport();

    /** @return a list of all Skia GPU unit tests in lexicographic order.  */
    const std::vector<UnitTest>& getUnitTests() const { return fUnitTests; }
    ////////////////////////////////////////////////////////////////////////////

private:
    struct UnitTestResult {
        UnitTest fUnitTest;
        std::vector<std::string> fErrors;
    };
    std::vector<UnitTestResult> fUnitTestResults;
    std::vector<SkiaBackend> fSupportedBackends;
    SkQPAssetManager* fAssetManager = nullptr;
    std::string fReportDirectory;
    std::vector<UnitTest> fUnitTests;

    SkQP(const SkQP&) = delete;
    SkQP& operator=(const SkQP&) = delete;
};
#endif  // skqp_DEFINED

