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

#ifndef sw_FrameBufferAndroid_hpp
#define sw_FrameBufferAndroid_hpp

#include "Main/FrameBuffer.hpp"
#include "Common/Debug.hpp"

struct ANativeWindow;
struct ANativeWindowBuffer;

namespace sw
{
	class FrameBufferAndroid : public FrameBuffer
	{
	public:
		FrameBufferAndroid(ANativeWindow *window, int width, int height);

		~FrameBufferAndroid() override;

		void flip(sw::Surface *source) override {blit(source, nullptr, nullptr);}
		void blit(sw::Surface *source, const Rect *sourceRect, const Rect *destRect) override;

		void *lock() override;
		void unlock() override;

		bool setSwapRectangle(int l, int t, int w, int h);

	private:
		ANativeWindow *nativeWindow;
		ANativeWindowBuffer *buffer;
	};
}

#endif   // sw_FrameBufferAndroid
