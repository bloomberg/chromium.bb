// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/render/trace_renderer.h"

#include "tools/render/layout_constants.h"

namespace quic_trace {
namespace render {

namespace {

// Passthrough vector shader.
const char* kVertexShader = R"(
in vec4 coord;
void main(void) {
  gl_Position = vec4(coord);
}
)";

// The geometry shader that accepts a packet represented as a 4-tuple of format
// (time, offset, size, kind) and represents it as a square, assigning
// appropriate color in process.
const char* kGeometryShader = R"(
uniform float distance_scale;
uniform vec2 margin;
uniform int highlighted_packet;

const float kSentPacketDurationMs = 1000;
const vec4 kUnit = vec4(1,1,1,1);

out vec4 v_color;

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

vec2 norm(vec2 x) {
  return x * 2 - vec2(1, 1);
}

vec4 mapPoint(float x, float y) {
  vec2 point = vec2(x, y);
  point = norm((point - program_state.offset) / program_state.viewport);
  return vec4(point * margin, 0, 1);
}

void main(void) {
  float time = gl_in[0].gl_Position[0];
  float offset = gl_in[0].gl_Position[1];
  float size = gl_in[0].gl_Position[2];
  float kind = gl_in[0].gl_Position[3];

  // Sent is blue.
  v_color = vec4(0.0, 0.0, 0.6, 1.0);
  // Ack is green.
  if (kind <= -1) {
    v_color = vec4(0.0, 0.6, 0.0, 1.0);
  }
  // Lost is red.
  if (kind <= -2) {
    v_color = vec4(0.8, 0.0, 0.0, 1.0);
  }
  // App-limited is orange.
  if (kind <= -3) {
    v_color = vec4(1.0, 0.425, 0.0, 1.0);
  }
  // Highlight the specified packet by making it brighter.
  if (highlighted_packet == gl_PrimitiveIDIn) {
    v_color = kUnit - (kUnit - v_color) * 0.3;
  }

  float duration = distance_scale * kSentPacketDurationMs;
  size *= distance_scale;

  gl_Position = mapPoint(time, offset);
  EmitVertex();

  gl_Position = mapPoint(time + duration, offset);
  EmitVertex();

  gl_Position = mapPoint(time, offset + size);
  EmitVertex();

  gl_Position = mapPoint(time + duration, offset + size);
  EmitVertex();

  EndPrimitive();
}
)";

// A passthrough fragment shader.
const char* kFragmentShader = R"(
in vec4 v_color;
out vec4 color;
void main(void) {
  color = v_color;
}
)";

float PacketTypeToFloat(PacketType type) {
  switch (type) {
    case PacketType::SENT:
      return 0;
    case PacketType::ACKED:
      return -1;
    case PacketType::LOST:
      return -2;
    case PacketType::APP_LIMITED:
      return -3;
  }
}
}  // namespace

TraceRenderer::TraceRenderer(const ProgramState* state)
    : state_(state), shader_(kVertexShader, kGeometryShader, kFragmentShader) {}

void TraceRenderer::PacketCountHint(size_t count) {
  DCHECK(!buffer_ready_);
  packet_buffer_.reserve(count);
}

void TraceRenderer::AddPacket(uint64_t time,
                              uint64_t offset,
                              uint64_t size,
                              PacketType type) {
  DCHECK(!buffer_ready_);
  packet_buffer_.push_back(
      Packet{static_cast<float>(time), static_cast<float>(offset),
             static_cast<float>(size), PacketTypeToFloat(type)});
  max_x_ = std::max(max_x_, time + kSentPacketDurationMs);
  max_y_ = std::max(max_y_, static_cast<float>(offset + size));
}

void TraceRenderer::FinishPackets() {
  glBindBuffer(GL_ARRAY_BUFFER, *buffer_);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(*packet_buffer_.data()) * packet_buffer_.size(),
               packet_buffer_.data(), GL_STATIC_DRAW);

  packet_count_ = packet_buffer_.size();
  packet_buffer_.clear();
  buffer_ready_ = true;
}

void TraceRenderer::Render() {
  DCHECK(buffer_ready_);
  glUseProgram(*shader_);
  glBindVertexArray(*array_);
  glBindBuffer(GL_ARRAY_BUFFER, *buffer_);

  // Distance scaling.
  // When packets are sufficiently far, they might get smaller than one pixel;
  // that makes rendering look noisy and useless.  Scale them back to ensure
  // they take up at least a pixel on X axis.  Note that we're actually scaling
  // to 1.1 pixels in order to avoid jitter caused by edge cases when zooming.
  float distance_scale = 1.f;
  float packet_size_in_pixels =
      kSentPacketDurationMs / state_->viewport().x * state_->window().x;
  if (packet_size_in_pixels < 1.1f) {
    distance_scale = 1.1f / packet_size_in_pixels;
  }

  state_->Bind(shader_);
  shader_.SetUniform("distance_scale", distance_scale);
  shader_.SetUniform("margin", 1.f - 2 * kTraceMarginX / state_->window().x,
                     1.f - 2 * kTraceMarginY / state_->window().y);
  shader_.SetUniform("highlighted_packet", highlighted_packet_);

  GlVertexArrayAttrib coord(shader_, "coord");
  glVertexAttribPointer(*coord, 4, GL_FLOAT, GL_FALSE, 0, 0);

  glEnable(GL_SCISSOR_TEST);
  glScissor(kTraceMarginX, kTraceMarginY,
            state_->window().x - 2 * kTraceMarginX,
            state_->window().y - 2 * kTraceMarginY);
  glDrawArrays(GL_POINTS, 0, packet_count_);
  glDisable(GL_SCISSOR_TEST);
}

// TODO(vasilvv): we now have enough information in ProcessedTrace to do this
// without a copy from GPU.
absl::optional<Box> TraceRenderer::BoundContainedPackets(Box boundary) {
  DCHECK(buffer_ready_);

  // Since we don't actually store the list of the rectangles in memory, we get
  // to fetch it from the GPU.
  glBindBuffer(GL_ARRAY_BUFFER, *buffer_);
  const Packet* packets = reinterpret_cast<Packet*>(glMapBufferRange(
      GL_ARRAY_BUFFER, 0, packet_count_ * sizeof(Packet), GL_MAP_READ_BIT));
  CHECK(packets != nullptr)
      << "Failed to fetch packets, GL error: " << glGetError();
  GlBufferMapCleanup cleanup(GL_ARRAY_BUFFER);

  const float time_start = boundary.origin.x;
  const float time_end = boundary.origin.x + boundary.size.x;
  const Packet* packets_end = packets + packet_count_;
  const auto compare = [](const Packet& a, const Packet& b) {
    return a.time < b.time;
  };
  const Packet* range_start = std::lower_bound(
      packets, packets_end, Packet{time_start, 0, 0, 0}, compare);
  const Packet* range_end = std::upper_bound(
      packets, packets_end, Packet{time_end, 0, 0, 0}, compare);

  if (range_start == packets_end || range_end == packets_end ||
      range_start > range_end) {
    return absl::nullopt;
  }

  absl::optional<Box> result;
  for (const Packet* it = range_start; it <= range_end; it++) {
    Box packet{vec2(it->time, it->offset),
               vec2(kSentPacketDurationMs, it->size)};
    if (IsInside(packet, boundary)) {
      result = result.has_value() ? BoundingBox(*result, packet) : packet;
    }
  }
  return result;
}

}  // namespace render
}  // namespace quic_trace
