//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// QueryGL.cpp: Implements the class methods for QueryGL.

#include "libANGLE/renderer/gl/QueryGL.h"

#include "common/debug.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace
{

GLuint64 MergeQueryResults(gl::QueryType type, GLuint64 currentResult, GLuint64 newResult)
{
    switch (type)
    {
        case gl::QueryType::AnySamples:
        case gl::QueryType::AnySamplesConservative:
            return (currentResult == GL_TRUE || newResult == GL_TRUE) ? GL_TRUE : GL_FALSE;

        case gl::QueryType::TransformFeedbackPrimitivesWritten:
            return currentResult + newResult;

        case gl::QueryType::TimeElapsed:
            return currentResult + newResult;

        case gl::QueryType::Timestamp:
            return newResult;

        case gl::QueryType::PrimitivesGenerated:
            return currentResult + newResult;

        default:
            UNREACHABLE();
            return 0;
    }
}

}  // anonymous namespace

namespace rx
{

QueryGL::QueryGL(gl::QueryType type) : QueryImpl(type)
{
}

QueryGL::~QueryGL()
{
}

StandardQueryGL::StandardQueryGL(gl::QueryType type,
                                 const FunctionsGL *functions,
                                 StateManagerGL *stateManager)
    : QueryGL(type),
      mType(type),
      mFunctions(functions),
      mStateManager(stateManager),
      mActiveQuery(0),
      mPendingQueries(),
      mResultSum(0)
{
}

StandardQueryGL::~StandardQueryGL()
{
    if (mActiveQuery != 0)
    {
        mStateManager->endQuery(mType, this, mActiveQuery);
        mFunctions->deleteQueries(1, &mActiveQuery);
        mActiveQuery = 0;
    }

    while (!mPendingQueries.empty())
    {
        GLuint id = mPendingQueries.front();
        mFunctions->deleteQueries(1, &id);
        mPendingQueries.pop_front();
    }
}

gl::Error StandardQueryGL::begin(const gl::Context *context)
{
    mResultSum = 0;
    return resume();
}

gl::Error StandardQueryGL::end(const gl::Context *context)
{
    return pause();
}

gl::Error StandardQueryGL::queryCounter(const gl::Context *context)
{
    ASSERT(mType == gl::QueryType::Timestamp);

    // Directly create a query for the timestamp and add it to the pending query queue, as timestamp
    // queries do not have the traditional begin/end block and never need to be paused/resumed
    GLuint query;
    mFunctions->genQueries(1, &query);
    mFunctions->queryCounter(query, GL_TIMESTAMP);
    mPendingQueries.push_back(query);

    return gl::NoError();
}

template <typename T>
gl::Error StandardQueryGL::getResultBase(T *params)
{
    ASSERT(mActiveQuery == 0);

    gl::Error error = flush(true);
    if (error.isError())
    {
        return error;
    }

    ASSERT(mPendingQueries.empty());
    *params = static_cast<T>(mResultSum);

    return gl::NoError();
}

gl::Error StandardQueryGL::getResult(const gl::Context *context, GLint *params)
{
    return getResultBase(params);
}

gl::Error StandardQueryGL::getResult(const gl::Context *context, GLuint *params)
{
    return getResultBase(params);
}

gl::Error StandardQueryGL::getResult(const gl::Context *context, GLint64 *params)
{
    return getResultBase(params);
}

gl::Error StandardQueryGL::getResult(const gl::Context *context, GLuint64 *params)
{
    return getResultBase(params);
}

gl::Error StandardQueryGL::isResultAvailable(const gl::Context *context, bool *available)
{
    ASSERT(mActiveQuery == 0);

    gl::Error error = flush(false);
    if (error.isError())
    {
        return error;
    }

    *available = mPendingQueries.empty();
    return gl::NoError();
}

gl::Error StandardQueryGL::pause()
{
    if (mActiveQuery != 0)
    {
        mStateManager->endQuery(mType, this, mActiveQuery);

        mPendingQueries.push_back(mActiveQuery);
        mActiveQuery = 0;
    }

    // Flush to make sure the pending queries don't add up too much.
    gl::Error error = flush(false);
    if (error.isError())
    {
        return error;
    }

    return gl::NoError();
}

gl::Error StandardQueryGL::resume()
{
    if (mActiveQuery == 0)
    {
        // Flush to make sure the pending queries don't add up too much.
        gl::Error error = flush(false);
        if (error.isError())
        {
            return error;
        }

        mFunctions->genQueries(1, &mActiveQuery);
        mStateManager->beginQuery(mType, this, mActiveQuery);
    }

    return gl::NoError();
}

gl::Error StandardQueryGL::flush(bool force)
{
    while (!mPendingQueries.empty())
    {
        GLuint id = mPendingQueries.front();
        if (!force)
        {
            GLuint resultAvailable = 0;
            mFunctions->getQueryObjectuiv(id, GL_QUERY_RESULT_AVAILABLE, &resultAvailable);
            if (resultAvailable == GL_FALSE)
            {
                return gl::NoError();
            }
        }

        // Even though getQueryObjectui64v was introduced for timer queries, there is nothing in the
        // standard that says that it doesn't work for any other queries. It also passes on all the
        // trybots, so we use it if it is available
        if (mFunctions->getQueryObjectui64v != nullptr)
        {
            GLuint64 result = 0;
            mFunctions->getQueryObjectui64v(id, GL_QUERY_RESULT, &result);
            mResultSum = MergeQueryResults(mType, mResultSum, result);
        }
        else
        {
            GLuint result = 0;
            mFunctions->getQueryObjectuiv(id, GL_QUERY_RESULT, &result);
            mResultSum = MergeQueryResults(mType, mResultSum, static_cast<GLuint64>(result));
        }

        mFunctions->deleteQueries(1, &id);

        mPendingQueries.pop_front();
    }

    return gl::NoError();
}

class SyncProviderGL
{
  public:
    virtual ~SyncProviderGL() {}
    virtual gl::Error flush(bool force, bool *finished) = 0;
};

class SyncProviderGLSync : public SyncProviderGL
{
  public:
    SyncProviderGLSync(const FunctionsGL *functions) : mFunctions(functions), mSync(nullptr)
    {
        mSync = mFunctions->fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    ~SyncProviderGLSync() override { mFunctions->deleteSync(mSync); }

    gl::Error flush(bool force, bool *finished) override
    {
        if (force)
        {
            mFunctions->clientWaitSync(mSync, 0, 0);
            *finished = true;
        }
        else
        {
            GLint value = 0;
            mFunctions->getSynciv(mSync, GL_SYNC_STATUS, 1, nullptr, &value);
            *finished = (value == GL_SIGNALED);
        }

        return gl::NoError();
    }

  private:
    const FunctionsGL *mFunctions;
    GLsync mSync;
};

class SyncProviderGLQuery : public SyncProviderGL
{
  public:
    SyncProviderGLQuery(const FunctionsGL *functions,
                        StateManagerGL *stateManager,
                        gl::QueryType type)
        : mFunctions(functions), mQuery(0)
    {
        mFunctions->genQueries(1, &mQuery);
        ANGLE_SWALLOW_ERR(stateManager->pauseQuery(type));
        mFunctions->beginQuery(ToGLenum(type), mQuery);
        mFunctions->endQuery(ToGLenum(type));
        ANGLE_SWALLOW_ERR(stateManager->resumeQuery(type));
    }

    ~SyncProviderGLQuery() override { mFunctions->deleteQueries(1, &mQuery); }

    gl::Error flush(bool force, bool *finished) override
    {
        if (force)
        {
            GLint result = 0;
            mFunctions->getQueryObjectiv(mQuery, GL_QUERY_RESULT, &result);
            *finished = true;
        }
        else
        {
            GLint available = 0;
            mFunctions->getQueryObjectiv(mQuery, GL_QUERY_RESULT_AVAILABLE, &available);
            *finished = (available == GL_TRUE);
        }

        return gl::NoError();
    }

  private:
    const FunctionsGL *mFunctions;
    GLuint mQuery;
};

SyncQueryGL::SyncQueryGL(gl::QueryType type,
                         const FunctionsGL *functions,
                         StateManagerGL *stateManager)
    : QueryGL(type),
      mFunctions(functions),
      mStateManager(stateManager),
      mSyncProvider(nullptr),
      mFinished(false)
{
    ASSERT(IsSupported(mFunctions));
    ASSERT(type == gl::QueryType::CommandsCompleted);
}

SyncQueryGL::~SyncQueryGL()
{
}

bool SyncQueryGL::IsSupported(const FunctionsGL *functions)
{
    return nativegl::SupportsFenceSync(functions) || nativegl::SupportsOcclusionQueries(functions);
}

gl::Error SyncQueryGL::begin(const gl::Context *context)
{
    return gl::NoError();
}

gl::Error SyncQueryGL::end(const gl::Context *context)
{
    if (nativegl::SupportsFenceSync(mFunctions))
    {
        mSyncProvider.reset(new SyncProviderGLSync(mFunctions));
    }
    else if (nativegl::SupportsOcclusionQueries(mFunctions))
    {
        mSyncProvider.reset(
            new SyncProviderGLQuery(mFunctions, mStateManager, gl::QueryType::AnySamples));
    }
    else
    {
        ASSERT(false);
        return gl::InternalError() << "No native support for sync queries.";
    }
    return gl::NoError();
}

gl::Error SyncQueryGL::queryCounter(const gl::Context *context)
{
    UNREACHABLE();
    return gl::NoError();
}

gl::Error SyncQueryGL::getResult(const gl::Context *context, GLint *params)
{
    return getResultBase(params);
}

gl::Error SyncQueryGL::getResult(const gl::Context *context, GLuint *params)
{
    return getResultBase(params);
}

gl::Error SyncQueryGL::getResult(const gl::Context *context, GLint64 *params)
{
    return getResultBase(params);
}

gl::Error SyncQueryGL::getResult(const gl::Context *context, GLuint64 *params)
{
    return getResultBase(params);
}

gl::Error SyncQueryGL::isResultAvailable(const gl::Context *context, bool *available)
{
    ANGLE_TRY(flush(false));
    *available = mFinished;
    return gl::NoError();
}

gl::Error SyncQueryGL::pause()
{
    return gl::NoError();
}

gl::Error SyncQueryGL::resume()
{
    return gl::NoError();
}

gl::Error SyncQueryGL::flush(bool force)
{
    if (mSyncProvider == nullptr)
    {
        ASSERT(mFinished);
        return gl::NoError();
    }

    ANGLE_TRY(mSyncProvider->flush(force, &mFinished));
    if (mFinished)
    {
        mSyncProvider.reset();
    }

    return gl::NoError();
}

template <typename T>
gl::Error SyncQueryGL::getResultBase(T *params)
{
    ANGLE_TRY(flush(true));
    *params = static_cast<T>(mFinished ? GL_TRUE : GL_FALSE);
    return gl::NoError();
}
}
