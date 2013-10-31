/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef CustomFilterParameterList_h
#define CustomFilterParameterList_h

#include "platform/PlatformExport.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/filters/custom/CustomFilterParameter.h"
#include "wtf/Vector.h"

namespace WebCore {

class PLATFORM_EXPORT CustomFilterParameterList {
public:
    CustomFilterParameterList();
    explicit CustomFilterParameterList(size_t);

    void blend(const CustomFilterParameterList& from, double progress, CustomFilterParameterList& resultList) const;
    bool operator==(const CustomFilterParameterList&) const;

    PassRefPtr<CustomFilterParameter> at(size_t index) const { return m_parameters.at(index); }
    size_t size() const { return m_parameters.size(); }
    void append(const PassRefPtr<CustomFilterParameter>& parameter) { m_parameters.append(parameter); }
    void sortParametersByName();
private:
#ifndef NDEBUG
    bool checkAlphabeticalOrder() const;
#endif
    typedef Vector<RefPtr<CustomFilterParameter> > CustomFilterParameterVector;
    CustomFilterParameterVector m_parameters;
};

} // namespace WebCore


#endif // CustomFilterParameterList_h
