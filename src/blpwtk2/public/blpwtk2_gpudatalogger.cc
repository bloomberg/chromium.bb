/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_gpudatalogger.h>

namespace blpwtk2 {

GpuDataLogger::~GpuDataLogger() {}

GpuMode GpuDataLogger::toWtk2GpuMode(gpu::GpuMode mode)
{
   // Alert us in case of upstream gpu::GpuMode update
   static_assert((int)gpu::GpuMode::DISABLED == 6, "gpu::GpuMode should only have 6 types");
   GpuMode wtk2Mode;
   switch(mode) {
        case gpu::GpuMode::HARDWARE_GL:
        case gpu::GpuMode::HARDWARE_METAL:
        case gpu::GpuMode::HARDWARE_VULKAN:
            wtk2Mode = GpuMode::kOOPHardwareAccelerated;
            break;
        case gpu::GpuMode::SWIFTSHADER:
            wtk2Mode = GpuMode::kOOPSoftware;
            break;
        case gpu::GpuMode::DISABLED:
            wtk2Mode = GpuMode::kDisabled;
            break;
        default:
            wtk2Mode = GpuMode::kUnknown;
    }
    return wtk2Mode;
}

const std::string& GpuDataLogger::getGpuModeDescription(GpuMode mode)
{
    static_assert((int)GpuMode::kDisabled == 3, "GpuMode should only have 4 types");
	static std::string modeDescriptions[] = {
		"Unknown",
		"GPU hardware accelerated",
		"Software based",
		"Disabled"
	};
    return modeDescriptions[(int)mode];
}


}  // namespace content
