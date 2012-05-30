/*
 * Copyright © 2012 Collabora, Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <cstdlib>

#include "android-framebuffer.h"

#ifdef ANDROID

#include <ui/FramebufferNativeWindow.h>

class AndroidFramebuffer {
public:
	int init();

	struct android_framebuffer fb_;

private:
	android::sp<android::FramebufferNativeWindow> nativefb_;
};

int AndroidFramebuffer::init()
{
	struct ANativeWindow *window;
	const framebuffer_device_t *fbdev;
	int ret1, ret2, ret3;

	nativefb_ = new android::FramebufferNativeWindow();
	fbdev = nativefb_->getDevice();

	if (!fbdev)
		return -1;

	fb_.priv = this;

	window = nativefb_.get();
	ret1 = window->query(window, NATIVE_WINDOW_WIDTH, &fb_.width);
	ret2 = window->query(window, NATIVE_WINDOW_HEIGHT, &fb_.height);
	ret3 = window->query(window, NATIVE_WINDOW_FORMAT, &fb_.format);
	fb_.xdpi = window->xdpi;
	fb_.ydpi = window->ydpi;
	fb_.refresh_rate = fbdev->fps;

	if (ret1 != android::NO_ERROR ||
	    ret2 != android::NO_ERROR ||
	    ret3 != android::NO_ERROR)
		return -1;

	fb_.native_window = reinterpret_cast<EGLNativeWindowType>(window);
	return 0;
}

void
android_framebuffer_destroy(struct android_framebuffer *fb)
{
	AndroidFramebuffer *afb = static_cast<AndroidFramebuffer*>(fb->priv);

	delete afb;
}

struct android_framebuffer *
android_framebuffer_create(void)
{
	AndroidFramebuffer *afb = new AndroidFramebuffer;

	if (afb->init() < 0) {
		delete afb;
		return NULL;
	}

	return &afb->fb_;
}

#endif /* ANDROID */
