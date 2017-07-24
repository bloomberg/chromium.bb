/*
 * Copyright (C) 2004, 2005, 2006, 2013 Apple Inc.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_build_config_h
#define WTF_build_config_h

#include "build/build_config.h"
#include "platform/wtf/Compiler.h"

/* There is an assumption in the project that either OS_WIN or OS_POSIX is
 * set. */
#if !defined(OS_WIN) && !defined(OS_POSIX)
#error Either OS_WIN or OS_POSIX needs to be set.
#endif

#endif  // WTF_build_config_h
