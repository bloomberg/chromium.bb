/* libs/graphics/ports/SkFontHost_fontconfig_control.h
**
** Copyright 2009, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef FontConfigControl_DEFINED
#define FontConfigControl_DEFINED
#pragma once

#include "SkPreConfig.h"

class FontConfigInterface;

// Sets the Skia FontHost to use the direct (non-IPC, requires filesystem
// access) FontConfig implementation. Any previously-set FontConfigInterface
// will be freed.
SK_API void SkiaFontConfigUseDirectImplementation();

// Sets the Skia FontHost to use the given implementation of FontConfig. This
// is normally used to configure the IPC-based implementation to get out of
// the sandbox.
//
// Ownership of the given pointer is transferred, and any previously-set
// FontConfigInterface will be freed.
SK_API void SkiaFontConfigSetImplementation(FontConfigInterface* font_config);

#endif  // FontConfigControl_DEFINED
