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

#include "tools/render/axis_renderer.h"

#include <array>

#include "tools/render/layout_constants.h"

namespace quic_trace {
namespace render {
namespace {
// The following shader adjusts window coordinates to GL coordinates.  It also
// shifts the coordinates so that (0, 0) is the starting point at which the axis
// should be drawn.
const char* kVertexShader = R"(
uniform vec2 axis_offset;

in vec2 coord;
void main(void) {
  gl_Position = windowToGl(coord + axis_offset);
}
)";

// Color everything black.
const char* kFragmentShader = R"(
out vec4 color;
void main(void) {
  color = vec4(0, 0, 0, 1);
}
)";

constexpr int kDistanceBetweenAxisAndTrace = 10;
constexpr float kAxisOffsetX = kTraceMarginX - kDistanceBetweenAxisAndTrace;
constexpr float kAxisOffsetY = kTraceMarginY - kDistanceBetweenAxisAndTrace;
constexpr int kTickSize = 10;

// A line as loaded into the vertex buffer.
struct Line {
  float x0, y0, x1, y1;
};

// A tick on the axis.
struct Tick {
  // |offset| here is the offset along x- or y-axis, reported in pixels.
  float offset;
  // The text of the tick.
  std::string text;
};

// Generate tick text for the X axis.
std::string TimeToString(float time) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%gs", time / 1e6);
  return buffer;
}

// Generate tick text for the Y axis.
std::string DataOffsetToString(float offset) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%gM", offset / 1e6f);
  return buffer;
}

// Finds an appropriate placement of ticks along either axis.  |offset| and
// |viewport_size| indicate which portion of the trace is currently being
// rendered, in axis units (microseconds or bytes), |axis_size| is the size of
// the axis in pixels, and |max_text_size| is an estimate of how big the text
// label can be in the dimension colinear with the axis.
template <std::string (*ToString)(float)>
std::vector<Tick> Ticks(float offset,
                        float viewport_size,
                        size_t axis_size,
                        size_t max_text_size) {
  size_t max_ticks = axis_size / (max_text_size + 16);
  // Try to find the distance between ticks.  We start by rounding down
  // |viewport_size| to a fairly small value and then go over potential
  // predefined candidates until we find the one that fits (i.e. if
  // viewport_size is 0.23s, we'd try 0.01, 0.02, 0.05, 0.1, 0.2, etc)
  float scale_base = std::pow(10.f, std::floor(std::log10(viewport_size)) - 2);
  const std::array<float, 9> scale_factors = {1.f,  2.f,   5.f,   10.f, 20.f,
                                              50.f, 100.f, 200.f, 500.f};
  float scale = 1.f;
  for (float scale_factor : scale_factors) {
    scale = scale_base * scale_factor;
    if (viewport_size / scale <= max_ticks) {
      break;
    }
  }
  if (viewport_size / scale > max_ticks) {
    LOG(ERROR) << "Failed to determine the correct number and distance between "
                  "ticks in the axis";
    return std::vector<Tick>();
  }

  std::vector<Tick> ticks;
  for (float current = offset - std::fmod(offset, scale);
       current < offset + viewport_size; current += scale) {
    // Since offset is non-negative, we have to allow some small room to render
    // the zero in most cases.
    if (current < offset - 1) {
      continue;
    }

    ticks.push_back(
        {(current - offset) / viewport_size * axis_size, ToString(current)});
  }
  return ticks;
}

}  // namespace

AxisRenderer::AxisRenderer(TextRenderer* text_factory,
                           const ProgramState* state)
    : shader_(kVertexShader, kFragmentShader),
      state_(state),
      text_renderer_(text_factory) {
  // Use a simple reference text to determine which spacing should be used
  // between the ticks on the axis.
  std::shared_ptr<const Text> reference_text =
      text_renderer_->RenderText("12.345s");
  reference_label_width_ = reference_text->width();
  reference_label_height_ = reference_text->height();
}

void AxisRenderer::Render() {
  const float axis_size_x = state_->window().x - 2 * kAxisOffsetX;
  const float axis_size_y = state_->window().y - 2 * kAxisOffsetY;

  // Note that the coordinates of the objects in this array are w.r.t the origin
  // of the axis lines that we are drawing.
  std::vector<Line> lines = {
      {0, 0, axis_size_x, 0},
      {0, 0, 0, axis_size_y},
  };
  for (const Tick& tick :
       Ticks<TimeToString>(state_->offset().x, state_->viewport().x,
                           axis_size_x, reference_label_width_)) {
    lines.push_back({tick.offset, 0, tick.offset, -kTickSize});
    std::shared_ptr<const Text> text = text_renderer_->RenderText(tick.text);
    text_renderer_->AddText(
        text,
        kAxisOffsetX + tick.offset - text->width() / 2,  // Center on X
        kAxisOffsetY - text->height() - kTickSize);      // Shift on Y
  }
  for (const Tick& tick :
       Ticks<DataOffsetToString>(state_->offset().y, state_->viewport().y,
                                 axis_size_y, reference_label_height_)) {
    lines.push_back({0, tick.offset, -10, tick.offset});
    std::shared_ptr<const Text> text = text_renderer_->RenderText(tick.text);
    text_renderer_->AddText(
        text,
        kAxisOffsetX - text->width() - kTickSize - 6,      // Shift on X
        kAxisOffsetY + tick.offset - text->height() / 2);  // Center on Y
  }

  GlVertexBuffer buffer;
  glBindBuffer(GL_ARRAY_BUFFER, *buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(*lines.data()) * lines.size(),
               lines.data(), GL_STATIC_DRAW);

  GlVertexArray array_;
  glBindVertexArray(*array_);

  glUseProgram(*shader_);
  state_->Bind(shader_);
  shader_.SetUniform("axis_offset", kAxisOffsetX, kAxisOffsetY);

  GlVertexArrayAttrib coord(shader_, "coord");
  glVertexAttribPointer(*coord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glDrawArrays(GL_LINES, 0, lines.size() * 2);
}

}  // namespace render
}  // namespace quic_trace
