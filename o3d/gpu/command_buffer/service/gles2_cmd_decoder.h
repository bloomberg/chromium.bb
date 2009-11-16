// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2Decoder class.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_GLES2_CMD_DECODER_H
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_GLES2_CMD_DECODER_H

#include <map>
#include "base/scoped_ptr.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/common_decoder.h"

namespace command_buffer {
namespace gles2 {

// This class maps one set of ids to another.
class IdMap {
 public:
  // Maps a client_id to a service_id. Return false if the client_id or
  // service_id are already mapped to something else.
  bool AddMapping(GLuint client_id, GLuint service_id);

  // Unmaps a pair of ids. Returns false if the pair were not previously mapped.
  bool RemoveMapping(GLuint client_id, GLuint service_id);

  // Gets the corresponding service_id for the given client_id.
  // Returns false if there is no corresponding service_id.
  bool GetServiceId(GLuint client_id, GLuint* service_id);

  // Gets the corresponding client_id for the given service_id.
  // Returns false if there is no corresponding client_id.
  bool GetClientId(GLuint service_id, GLuint* client_id);

 private:
  // TODO(gman): Replace with faster implementation.
  typedef std::map<GLuint, GLuint> MapType;
  MapType id_map_;
};

// This class implements the AsyncAPIInterface interface, decoding GAPI
// commands and sending them to a GAPI interface.
class GLES2Decoder : public CommonDecoder {
 public:
  typedef parse_error::ParseError ParseError;

  GLES2Decoder();
  virtual ~GLES2Decoder() {
  }

  // Overridden from AsyncAPIInterface.
  virtual ParseError DoCommand(unsigned int command,
                               unsigned int arg_count,
                               const void* args);

  // Overridden from AsyncAPIInterface.
  virtual const char* GetCommandName(unsigned int command_id);

 private:
  // Gets the address of shared memory.
  // Parameters:
  //   shm_id: the id of the shared memory buffer.
  //   offset: the offset of the data in the shared memory buffer.
  //   size: size of area to get.
  // Returns:
  //   NULL if shm_id isn't a valid shared memory buffer ID or if the offset is
  //   outside or if the offset + size are outside.
  void* GetSharedMemory(unsigned int shm_id, unsigned int offset,
                        unsigned int size);

  // Typed version of GetSharedMemory
  template <typename T>
  T GetSharedMemoryAs(unsigned int shm_id, unsigned int offset,
                      unsigned int size) {
    return static_cast<T>(GetSharedMemory(shm_id, offset, size));
  }

  // Template to help call glGenXXX functions.
  template <void gl_gen_function(GLsizei, GLuint*)>
  bool GenGLObjects(GLsizei n, const GLuint* client_ids) {
    // TODO(gman): Verify client ids are unused.
    scoped_array<GLuint>temp(new GLuint[n]);
    gl_gen_function(n, temp.get());
    // TODO(gman): check for success before copying results.
    for (GLsizei ii = 0; ii < n; ++ii) {
      if (!id_map_.AddMapping(client_ids[ii], temp[ii])) {
        // TODO(gman): fail.
      }
    }
    return true;
  }

  // Template to help call glDeleteXXX functions.
  template <void gl_delete_function(GLsizei, GLuint*)>
  bool DeleteGLObjects(GLsizei n, const GLuint* client_ids) {
    scoped_array<GLuint>temp(new GLuint[n]);
    // TODO(gman): check for success before copying results.
    for (GLsizei ii = 0; ii < n; ++ii) {
      if (id_map_.GetServiceId(client_ids[ii], &temp[ii])) {
        id_map_.RemoveMapping(client_ids[ii], temp[ii]);
      } else {
        temp[ii] = 0;
      }
    }
    gl_delete_function(n, temp.get());
    return true;
  }

  // Wrapper for glCreateProgram
  void CreateProgramHelper(GLuint client_id);

  // Wrapper for glCreateShader
  void CreateShaderHelper(GLenum type, GLuint client_id);

  // Wrapper for glBindBuffer since we need to track the current targets.
  void DoBindBuffer(GLenum target, GLuint buffer);

  // Wrapper for glDeleteProgram.
  void DoDeleteProgram(GLuint program);

  // Wrapper for glDeleteShader.
  void DoDeleteShader(GLuint shader);

  // Generate a member function prototype for each command in an automated and
  // typesafe way.
  #define GLES2_CMD_OP(name) \
     ParseError Handle ## name(             \
       unsigned int arg_count,              \
       const gles2::name& args);            \

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP

  // Map of client ids to GL ids.
  IdMap id_map_;
  GLES2Util util_;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment_;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment_;

  // The currently bound array buffer. If this is 0 it is illegal to call
  // glVertexAttribPointer.
  GLuint bound_array_buffer_;

  // The currently bound element array buffer. If this is 0 it is illegal
  // to call glDrawElements.
  GLuint bound_element_array_buffer_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Decoder);
};

}  // namespace gles2
}  // namespace command_buffer

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_GLES2_CMD_DECODER_H

