//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Observer:
//   Implements the Observer pattern for sending state change notifications
//   from Subject objects to dependent Observer objects.
//
//   See design document:
//   https://docs.google.com/document/d/15Edfotqg6_l1skTEL8ADQudF_oIdNa7i8Po43k6jMd4/

#include "libANGLE/Observer.h"

#include <algorithm>

#include "common/debug.h"

namespace angle
{
namespace
{
template <typename HaystackT, typename NeedleT>
bool IsInContainer(const HaystackT &haystack, const NeedleT &needle)
{
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}
}  // anonymous namespace

// Observer implementation.
ObserverInterface::~ObserverInterface() = default;

// Subject implementation.
Subject::Subject()
{
}

Subject::~Subject()
{
    resetObservers();
}

bool Subject::hasObservers() const
{
    return !mObservers.empty();
}

ANGLE_INLINE void Subject::addObserver(ObserverBinding *observer)
{
    ASSERT(!IsInContainer(mObservers, observer));
    mObservers.push_back(observer);
}

ANGLE_INLINE void Subject::removeObserver(ObserverBinding *observer)
{
    ASSERT(IsInContainer(mObservers, observer));
    size_t len = mObservers.size() - 1;
    for (size_t index = 0; index < len; ++index)
    {
        if (mObservers[index] == observer)
        {
            mObservers[index] = mObservers[len];
            break;
        }
    }
    mObservers.pop_back();
}

void Subject::onStateChange(const gl::Context *context, SubjectMessage message) const
{
    if (mObservers.empty())
        return;

    for (const angle::ObserverBinding *receiver : mObservers)
    {
        receiver->onStateChange(context, message);
    }
}

void Subject::resetObservers()
{
    for (angle::ObserverBinding *observer : mObservers)
    {
        observer->onSubjectReset();
    }
    mObservers.clear();
}

// ObserverBinding implementation.
ObserverBinding::ObserverBinding(ObserverInterface *observer, SubjectIndex index)
    : mSubject(nullptr), mObserver(observer), mIndex(index)
{
    ASSERT(observer);
}

ObserverBinding::~ObserverBinding()
{
    reset();
}

ObserverBinding::ObserverBinding(const ObserverBinding &other) = default;

ObserverBinding &ObserverBinding::operator=(const ObserverBinding &other) = default;

void ObserverBinding::bind(Subject *subject)
{
    ASSERT(mObserver);
    if (mSubject)
    {
        mSubject->removeObserver(this);
    }

    mSubject = subject;

    if (mSubject)
    {
        mSubject->addObserver(this);
    }
}

void ObserverBinding::onStateChange(const gl::Context *context, SubjectMessage message) const
{
    mObserver->onSubjectStateChange(context, mIndex, message);
}

void ObserverBinding::onSubjectReset()
{
    mSubject = nullptr;
}

const Subject *ObserverBinding::getSubject() const
{
    return mSubject;
}
}  // namespace angle
