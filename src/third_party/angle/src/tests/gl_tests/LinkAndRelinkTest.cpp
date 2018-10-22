//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// LinkAndRelinkFailureTest:
//   Link and relink failure tests for rendering pipeline and compute pipeline.

#include <vector>
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class LinkAndRelinkTest : public ANGLETest
{
  protected:
    LinkAndRelinkTest() {}
};

class LinkAndRelinkTestES31 : public ANGLETest
{
  protected:
    LinkAndRelinkTestES31() {}
};

// When a program link or relink fails, if you try to install the unsuccessfully
// linked program (via UseProgram) and start rendering or dispatch compute,
// We can not always report INVALID_OPERATION for rendering/compute pipeline.
// The result depends on the previous state: Whether a valid program is
// installed in current GL state before the link.
// If a program successfully relinks when it is in use, the program might
// change from a rendering program to a compute program in theory,
// or vice versa.

// When program link fails and no valid rendering program is installed in the GL
// state before the link, it should report an error for UseProgram and
// DrawArrays/DrawElements.
TEST_P(LinkAndRelinkTest, RenderingProgramFailsWithoutProgramInstalled)
{
    glUseProgram(0);
    GLuint program = glCreateProgram();

    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_FALSE(linkStatus);

    glUseProgram(program);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// When program link or relink fails and a valid rendering program is installed
// in the GL state before the link, using the failed program via UseProgram
// should report an error, but starting rendering should succeed.
// However, dispatching compute always fails.
TEST_P(LinkAndRelinkTest, RenderingProgramFailsWithProgramInstalled)
{
    // TODO(lucferron): Diagnose and fix
    // http://anglebug.com/2648
    ANGLE_SKIP_TEST_IF(IsVulkan());

    // Install a render program in current GL state via UseProgram, then render.
    // It should succeed.
    const std::string vsSource =
        R"(void main()
        {
        })";

    const std::string fsSource =
        R"(void main()
        {
        })";

    GLuint program = glCreateProgram();

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);

    EXPECT_NE(0u, vs);
    EXPECT_NE(0u, fs);

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glLinkProgram(program);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_TRUE(linkStatus);

    EXPECT_GL_NO_ERROR();

    glUseProgram(program);
    EXPECT_GL_NO_ERROR();
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    glDispatchCompute(8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Link failure, and a valid program has been installed in the GL state.
    GLuint programNull = glCreateProgram();

    glLinkProgram(programNull);
    glGetProgramiv(programNull, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_FALSE(linkStatus);

    // Starting rendering should succeed.
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    glDispatchCompute(8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Using the unsuccessfully linked program should report an error.
    glUseProgram(programNull);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Using the unsuccessfully linked program, that program should not
    // replace the program binary residing in the GL state. It will not make
    // the installed program invalid either, like what UseProgram(0) can do.
    // So, starting rendering should succeed.
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    glDispatchCompute(8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // We try to relink the installed program, but make it fail.

    // No vertex shader, relink fails.
    glDetachShader(program, vs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_FALSE(linkStatus);
    EXPECT_GL_NO_ERROR();

    // Starting rendering should succeed.
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    glDispatchCompute(8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Using the unsuccessfully relinked program should report an error.
    glUseProgram(program);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Using the unsuccessfully relinked program, that program should not
    // replace the program binary residing in the GL state. It will not make
    // the installed program invalid either, like what UseProgram(0) can do.
    // So, starting rendering should succeed.
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    glDispatchCompute(8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// When program link fails and no valid compute program is installed in the GL
// state before the link, it should report an error for UseProgram and
// DispatchCompute.
TEST_P(LinkAndRelinkTestES31, ComputeProgramFailsWithoutProgramInstalled)
{
    glUseProgram(0);
    GLuint program = glCreateProgram();

    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_FALSE(linkStatus);

    glUseProgram(program);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glDispatchCompute(8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// When program link or relink fails and a valid compute program is installed in
// the GL state before the link, using the failed program via UseProgram should
// report an error, but dispatching compute should succeed.
// However, starting rendering always fails.
TEST_P(LinkAndRelinkTestES31, ComputeProgramFailsWithProgramInstalled)
{
    // Install a compute program in the GL state via UseProgram, then dispatch
    // compute. It should succeed.
    const std::string csSource =
        R"(#version 310 es
        layout(local_size_x=1) in;
        void main()
        {
        })";

    GLuint program = glCreateProgram();

    GLuint cs = CompileShader(GL_COMPUTE_SHADER, csSource);
    EXPECT_NE(0u, cs);

    glAttachShader(program, cs);
    glDeleteShader(cs);

    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_TRUE(linkStatus);

    EXPECT_GL_NO_ERROR();

    glUseProgram(program);
    EXPECT_GL_NO_ERROR();
    glDispatchCompute(8, 4, 2);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Link failure, and a valid program has been installed in the GL state.
    GLuint programNull = glCreateProgram();

    glLinkProgram(programNull);
    glGetProgramiv(programNull, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_FALSE(linkStatus);

    // Dispatching compute should succeed.
    glDispatchCompute(8, 4, 2);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Using the unsuccessfully linked program should report an error.
    glUseProgram(programNull);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Using the unsuccessfully linked program, that program should not
    // replace the program binary residing in the GL state. It will not make
    // the installed program invalid either, like what UseProgram(0) can do.
    // So, dispatching compute should succeed.
    glDispatchCompute(8, 4, 2);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // We try to relink the installed program, but make it fail.

    // No compute shader, relink fails.
    glDetachShader(program, cs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_FALSE(linkStatus);
    EXPECT_GL_NO_ERROR();

    // Dispatching compute should succeed.
    glDispatchCompute(8, 4, 2);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Using the unsuccessfully relinked program should report an error.
    glUseProgram(program);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Using the unsuccessfully relinked program, that program should not
    // replace the program binary residing in the GL state. It will not make
    // the installed program invalid either, like what UseProgram(0) can do.
    // So, dispatching compute should succeed.
    glDispatchCompute(8, 4, 2);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// If you compile and link a compute program successfully and use the program,
// then dispatching compute can succeed, while starting rendering will fail.
// If you relink the compute program to a rendering program when it is in use,
// then dispatching compute will fail, but starting rendering can succeed.
TEST_P(LinkAndRelinkTestES31, RelinkProgramSucceedsFromComputeToRendering)
{
    const std::string csSource =
        R"(#version 310 es
        layout(local_size_x=1) in;
        void main()
        {
        })";

    GLuint program = glCreateProgram();

    GLuint cs = CompileShader(GL_COMPUTE_SHADER, csSource);
    EXPECT_NE(0u, cs);

    glAttachShader(program, cs);
    glDeleteShader(cs);

    glLinkProgram(program);
    glDetachShader(program, cs);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_TRUE(linkStatus);

    EXPECT_GL_NO_ERROR();

    glUseProgram(program);
    EXPECT_GL_NO_ERROR();
    glDispatchCompute(8, 4, 2);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    const std::string vsSource =
        R"(void main()
        {
        })";

    const std::string fsSource =
        R"(void main()
        {
        })";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    EXPECT_NE(0u, vs);
    EXPECT_NE(0u, fs);

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glLinkProgram(program);
    glDetachShader(program, vs);
    glDetachShader(program, fs);
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_TRUE(linkStatus);

    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    glDispatchCompute(8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// If you compile and link a rendering program successfully and use the program,
// then starting rendering can succeed, while dispatching compute will fail.
// If you relink the rendering program to a compute program when it is in use,
// then starting rendering will fail, but dispatching compute can succeed.
TEST_P(LinkAndRelinkTestES31, RelinkProgramSucceedsFromRenderingToCompute)
{
    const std::string vsSource =
        R"(void main()
        {
        })";

    const std::string fsSource =
        R"(void main()
        {
        })";

    GLuint program = glCreateProgram();

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);

    EXPECT_NE(0u, vs);
    EXPECT_NE(0u, fs);

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glLinkProgram(program);
    glDetachShader(program, vs);
    glDetachShader(program, fs);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_TRUE(linkStatus);

    EXPECT_GL_NO_ERROR();

    glUseProgram(program);
    EXPECT_GL_NO_ERROR();
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    glDispatchCompute(8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    const std::string csSource =
        R"(#version 310 es
        layout(local_size_x=1) in;
        void main()
        {
        })";

    GLuint cs = CompileShader(GL_COMPUTE_SHADER, csSource);
    EXPECT_NE(0u, cs);

    glAttachShader(program, cs);
    glDeleteShader(cs);

    glLinkProgram(program);
    glDetachShader(program, cs);
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    EXPECT_GL_TRUE(linkStatus);

    EXPECT_GL_NO_ERROR();

    glDispatchCompute(8, 4, 2);
    EXPECT_GL_NO_ERROR();

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

ANGLE_INSTANTIATE_TEST(LinkAndRelinkTest,
                       ES2_OPENGL(),
                       ES2_OPENGLES(),
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_OPENGL(),
                       ES3_OPENGLES(),
                       ES3_D3D11(),
                       ES2_VULKAN());
ANGLE_INSTANTIATE_TEST(LinkAndRelinkTestES31, ES31_OPENGL(), ES31_OPENGLES());

}  // namespace
