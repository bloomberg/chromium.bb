/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "GraphicsContext3D.h"

#include "CachedImage.h"
#include "CanvasBuffer.h"
#include "CanvasByteArray.h"
#include "CanvasFloatArray.h"
#include "CanvasFramebuffer.h"
#include "CanvasIntArray.h"
#include "CanvasObject.h"
#include "CanvasProgram.h"
#include "CanvasRenderbuffer.h"
#include "CanvasRenderingContext3D.h"
#include "CanvasShader.h"
#include "CanvasTexture.h"
#include "CanvasUnsignedByteArray.h"
#include "CString.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "NotImplemented.h"
#include <wtf/FastMalloc.h>

#include <stdio.h>

#if PLATFORM(WIN_OS)
#include <windows.h>
#endif

#include "GL/glew.h"

#include "NativeImageSkia.h"

using namespace std;

namespace WebCore {

// GraphicsContext3DInternal -----------------------------------------------------

// Uncomment this to render to a separate window for debugging
// #define RENDER_TO_DEBUGGING_WINDOW

#define EXTRACT(val) (val == NULL ? 0 : val->object())

class GraphicsContext3DInternal {
public:
    GraphicsContext3DInternal();
    ~GraphicsContext3DInternal();

    void checkError() const;
    bool makeContextCurrent();

    PlatformGraphicsContext3D platformGraphicsContext3D() const;
    Platform3DObject platformTexture() const;

    void reshape(int width, int height);

    void beginPaint(CanvasRenderingContext3D* context);

    bool validateTextureTarget(int target);
    bool validateTextureParameter(int param);

    void activeTexture(unsigned long texture);
    void bindBuffer(unsigned long target,
                    CanvasBuffer* buffer);
    void bindTexture(unsigned long target,
                     CanvasTexture* texture);
    void bufferDataImpl(unsigned long target, int size, const void* data, unsigned long usage);
    void colorMask(bool red, bool green, bool blue, bool alpha);
    void depthMask(bool flag);
    void disable(unsigned long cap);
    void disableVertexAttribArray(unsigned long index);
    void enable(unsigned long cap);
    void enableVertexAttribArray(unsigned long index);
    void useProgram(CanvasProgram* program);
    void vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                             unsigned long stride, unsigned long offset);
    void viewportImpl(long x, long y, unsigned long width, unsigned long height);

private:
    unsigned int m_texture;
    unsigned int m_fbo;
    unsigned int m_depthBuffer;

    // Objects for flipping the render output vertically
    unsigned int m_altTexture;
    unsigned int m_quadVBO;
    unsigned int m_quadProgram;
    unsigned int m_quadTexLocation;

    // Storage for saving/restoring buffer, vertex attribute,
    // blending, and program state when drawing flipped quad
    unsigned int m_currentProgram;
    bool m_blendEnabled;
    bool m_depthTestEnabled;
    bool m_depthMaskEnabled;
    bool m_colorMask[4];
    unsigned int m_boundArrayBuffer;
    class VertexAttribPointerState {
    public:
        VertexAttribPointerState();

        bool enabled;
        unsigned long buffer;
        unsigned long indx;
        int size;
        int type;
        bool normalized;
        unsigned long stride;
        unsigned long offset;
    };
    VertexAttribPointerState m_vertexAttribPointerState[1];
    unsigned int m_activeTextureUnit;
    class TextureUnitState {
    public:
        TextureUnitState();

        unsigned long target;
        unsigned int texture;
    };
    TextureUnitState m_textureUnitState[1];
    int m_viewport[4];

#if PLATFORM(WIN_OS)
    HWND  m_canvasWindow;
    HDC   m_canvasDC;
    HGLRC m_contextObj;
#else
    #error Must port GraphicsContext3D to your platform
#endif
};

GraphicsContext3DInternal::VertexAttribPointerState::VertexAttribPointerState()
    : enabled(false)
    , buffer(0)
    , indx(0)
    , size(0)
    , type(0)
    , normalized(false)
    , stride(0)
    , offset(0)
{
}

GraphicsContext3DInternal::TextureUnitState::TextureUnitState()
    : target(0)
    , texture(0)
{
}

GraphicsContext3DInternal::GraphicsContext3DInternal()
    : m_texture(0)
    , m_fbo(0)
    , m_depthBuffer(0)
    , m_altTexture(0)
    , m_quadVBO(0)
    , m_quadProgram(0)
    , m_quadTexLocation(0)
    , m_currentProgram(0)
    , m_blendEnabled(false)
    , m_depthTestEnabled(false)
    , m_depthMaskEnabled(true)
    , m_boundArrayBuffer(0)
    , m_activeTextureUnit(0)
#if PLATFORM(WIN_OS)
    , m_canvasWindow(NULL)
    , m_canvasDC(NULL)
    , m_contextObj(NULL)
#else
#error Must port to your platform
#endif
{
    for (int i = 0; i < 4; i++) {
        m_viewport[i] = 0;
        m_colorMask[i] = true;
    }
#if PLATFORM(WIN_OS)
    WNDCLASS wc;
    if (!GetClassInfo(GetModuleHandle(NULL), L"CANVASGL", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASS));
        wc.style = CS_OWNDC;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = L"CANVASGL";

        if (!RegisterClass(&wc)) {
            printf("GraphicsContext3D: RegisterClass failed\n");
            return;
        }
    }

    m_canvasWindow = CreateWindow(L"CANVASGL", L"CANVASGL",
                                  WS_CAPTION,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!m_canvasWindow) {
        printf("GraphicsContext3DInternal: CreateWindow failed\n");
        return;
    }

    // get the device context
    m_canvasDC = GetDC(m_canvasWindow);
    if (!m_canvasDC) {
        printf("GraphicsContext3DInternal: GetDC failed\n");
        return;
    }

    // find default pixel format
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL
#ifdef RENDER_TO_DEBUGGING_WINDOW
        | PFD_DOUBLEBUFFER
#endif // RENDER_TO_DEBUGGING_WINDOW
        ;
    int pixelformat = ChoosePixelFormat(m_canvasDC, &pfd);

    // set the pixel format for the dc
    if (!SetPixelFormat(m_canvasDC, pixelformat, &pfd)) {
        printf("GraphicsContext3D: SetPixelFormat failed\n");
        return;
    }

    // create rendering context
    m_contextObj = wglCreateContext(m_canvasDC);
    if (!m_contextObj) {
        printf("GraphicsContext3D: wglCreateContext failed\n");
        return;
    }

    if (!wglMakeCurrent(m_canvasDC, m_contextObj)) {
        printf("GraphicsContext3D: wglMakeCurrent failed\n");
        return;
    }

#ifdef RENDER_TO_DEBUGGING_WINDOW
    typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
    PFNWGLSWAPINTERVALEXTPROC setSwapInterval = NULL;
    setSwapInterval = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
    if (setSwapInterval != NULL)
        setSwapInterval(1);
#endif // RENDER_TO_DEBUGGING_WINDOW

#else
#error Must port to your platform
#endif

    // Initialize GLEW and check for GL 2.0 support by the drivers.
    GLenum glewInitResult = glewInit();
    if (!glewIsSupported("GL_VERSION_2_0")) {
        printf("GraphicsContext3D: OpenGL 2.0 not supported\n");
        return;
    }
}

GraphicsContext3DInternal::~GraphicsContext3DInternal()
{
    makeContextCurrent();
#ifndef RENDER_TO_DEBUGGING_WINDOW
    glDeleteRenderbuffersEXT(1, &m_depthBuffer);
    glDeleteTextures(1, &m_texture);
    glDeleteTextures(1, &m_altTexture);
    glDeleteFramebuffersEXT(1, &m_fbo);
#endif // !RENDER_TO_DEBUGGING_WINDOW
#if PLATFORM(WIN_OS)
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(m_contextObj);
    ReleaseDC(m_canvasWindow, m_canvasDC);
    DestroyWindow(m_canvasWindow);
#else
#error Must port to your platform
#endif
    m_contextObj = NULL;
}

void GraphicsContext3DInternal::checkError() const
{
    // FIXME: This needs to only be done in the debug context. It
    // will need to throw an exception on error.
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("GL Error : %x\n", error);
        notImplemented();
    }
}

bool GraphicsContext3DInternal::makeContextCurrent()
{
#if PLATFORM(WIN_OS)
    if (wglGetCurrentContext() != m_contextObj)
        if (wglMakeCurrent(m_canvasDC, m_contextObj))
            return true;
#else
#error Must port to your platform
#endif
    return false;
}

PlatformGraphicsContext3D GraphicsContext3DInternal::platformGraphicsContext3D() const
{
    return m_contextObj;
}

Platform3DObject GraphicsContext3DInternal::platformTexture() const
{
    return m_texture;
}

static int createTextureObject()
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

void GraphicsContext3DInternal::reshape(int width, int height)
{
#ifdef RENDER_TO_DEBUGGING_WINDOW
    SetWindowPos(m_canvasWindow, HWND_TOP, 0, 0, width, height,
                 SWP_NOMOVE);
    ShowWindow(m_canvasWindow, SW_SHOW);
#endif

    makeContextCurrent();

#ifndef RENDER_TO_DEBUGGING_WINDOW
    if (m_texture == 0) {
        // Generate the texture objects
        m_texture = createTextureObject();
        m_altTexture = createTextureObject();

        // Generate the framebuffer object
        glGenFramebuffersEXT(1, &m_fbo);

        // Generate the depth buffer
        glGenRenderbuffersEXT(1, &m_depthBuffer);

        checkError();
    }

    // Reallocate the color and depth buffers
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, m_altTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthBuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width, height);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthBuffer);
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        printf("GraphicsContext3D: framebuffer was incomplete\n");

        // FIXME: cleanup.
        notImplemented();
    }
#endif // RENDER_TO_DEBUGGING_WINDOW

    glClear(GL_COLOR_BUFFER_BIT);
    viewportImpl(0, 0, width, height);
}

void GraphicsContext3DInternal::beginPaint(CanvasRenderingContext3D* context)
{
    makeContextCurrent();

#ifdef RENDER_TO_DEBUGGING_WINDOW
    SwapBuffers(m_canvasDC);
#else
    if (m_quadVBO == 0) {
        // Prepare necessary objects for rendering.
        glGenBuffers(1, &m_quadVBO);
        GLfloat vertices[] = {-1.0f, -1.0f,
                               1.0f, -1.0f,
                               1.0f,  1.0f,
                              -1.0f, -1.0f,
                               1.0f,  1.0f,
                              -1.0f,  1.0f};
        glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // Vertex shader does vertical flip.
        const GLchar* vertexShaderSrc =
            "attribute vec2 g_Position;\n"
            "varying vec2 texCoord;\n"
            "void main()\n"
            "{\n"
            "  texCoord = vec2((g_Position.x + 1.0) * 0.5,\n"
            "                  (1.0 - g_Position.y) * 0.5);\n"
            "  gl_Position = vec4(g_Position, 0.0, 1.0);\n"
            "}\n";
        // Fragment shader does optional premultiplication of alpha
        // into color channels.
        const GLchar* fragmentShaderNoPremultSrc =
            "varying vec2 texCoord;\n"
            "uniform sampler2D texSampler;\n"
            "void main()\n"
            "{\n"
            "  gl_FragColor = texture2D(texSampler, texCoord);\n"
            "}\n";
        const GLchar* fragmentShaderPremultSrc =
            "varying vec2 texCoord;\n"
            "uniform sampler2D texSampler;\n"
            "void main()\n"
            "{\n"
            "  vec4 color = texture2D(texSampler, texCoord);\n"
            "  gl_FragColor = vec4(color.r * color.a,\n"
            "                      color.g * color.a,\n"
            "                      color.b * color.a,\n"
            "                      color.a);\n"
            "}\n";
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSrc, NULL);
        checkError();
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        // FIXME: hook up fragmentShaderPremultSrc based on canvas
        // context attributes.
        glShaderSource(fragmentShader, 1, &fragmentShaderNoPremultSrc, NULL);
        checkError();
        m_quadProgram = glCreateProgram();
        glAttachShader(m_quadProgram, vertexShader);
        glAttachShader(m_quadProgram, fragmentShader);
        glBindAttribLocation(m_quadProgram, 0, "g_Position");
        glLinkProgram(m_quadProgram);
        checkError();
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        m_quadTexLocation = glGetUniformLocation(m_quadProgram, "texSampler");
        checkError();
    }

    // We've just rendered a frame into m_texture. Bind m_altTexture
    // as the framebuffer texture, and draw m_texture on to a quad,
    // flipping it vertically and performing alpha premultiplication
    // and color channel swizzling. Then read back the FBO.
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_altTexture, 0);
    glUseProgram(m_quadProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glUniform1i(m_quadTexLocation, 0);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(false);
    glColorMask(true, true, true, true);

    HTMLCanvasElement* canvas = context->canvas();
    ImageBuffer* imageBuffer = canvas->buffer();
    const SkBitmap& bitmap = *imageBuffer->context()->platformContext()->bitmap();
    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    glViewport(0, 0, bitmap.width(), bitmap.height());

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Read back the frame buffer.
    SkAutoLockPixels bitmapLock(bitmap);
    glReadPixels(0, 0, bitmap.width(), bitmap.height(), GL_BGRA, GL_UNSIGNED_BYTE, bitmap.getPixels());

    // Restore the previous FBO state.
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);

    // Restore the user's OpenGL state.
    glUseProgram(m_currentProgram);
    const VertexAttribPointerState& state = m_vertexAttribPointerState[0];
    if (state.buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, state.buffer);
        glVertexAttribPointer(state.indx, state.size, state.type, state.normalized,
                              state.stride, reinterpret_cast<void*>(static_cast<intptr_t>(state.offset)));
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_boundArrayBuffer);
    if (state.enabled)
        glEnableVertexAttribArray(0);
    else
        glDisableVertexAttribArray(0);
    const TextureUnitState& texState = m_textureUnitState[0];
    glBindTexture(texState.target, texState.texture);
    glActiveTexture(m_activeTextureUnit);
    if (m_blendEnabled)
        glEnable(GL_BLEND);
    if (m_depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    glDepthMask(m_depthMaskEnabled);
    glColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
    glViewport(m_viewport[0], m_viewport[1],
               m_viewport[2], m_viewport[3]);
#endif // RENDER_TO_DEBUGGING_WINDOW
}

bool GraphicsContext3DInternal::validateTextureTarget(int target)
{
    return (target == GL_TEXTURE_2D ||
            target == GL_TEXTURE_CUBE_MAP);
}

bool GraphicsContext3DInternal::validateTextureParameter(int param)
{
    return (param == GL_TEXTURE_MAG_FILTER ||
            param == GL_TEXTURE_MIN_FILTER ||
            param == GL_TEXTURE_WRAP_S ||
            param == GL_TEXTURE_WRAP_T);
}

void GraphicsContext3DInternal::activeTexture(unsigned long texture)
{
    // FIXME: query number of textures available.
    if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0+32)
        // FIXME: raise exception.
        return;

    makeContextCurrent();
    m_activeTextureUnit = texture;
    glActiveTexture(texture);
}

void GraphicsContext3DInternal::bindBuffer(unsigned long target,
                                           CanvasBuffer* buffer)
{
    makeContextCurrent();
    GLuint bufID = EXTRACT(buffer);
    if (target == GL_ARRAY_BUFFER)
        m_boundArrayBuffer = bufID;
    glBindBuffer(target, bufID);
}

// If we didn't have to hack GL_TEXTURE_WRAP_R for cube maps,
// we could just use:
// GL_SAME_METHOD_2_X2(BindTexture, bindTexture, unsigned long, CanvasTexture*)
void GraphicsContext3DInternal::bindTexture(unsigned long target,
                                            CanvasTexture* texture)
{
    makeContextCurrent();
    unsigned int textureObject = EXTRACT(texture);

    if (m_activeTextureUnit == 0) {
        TextureUnitState& state = m_textureUnitState[m_activeTextureUnit];
        state.target = target;
        state.texture = textureObject;
    }

    glBindTexture(target, textureObject);

    // FIXME: GL_TEXTURE_WRAP_R isn't exposed in the OpenGL ES 2.0
    // API. On desktop OpenGL implementations it seems necessary to
    // set this wrap mode to GL_CLAMP_TO_EDGE to get correct behavior
    // of cube maps.
    if (texture != NULL) {
        if (target == GL_TEXTURE_CUBE_MAP) {
            if (!texture->isCubeMapRWrapModeInitialized()) {
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                texture->setCubeMapRWrapModeInitialized(true);
            }
        } else
            texture->setCubeMapRWrapModeInitialized(false);
    }
}

void GraphicsContext3DInternal::bufferDataImpl(unsigned long target, int size, const void* data, unsigned long usage)
{
    makeContextCurrent();
    // FIXME: make this verification more efficient.
    GLint binding = 0;
    GLenum binding_target = GL_ARRAY_BUFFER_BINDING;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        binding_target = GL_ELEMENT_ARRAY_BUFFER_BINDING;
    glGetIntegerv(binding_target, &binding);
    if (binding <= 0) {
        // FIXME: raise exception.
        // LogMessagef(("bufferData: no buffer bound"));
        return;
    }

    glBufferData(target,
                   size,
                   data,
                   usage);
}

void GraphicsContext3DInternal::colorMask(bool red, bool green, bool blue, bool alpha)
{
    makeContextCurrent();
    m_colorMask[0] = red;
    m_colorMask[1] = green;
    m_colorMask[2] = blue;
    m_colorMask[3] = alpha;
    glColorMask(red, green, blue, alpha);
}

void GraphicsContext3DInternal::depthMask(bool flag)
{
    makeContextCurrent();
    m_depthMaskEnabled = flag;
    glDepthMask(flag);
}

void GraphicsContext3DInternal::disable(unsigned long cap)
{
    makeContextCurrent();
    switch (cap) {
        case GL_BLEND:
            m_blendEnabled = false;
        case GL_DEPTH_TEST:
            m_depthTestEnabled = false;
        default:
            break;
    }
    glDisable(cap);
}

void GraphicsContext3DInternal::disableVertexAttribArray(unsigned long index)
{
    makeContextCurrent();
    if (index == 0) {
        m_vertexAttribPointerState[0].enabled = false;
    }
    glDisableVertexAttribArray(index);
}

void GraphicsContext3DInternal::enable(unsigned long cap)
{
    makeContextCurrent();
    switch (cap) {
        case GL_BLEND:
            m_blendEnabled = true;
        case GL_DEPTH_TEST:
            m_depthTestEnabled = true;
        default:
            break;
    }
    glEnable(cap);
}

void GraphicsContext3DInternal::enableVertexAttribArray(unsigned long index)
{
    makeContextCurrent();
    if (index == 0)
        m_vertexAttribPointerState[0].enabled = true;
    glEnableVertexAttribArray(index);
}

void GraphicsContext3DInternal::useProgram(CanvasProgram* program)
{
    makeContextCurrent();
    m_currentProgram = EXTRACT(program);
    glUseProgram(m_currentProgram);
}

void GraphicsContext3DInternal::vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                                                    unsigned long stride, unsigned long offset)
{
    makeContextCurrent();

    if (m_boundArrayBuffer <= 0) {
        // FIXME: raise exception.
        // LogMessagef(("bufferData: no buffer bound"));
        return;
    }

    if (indx == 0) {
        VertexAttribPointerState& state = m_vertexAttribPointerState[0];
        state.buffer = m_boundArrayBuffer;
        state.indx = indx;
        state.size = size;
        state.type = type;
        state.normalized = normalized;
        state.stride = stride;
        state.offset = offset;
    }

    glVertexAttribPointer(indx, size, type, normalized, stride,
                          reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3DInternal::viewportImpl(long x, long y, unsigned long width, unsigned long height)
{
    m_viewport[0] = x;
    m_viewport[1] = y;
    m_viewport[2] = width;
    m_viewport[3] = height;
    glViewport(x, y, width, height);
}

// GraphicsContext3D -----------------------------------------------------

#define GRAPHICS_CONTEXT_NAME GraphicsContext3D

/* Helper macros for when we're just wrapping a gl method, so that
 * we can avoid having to type this 500 times.  Note that these MUST
 * NOT BE USED if we need to check any of the parameters.
 */

#define GL_SAME_METHOD_0(glname, name)                                         \
void GRAPHICS_CONTEXT_NAME::##name()                                           \
{                                                                              \
    makeContextCurrent(); gl##glname();                                        \
}

#define GL_SAME_METHOD_1(glname, name, t1)                                     \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1)                                      \
{                                                                              \
    makeContextCurrent(); gl##glname(a1);                                      \
}

#define GL_SAME_METHOD_1_X(glname, name, t1)                                   \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1)                                      \
{                                                                              \
    makeContextCurrent(); gl##glname(EXTRACT(a1));                             \
}

#define GL_SAME_METHOD_2(glname, name, t1, t2)                                 \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2)                               \
{                                                                              \
    makeContextCurrent(); gl##glname(a1,a2);                                   \
}

#define GL_SAME_METHOD_2_X12(glname, name, t1, t2)                             \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2)                               \
{                                                                              \
    makeContextCurrent(); gl##glname(EXTRACT(a1),EXTRACT(a2));                 \
}

#define GL_SAME_METHOD_2_X2(glname, name, t1, t2)                              \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2)                               \
{                                                                              \
    makeContextCurrent(); gl##glname(a1,EXTRACT(a2));                          \
}

#define GL_SAME_METHOD_3(glname, name, t1, t2, t3)                             \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2, t3 a3)                        \
{                                                                              \
    makeContextCurrent(); gl##glname(a1,a2,a3);                                \
}

#define GL_SAME_METHOD_3_X12(glname, name, t1, t2, t3)                         \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2, t3 a3)                        \
{                                                                              \
    makeContextCurrent(); gl##glname(EXTRACT(a1),EXTRACT(a2),a3);              \
}

#define GL_SAME_METHOD_3_X2(glname, name, t1, t2, t3)                          \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2, t3 a3)                        \
{                                                                              \
    makeContextCurrent(); gl##glname(a1,EXTRACT(a2),a3);                       \
}

#define GL_SAME_METHOD_4(glname, name, t1, t2, t3, t4)                         \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2, t3 a3, t4 a4)                 \
{                                                                              \
    makeContextCurrent(); gl##glname(a1,a2,a3,a4);                             \
}

#define GL_SAME_METHOD_4_X4(glname, name, t1, t2, t3, t4)                      \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2, t3 a3, t4 a4)                 \
{                                                                              \
    makeContextCurrent(); gl##glname(a1,a2,a3,EXTRACT(a4));                    \
}

#define GL_SAME_METHOD_5(glname, name, t1, t2, t3, t4, t5)                     \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)          \
{                                                                              \
    makeContextCurrent(); gl##glname(a1,a2,a3,a4,a5);                          \
}

#define GL_SAME_METHOD_5_X4(glname, name, t1, t2, t3, t4, t5)                  \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)          \
{                                                                              \
    makeContextCurrent(); gl##glname(a1,a2,a3,EXTRACT(a4),a5);                 \
}

#define GL_SAME_METHOD_6(glname, name, t1, t2, t3, t4, t5, t6)                 \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6)   \
{                                                                              \
    makeContextCurrent(); gl##glname(a1,a2,a3,a4,a5,a6);                       \
}

#define GL_SAME_METHOD_8(glname, name, t1, t2, t3, t4, t5, t6, t7, t8)                       \
void GRAPHICS_CONTEXT_NAME::##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8)   \
{                                                                                            \
    makeContextCurrent(); gl##glname(a1,a2,a3,a4,a5,a6,a7,a8);                               \
}

GraphicsContext3D::GraphicsContext3D()
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_internal(new GraphicsContext3DInternal())
{
}

GraphicsContext3D::~GraphicsContext3D()
{
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_internal->platformGraphicsContext3D();
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_internal->platformTexture();
}

void GraphicsContext3D::checkError() const
{
    m_internal->checkError();
}

void GraphicsContext3D::makeContextCurrent()
{
    m_internal->makeContextCurrent();
}

void GraphicsContext3D::reshape(int width, int height)
{
    if (width == m_currentWidth && height == m_currentHeight)
        return;

    m_currentWidth = width;
    m_currentHeight = height;

    m_internal->reshape(width, height);
}

void GraphicsContext3D::beginPaint(CanvasRenderingContext3D* context)
{
    m_internal->beginPaint(context);
}

void GraphicsContext3D::endPaint()
{
}

int GraphicsContext3D::sizeInBytes(int type)
{
    switch (type) {
        case GL_BYTE:
            return sizeof(GLbyte);
        case GL_UNSIGNED_BYTE:
            return sizeof(GLubyte);
        case GL_SHORT:
            return sizeof(GLshort);
        case GL_UNSIGNED_SHORT:
            return sizeof(GLushort);
        case GL_INT:
            return sizeof(GLint);
        case GL_UNSIGNED_INT:
            return sizeof(GLuint);
        case GL_FLOAT:
            return sizeof(GLfloat);
        default:
            return 0;
    }
}

unsigned GraphicsContext3D::createBuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenBuffers(1, &o);
    return o;
}

unsigned GraphicsContext3D::createFramebuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenFramebuffers(1, &o);
    return o;
}

unsigned GraphicsContext3D::createProgram()
{
    makeContextCurrent();
    return glCreateProgram();
}

unsigned GraphicsContext3D::createRenderbuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenRenderbuffers(1, &o);
    return o;
}

unsigned GraphicsContext3D::createShader(ShaderType type)
{
    makeContextCurrent();
    return glCreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

unsigned GraphicsContext3D::createTexture()
{
    makeContextCurrent();
    GLuint o;
    glGenTextures(1, &o);
    return o;
}

void GraphicsContext3D::deleteBuffer(unsigned buffer)
{
    makeContextCurrent();
    glDeleteBuffers(1, &buffer);
}

void GraphicsContext3D::deleteFramebuffer(unsigned framebuffer)
{
    makeContextCurrent();
    glDeleteFramebuffers(1, &framebuffer);
}

void GraphicsContext3D::deleteProgram(unsigned program)
{
    makeContextCurrent();
    glDeleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(unsigned renderbuffer)
{
    makeContextCurrent();
    glDeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContext3D::deleteShader(unsigned shader)
{
    makeContextCurrent();
    glDeleteShader(shader);
}

void GraphicsContext3D::deleteTexture(unsigned texture)
{
    makeContextCurrent();
    glDeleteTextures(1, &texture);
}

void GraphicsContext3D::activeTexture(unsigned long texture)
{
    m_internal->activeTexture(texture);
}

GL_SAME_METHOD_2_X12(AttachShader, attachShader, CanvasProgram*, CanvasShader*)

void GraphicsContext3D::bindAttribLocation(CanvasProgram* program,
                                           unsigned long index,
                                           const String& name)
{
    if (!program)
        return;
    makeContextCurrent();
    glBindAttribLocation(EXTRACT(program), index, name.utf8().data());
}

void GraphicsContext3D::bindBuffer(unsigned long target,
                                   CanvasBuffer* buffer)
{
    m_internal->bindBuffer(target, buffer);
}

GL_SAME_METHOD_2_X2(BindFramebuffer, bindFramebuffer, unsigned long, CanvasFramebuffer*)

GL_SAME_METHOD_2_X2(BindRenderbuffer, bindRenderbuffer, unsigned long, CanvasRenderbuffer*)

// If we didn't have to hack GL_TEXTURE_WRAP_R for cube maps,
// we could just use:
// GL_SAME_METHOD_2_X2(BindTexture, bindTexture, unsigned long, CanvasTexture*)
void GraphicsContext3D::bindTexture(unsigned long target,
                                    CanvasTexture* texture)
{
    m_internal->bindTexture(target, texture);
}

GL_SAME_METHOD_4(BlendColor, blendColor, double, double, double, double)

GL_SAME_METHOD_1(BlendEquation, blendEquation, unsigned long)

GL_SAME_METHOD_2(BlendEquationSeparate, blendEquationSeparate, unsigned long, unsigned long)

GL_SAME_METHOD_2(BlendFunc, blendFunc, unsigned long, unsigned long)

GL_SAME_METHOD_4(BlendFuncSeparate, blendFuncSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

void GraphicsContext3D::bufferData(unsigned long target, int size, unsigned long usage)
{
    m_internal->bufferDataImpl(target, size, NULL, usage);
}

void GraphicsContext3D::bufferData(unsigned long target, CanvasArray* array, unsigned long usage)
{
    m_internal->bufferDataImpl(target, array->sizeInBytes(), array->baseAddress(), usage);
}

void GraphicsContext3D::bufferSubData(unsigned long target, long offset, CanvasArray* array)
{
    if (!array || !array->length())
        return;

    makeContextCurrent();
    // FIXME: make this verification more efficient.
    GLint binding = 0;
    GLenum binding_target = GL_ARRAY_BUFFER_BINDING;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        binding_target = GL_ELEMENT_ARRAY_BUFFER_BINDING;
    glGetIntegerv(binding_target, &binding);
    if (binding <= 0) {
        // FIXME: raise exception.
        // LogMessagef(("bufferSubData: no buffer bound"));
        return;
    }
    glBufferSubData(target, offset, array->sizeInBytes(), array->baseAddress());
}

unsigned long GraphicsContext3D::checkFramebufferStatus(unsigned long target)
{
    makeContextCurrent();
    return glCheckFramebufferStatus(target);
}

GL_SAME_METHOD_1(Clear, clear, unsigned long)

GL_SAME_METHOD_4(ClearColor, clearColor, double, double, double, double)

GL_SAME_METHOD_1(ClearDepth, clearDepth, double)

GL_SAME_METHOD_1(ClearStencil, clearStencil, long)

void GraphicsContext3D::colorMask(bool red, bool green, bool blue, bool alpha)
{
    m_internal->colorMask(red, green, blue, alpha);
}

GL_SAME_METHOD_1_X(CompileShader, compileShader, CanvasShader*)

GL_SAME_METHOD_8(CopyTexImage2D, copyTexImage2D, unsigned long, long, unsigned long, long, long, unsigned long, unsigned long, long)

GL_SAME_METHOD_8(CopyTexSubImage2D, copyTexSubImage2D, unsigned long, long, long, long, long, long, unsigned long, unsigned long)

GL_SAME_METHOD_1(CullFace, cullFace, unsigned long)

GL_SAME_METHOD_1(DepthFunc, depthFunc, unsigned long)

void GraphicsContext3D::depthMask(bool flag)
{
    m_internal->depthMask(flag);
}

GL_SAME_METHOD_2(DepthRange, depthRange, double, double)

void GraphicsContext3D::detachShader(CanvasProgram* program, CanvasShader* shader)
{
    if (!program || !shader)
        return;

    makeContextCurrent();
    glDetachShader(EXTRACT(program), EXTRACT(shader));
}

void GraphicsContext3D::disable(unsigned long cap)
{
    m_internal->disable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(unsigned long index)
{
    m_internal->disableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(unsigned long mode, long first, long count)
{
    switch (mode) {
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_POINTS:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
        case GL_LINES:
            break;
        default:
            // FIXME: output log message, raise exception.
            // LogMessage(NS_LITERAL_CSTRING("drawArrays: invalid mode"));
            // return NS_ERROR_DOM_SYNTAX_ERR;
            return;
    }

    if (first+count < first || first+count < count) {
        // FIXME: output log message, raise exception.
        // LogMessage(NS_LITERAL_CSTRING("drawArrays: overflow in first+count"));
        // return NS_ERROR_INVALID_ARG;
        return;
    }

    // FIXME: validate against currently bound buffer.
    // if (!ValidateBuffers(first+count))
    //     return NS_ERROR_INVALID_ARG;

    makeContextCurrent();
    glDrawArrays(mode, first, count);
}

void GraphicsContext3D::drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset)
{
    makeContextCurrent();
    // FIXME: make this verification more efficient.
    GLint binding = 0;
    GLenum binding_target = GL_ELEMENT_ARRAY_BUFFER_BINDING;
    glGetIntegerv(binding_target, &binding);
    if (binding <= 0) {
        // FIXME: raise exception.
        // LogMessagef(("bufferData: no buffer bound"));
        return;
    }
    glDrawElements(mode, count, type,
                   reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::enable(unsigned long cap)
{
    m_internal->enable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(unsigned long index)
{
    m_internal->enableVertexAttribArray(index);
}

GL_SAME_METHOD_0(Finish, finish)

GL_SAME_METHOD_0(Flush, flush)

GL_SAME_METHOD_4_X4(FramebufferRenderbuffer, framebufferRenderbuffer, unsigned long, unsigned long, unsigned long, CanvasRenderbuffer*)

GL_SAME_METHOD_5_X4(FramebufferTexture2D, framebufferTexture2D, unsigned long, unsigned long, unsigned long, CanvasTexture*, long)

GL_SAME_METHOD_1(FrontFace, frontFace, unsigned long)

void GraphicsContext3D::generateMipmap(unsigned long target)
{
    makeContextCurrent();
    if (glGenerateMipmapEXT) {
        glGenerateMipmapEXT(target);
    }
    // FIXME: provide alternative code path? This will be unpleasant
    // to implement if glGenerateMipmapEXT is not available -- it will
    // require a texture readback and re-upload.
}

int GraphicsContext3D::getAttribLocation(CanvasProgram* program, const String& name)
{
    if (!program)
        return -1;

    makeContextCurrent();
    return glGetAttribLocation(EXTRACT(program), name.utf8().data());
}

bool GraphicsContext3D::getBoolean(unsigned long pname)
{
    makeContextCurrent();
    GLboolean val;
    // FIXME: validate pname to ensure it returns only a single value.
    glGetBooleanv(pname, &val);
    return static_cast<bool>(val);
}

PassRefPtr<CanvasUnsignedByteArray> GraphicsContext3D::getBooleanv(unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

int GraphicsContext3D::getBufferParameteri(unsigned long target, unsigned long pname)
{
    makeContextCurrent();
    GLint data;
    glGetBufferParameteriv(target, pname, &data);
    return static_cast<int>(data);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getBufferParameteriv(unsigned long target, unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

unsigned long GraphicsContext3D::getError()
{
    makeContextCurrent();
    return glGetError();
}

float GraphicsContext3D::getFloat(unsigned long pname)
{
    makeContextCurrent();
    GLfloat val;
    // FIXME: validate pname to ensure it returns only a single value.
    glGetFloatv(pname, &val);
    return static_cast<float>(val);
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getFloatv(unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

int GraphicsContext3D::getFramebufferAttachmentParameteri(unsigned long target,
                                                          unsigned long attachment,
                                                          unsigned long pname)
{
    makeContextCurrent();
    GLint data;
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, &data);
    return static_cast<int>(data);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getFramebufferAttachmentParameteriv(unsigned long target,
                                                                                  unsigned long attachment,
                                                                                  unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

int GraphicsContext3D::getInteger(unsigned long pname)
{
    makeContextCurrent();
    GLint val;
    // FIXME: validate pname to ensure it returns only a single value.
    glGetIntegerv(pname, &val);
    return static_cast<int>(val);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getIntegerv(unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

int GraphicsContext3D::getProgrami(CanvasProgram* program,
                                   unsigned long pname)
{
    makeContextCurrent();
    GLint param;
    glGetProgramiv(EXTRACT(program), pname, &param);
    return static_cast<int>(param);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getProgramiv(CanvasProgram* program,
                                                           unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

String GraphicsContext3D::getProgramInfoLog(CanvasProgram* program)
{
    makeContextCurrent();
    GLuint programID = EXTRACT(program);
    GLint logLength;
    glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength == 0)
        return String();
    GLchar* log = NULL;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return String();
    GLsizei returnedLogLength;
    glGetProgramInfoLog(programID, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    String res = String::fromUTF8(log, returnedLogLength);
    fastFree(log);
    return res;
}

int GraphicsContext3D::getRenderbufferParameteri(unsigned long target,
                                                 unsigned long pname)
{
    makeContextCurrent();
    GLint param;
    glGetRenderbufferParameteriv(target, pname, &param);
    return static_cast<int>(param);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getRenderbufferParameteriv(unsigned long target,
                                                                         unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

int GraphicsContext3D::getShaderi(CanvasShader* shader,
                                  unsigned long pname)
{
    makeContextCurrent();
    GLint param;
    glGetShaderiv(EXTRACT(shader), pname, &param);
    return static_cast<int>(param);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getShaderiv(CanvasShader* shader,
                                                          unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

String GraphicsContext3D::getShaderInfoLog(CanvasShader* shader)
{
    makeContextCurrent();
    GLuint shaderID = EXTRACT(shader);
    GLint logLength;
    glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength == 0)
        return String();
    GLchar* log = NULL;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return String();
    GLsizei returnedLogLength;
    glGetShaderInfoLog(shaderID, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    String res = String::fromUTF8(log, returnedLogLength);
    fastFree(log);
    return res;
}

String GraphicsContext3D::getShaderSource(CanvasShader* shader)
{
    makeContextCurrent();
    GLuint shaderID = EXTRACT(shader);
    GLint logLength;
    glGetShaderiv(shaderID, GL_SHADER_SOURCE_LENGTH, &logLength);
    if (logLength == 0)
        return String();
    GLchar* log = NULL;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return String();
    GLsizei returnedLogLength;
    glGetShaderSource(shaderID, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    String res = String::fromUTF8(log, returnedLogLength);
    fastFree(log);
    return res;
}

String GraphicsContext3D::getString(unsigned long name)
{
    makeContextCurrent();
    return String::fromUTF8(reinterpret_cast<const char*>(glGetString(name)));
}

float GraphicsContext3D::getTexParameterf(unsigned long target, unsigned long pname)
{
    makeContextCurrent();
    if (!m_internal->validateTextureTarget(target)) {
        // FIXME: throw exception.
        return 0;
    }

    if (!m_internal->validateTextureParameter(pname)) {
        // FIXME: throw exception.
        return 0;
    }

    GLfloat param;
    glGetTexParameterfv(target, pname, &param);
    return static_cast<float>(param);
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getTexParameterfv(unsigned long target, unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

int GraphicsContext3D::getTexParameteri(unsigned long target, unsigned long pname)
{
    makeContextCurrent();
    if (!m_internal->validateTextureTarget(target)) {
        // FIXME: throw exception.
        return 0;
    }

    if (!m_internal->validateTextureParameter(pname)) {
        // FIXME: throw exception.
        return 0;
    }

    GLint param;
    glGetTexParameteriv(target, pname, &param);
    return static_cast<int>(param);
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getTexParameteriv(unsigned long target, unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

float GraphicsContext3D::getUniformf(CanvasProgram* program, long location)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getUniformfv(CanvasProgram* program, long location)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

int GraphicsContext3D::getUniformi(CanvasProgram* program, long location)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getUniformiv(CanvasProgram* program, long location)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

long GraphicsContext3D::getUniformLocation(CanvasProgram* program, const String& name)
{
    if (!program)
        return -1;

    makeContextCurrent();
    return glGetUniformLocation(EXTRACT(program), name.utf8().data());
}

float GraphicsContext3D::getVertexAttribf(unsigned long index,
                                          unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getVertexAttribfv(unsigned long index,
                                                                  unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

int GraphicsContext3D::getVertexAttribi(unsigned long index,
                                        unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getVertexAttribiv(unsigned long index,
                                                                unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return NULL;
}

long GraphicsContext3D::getVertexAttribOffset(unsigned long index, unsigned long pname)
{
    // FIXME: implement.
    notImplemented();
    return 0;
}

GL_SAME_METHOD_2(Hint, hint, unsigned long, unsigned long);

bool GraphicsContext3D::isBuffer(CanvasBuffer* buffer)
{
    makeContextCurrent();
    return glIsBuffer(EXTRACT(buffer));
}

bool GraphicsContext3D::isEnabled(unsigned long cap)
{
    makeContextCurrent();
    return glIsEnabled(cap);
}

bool GraphicsContext3D::isFramebuffer(CanvasFramebuffer* framebuffer)
{
    makeContextCurrent();
    return glIsFramebuffer(EXTRACT(framebuffer));
}

bool GraphicsContext3D::isProgram(CanvasProgram* program)
{
    makeContextCurrent();
    return glIsProgram(EXTRACT(program));
}

bool GraphicsContext3D::isRenderbuffer(CanvasRenderbuffer* renderbuffer)
{
    makeContextCurrent();
    return glIsRenderbuffer(EXTRACT(renderbuffer));
}

bool GraphicsContext3D::isShader(CanvasShader* shader)
{
    makeContextCurrent();
    return glIsShader(EXTRACT(shader));
}

bool GraphicsContext3D::isTexture(CanvasTexture* texture)
{
    makeContextCurrent();
    return glIsTexture(EXTRACT(texture));
}

GL_SAME_METHOD_1(LineWidth, lineWidth, double)

GL_SAME_METHOD_1_X(LinkProgram, linkProgram, CanvasProgram*)

void GraphicsContext3D::pixelStorei(unsigned long pname, long param)
{
    if (pname != GL_PACK_ALIGNMENT &&
        pname != GL_UNPACK_ALIGNMENT) {
        // FIXME: force fake GL error to be produced and throw
        // exception.
        return;
    }

    makeContextCurrent();
    glPixelStorei(pname, param);
}

GL_SAME_METHOD_2(PolygonOffset, polygonOffset, double, double)

void GraphicsContext3D::releaseShaderCompiler()
{
}

GL_SAME_METHOD_4(RenderbufferStorage, renderbufferStorage, unsigned long, unsigned long, unsigned long, unsigned long)

GL_SAME_METHOD_2(SampleCoverage, sampleCoverage, double, bool)

GL_SAME_METHOD_4(Scissor, scissor, long, long, unsigned long, unsigned long)

void GraphicsContext3D::shaderSource(CanvasShader* shader, const String& source)
{
    makeContextCurrent();
    CString str = source.utf8();
    const char* data = str.data();
    GLint length = str.length();
    glShaderSource(EXTRACT(shader), 1, &data, &length);
}

GL_SAME_METHOD_3(StencilFunc, stencilFunc, unsigned long, long, unsigned long)

GL_SAME_METHOD_4(StencilFuncSeparate, stencilFuncSeparate, unsigned long, unsigned long, long, unsigned long)

GL_SAME_METHOD_1(StencilMask, stencilMask, unsigned long)

GL_SAME_METHOD_2(StencilMaskSeparate, stencilMaskSeparate, unsigned long, unsigned long)

GL_SAME_METHOD_3(StencilOp, stencilOp, unsigned long, unsigned long, unsigned long)

GL_SAME_METHOD_4(StencilOpSeparate, stencilOpSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

int GraphicsContext3D::texImage2D(unsigned target,
                                  unsigned level,
                                  unsigned internalformat,
                                  unsigned width,
                                  unsigned height,
                                  unsigned border,
                                  unsigned format,
                                  unsigned type,
                                  CanvasArray* pixels)
{
    // FIXME: must do validation similar to JOGL's to ensure that
    // the incoming array is of the appropriate length.
    glTexImage2D(target,
                 level,
                 internalformat,
                 width,
                 height,
                 border,
                 format,
                 type,
                 pixels->baseAddress());
    return 0;
}

int GraphicsContext3D::texImage2D(unsigned target,
                                  unsigned level,
                                  unsigned internalformat,
                                  unsigned width,
                                  unsigned height,
                                  unsigned border,
                                  unsigned format,
                                  unsigned type,
                                  ImageData* pixels)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

// Remove premultiplied alpha from color channels.
// FIXME: this is lossy. Must retrieve original values from HTMLImageElement.
static void unmultiplyAlpha(unsigned char* rgbaData, int numPixels)
{
    for (int j = 0; j < numPixels; j++) {
        float b = rgbaData[4*j+0] / 255.0f;
        float g = rgbaData[4*j+1] / 255.0f;
        float r = rgbaData[4*j+2] / 255.0f;
        float a = rgbaData[4*j+3] / 255.0f;
        if (a > 0.0f) {
            b /= a;
            g /= a;
            r /= a;
            b = (b > 1.0f) ? 1.0f : b;
            g = (g > 1.0f) ? 1.0f : g;
            r = (r > 1.0f) ? 1.0f : r;
            rgbaData[4*j+0] = (unsigned char) (b * 255.0f);
            rgbaData[4*j+1] = (unsigned char) (g * 255.0f);
            rgbaData[4*j+2] = (unsigned char) (r * 255.0f);
        }
    }
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, HTMLImageElement* image,
                                  bool flipY, bool premultiplyAlpha)
{
    CachedImage* cachedImage = image->cachedImage();
    if (cachedImage == NULL) {
        ASSERT_NOT_REACHED();
        return -1;
    }
    Image* img = cachedImage->image();
    NativeImageSkia* skiaImage = img->nativeImageForCurrentFrame();
    if (skiaImage == NULL) {
        ASSERT_NOT_REACHED();
        return -1;
    }
    SkBitmap::Config skiaConfig = skiaImage->config();
    // FIXME: must support more image configurations.
    if (skiaConfig != SkBitmap::kARGB_8888_Config) {
        ASSERT_NOT_REACHED();
        return -1;
    }
    SkBitmap& skiaImageRef = *skiaImage;
    SkAutoLockPixels lock(skiaImageRef);
    int width = skiaImage->width();
    int height = skiaImage->height();
    if (flipY) {
        // Need to flip images vertically. To avoid making a copy of
        // the entire image, we perform a ton of glTexSubImage2D
        // calls. FIXME: should rethink this strategy for efficiency.
        glTexImage2D(target, level, GL_RGBA8,
                     width,
                     height,
                     0,
                     GL_BGRA,
                     GL_UNSIGNED_BYTE,
                     NULL);
        unsigned char* pixels =
            reinterpret_cast<unsigned char*>(skiaImage->getPixels());
        int rowBytes = skiaImage->rowBytes();

        unsigned char* row = NULL;
        bool allocatedRow = false;
        if (!premultiplyAlpha) {
            row = new unsigned char[rowBytes];
            allocatedRow = true;
        }
        for (int i = 0; i < height; i++) {
            if (premultiplyAlpha)
                row = pixels + (rowBytes * i);
            else {
                memcpy(row, pixels + (rowBytes * i), rowBytes);
                unmultiplyAlpha(row, width);
            }
            glTexSubImage2D(target, level, 0, height - i - 1,
                            width, 1,
                            GL_BGRA,
                            GL_UNSIGNED_BYTE,
                            row);
        }
        if (allocatedRow)
            delete[] row;
    } else {
        // The pixels of cube maps' faces are defined with a top-down
        // scanline ordering, unlike GL_TEXTURE_2D, so when uploading
        // these, the above vertical flip is the wrong thing to do.
        if (premultiplyAlpha)
            glTexImage2D(target, level, GL_RGBA8,
                         width,
                         height,
                         0,
                         GL_BGRA,
                         GL_UNSIGNED_BYTE,
                         skiaImage->getPixels());
        else {
            glTexImage2D(target, level, GL_RGBA8,
                         width,
                         height,
                         0,
                         GL_BGRA,
                         GL_UNSIGNED_BYTE,
                         NULL);
            unsigned char* pixels =
                reinterpret_cast<unsigned char*>(skiaImage->getPixels());
            int rowBytes = skiaImage->rowBytes();
            unsigned char* row = new unsigned char[rowBytes];
            for (int i = 0; i < height; i++) {
                memcpy(row, pixels + (rowBytes * i), rowBytes);
                unmultiplyAlpha(row, width);
                glTexSubImage2D(target, level, 0, i,
                                width, 1,
                                GL_BGRA,
                                GL_UNSIGNED_BYTE,
                                row);
            }
            delete[] row;
        }
    }
    return 0;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas,
                                  bool flipY, bool premultiplyAlpha)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, HTMLVideoElement* video,
                                  bool flipY, bool premultiplyAlpha)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

GL_SAME_METHOD_3(TexParameterf, texParameterf, unsigned, unsigned, float);

GL_SAME_METHOD_3(TexParameteri, texParameteri, unsigned, unsigned, int);

int GraphicsContext3D::texSubImage2D(unsigned target,
                                     unsigned level,
                                     unsigned xoffset,
                                     unsigned yoffset,
                                     unsigned width,
                                     unsigned height,
                                     unsigned format,
                                     unsigned type,
                                     CanvasArray* pixels)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target,
                                     unsigned level,
                                     unsigned xoffset,
                                     unsigned yoffset,
                                     unsigned width,
                                     unsigned height,
                                     unsigned format,
                                     unsigned type,
                                     ImageData* pixels)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target,
                                     unsigned level,
                                     unsigned xoffset,
                                     unsigned yoffset,
                                     unsigned width,
                                     unsigned height,
                                     HTMLImageElement* image,
                                     bool flipY,
                                     bool premultiplyAlpha)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target,
                                     unsigned level,
                                     unsigned xoffset,
                                     unsigned yoffset,
                                     unsigned width,
                                     unsigned height,
                                     HTMLCanvasElement* canvas,
                                     bool flipY,
                                     bool premultiplyAlpha)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target,
                                     unsigned level,
                                     unsigned xoffset,
                                     unsigned yoffset,
                                     unsigned width,
                                     unsigned height,
                                     HTMLVideoElement* video,
                                     bool flipY,
                                     bool premultiplyAlpha)
{
    // FIXME: implement.
    notImplemented();
    return -1;
}

GL_SAME_METHOD_2(Uniform1f, uniform1f, long, float)

void GraphicsContext3D::uniform1fv(long location, float* v, int size)
{
    makeContextCurrent();
    glUniform1fv(location, size, v);
}

GL_SAME_METHOD_2(Uniform1i, uniform1i, long, int)

void GraphicsContext3D::uniform1iv(long location, int* v, int size)
{
    makeContextCurrent();
    glUniform1iv(location, size, v);
}

GL_SAME_METHOD_3(Uniform2f, uniform2f, long, float, float)

void GraphicsContext3D::uniform2fv(long location, float* v, int size)
{
    makeContextCurrent();
    glUniform2fv(location, size, v);
}

GL_SAME_METHOD_3(Uniform2i, uniform2i, long, int, int)

void GraphicsContext3D::uniform2iv(long location, int* v, int size)
{
    makeContextCurrent();
    glUniform2iv(location, size, v);
}

GL_SAME_METHOD_4(Uniform3f, uniform3f, long, float, float, float)

void GraphicsContext3D::uniform3fv(long location, float* v, int size)
{
    makeContextCurrent();
    glUniform3fv(location, size, v);
}

GL_SAME_METHOD_4(Uniform3i, uniform3i, long, int, int, int)

void GraphicsContext3D::uniform3iv(long location, int* v, int size)
{
    makeContextCurrent();
    glUniform3iv(location, size, v);
}

GL_SAME_METHOD_5(Uniform4f, uniform4f, long, float, float, float, float)

void GraphicsContext3D::uniform4fv(long location, float* v, int size)
{
    makeContextCurrent();
    glUniform4fv(location, size, v);
}

GL_SAME_METHOD_5(Uniform4i, uniform4i, long, int, int, int, int)

void GraphicsContext3D::uniform4iv(long location, int* v, int size)
{
    makeContextCurrent();
    glUniform4iv(location, size, v);
}

void GraphicsContext3D::uniformMatrix2fv(long location, bool transpose, float* value, int size)
{
    makeContextCurrent();
    glUniformMatrix2fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix3fv(long location, bool transpose, float* value, int size)
{
    makeContextCurrent();
    glUniformMatrix3fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix4fv(long location, bool transpose, float* value, int size)
{
    makeContextCurrent();
    glUniformMatrix4fv(location, size, transpose, value);
}

void GraphicsContext3D::useProgram(CanvasProgram* program)
{
    m_internal->useProgram(program);
}

GL_SAME_METHOD_1_X(ValidateProgram, validateProgram, CanvasProgram*)

GL_SAME_METHOD_2(VertexAttrib1f, vertexAttrib1f, unsigned long, float)

void GraphicsContext3D::vertexAttrib1fv(unsigned long indx, float* values)
{
    makeContextCurrent();
    glVertexAttrib1fv(indx, values);
}

GL_SAME_METHOD_3(VertexAttrib2f, vertexAttrib2f, unsigned long, float, float)

void GraphicsContext3D::vertexAttrib2fv(unsigned long indx, float* values)
{
    makeContextCurrent();
    glVertexAttrib2fv(indx, values);
}

GL_SAME_METHOD_4(VertexAttrib3f, vertexAttrib3f, unsigned long, float, float, float)

void GraphicsContext3D::vertexAttrib3fv(unsigned long indx, float* values)
{
    makeContextCurrent();
    glVertexAttrib3fv(indx, values);
}

GL_SAME_METHOD_5(VertexAttrib4f, vertexAttrib4f, unsigned long, float, float, float, float)

void GraphicsContext3D::vertexAttrib4fv(unsigned long indx, float* values)
{
    makeContextCurrent();
    glVertexAttrib4fv(indx, values);
}

void GraphicsContext3D::vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                                            unsigned long stride, unsigned long offset)
{
    m_internal->vertexAttribPointer(indx, size, type, normalized, stride, offset);
}

void GraphicsContext3D::viewport(long x, long y, unsigned long width, unsigned long height)
{
    makeContextCurrent();
    m_internal->viewportImpl(x, y, width, height);
}

}

#endif // ENABLE(3D_CANVAS)
