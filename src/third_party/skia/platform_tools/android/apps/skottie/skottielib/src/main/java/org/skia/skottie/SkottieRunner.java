/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

package org.skia.skottie;

import android.animation.Animator;
import android.animation.TimeInterpolator;
import android.graphics.SurfaceTexture;
import android.graphics.drawable.Animatable;
import android.opengl.GLUtils;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Choreographer;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;

import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

class SkottieRunner {
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
    private static final int EGL_OPENGL_ES2_BIT = 4;
    private static final int STENCIL_BUFFER_SIZE = 8;
    private static final long TIME_OUT_MS = 10000;
    private static final String LOG_TAG = "SkottiePlayer";

    private static SkottieRunner sInstance;

    private HandlerThread mGLThreadLooper;
    private Handler mGLThread;
    private EGL10 mEgl;
    private EGLDisplay mEglDisplay;
    private EGLConfig mEglConfig;
    private EGLContext mEglContext;
    private EGLSurface mPBufferSurface;
    private long mNativeProxy;

    static {
        System.loadLibrary("skottie_android");
    }
    /**
     * Gets SkottieRunner singleton instance.
     */
    public static synchronized SkottieRunner getInstance() {
        if (sInstance == null) {
            sInstance = new SkottieRunner();
        }
        return sInstance;
    }

    /**
     * Create a new animation by feeding data from "is" and replaying in a TextureView.
     * TextureView is tracked internally for SurfaceTexture state.
     */
    public SkottieAnimation createAnimation(TextureView view, InputStream is, int backgroundColor, int repeatCount) {
        return new SkottieAnimation(view, is, backgroundColor, repeatCount);
    }

    /**
     * Create a new animation by feeding data from "is" and replaying in a SurfaceTexture.
     * SurfaceTexture is possibly taken from a TextureView and can be updated with
     * updateAnimationSurface.
     */
    public SkottieAnimation createAnimation(SurfaceTexture surfaceTexture, InputStream is) {
        return new SkottieAnimation(surfaceTexture, is);
    }

    /**
     * Create a new animation by feeding data from "is" and replaying in a SurfaceView.
     * State is controlled internally by SurfaceHolder.
     */
    public SkottieAnimation createAnimation(SurfaceView view, InputStream is, int backgroundColor, int repeatCount) {
        return new SkottieAnimation(view, is, backgroundColor, repeatCount);
    }

    /**
     * Pass a new SurfaceTexture: use this method only if managing TextureView outside
     * SkottieRunner.
     */
    public void updateAnimationSurface(Animatable animation, SurfaceTexture surfaceTexture,
                                       int width, int height) {
        try {
            runOnGLThread(() -> {
                ((SkottieAnimation) animation).setSurfaceTexture(surfaceTexture);
                ((SkottieAnimation) animation).updateSurface(width, height);
            });
        }
        catch (Throwable t) {
            Log.e(LOG_TAG, "update failed", t);
            throw new RuntimeException(t);
        }
    }

    private SkottieRunner()
    {
        mGLThreadLooper = new HandlerThread("SkottieAnimator");
        mGLThreadLooper.start();
        mGLThread = new Handler(mGLThreadLooper.getLooper());
        initGl();
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            runOnGLThread(this::doFinishGL);
        } finally {
            super.finalize();
        }
    }

    private long getNativeProxy() { return mNativeProxy; }

    private class RunSignalAndCatch implements Runnable {
        public Throwable error;
        private Runnable mRunnable;
        private CountDownLatch mFence;

        RunSignalAndCatch(Runnable run, CountDownLatch fence) {
            mRunnable = run;
            mFence = fence;
        }

        @Override
        public void run() {
            try {
                mRunnable.run();
            } catch (Throwable t) {
                error = t;
            } finally {
                mFence.countDown();
            }
        }
    }

    private void runOnGLThread(Runnable r) throws Throwable {
        runOnGLThread(r, false);
    }

    private void runOnGLThread(Runnable r, boolean postAtFront) throws Throwable {

        CountDownLatch fence = new CountDownLatch(1);
        RunSignalAndCatch wrapper = new RunSignalAndCatch(r, fence);
        if (postAtFront) {
            mGLThread.postAtFrontOfQueue(wrapper);
        } else {
            mGLThread.post(wrapper);
        }
        if (!fence.await(TIME_OUT_MS, TimeUnit.MILLISECONDS)) {
            throw new TimeoutException();
        }
        if (wrapper.error != null) {
            throw wrapper.error;
        }
    }

    private void initGl()
    {
        try {
            runOnGLThread(mDoInitGL);
        }
        catch (Throwable t) {
            Log.e(LOG_TAG, "initGl failed", t);
            throw new RuntimeException(t);
        }
    }

    private Runnable mDoInitGL = new Runnable() {
        @Override
        public void run() {
            mEgl = (EGL10) EGLContext.getEGL();

            mEglDisplay = mEgl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
            if (mEglDisplay == EGL10.EGL_NO_DISPLAY) {
                throw new RuntimeException("eglGetDisplay failed "
                        + GLUtils.getEGLErrorString(mEgl.eglGetError()));
            }

            int[] version = new int[2];
            if (!mEgl.eglInitialize(mEglDisplay, version)) {
                throw new RuntimeException("eglInitialize failed " +
                        GLUtils.getEGLErrorString(mEgl.eglGetError()));
            }

            mEglConfig = chooseEglConfig();
            if (mEglConfig == null) {
                throw new RuntimeException("eglConfig not initialized");
            }

            mEglContext = createContext(mEgl, mEglDisplay, mEglConfig);

            int[] attribs = new int[] {
                    EGL10.EGL_WIDTH, 1,
                    EGL10.EGL_HEIGHT, 1,
                    EGL10.EGL_NONE
            };

            mPBufferSurface = mEgl.eglCreatePbufferSurface(mEglDisplay, mEglConfig, attribs);
            if (mPBufferSurface == null || mPBufferSurface == EGL10.EGL_NO_SURFACE) {
                int error = mEgl.eglGetError();
                throw new RuntimeException("createPbufferSurface failed "
                        + GLUtils.getEGLErrorString(error));
            }

            if (!mEgl.eglMakeCurrent(mEglDisplay, mPBufferSurface, mPBufferSurface, mEglContext)) {
                throw new RuntimeException("eglMakeCurrent failed "
                        + GLUtils.getEGLErrorString(mEgl.eglGetError()));
            }

            mNativeProxy = nCreateProxy();
        }
    };

    EGLContext createContext(EGL10 egl, EGLDisplay eglDisplay, EGLConfig eglConfig) {
        int[] attrib_list = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE };
        return egl.eglCreateContext(eglDisplay, eglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
    }

    private EGLConfig chooseEglConfig() {
        int[] configsCount = new int[1];
        EGLConfig[] configs = new EGLConfig[1];
        int[] configSpec = getConfig();
        if (!mEgl.eglChooseConfig(mEglDisplay, configSpec, configs, 1, configsCount)) {
            throw new IllegalArgumentException("eglChooseConfig failed " +
                    GLUtils.getEGLErrorString(mEgl.eglGetError()));
        } else if (configsCount[0] > 0) {
            return configs[0];
        }
        return null;
    }

    private int[] getConfig() {
        return new int[] {
                EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL10.EGL_RED_SIZE, 8,
                EGL10.EGL_GREEN_SIZE, 8,
                EGL10.EGL_BLUE_SIZE, 8,
                EGL10.EGL_ALPHA_SIZE, 8,
                EGL10.EGL_DEPTH_SIZE, 0,
                EGL10.EGL_STENCIL_SIZE, STENCIL_BUFFER_SIZE,
                EGL10.EGL_NONE
        };
    }

    private void doFinishGL() {
        nDeleteProxy(mNativeProxy);
        mNativeProxy = 0;
        if (mEglDisplay != null) {
            if (mEglContext != null) {
                mEgl.eglDestroyContext(mEglDisplay, mEglContext);
                mEglContext = null;
            }
            if (mPBufferSurface != null) {
                mEgl.eglDestroySurface(mEglDisplay, mPBufferSurface);
                mPBufferSurface = null;
            }

            mEgl.eglMakeCurrent(mEglDisplay, EGL10.EGL_NO_SURFACE,  EGL10.EGL_NO_SURFACE,
                    EGL10.EGL_NO_CONTEXT);

            mEgl.eglTerminate(mEglDisplay);
            mEglDisplay = null;
        }
    }

    public class SkottieAnimation extends Animator implements Choreographer.FrameCallback,
            TextureView.SurfaceTextureListener, SurfaceHolder.Callback {
        boolean mIsRunning = false;
        SurfaceTexture mSurfaceTexture;
        EGLSurface mEglSurface;
        boolean mNewSurface = false;
        SurfaceHolder mSurfaceHolder;
        boolean mValidSurface = false;

        private int mRepeatCount;
        private int mRepeatCounter;
        private int mSurfaceWidth = 0;
        private int mSurfaceHeight = 0;
        private int mBackgroundColor;
        private long mNativeProxy;
        private long mDuration;  // duration in ms of the animation
        private float mProgress; // animation progress in the range of 0.0f to 1.0f
        private long mAnimationStartTime; // time in System.nanoTime units, when started

        private SkottieAnimation(SurfaceTexture surfaceTexture, InputStream is) {
            if (init(is)) {
                mSurfaceTexture = surfaceTexture;
            }
        }
        private SkottieAnimation(TextureView view, InputStream is, int backgroundColor, int repeatCount) {
            if (init(is)) {
                mSurfaceTexture = view.getSurfaceTexture();
            }
            view.setSurfaceTextureListener(this);
            mBackgroundColor = backgroundColor;
            mRepeatCount = repeatCount;
            mRepeatCounter = mRepeatCount;
        }

        private SkottieAnimation(SurfaceView view, InputStream is, int backgroundColor, int repeatCount) {
            if (init(is)) {
                mSurfaceHolder = view.getHolder();
            }
            mSurfaceHolder.addCallback(this);
            mBackgroundColor = backgroundColor;
            mRepeatCount = repeatCount;
            mRepeatCounter = mRepeatCount;
        }

        private void setSurfaceTexture(SurfaceTexture s) {
            mSurfaceTexture = s;
        }

        private ByteBuffer convertToByteBuffer(InputStream is) throws IOException {
            if (is instanceof FileInputStream) {
                FileChannel fileChannel = ((FileInputStream)is).getChannel();
                return fileChannel.map(FileChannel.MapMode.READ_ONLY,
                                       fileChannel.position(), fileChannel.size());
            }

            ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
            byte[] tmpStorage = new byte[4096];
            int bytesRead;
            while ((bytesRead = is.read(tmpStorage, 0, tmpStorage.length)) != -1) {
                byteStream.write(tmpStorage, 0, bytesRead);
            }

            byteStream.flush();
            tmpStorage = byteStream.toByteArray();

            ByteBuffer buffer = ByteBuffer.allocateDirect(tmpStorage.length);
            buffer.order(ByteOrder.nativeOrder());
            buffer.put(tmpStorage, 0, tmpStorage.length);
            return buffer.asReadOnlyBuffer();
        }

        private boolean init(InputStream is) {

            ByteBuffer byteBuffer;
            try {
                byteBuffer = convertToByteBuffer(is);
            } catch (IOException e) {
                Log.e(LOG_TAG, "failed to read input stream", e);
                return false;
            }

            long proxy = SkottieRunner.getInstance().getNativeProxy();
            mNativeProxy = nCreateProxy(proxy, byteBuffer);
            mDuration = nGetDuration(mNativeProxy);
            mProgress = 0f;
            return true;
        }

        @Override
        protected void finalize() throws Throwable {
            try {
                end();
                nDeleteProxy(mNativeProxy);
                mNativeProxy = 0;
            } finally {
                super.finalize();
            }
        }

        // Always call this on GL thread
        public void updateSurface(int width, int height) {
            mSurfaceWidth = width;
            mSurfaceHeight = height;
            mNewSurface = true;
            drawFrame();
        }

        @Override
        public void start() {
            try {
                runOnGLThread(() -> {
                    if (!mIsRunning) {
                        long currentTime = System.nanoTime();
                        mAnimationStartTime = currentTime - (long)(1000000 * mDuration * mProgress);
                        mIsRunning = true;
                        mNewSurface = true;
                        mRepeatCounter = mRepeatCount;
                        doFrame(currentTime);
                    }
                });
            }
            catch (Throwable t) {
                Log.e(LOG_TAG, "start failed", t);
                throw new RuntimeException(t);
            }
            for (AnimatorListener l : this.getListeners()) {
                l.onAnimationStart(this);
            }
        }

        @Override
        public void end() {
            try {
                runOnGLThread(() -> {
                    mIsRunning = false;
                    if (mEglSurface != null) {
                        // Ensure we always have a valid surface & context.
                        mEgl.eglMakeCurrent(mEglDisplay, mPBufferSurface, mPBufferSurface,
                                mEglContext);
                        mEgl.eglDestroySurface(mEglDisplay, mEglSurface);
                        mEglSurface = null;
                    }
                });
            }
            catch (Throwable t) {
                Log.e(LOG_TAG, "stop failed", t);
                throw new RuntimeException(t);
            }
            for (AnimatorListener l : this.getListeners()) {
                l.onAnimationEnd(this);
            }
        }

        @Override
        public void pause() {
            try {
                runOnGLThread(() -> {
                    mIsRunning = false;
                });
            }
            catch (Throwable t) {
                Log.e(LOG_TAG, "pause failed", t);
                throw new RuntimeException(t);
            }
        }

        @Override
        public void resume() {
            try {
                runOnGLThread(() -> {
                    if (!mIsRunning) {
                        long currentTime = System.nanoTime();
                        mAnimationStartTime = currentTime - (long)(1000000 * mDuration * mProgress);
                        mIsRunning = true;
                        mNewSurface = true;
                        doFrame(currentTime);
                    }
                });
            }
            catch (Throwable t) {
                Log.e(LOG_TAG, "resume failed", t);
                throw new RuntimeException(t);
            }
        }

        // TODO: add support for start delay
        @Override
        public long getStartDelay() {
            return 0;
        }

        // TODO: add support for start delay
        @Override
        public void setStartDelay(long startDelay) {

        }

        @Override
        public Animator setDuration(long duration) {
            return null;
        }

        @Override
        public boolean isRunning() {
            return mIsRunning;
        }

        @Override
        public long getDuration() {
            return mDuration;
        }

        @Override
        public long getTotalDuration() {
            if (mRepeatCount == -1) {
                return DURATION_INFINITE;
            }
            // TODO: add start delay when implemented
            return mDuration * (1 + mRepeatCount);
        }

        // TODO: support TimeInterpolators
        @Override
        public void setInterpolator(TimeInterpolator value) {

        }

        public void setProgress(float progress) {
            try {
                runOnGLThread(() -> {
                    mProgress = progress;
                    if (mIsRunning) {
                        mAnimationStartTime = System.nanoTime()
                                - (long)(1000000 * mDuration * mProgress);
                    }
                    drawFrame();
                });
            }
            catch (Throwable t) {
                Log.e(LOG_TAG, "setProgress failed", t);
                throw new RuntimeException(t);
            }
        }

        public float getProgress() {
            return mProgress;
        }

        private void drawFrame() {
            try {
                boolean forceDraw = false;
                if (mNewSurface) {
                    forceDraw = true;
                    // if there is a new SurfaceTexture, we need to recreate the EGL surface.
                    if (mEglSurface != null) {
                        mEgl.eglDestroySurface(mEglDisplay, mEglSurface);
                        mEglSurface = null;
                    }
                    mNewSurface = false;
                }

                if (mEglSurface == null) {
                    // block for Texture Views
                    if (mSurfaceTexture != null) {
                        mEglSurface = mEgl.eglCreateWindowSurface(mEglDisplay, mEglConfig,
                                mSurfaceTexture, null);
                        checkSurface();
                    // block for Surface Views
                    } else if (mSurfaceHolder != null) {
                        mEglSurface = mEgl.eglCreateWindowSurface(mEglDisplay, mEglConfig,
                            mSurfaceHolder, null);
                        checkSurface();
                    }
                }

                if (mEglSurface != null) {
                    if (!mEgl.eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
                        // If eglMakeCurrent failed, recreate EGL surface on next frame.
                        Log.w(LOG_TAG, "eglMakeCurrent failed "
                                + GLUtils.getEGLErrorString(mEgl.eglGetError()));
                        mNewSurface = true;
                        return;
                    }
                    // only if nDrawFrames() returns true do we need to swap buffers
                    if(nDrawFrame(mNativeProxy, mSurfaceWidth, mSurfaceHeight, false,
                            mProgress, mBackgroundColor, forceDraw)) {
                        if (!mEgl.eglSwapBuffers(mEglDisplay, mEglSurface)) {
                            int error = mEgl.eglGetError();
                            if (error == EGL10.EGL_BAD_SURFACE
                                || error == EGL10.EGL_BAD_NATIVE_WINDOW) {
                                // For some reason our surface was destroyed. Recreate EGL surface
                                // on next frame.
                                mNewSurface = true;
                                // This really shouldn't happen, but if it does we can recover
                                // easily by just not trying to use the surface anymore
                                Log.w(LOG_TAG, "swapBuffers failed "
                                    + GLUtils.getEGLErrorString(error));
                                return;
                            }

                            // Some other fatal EGL error happened, log an error and stop the
                            // animation.
                            throw new RuntimeException("Cannot swap buffers "
                                + GLUtils.getEGLErrorString(error));
                        }
                    }


                    // If animation stopped, release EGL surface.
                    if (!mIsRunning) {
                        // Ensure we always have a valid surface & context.
                        mEgl.eglMakeCurrent(mEglDisplay, mPBufferSurface, mPBufferSurface,
                                mEglContext);
                        mEgl.eglDestroySurface(mEglDisplay, mEglSurface);
                        mEglSurface = null;
                    }
                }
            } catch (Throwable t) {
                Log.e(LOG_TAG, "drawFrame failed", t);
                mIsRunning = false;
            }
        }

        private void checkSurface() throws RuntimeException {
            // ensure eglSurface was created
            if (mEglSurface == null || mEglSurface == EGL10.EGL_NO_SURFACE) {
                // If failed to create a surface, log an error and stop the animation
                int error = mEgl.eglGetError();
                throw new RuntimeException("createWindowSurface failed "
                    + GLUtils.getEGLErrorString(error));
            }
        }

        @Override
        public void doFrame(long frameTimeNanos) {
            if (mIsRunning) {
                // Schedule next frame.
                Choreographer.getInstance().postFrameCallback(this);

                // Advance animation.
                long durationNS = mDuration * 1000000;
                long timeSinceAnimationStartNS = frameTimeNanos - mAnimationStartTime;
                long animationProgressNS = timeSinceAnimationStartNS % durationNS;
                mProgress = animationProgressNS / (float)durationNS;
                if (timeSinceAnimationStartNS > durationNS) {
                    mAnimationStartTime += durationNS;  // prevents overflow
                }
                if (timeSinceAnimationStartNS > durationNS) {
                    if (mRepeatCounter > 0) {
                        mRepeatCounter--;
                    } else if (mRepeatCounter == 0) {
                        mIsRunning = false;
                        mProgress = 1;
                    }
                }
            }
            if (mValidSurface) {
                drawFrame();
            }
        }

        @Override
        public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
            // will be called on UI thread
            mValidSurface = true;
            try {
                runOnGLThread(() -> {
                    mSurfaceTexture = surface;
                    updateSurface(width, height);
                });
            }
            catch (Throwable t) {
                Log.e(LOG_TAG, "updateSurface failed", t);
                throw new RuntimeException(t);
            }
        }

        @Override
        public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
            // will be called on UI thread
            onSurfaceTextureAvailable(surface, width, height);
        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
            // will be called on UI thread
            onSurfaceTextureAvailable(null, 0, 0);
            mValidSurface = false;
            return true;
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {

        }

        // Inherited from SurfaceHolder
        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            mValidSurface = true;
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            mValidSurface = true;
            try {
                runOnGLThread(() -> {
                    mSurfaceHolder = holder;
                    updateSurface(width, height);
                });
            }
            catch (Throwable t) {
                Log.e(LOG_TAG, "updateSurface failed", t);
                throw new RuntimeException(t);
            }
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            mValidSurface = false;
            surfaceChanged(null, 0, 0, 0);
        }

        private native long nCreateProxy(long runner, ByteBuffer data);
        private native void nDeleteProxy(long nativeProxy);
        private native boolean nDrawFrame(long nativeProxy, int width, int height,
                                          boolean wideColorGamut, float progress,
                                          int backgroundColor, boolean forceDraw);
        private native long nGetDuration(long nativeProxy);
    }

    private static native long nCreateProxy();
    private static native void nDeleteProxy(long nativeProxy);
}

