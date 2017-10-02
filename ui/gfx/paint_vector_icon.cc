// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/paint_vector_icon.h"

#include <map>
#include <tuple>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {

namespace {

struct CompareIconDescription {
  bool operator()(const IconDescription& a, const IconDescription& b) const {
    const VectorIcon* a_icon = &a.icon;
    const VectorIcon* b_icon = &b.icon;
    const VectorIcon* a_badge = &a.badge_icon;
    const VectorIcon* b_badge = &b.badge_icon;
    return std::tie(a_icon, a.dip_size, a.color, a.elapsed_time, a_badge) <
           std::tie(b_icon, b.dip_size, b.color, b.elapsed_time, b_badge);
  }
};

// Helper that simplifies iterating over a sequence of PathElements.
class PathParser {
 public:
  PathParser(const PathElement* path_elements)
      : path_elements_(path_elements) {}
  ~PathParser() {}

  void Advance() { command_index_ += GetArgumentCount() + 1; }

  CommandType CurrentCommand() const {
    return path_elements_[command_index_].command;
  }

  SkScalar GetArgument(int index) const {
    DCHECK_LT(index, GetArgumentCount());
    return path_elements_[command_index_ + 1 + index].arg;
  }

 private:
  int GetArgumentCount() const {
    switch (CurrentCommand()) {
      case STROKE:
      case H_LINE_TO:
      case R_H_LINE_TO:
      case V_LINE_TO:
      case R_V_LINE_TO:
      case CANVAS_DIMENSIONS:
      case PATH_COLOR_ALPHA:
        return 1;

      case MOVE_TO:
      case R_MOVE_TO:
      case LINE_TO:
      case R_LINE_TO:
        return 2;

      case CIRCLE:
      case TRANSITION_END:
        return 3;

      case PATH_COLOR_ARGB:
      case CUBIC_TO_SHORTHAND:
      case CLIP:
        return 4;

      case ROUND_RECT:
        return 5;

      case CUBIC_TO:
      case R_CUBIC_TO:
        return 6;

      case ARC_TO:
      case R_ARC_TO:
        return 7;

      case NEW_PATH:
      case PATH_MODE_CLEAR:
      case CAP_SQUARE:
      case CLOSE:
      case DISABLE_AA:
      case FLIPS_IN_RTL:
      case TRANSITION_FROM:
      case TRANSITION_TO:
      case END:
        return 0;
    }

    NOTREACHED();
    return 0;
  }

  const PathElement* path_elements_;
  int command_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PathParser);
};

// Translates a string such as "MOVE_TO" into a command such as MOVE_TO.
CommandType CommandFromString(const std::string& source) {
#define RETURN_IF_IS(command) \
  if (source == #command)     \
    return command;

  RETURN_IF_IS(NEW_PATH);
  RETURN_IF_IS(PATH_COLOR_ALPHA);
  RETURN_IF_IS(PATH_COLOR_ARGB);
  RETURN_IF_IS(PATH_MODE_CLEAR);
  RETURN_IF_IS(STROKE);
  RETURN_IF_IS(CAP_SQUARE);
  RETURN_IF_IS(MOVE_TO);
  RETURN_IF_IS(R_MOVE_TO);
  RETURN_IF_IS(ARC_TO);
  RETURN_IF_IS(R_ARC_TO);
  RETURN_IF_IS(LINE_TO);
  RETURN_IF_IS(R_LINE_TO);
  RETURN_IF_IS(H_LINE_TO);
  RETURN_IF_IS(R_H_LINE_TO);
  RETURN_IF_IS(V_LINE_TO);
  RETURN_IF_IS(R_V_LINE_TO);
  RETURN_IF_IS(CUBIC_TO);
  RETURN_IF_IS(R_CUBIC_TO);
  RETURN_IF_IS(CUBIC_TO_SHORTHAND);
  RETURN_IF_IS(CIRCLE);
  RETURN_IF_IS(ROUND_RECT);
  RETURN_IF_IS(CLOSE);
  RETURN_IF_IS(CANVAS_DIMENSIONS);
  RETURN_IF_IS(CLIP);
  RETURN_IF_IS(DISABLE_AA);
  RETURN_IF_IS(FLIPS_IN_RTL);
  RETURN_IF_IS(END);
#undef RETURN_IF_IS

  NOTREACHED() << "Unrecognized command: " << source;
  return CLOSE;
}

std::vector<PathElement> PathFromSource(const std::string& source) {
  std::vector<PathElement> path;
  std::vector<std::string> pieces = base::SplitString(
      source, "\n ,f", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& piece : pieces) {
    double value = 0;
    int hex_value = 0;
    if (base::StringToDouble(piece, &value))
      path.push_back(PathElement(SkDoubleToScalar(value)));
    else if (base::HexStringToInt(piece, &hex_value))
      path.push_back(PathElement(SkIntToScalar(hex_value)));
    else
      path.push_back(PathElement(CommandFromString(piece)));
  }
  return path;
}

void PaintPath(Canvas* canvas,
               const PathElement* path_elements,
               int dip_size,
               SkColor color,
               const base::TimeDelta& elapsed_time) {
  SkPath path;
  path.setFillType(SkPath::kEvenOdd_FillType);

  int canvas_size = kReferenceSizeDip;
  std::vector<SkPath> paths;
  std::vector<cc::PaintFlags> flags_array;
  SkRect clip_rect = SkRect::MakeEmpty();
  bool flips_in_rtl = false;
  CommandType previous_command_type = NEW_PATH;

  for (PathParser parser(path_elements); parser.CurrentCommand() != END;
       parser.Advance()) {
    auto arg = [&parser](int i) { return parser.GetArgument(i); };
    const CommandType command_type = parser.CurrentCommand();
    auto start_new_path = [&paths]() {
      paths.push_back(SkPath());
      paths.back().setFillType(SkPath::kEvenOdd_FillType);
    };
    auto start_new_flags = [&flags_array, &color]() {
      flags_array.push_back(cc::PaintFlags());
      flags_array.back().setColor(color);
      flags_array.back().setAntiAlias(true);
      flags_array.back().setStrokeCap(cc::PaintFlags::kRound_Cap);
    };

    if (paths.empty() || command_type == NEW_PATH) {
      start_new_path();
      start_new_flags();
    }

    SkPath& path = paths.back();
    cc::PaintFlags& flags = flags_array.back();
    switch (command_type) {
      // Handled above.
      case NEW_PATH:
        break;

      case PATH_COLOR_ALPHA:
        flags.setAlpha(SkScalarFloorToInt(arg(0)));
        break;

      case PATH_COLOR_ARGB:
        flags.setColor(SkColorSetARGB(
            SkScalarFloorToInt(arg(0)), SkScalarFloorToInt(arg(1)),
            SkScalarFloorToInt(arg(2)), SkScalarFloorToInt(arg(3))));
        break;

      case PATH_MODE_CLEAR:
        flags.setBlendMode(SkBlendMode::kClear);
        break;

      case STROKE:
        flags.setStyle(cc::PaintFlags::kStroke_Style);
        flags.setStrokeWidth(arg(0));
        break;

      case CAP_SQUARE:
        flags.setStrokeCap(cc::PaintFlags::kSquare_Cap);
        break;

      case MOVE_TO:
        path.moveTo(arg(0), arg(1));
        break;

      case R_MOVE_TO:
        if (previous_command_type == CLOSE) {
          // This triggers injectMoveToIfNeeded() so that the next subpath
          // will start at the correct place. See [
          // https://www.w3.org/TR/SVG/paths.html#PathDataClosePathCommand ].
          path.rLineTo(0, 0);
        }

        path.rMoveTo(arg(0), arg(1));
        break;

      case ARC_TO:
      case R_ARC_TO: {
        SkScalar rx = arg(0);
        SkScalar ry = arg(1);
        SkScalar angle = arg(2);
        SkScalar large_arc_flag = arg(3);
        SkScalar arc_sweep_flag = arg(4);
        SkScalar x = arg(5);
        SkScalar y = arg(6);

        auto path_fn =
            command_type == ARC_TO
                ? static_cast<void (SkPath::*)(
                      SkScalar, SkScalar, SkScalar, SkPath::ArcSize,
                      SkPath::Direction, SkScalar, SkScalar)>(&SkPath::arcTo)
                : &SkPath::rArcTo;
        (path.*path_fn)(
            rx, ry, angle,
            large_arc_flag ? SkPath::kLarge_ArcSize : SkPath::kSmall_ArcSize,
            arc_sweep_flag ? SkPath::kCW_Direction : SkPath::kCCW_Direction, x,
            y);
        break;
      }

      case LINE_TO:
        path.lineTo(arg(0), arg(1));
        break;

      case R_LINE_TO:
        path.rLineTo(arg(0), arg(1));
        break;

      case H_LINE_TO: {
        SkPoint last_point;
        path.getLastPt(&last_point);
        path.lineTo(arg(0), last_point.fY);
        break;
      }

      case R_H_LINE_TO:
        path.rLineTo(arg(0), 0);
        break;

      case V_LINE_TO: {
        SkPoint last_point;
        path.getLastPt(&last_point);
        path.lineTo(last_point.fX, arg(0));
        break;
      }

      case R_V_LINE_TO:
        path.rLineTo(0, arg(0));
        break;

      case CUBIC_TO:
        path.cubicTo(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        break;

      case R_CUBIC_TO:
        path.rCubicTo(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        break;

      case CUBIC_TO_SHORTHAND: {
        // Compute the first control point (|x1| and |y1|) as the reflection
        // of the second control point on the previous command relative to
        // the current point. If there is no previous command or if the
        // previous command is not a cubic Bezier curve, the first control
        // point is coincident with the current point. Refer to the SVG
        // path specs for further details.
        SkPoint last_point;
        path.getLastPt(&last_point);
        SkScalar delta_x = 0;
        SkScalar delta_y = 0;
        if (previous_command_type == CUBIC_TO ||
            previous_command_type == R_CUBIC_TO ||
            previous_command_type == CUBIC_TO_SHORTHAND) {
          SkPoint last_control_point = path.getPoint(path.countPoints() - 2);
          delta_x = last_point.fX - last_control_point.fX;
          delta_y = last_point.fY - last_control_point.fY;
        }

        SkScalar x1 = last_point.fX + delta_x;
        SkScalar y1 = last_point.fY + delta_y;
        path.cubicTo(x1, y1, arg(0), arg(1), arg(2), arg(3));
        break;
      }

      case CIRCLE:
        path.addCircle(arg(0), arg(1), arg(2));
        break;

      case ROUND_RECT:
        path.addRoundRect(SkRect::MakeXYWH(arg(0), arg(1), arg(2), arg(3)),
                          arg(4), arg(4));
        break;

      case CLOSE:
        path.close();
        break;

      case CANVAS_DIMENSIONS:
        canvas_size = SkScalarTruncToInt(arg(0));
        break;

      case CLIP:
        clip_rect = SkRect::MakeXYWH(arg(0), arg(1), arg(2), arg(3));
        break;

      case DISABLE_AA:
        flags.setAntiAlias(false);
        break;

      case FLIPS_IN_RTL:
        flips_in_rtl = true;
        break;

      // Transitions work by pushing new paths and a new set of flags onto the
      // stack. When TRANSITION_END is seen, the paths and flags are
      // interpolated based on |elapsed_time| and the tween type.
      case TRANSITION_FROM: {
        start_new_path();
        break;
      }

      case TRANSITION_TO: {
        start_new_path();
        start_new_flags();
        break;
      }

      case TRANSITION_END: {
        DCHECK_GT(paths.size(), 2U);
        // TODO(estade): check whether this operation (interpolation) is costly,
        // and remove this TRACE log if not.
        TRACE_EVENT0("ui", "PaintVectorIcon TRANSITION_END");

        const base::TimeDelta delay =
            base::TimeDelta::FromMillisecondsD(SkScalarToDouble(arg(0)));
        const base::TimeDelta duration =
            base::TimeDelta::FromMillisecondsD(SkScalarToDouble(arg(1)));

        double state = 0;
        if (elapsed_time >= delay + duration) {
          state = 1;
        } else if (elapsed_time > delay) {
          DCHECK(!duration.is_zero());
          state = (elapsed_time - delay).InMicroseconds() /
                  static_cast<double>(duration.InMicroseconds());
        }

        auto weight = Tween::CalculateValue(
            static_cast<Tween::Type>(SkScalarTruncToInt(arg(2))), state);

        SkPath path1, path2;
        path1.swap(paths.back());
        paths.pop_back();
        path2.swap(paths.back());
        paths.pop_back();

        SkPath interpolated_path;
        bool could_interpolate =
            path1.interpolate(path2, weight, &interpolated_path);
        DCHECK(could_interpolate);
        paths.back().addPath(interpolated_path);

        // Perform manual interpolation of flags properties. TODO(estade): fill
        // more of these in as necessary.
        DCHECK_GT(flags_array.size(), 1U);
        cc::PaintFlags& end_flags = flags_array.back();
        cc::PaintFlags& start_flags = flags_array[flags_array.size() - 2];

        start_flags.setColor(Tween::ColorValueBetween(
            weight, start_flags.getColor(), end_flags.getColor()));

        flags_array.pop_back();
        break;
      }

      case END:
        NOTREACHED();
        break;
    }

    previous_command_type = command_type;
  }

  ScopedCanvas scoped_canvas(canvas);

  if (dip_size != canvas_size) {
    SkScalar scale = SkIntToScalar(dip_size) / SkIntToScalar(canvas_size);
    canvas->sk_canvas()->scale(scale, scale);
  }

  ScopedRTLFlipCanvas scoped_rtl_flip_canvas(canvas, canvas_size, flips_in_rtl);

  if (!clip_rect.isEmpty())
    canvas->sk_canvas()->clipRect(clip_rect);

  DCHECK_EQ(flags_array.size(), paths.size());
  for (size_t i = 0; i < paths.size(); ++i)
    canvas->DrawPath(paths[i], flags_array[i]);
}

class VectorIconSource : public CanvasImageSource {
 public:
  explicit VectorIconSource(const IconDescription& data)
      : CanvasImageSource(Size(data.dip_size, data.dip_size), false),
        data_(data) {}

  VectorIconSource(const std::string& definition, int dip_size, SkColor color)
      : CanvasImageSource(Size(dip_size, dip_size), false),
        data_(kNoneIcon, dip_size, color, base::TimeDelta(), kNoneIcon),
        path_(PathFromSource(definition)) {}

  ~VectorIconSource() override {}

  // CanvasImageSource:
  bool HasRepresentationAtAllScales() const override {
    return !data_.icon.is_empty();
  }

  void Draw(Canvas* canvas) override {
    if (path_.empty()) {
      PaintVectorIcon(canvas, data_.icon, size_.width(), data_.color,
                      data_.elapsed_time);
      if (!data_.badge_icon.is_empty())
        PaintVectorIcon(canvas, data_.badge_icon, size_.width(), data_.color);
    } else {
      PaintPath(canvas, path_.data(), size_.width(), data_.color,
                base::TimeDelta());
    }
  }

 private:
  const IconDescription data_;
  const std::vector<PathElement> path_;

  DISALLOW_COPY_AND_ASSIGN(VectorIconSource);
};

// This class caches vector icons (as ImageSkia) so they don't have to be drawn
// more than once. This also guarantees the backing data for the images returned
// by CreateVectorIcon will persist in memory until program termination.
class VectorIconCache {
 public:
  VectorIconCache() {}
  ~VectorIconCache() {}

  ImageSkia GetOrCreateIcon(const IconDescription& description) {
    auto iter = images_.find(description);
    if (iter != images_.end())
      return iter->second;

    ImageSkia icon_image(base::MakeUnique<VectorIconSource>(description),
                         Size(description.dip_size, description.dip_size));
    images_.insert(std::make_pair(description, icon_image));
    return icon_image;
  }

 private:
  std::map<IconDescription, ImageSkia, CompareIconDescription> images_;

  DISALLOW_COPY_AND_ASSIGN(VectorIconCache);
};

static base::LazyInstance<VectorIconCache>::DestructorAtExit g_icon_cache =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

IconDescription::IconDescription(const IconDescription& other) = default;

IconDescription::IconDescription(const VectorIcon& icon,
                                 int dip_size,
                                 SkColor color,
                                 const base::TimeDelta& elapsed_time,
                                 const VectorIcon& badge_icon)
    : icon(icon),
      dip_size(dip_size),
      color(color),
      elapsed_time(elapsed_time),
      badge_icon(badge_icon) {
  if (dip_size == 0)
    this->dip_size = GetDefaultSizeOfVectorIcon(icon);

  // If an icon has a .1x.icon version, it should only be rendered at the size
  // specified in that definition.
  if (icon.path_1x)
    DCHECK_EQ(this->dip_size, GetDefaultSizeOfVectorIcon(icon));
}

IconDescription::~IconDescription() {}

const VectorIcon kNoneIcon = {};

void PaintVectorIcon(Canvas* canvas,
                     const VectorIcon& icon,
                     SkColor color,
                     const base::TimeDelta& elapsed_time) {
  PaintVectorIcon(canvas, icon, GetDefaultSizeOfVectorIcon(icon), color,
                  elapsed_time);
}

void PaintVectorIcon(Canvas* canvas,
                     const VectorIcon& icon,
                     int dip_size,
                     SkColor color,
                     const base::TimeDelta& elapsed_time) {
  DCHECK(!icon.is_empty());
  const PathElement* path =
      (canvas->image_scale() == 1.f && icon.path_1x) ? icon.path_1x : icon.path;
  PaintPath(canvas, path, dip_size, color, elapsed_time);
}

ImageSkia CreateVectorIcon(const IconDescription& params) {
  if (params.icon.is_empty())
    return gfx::ImageSkia();

  return g_icon_cache.Get().GetOrCreateIcon(params);
}

ImageSkia CreateVectorIcon(const VectorIcon& icon, SkColor color) {
  return CreateVectorIcon(icon, GetDefaultSizeOfVectorIcon(icon), color);
}

ImageSkia CreateVectorIcon(const VectorIcon& icon,
                           int dip_size,
                           SkColor color) {
  return CreateVectorIcon(
      IconDescription(icon, dip_size, color, base::TimeDelta(), kNoneIcon));
}

ImageSkia CreateVectorIconWithBadge(const VectorIcon& icon,
                                    int dip_size,
                                    SkColor color,
                                    const VectorIcon& badge_icon) {
  return CreateVectorIcon(
      IconDescription(icon, dip_size, color, base::TimeDelta(), badge_icon));
}

ImageSkia CreateVectorIconFromSource(const std::string& source,
                                     int dip_size,
                                     SkColor color) {
  return CanvasImageSource::MakeImageSkia<VectorIconSource>(source, dip_size,
                                                            color);
}

int GetDefaultSizeOfVectorIcon(const VectorIcon& icon) {
  const PathElement* one_x_path = icon.path_1x ? icon.path_1x : icon.path;
  return one_x_path[0].command == CANVAS_DIMENSIONS ? one_x_path[1].arg
                                                    : kReferenceSizeDip;
}

base::TimeDelta GetDurationOfAnimation(const VectorIcon& icon) {
  base::TimeDelta last_motion;
  for (PathParser parser(icon.path); parser.CurrentCommand() != END;
       parser.Advance()) {
    if (parser.CurrentCommand() != TRANSITION_END)
      continue;

    auto end_time = base::TimeDelta::FromMillisecondsD(parser.GetArgument(0)) +
                    base::TimeDelta::FromMillisecondsD(parser.GetArgument(1));
    if (end_time > last_motion)
      last_motion = end_time;
  }
  return last_motion;
}

}  // namespace gfx
