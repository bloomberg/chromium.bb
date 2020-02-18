//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureUploadBenchmark:
//   Performance test for uploading texture data.
//

#include "ANGLEPerfTest.h"

#include <iostream>
#include <random>
#include <sstream>

#include "test_utils/gl_raii.h"
#include "util/shader_utils.h"

using namespace angle;

namespace
{
constexpr unsigned int kIterationsPerStep = 2;

struct TextureUploadParams final : public RenderTestParams
{
    TextureUploadParams()
    {
        iterationsPerStep = kIterationsPerStep;
        trackGpuTime      = true;

        baseSize     = 1024;
        subImageSize = 64;

        webgl = false;
    }

    std::string story() const override;

    GLsizei baseSize;
    GLsizei subImageSize;

    bool webgl;
};

std::ostream &operator<<(std::ostream &os, const TextureUploadParams &params)
{
    os << params.backendAndStory().substr(1);
    return os;
}

std::string TextureUploadParams::story() const
{
    std::stringstream strstr;

    strstr << RenderTestParams::story();

    if (webgl)
    {
        strstr << "_webgl";
    }

    return strstr.str();
}

class TextureUploadBenchmarkBase : public ANGLERenderTest,
                                   public ::testing::WithParamInterface<TextureUploadParams>
{
  public:
    TextureUploadBenchmarkBase(const char *benchmarkName);

    void initializeBenchmark() override;
    void destroyBenchmark() override;

  protected:
    void initShaders();

    GLuint mProgram    = 0;
    GLint mPositionLoc = -1;
    GLint mSamplerLoc  = -1;
    GLuint mTexture    = 0;
    std::vector<float> mTextureData;
};

class TextureUploadSubImageBenchmark : public TextureUploadBenchmarkBase
{
  public:
    TextureUploadSubImageBenchmark() : TextureUploadBenchmarkBase("TexSubImage")
    {
        addExtensionPrerequisite("GL_EXT_texture_storage");
    }

    void initializeBenchmark() override
    {
        TextureUploadBenchmarkBase::initializeBenchmark();

        const auto &params = GetParam();
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, params.baseSize, params.baseSize);
    }

    void drawBenchmark() override;
};

class TextureUploadFullMipBenchmark : public TextureUploadBenchmarkBase
{
  public:
    TextureUploadFullMipBenchmark() : TextureUploadBenchmarkBase("TextureUpload") {}

    void drawBenchmark() override;
};

TextureUploadBenchmarkBase::TextureUploadBenchmarkBase(const char *benchmarkName)
    : ANGLERenderTest(benchmarkName, GetParam())
{
    setWebGLCompatibilityEnabled(GetParam().webgl);
    setRobustResourceInit(GetParam().webgl);

    // Crashes on nvidia+d3d11. http://crbug.com/945415
    if (GetParam().getRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        mSkipTest = true;
    }
}

void TextureUploadBenchmarkBase::initializeBenchmark()
{
    const auto &params = GetParam();

    initShaders();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, getWindow()->getWidth(), getWindow()->getHeight());

    if (params.webgl)
    {
        glRequestExtensionANGLE("GL_EXT_disjoint_timer_query");
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ASSERT_TRUE(params.baseSize >= params.subImageSize);
    mTextureData.resize(params.baseSize * params.baseSize * 4, 0.5);

    ASSERT_GL_NO_ERROR();
}

void TextureUploadBenchmarkBase::initShaders()
{
    constexpr char kVS[] = R"(attribute vec4 a_position;
void main()
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(precision mediump float;
uniform sampler2D s_texture;
void main()
{
    gl_FragColor = texture2D(s_texture, vec2(0, 0));
})";

    mProgram = CompileProgram(kVS, kFS);
    ASSERT_NE(0u, mProgram);

    mPositionLoc = glGetAttribLocation(mProgram, "a_position");
    mSamplerLoc  = glGetUniformLocation(mProgram, "s_texture");
    glUseProgram(mProgram);
    glUniform1i(mSamplerLoc, 0);

    glDisable(GL_DEPTH_TEST);

    ASSERT_GL_NO_ERROR();
}

void TextureUploadBenchmarkBase::destroyBenchmark()
{
    glDeleteTextures(1, &mTexture);
    glDeleteProgram(mProgram);
}

void TextureUploadSubImageBenchmark::drawBenchmark()
{
    const auto &params = GetParam();

    startGpuTimer();
    for (unsigned int iteration = 0; iteration < params.iterationsPerStep; ++iteration)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, rand() % (params.baseSize - params.subImageSize),
                        rand() % (params.baseSize - params.subImageSize), params.subImageSize,
                        params.subImageSize, GL_RGBA, GL_UNSIGNED_BYTE, mTextureData.data());

        // Perform a draw just so the texture data is flushed.  With the position attributes not
        // set, a constant default value is used, resulting in a very cheap draw.
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    stopGpuTimer();

    ASSERT_GL_NO_ERROR();
}

void TextureUploadFullMipBenchmark::drawBenchmark()
{
    const auto &params = GetParam();

    startGpuTimer();
    for (size_t it = 0; it < params.iterationsPerStep; ++it)
    {
        // Stage data for all mips
        GLint mip = 0;
        for (GLsizei levelSize = params.baseSize; levelSize > 0; levelSize >>= 1)
        {
            glTexImage2D(GL_TEXTURE_2D, mip++, GL_RGBA, levelSize, levelSize, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, mTextureData.data());
        }

        // Perform a draw just so the texture data is flushed.  With the position attributes not
        // set, a constant default value is used, resulting in a very cheap draw.
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    stopGpuTimer();

    ASSERT_GL_NO_ERROR();
}

TextureUploadParams D3D11Params(bool webglCompat)
{
    TextureUploadParams params;
    params.eglParameters = egl_platform::D3D11();
    params.webgl         = webglCompat;
    return params;
}

TextureUploadParams OpenGLOrGLESParams(bool webglCompat)
{
    TextureUploadParams params;
    params.eglParameters = egl_platform::OPENGL_OR_GLES();
    params.webgl         = webglCompat;
    return params;
}

TextureUploadParams VulkanParams(bool webglCompat)
{
    TextureUploadParams params;
    params.eglParameters = egl_platform::VULKAN();
    params.webgl         = webglCompat;
    return params;
}

}  // anonymous namespace

TEST_P(TextureUploadSubImageBenchmark, Run)
{
    run();
}

TEST_P(TextureUploadFullMipBenchmark, Run)
{
    run();
}

using namespace params;

ANGLE_INSTANTIATE_TEST(TextureUploadSubImageBenchmark,
                       D3D11Params(false),
                       D3D11Params(true),
                       OpenGLOrGLESParams(false),
                       OpenGLOrGLESParams(true),
                       VulkanParams(false),
                       NullDevice(VulkanParams(false)),
                       VulkanParams(true));

ANGLE_INSTANTIATE_TEST(TextureUploadFullMipBenchmark,
                       D3D11Params(false),
                       D3D11Params(true),
                       OpenGLOrGLESParams(false),
                       OpenGLOrGLESParams(true),
                       VulkanParams(false),
                       NullDevice(VulkanParams(false)),
                       VulkanParams(true));
