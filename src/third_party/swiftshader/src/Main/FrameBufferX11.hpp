// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef sw_FrameBufferX11_hpp
#define sw_FrameBufferX11_hpp

#include "Main/FrameBuffer.hpp"
#include "Common/Debug.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

namespace sw
{
	class FrameBufferX11 : public FrameBuffer
	{
	public:
		FrameBufferX11(Display *display, Window window, int width, int height);

		~FrameBufferX11() override;

		void flip(sw::Surface *source) override { blit(source, nullptr, nullptr); }
		void blit(sw::Surface *source, const Rect *sourceRect, const Rect *destRect) override;

		void *lock() override;
		void unlock() override;

	private:
		const bool ownX11;
		Display *x_display;
		const Window x_window;
		XImage *x_image = nullptr;
		GC x_gc;

		bool mit_shm;
		XShmSegmentInfo shminfo;
	};
}

#endif   // sw_FrameBufferX11_hpp
