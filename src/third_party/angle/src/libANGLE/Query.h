//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query.h: Defines the gl::Query class

#ifndef LIBANGLE_QUERY_H_
#define LIBANGLE_QUERY_H_

#include "common/PackedEnums.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"

#include "common/angleutils.h"

#include "angle_gl.h"

namespace rx
{
class QueryImpl;
}

namespace gl
{

class Query final : public RefCountObject, public LabeledObject
{
  public:
    Query(rx::QueryImpl *impl, GLuint id);
    ~Query() override;
    void onDestroy(const Context *context) override;

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    Error begin(const Context *context);
    Error end(const Context *context);
    Error queryCounter(const Context *context);
    Error getResult(const Context *context, GLint *params);
    Error getResult(const Context *context, GLuint *params);
    Error getResult(const Context *context, GLint64 *params);
    Error getResult(const Context *context, GLuint64 *params);
    Error isResultAvailable(const Context *context, bool *available);

    QueryType getType() const;

    rx::QueryImpl *getImplementation();
    const rx::QueryImpl *getImplementation() const;

  private:
    rx::QueryImpl *mQuery;

    std::string mLabel;
};
}

#endif  // LIBANGLE_QUERY_H_
