// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include <string>
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"

namespace command_buffer {
namespace gles2 {

bool IdMap::AddMapping(GLuint client_id, GLuint service_id) {
  std::pair<MapType::iterator, bool> result = id_map_.insert(
      std::make_pair(client_id, service_id));
  return result.second;
}

bool IdMap::RemoveMapping(GLuint client_id, GLuint service_id) {
  MapType::iterator iter = id_map_.find(client_id);
  if (iter != id_map_.end() && iter->second == service_id) {
    id_map_.erase(iter);
    return true;
  }
  return false;
}

bool IdMap::GetServiceId(GLuint client_id, GLuint* service_id) {
  DCHECK(service_id);
  MapType::iterator iter = id_map_.find(client_id);
  if (iter != id_map_.end()) {
    *service_id = iter->second;
    return true;
  }
  return false;
}

bool IdMap::GetClientId(GLuint service_id, GLuint* client_id) {
  DCHECK(client_id);
  MapType::iterator end(id_map_.end());
  for (MapType::iterator iter(id_map_.begin());
       iter != end;
       ++iter) {
    if (iter->second == service_id) {
      *client_id = iter->first;
      return true;
    }
  }
  return false;
}

namespace {

// Returns the address of the first byte after a struct.
template <typename T>
const void* AddressAfterStruct(const T& pod) {
  return reinterpret_cast<const uint8*>(&pod) + sizeof(pod);
}

// Returns the address of the frst byte after the struct.
template <typename RETURN_TYPE, typename COMMAND_TYPE>
RETURN_TYPE GetImmediateDataAs(const COMMAND_TYPE& pod) {
  return static_cast<RETURN_TYPE>(const_cast<void*>(AddressAfterStruct(pod)));
}

// Returns the size in bytes of the data of an Immediate command, a command with
// its data inline in the command buffer.
template <typename T>
unsigned int ImmediateDataSize(uint32 arg_count) {
  return static_cast<unsigned int>(
      (arg_count + 1 - ComputeNumEntries(sizeof(T))) *
      sizeof(CommandBufferEntry));  // NOLINT
}

// Checks if there is enough immediate data.
template<typename T>
bool CheckImmediateDataSize(
    unsigned int arg_count,
    GLuint count,
    size_t size,
    unsigned int elements_per_unit) {
  return ImmediateDataSize<T>(arg_count) == count * size * elements_per_unit;
}

// A struct to hold info about each command.
struct CommandInfo {
  int arg_flags;  // How to handle the arguments for this command
  int arg_count;  // How many arguments are expected for this command.
};

// A table of CommandInfo for all the commands.
const CommandInfo g_command_info[] = {
  #define GLES2_CMD_OP(name) {                                            \
    name::kArgFlags,                                                      \
    sizeof(name) / sizeof(CommandBufferEntry) - 1, },  /* NOLINT */       \

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP
};

// These commands convert from c calls to local os calls.
void GLGenBuffersHelper(GLsizei n, GLuint* ids) {
  glGenBuffers(n, ids);
}

void GLGenFramebuffersHelper(GLsizei n, GLuint* ids) {
  glGenFramebuffers(n, ids);
}

void GLGenRenderbuffersHelper(GLsizei n, GLuint* ids) {
  glGenRenderbuffers(n, ids);
}

void GLGenTexturesHelper(GLsizei n, GLuint* ids) {
  glGenTextures(n, ids);
}

void GLDeleteBuffersHelper(GLsizei n, GLuint* ids) {
  glDeleteBuffers(n, ids);
}

void GLDeleteFramebuffersHelper(GLsizei n, GLuint* ids) {
  glDeleteFramebuffers(n, ids);
}

void GLDeleteRenderbuffersHelper(GLsizei n, GLuint* ids) {
  glDeleteRenderbuffers(n, ids);
}

void GLDeleteTexturesHelper(GLsizei n, GLuint* ids) {
  glDeleteTextures(n, ids);
}

}  // anonymous namespace.

GLES2Decoder::GLES2Decoder()
    : util_(0),  // TODO(gman): Set to actual num compress texture formats.
      pack_alignment_(4),
      unpack_alignment_(4),
      bound_array_buffer_(0),
      bound_element_array_buffer_(0) {
}

// Decode command with its arguments, and call the corresponding GAPIInterface
// method.
// Note: args is a pointer to the command buffer. As such, it could be changed
// by a (malicious) client at any time, so if validation has to happen, it
// should operate on a copy of them.
parse_error::ParseError GLES2Decoder::DoCommand(
    unsigned int command,
    unsigned int arg_count,
    const void* cmd_data) {
  unsigned int command_index = command - kStartPoint - 1;
  if (command_index < arraysize(g_command_info)) {
    const CommandInfo& info = g_command_info[command_index];
    unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
    if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
        (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
      switch (command) {
        #define GLES2_CMD_OP(name)                                 \
          case name::kCmdId:                                       \
            return Handle ## name(                                 \
                arg_count,                                         \
                *static_cast<const name*>(cmd_data));              \

        GLES2_COMMAND_LIST(GLES2_CMD_OP)

        #undef GLES2_CMD_OP
      }
    } else {
      return parse_error::kParseInvalidArguments;
    }
  }
  return DoCommonCommand(command, arg_count, cmd_data);
}

}  // namespace gles2
}  // namespace command_buffer

// This is included so the compiler will make these inline.
#include "gpu/command_buffer/service/gles2_cmd_decoder_validate.h"

namespace command_buffer {
namespace gles2 {

void GLES2Decoder::CreateProgramHelper(GLuint client_id) {
  // TODO(gman): verify client_id is unused.
  GLuint service_id = glCreateProgram();
  if (service_id) {
    id_map_.AddMapping(client_id, service_id);
  }
}

void GLES2Decoder::CreateShaderHelper(GLenum type, GLuint client_id) {
  // TODO(gman): verify client_id is unused.
  GLuint service_id = glCreateShader(type);
  if (service_id) {
    id_map_.AddMapping(client_id, service_id);
  }
}

void GLES2Decoder::DoBindBuffer(GLenum target, GLuint buffer) {
  switch (target) {
    case GL_ARRAY_BUFFER:
      bound_array_buffer_ = buffer;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      bound_element_array_buffer_ = buffer;
      break;
    default:
      break;
  }
  glBindBuffer(target, buffer);
}

void GLES2Decoder::DoDeleteProgram(GLuint program) {
  GLuint service_id;
  if (id_map_.GetServiceId(program, &service_id)) {
    glDeleteProgram(service_id);
    id_map_.RemoveMapping(program, service_id);
  }
}

void GLES2Decoder::DoDeleteShader(GLuint shader) {
  GLuint service_id;
  if (id_map_.GetServiceId(shader, &service_id)) {
    glDeleteProgram(service_id);
    id_map_.RemoveMapping(shader, service_id);
  }
}

parse_error::ParseError GLES2Decoder::HandleDrawElements(
    unsigned int arg_count, const gles2::DrawElements& c) {
  if (bound_element_array_buffer_ != 0) {
    GLenum mode = c.mode;
    GLsizei count = c.count;
    GLenum type = c.type;
    const GLvoid* indices = reinterpret_cast<const GLvoid*>(c.index_offset);
    glDrawElements(mode, count, type, indices);
  } else {
    // TODO(gman): set our wrapped glGetError value to GL_INVALID_VALUE
    DCHECK(false);
  }
  return parse_error::kParseNoError;
}

namespace {

// Calls glShaderSource for the various versions of the ShaderSource command.
// Assumes that data / data_size points to a piece of memory that is in range
// of whatever context it came from (shared memory, immediate memory, bucket
// memory.)
parse_error::ParseError ShaderSourceHelper(
    GLuint shader, GLsizei count, const char* data, uint32 data_size) {
  std::vector<std::string> strings(count);
  scoped_array<const char*> string_pointers(new const char* [count]);

  const uint32* ends = reinterpret_cast<const uint32*>(data);
  uint32 start_offset = count * sizeof(*ends);
  if (start_offset > data_size) {
    return parse_error::kParseOutOfBounds;
  }
  for (GLsizei ii = 0; ii < count; ++ii) {
    uint32 end_offset = ends[ii];
    if (end_offset > data_size || end_offset < start_offset) {
      return parse_error::kParseOutOfBounds;
    }
    strings[ii] = std::string(data + start_offset, end_offset - start_offset);
    string_pointers[ii] = strings[ii].c_str();
  }

  glShaderSource(shader, count, string_pointers.get(), NULL);
  return parse_error::kParseNoError;
}

}  // anonymous namespace.

parse_error::ParseError GLES2Decoder::HandleShaderSource(
    unsigned int arg_count, const gles2::ShaderSource& c) {
  GLuint shader = c.shader;
  GLsizei count = c.count;
  uint32 data_size = c.data_size;
  const char** data = GetSharedMemoryAs<const char**>(
      c.data_shm_id, c.data_shm_offset, data_size);
  parse_error::ParseError result =
      ValidateShaderSource(this, arg_count, shader, count, data, NULL);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  return ShaderSourceHelper(
      shader, count, reinterpret_cast<const char*>(data), data_size);
}

parse_error::ParseError GLES2Decoder::HandleShaderSourceImmediate(
  unsigned int arg_count, const gles2::ShaderSourceImmediate& c) {
  GLuint shader = c.shader;
  GLsizei count = c.count;
  uint32 data_size = c.data_size;
  // TODO(gman): need to check that data_size is in range for arg_count.
  const char** data = GetImmediateDataAs<const char**>(c);
  parse_error::ParseError result =
      ValidateShaderSourceImmediate(
          this, arg_count, shader, count, data, NULL);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  return ShaderSourceHelper(
      shader, count, reinterpret_cast<const char*>(data), data_size);
}

parse_error::ParseError GLES2Decoder::HandleVertexAttribPointer(
    unsigned int arg_count, const gles2::VertexAttribPointer& c) {
  if (bound_array_buffer_ != 0) {
    GLuint indx = c.indx;
    GLint size = c.size;
    GLenum type = c.type;
    GLboolean normalized = c.normalized;
    GLsizei stride = c.stride;
    GLuint offset = c.offset;
    const void* ptr = reinterpret_cast<const void*>(c.offset);
    parse_error::ParseError result =
        ValidateVertexAttribPointer(
            this, arg_count, indx, size, type, normalized, stride, ptr);
    if (result != parse_error::kParseNoError) {
      return result;
    }
    glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
  } else {
    // TODO(gman): set our wrapped glGetError value to GL_INVALID_VALUE
    DCHECK(false);
  }
  return parse_error::kParseNoError;
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h"

}  // namespace gles2
}  // namespace command_buffer

