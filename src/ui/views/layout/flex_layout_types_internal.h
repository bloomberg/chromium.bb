// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FLEX_LAYOUT_TYPES_INTERNAL_H_
#define UI_VIEWS_LAYOUT_FLEX_LAYOUT_TYPES_INTERNAL_H_

#include <string>

#include "base/optional.h"
#include "ui/views/layout/flex_layout_types.h"

namespace gfx {
class Insets;
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace views {

class SizeBounds;

namespace internal {

// Represents insets in a single dimension.
class Inset1D {
 public:
  constexpr Inset1D() = default;
  constexpr explicit Inset1D(int all) : leading_(all), trailing_(all) {}
  constexpr Inset1D(int leading, int trailing)
      : leading_(leading), trailing_(trailing) {}

  constexpr int leading() const { return leading_; }
  void set_leading(int leading) { leading_ = leading; }

  constexpr int trailing() const { return trailing_; }
  void set_trailing(int trailing) { trailing_ = trailing; }

  constexpr int size() const { return leading_ + trailing_; }

  void SetInsets(int leading, int trailing);
  void Expand(int delta_leading, int delta_trailing);

  constexpr bool is_empty() const { return leading_ == 0 && trailing_ == 0; }
  bool operator==(const Inset1D& other) const;
  bool operator!=(const Inset1D& other) const;
  bool operator<(const Inset1D& other) const;

  std::string ToString() const;

 private:
  int leading_ = 0;
  int trailing_ = 0;
};

// Represents a line segment in one dimension with a starting point and length.
class Span {
 public:
  constexpr Span() = default;
  constexpr Span(int start, int length) : start_(start), length_(length) {}

  constexpr int start() const { return start_; }
  void set_start(int start) { start_ = start; }

  constexpr int length() const { return length_; }
  void set_length(int length) {
    DCHECK_GE(length, 0);
    length_ = length;
  }

  constexpr int end() const { return start_ + length_; }
  void set_end(int end) {
    DCHECK_GE(end, start_);
    length_ = end - start_;
  }

  void SetSpan(int start, int length);

  // Expands the span by |leading| at the front (reducing the value of start()
  // if |leading| is positive) and by |trailing| at the end (increasing the
  // value of end() if |trailing| is positive).
  void Expand(int leading, int trailing);

  // Opposite of Expand(). Shrinks each end of the span by the specified amount.
  void Inset(int leading, int trailing);
  void Inset(const Inset1D& insets);

  // Centers the span in another span, with optional margins.
  // Overflow is handled gracefully.
  void Center(const Span& container, const Inset1D& margins = Inset1D());

  // Aligns the span in another span, with optional margins, using the specified
  // alignment. Overflow is handled gracefully.
  void Align(const Span& container,
             LayoutAlignment alignment,
             const Inset1D& margins = Inset1D());

  constexpr bool is_empty() const { return length_ == 0; }
  bool operator==(const Span& other) const;
  bool operator!=(const Span& other) const;
  bool operator<(const Span& other) const;

  std::string ToString() const;

 private:
  int start_ = 0;
  int length_ = 0;
};

// Represents a point in layout space - that is, a point on the main and cross
// axes of the layout (regardless of whether it is vertically or horizontally
// oriented.
class NormalizedPoint {
 public:
  constexpr NormalizedPoint() = default;
  constexpr NormalizedPoint(int main, int cross) : main_(main), cross_(cross) {}

  constexpr int main() const { return main_; }
  void set_main(int main) { main_ = main; }

  constexpr int cross() const { return cross_; }
  void set_cross(int cross) { cross_ = cross; }

  void SetPoint(int main, int cross);
  void Offset(int delta_main, int delta_cross);

  bool operator==(const NormalizedPoint& other) const;
  bool operator!=(const NormalizedPoint& other) const;
  bool operator<(const NormalizedPoint& other) const;

  std::string ToString() const;

 private:
  int main_ = 0;
  int cross_ = 0;
};

// Represents a size in layout space - that is, a size on the main and cross
// axes of the layout (regardless of whether it is vertically or horizontally
// oriented.
class NormalizedSize {
 public:
  constexpr NormalizedSize() = default;
  constexpr NormalizedSize(int main, int cross) : main_(main), cross_(cross) {}

  constexpr int main() const { return main_; }
  void set_main(int main) { main_ = main; }

  constexpr int cross() const { return cross_; }
  void set_cross(int cross) { cross_ = cross; }

  void SetSize(int main, int cross);
  void Enlarge(int delta_main, int delta_cross);
  void SetToMax(int main, int cross);
  void SetToMax(const NormalizedSize& other);
  void SetToMin(int main, int cross);
  void SetToMin(const NormalizedSize& other);

  constexpr bool is_empty() const { return main_ == 0 || cross_ == 0; }

  bool operator==(const NormalizedSize& other) const;
  bool operator!=(const NormalizedSize& other) const;
  bool operator<(const NormalizedSize& other) const;

  std::string ToString() const;

 private:
  int main_ = 0;
  int cross_ = 0;
};

// Represents insets in layout space - that is, insets on the main and cross
// axes of the layout (regardless of whether it is vertically or horizontally
// oriented.
class NormalizedInsets {
 public:
  constexpr NormalizedInsets() = default;
  constexpr explicit NormalizedInsets(int all) : main_(all), cross_(all) {}
  constexpr NormalizedInsets(int main, int cross)
      : main_(main), cross_(cross) {}
  constexpr NormalizedInsets(const Inset1D& main, const Inset1D& cross)
      : main_(main), cross_(cross) {}
  constexpr NormalizedInsets(int main_leading,
                             int cross_leading,
                             int main_trailing,
                             int cross_trailing)
      : main_(main_leading, main_trailing),
        cross_(cross_leading, cross_trailing) {}

  constexpr int main_leading() const { return main_.leading(); }
  void set_main_leading(int main_leading) { main_.set_leading(main_leading); }

  constexpr int main_trailing() const { return main_.trailing(); }
  void set_main_trailing(int main_trailing) {
    main_.set_trailing(main_trailing);
  }

  constexpr int main_size() const { return main_.size(); }

  constexpr int cross_leading() const { return cross_.leading(); }
  void set_cross_leading(int cross_leading) {
    cross_.set_leading(cross_leading);
  }

  constexpr int cross_trailing() const { return cross_.trailing(); }
  void set_cross_trailing(int cross_trailing) {
    cross_.set_trailing(cross_trailing);
  }

  constexpr int cross_size() const { return cross_.size(); }

  const Inset1D& main() const { return main_; }
  void set_main(const Inset1D& main) { main_ = main; }

  const Inset1D& cross() const { return cross_; }
  void set_cross(const Inset1D& cross) { cross_ = cross; }

  bool operator==(const NormalizedInsets& other) const;
  bool operator!=(const NormalizedInsets& other) const;
  bool operator<(const NormalizedInsets& other) const;

  std::string ToString() const;

 private:
  Inset1D main_;
  Inset1D cross_;
};

// Represents size bounds in layout space - that is, a set of size bounds using
// the main and cross axes of the layout (regardless of whether it is vertically
// or horizontally oriented).
class NormalizedSizeBounds {
 public:
  NormalizedSizeBounds();
  NormalizedSizeBounds(const base::Optional<int>& main,
                       const base::Optional<int>& cross);
  explicit NormalizedSizeBounds(const NormalizedSize& size);
  NormalizedSizeBounds(const NormalizedSizeBounds& size_bounds);

  const base::Optional<int>& main() const { return main_; }
  void set_main(const base::Optional<int>& main) { main_ = main; }

  const base::Optional<int>& cross() const { return cross_; }
  void set_cross(const base::Optional<int>& cross) { cross_ = cross; }

  void Expand(int main, int cross);

  bool operator==(const NormalizedSizeBounds& other) const;
  bool operator!=(const NormalizedSizeBounds& other) const;
  bool operator<(const NormalizedSizeBounds& other) const;

  std::string ToString() const;

 private:
  base::Optional<int> main_;
  base::Optional<int> cross_;
};

// Represents a rectangle in layout space - that is, a rectangle whose
// dimensions align with the main and cross axis of the layout (regardless of
// whether the layout is vertically or horizontally oriented).
class NormalizedRect {
 public:
  constexpr NormalizedRect() = default;
  constexpr NormalizedRect(const NormalizedPoint& origin,
                           const NormalizedSize& size)
      : origin_(origin), size_(size) {}
  constexpr NormalizedRect(const Span& main, const Span& cross)
      : origin_(main.start(), cross.start()),
        size_(main.length(), cross.length()) {}
  constexpr NormalizedRect(int origin_main,
                           int origin_cross,
                           int size_main,
                           int size_cross)
      : origin_(origin_main, origin_cross), size_(size_main, size_cross) {}

  constexpr int origin_main() const { return origin_.main(); }
  void set_origin_main(int main) { origin_.set_main(main); }

  constexpr int origin_cross() const { return origin_.cross(); }
  void set_origin_cross(int cross) { origin_.set_cross(cross); }

  constexpr const NormalizedPoint& origin() const { return origin_; }
  void set_origin(const NormalizedPoint& origin) { origin_ = origin; }

  constexpr int size_main() const { return size_.main(); }
  void set_size_main(int main) { size_.set_main(main); }

  constexpr int size_cross() const { return size_.cross(); }
  void set_size_cross(int cross) { size_.set_cross(cross); }

  constexpr const NormalizedSize& size() const { return size_; }
  void set_size(const NormalizedSize& size) { size_ = size; }

  constexpr int max_main() const { return origin_.main() + size_.main(); }
  constexpr int max_cross() const { return origin_.cross() + size_.cross(); }

  Span GetMainSpan() const;
  void SetMainSpan(const Span& span);
  void AlignMain(const Span& container,
                 LayoutAlignment alignment,
                 const Inset1D& margins = Inset1D());

  Span GetCrossSpan() const;
  void SetCrossSpan(const Span& span);
  void AlignCross(const Span& container,
                  LayoutAlignment alignment,
                  const Inset1D& margins = Inset1D());

  void SetRect(int origin_main,
               int origin_cross,
               int size_main,
               int size_cross);
  void SetByBounds(int origin_main,
                   int origin_cross,
                   int max_main,
                   int max_cross);
  void Inset(const NormalizedInsets& insets);
  void Inset(int main, int cross);
  void Inset(int main_leading,
             int cross_leading,
             int main_trailing,
             int cross_trailing);
  void Offset(int main, int cross);

  constexpr bool is_empty() const { return size_.is_empty(); }
  bool operator==(const NormalizedRect& other) const;
  bool operator!=(const NormalizedRect& other) const;
  bool operator<(const NormalizedRect& other) const;

  std::string ToString() const;

 private:
  NormalizedPoint origin_;
  NormalizedSize size_;
};

// Normalization and Denormalization -------------------------------------------

NormalizedPoint Normalize(LayoutOrientation orientation,
                          const gfx::Point& point);
gfx::Point Denormalize(LayoutOrientation orientation,
                       const NormalizedPoint& point);

NormalizedSize Normalize(LayoutOrientation orientation, const gfx::Size& point);
gfx::Size Denormalize(LayoutOrientation orientation,
                      const NormalizedSize& point);

NormalizedSizeBounds Normalize(LayoutOrientation orientation,
                               const SizeBounds& point);
SizeBounds Denormalize(LayoutOrientation orientation,
                       const NormalizedSizeBounds& point);

NormalizedInsets Normalize(LayoutOrientation orientation,
                           const gfx::Insets& point);
gfx::Insets Denormalize(LayoutOrientation orientation,
                        const NormalizedInsets& point);

NormalizedRect Normalize(LayoutOrientation orientation, const gfx::Rect& rect);
gfx::Rect Denormalize(LayoutOrientation orientation,
                      const NormalizedRect& rect);

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_LAYOUT_FLEX_LAYOUT_TYPES_INTERNAL_H_
