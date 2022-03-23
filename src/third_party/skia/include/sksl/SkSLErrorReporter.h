/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_ERROR_REPORTER
#define SKSL_ERROR_REPORTER

#include "include/core/SkTypes.h"
#include "include/private/SkSLString.h"

#include <string>
#include <string_view>
#include <vector>

namespace SkSL {

class Position;

/**
 * Class which is notified in the event of an error.
 */
class ErrorReporter {
public:
    ErrorReporter() {}

    virtual ~ErrorReporter() {
        SkASSERT(fPendingErrors.empty());
    }

    void error(std::string_view msg, Position position);

    /**
     * Reports an error message at the given line of the source text. Errors reported
     * with a line of -1 will be queued until line number information can be determined.
     */
    void error(Position pos, std::string_view msg);

    const char* source() const { return fSource; }

    void setSource(const char* source) { fSource = source; }

    void reportPendingErrors(Position pos);

    int errorCount() const {
        return fErrorCount;
    }

    void resetErrorCount() {
        fErrorCount = 0;
    }

protected:
    /**
     * Called when an error is reported.
     */
    virtual void handleError(std::string_view msg, Position position) = 0;

private:
    Position position(int offset) const;

    const char* fSource = nullptr;
    std::vector<std::string> fPendingErrors;
    int fErrorCount = 0;
};

/**
 * Error reporter for tests that need an SkSL context; aborts immediately if an error is reported.
 */
class TestingOnly_AbortErrorReporter : public ErrorReporter {
public:
    void handleError(std::string_view msg, Position pos) override;
};

} // namespace SkSL

#endif
