//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query9.cpp: Defines the rx::Query9 class which implements rx::QueryImpl.

#include "libANGLE/renderer/d3d/d3d9/Query9.h"
#include "libANGLE/renderer/d3d/d3d9/renderer9_utils.h"
#include "libANGLE/renderer/d3d/d3d9/Renderer9.h"

#include <GLES2/gl2ext.h>

namespace rx
{
Query9::Query9(Renderer9 *renderer, gl::QueryType type)
    : QueryImpl(type),
      mGetDataAttemptCount(0),
      mResult(GL_FALSE),
      mQueryFinished(false),
      mRenderer(renderer),
      mQuery(nullptr)
{
}

Query9::~Query9()
{
    SafeRelease(mQuery);
}

gl::Error Query9::begin(const gl::Context *context)
{
    D3DQUERYTYPE d3dQueryType = gl_d3d9::ConvertQueryType(getType());
    if (mQuery == nullptr)
    {
        HRESULT result = mRenderer->getDevice()->CreateQuery(d3dQueryType, &mQuery);
        if (FAILED(result))
        {
            return gl::OutOfMemory() << "Internal query creation failed, " << gl::FmtHR(result);
        }
    }

    if (d3dQueryType != D3DQUERYTYPE_EVENT)
    {
        HRESULT result = mQuery->Issue(D3DISSUE_BEGIN);
        ASSERT(SUCCEEDED(result));
        if (FAILED(result))
        {
            return gl::OutOfMemory() << "Failed to begin internal query, " << gl::FmtHR(result);
        }
    }

    return gl::NoError();
}

gl::Error Query9::end(const gl::Context *context)
{
    ASSERT(mQuery);

    HRESULT result = mQuery->Issue(D3DISSUE_END);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to end internal query, " << gl::FmtHR(result);
    }

    mQueryFinished = false;
    mResult = GL_FALSE;

    return gl::NoError();
}

gl::Error Query9::queryCounter(const gl::Context *context)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "Unimplemented";
}

template <typename T>
gl::Error Query9::getResultBase(T *params)
{
    while (!mQueryFinished)
    {
        gl::Error error = testQuery();
        if (error.isError())
        {
            return error;
        }

        if (!mQueryFinished)
        {
            Sleep(0);
        }
    }

    ASSERT(mQueryFinished);
    *params = static_cast<T>(mResult);
    return gl::NoError();
}

gl::Error Query9::getResult(const gl::Context *context, GLint *params)
{
    return getResultBase(params);
}

gl::Error Query9::getResult(const gl::Context *context, GLuint *params)
{
    return getResultBase(params);
}

gl::Error Query9::getResult(const gl::Context *context, GLint64 *params)
{
    return getResultBase(params);
}

gl::Error Query9::getResult(const gl::Context *context, GLuint64 *params)
{
    return getResultBase(params);
}

gl::Error Query9::isResultAvailable(const gl::Context *context, bool *available)
{
    gl::Error error = testQuery();
    if (error.isError())
    {
        return error;
    }

    *available = mQueryFinished;

    return gl::NoError();
}

gl::Error Query9::testQuery()
{
    if (!mQueryFinished)
    {
        ASSERT(mQuery);

        HRESULT result = S_OK;
        switch (getType())
        {
            case gl::QueryType::AnySamples:
            case gl::QueryType::AnySamplesConservative:
            {
                DWORD numPixels = 0;
                result = mQuery->GetData(&numPixels, sizeof(numPixels), D3DGETDATA_FLUSH);
                if (result == S_OK)
                {
                    mQueryFinished = true;
                    mResult        = (numPixels > 0) ? GL_TRUE : GL_FALSE;
                }
                break;
            }

            case gl::QueryType::CommandsCompleted:
            {
                BOOL completed = FALSE;
                result = mQuery->GetData(&completed, sizeof(completed), D3DGETDATA_FLUSH);
                if (result == S_OK)
                {
                    mQueryFinished = true;
                    mResult        = (completed == TRUE) ? GL_TRUE : GL_FALSE;
                }
                break;
            }

            default:
                UNREACHABLE();
                break;
        }

        if (!mQueryFinished)
        {
            if (d3d9::isDeviceLostError(result))
            {
                mRenderer->notifyDeviceLost();
                return gl::OutOfMemory() << "Failed to test get query result, device is lost.";
            }

            mGetDataAttemptCount++;
            bool checkDeviceLost =
                (mGetDataAttemptCount % kPollingD3DDeviceLostCheckFrequency) == 0;
            if (checkDeviceLost && mRenderer->testDeviceLost())
            {
                mRenderer->notifyDeviceLost();
                return gl::OutOfMemory() << "Failed to test get query result, device is lost.";
            }
        }
    }

    return gl::NoError();
}

}
