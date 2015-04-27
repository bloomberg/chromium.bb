// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_THROBBER_H_
#define UI_VIEWS_CONTROLS_THROBBER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}

namespace views {

// Throbbers display an animation, usually used as a status indicator.

class VIEWS_EXPORT Throbber : public View {
 public:
  // |frame_time_ms| is the amount of time that should elapse between frames
  //                 (in milliseconds)
  // If |paint_while_stopped| is false, this view will be invisible when not
  // running.
  Throbber(int frame_time_ms, bool paint_while_stopped);
  Throbber(int frame_time_ms, bool paint_while_stopped, gfx::ImageSkia* frames);
  ~Throbber() override;

  // Start and stop the throbber animation
  virtual void Start();
  virtual void Stop();

  // Set custom throbber frames. Otherwise IDR_THROBBER is loaded.
  void SetFrames(const gfx::ImageSkia* frames);

  // Overridden from View:
  gfx::Size GetPreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

 protected:
  // Specifies whether the throbber is currently animating or not
  bool IsRunning() const;
  base::TimeTicks start_time() const { return start_time_; }

 private:
  bool paint_while_stopped_;
  int frame_count_;  // How many frames we have.
  base::TimeTicks start_time_;  // Time when Start was called.
  const gfx::ImageSkia* frames_;  // Frames images.
  base::TimeDelta frame_time_;  // How long one frame is displayed.
  base::RepeatingTimer<Throbber> timer_;  // Used to schedule Run calls.

  DISALLOW_COPY_AND_ASSIGN(Throbber);
};

// A SmoothedThrobber is a throbber that is representing potentially short
// and nonoverlapping bursts of work.  SmoothedThrobber ignores small
// pauses in the work stops and starts, and only starts its throbber after
// a small amount of work time has passed.
class VIEWS_EXPORT SmoothedThrobber : public Throbber {
 public:
  explicit SmoothedThrobber(int frame_delay_ms);
  SmoothedThrobber(int frame_delay_ms, gfx::ImageSkia* frames);
  ~SmoothedThrobber() override;

  void Start() override;
  void Stop() override;

  void set_start_delay_ms(int value) { start_delay_ms_ = value; }
  void set_stop_delay_ms(int value) { stop_delay_ms_ = value; }

 private:
  // Called when the startup-delay timer fires
  // This function starts the actual throbbing.
  void StartDelayOver();

  // Called when the shutdown-delay timer fires.
  // This function stops the actual throbbing.
  void StopDelayOver();

  // Delay after work starts before starting throbber, in milliseconds.
  int start_delay_ms_;

  // Delay after work stops before stopping, in milliseconds.
  int stop_delay_ms_;

  base::OneShotTimer<SmoothedThrobber> start_timer_;
  base::OneShotTimer<SmoothedThrobber> stop_timer_;

  DISALLOW_COPY_AND_ASSIGN(SmoothedThrobber);
};

// A throbber that follows material design guidelines. This is a specialization
// of Throbber for now, but should probably be folded into/replace Throbber
// eventually.
class VIEWS_EXPORT MaterialThrobber : public Throbber {
 public:
  MaterialThrobber();
  ~MaterialThrobber() override;

  // Stop spinning and, if checked is true, display a checkmark.
  void SetChecked(bool checked);

  void set_preferred_diameter(int diameter) { preferred_diameter_ = diameter; }

  // View implementation.
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // Paints this throbber in the "waiting" state, for example when waiting for
  // an initial network response.
  void PaintWaiting(gfx::Canvas* canvas);

  // Paints this throbber in its normal state. Corresponds to MD throbber.
  void PaintSpinning(gfx::Canvas* canvas);

  // Common painting code for PaintWaiting and PaintSpinning.
  void PaintArc(gfx::Canvas* canvas, SkScalar start_angle, SkScalar sweep);

  // The preferred width and height for this view. Zero indicates the size is
  // set by the parent class (i.e. matches the size of the pre-material
  // sprites).
  int preferred_diameter_;

  // Whether or not we should display a checkmark.
  bool checked_;

  // The checkmark image. Will be null until it's used (if ever).
  const gfx::ImageSkia* checkmark_;

  DISALLOW_COPY_AND_ASSIGN(MaterialThrobber);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_THROBBER_H_
