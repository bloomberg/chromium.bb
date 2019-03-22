// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_IMPL_H_

#include <memory>

#include "base/containers/ring_buffer.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/trainer.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Real implementation of Modeller.
// It monitors user-requested brightness changes, ambient light values and
// trains personal brightness curves when user remains idle for a period of
// time.
// An object of this class must be used on the same thread that created this
// object.
class ModellerImpl : public Modeller,
                     public AlsReader::Observer,
                     public BrightnessMonitor::Observer,
                     public ui::UserActivityObserver {
 public:
  // TODO(jiameng): we currently use past 10 seconds of ambient values to
  // calculate average. May revise.
  static constexpr int kAmbientLightHorizonSeconds = 10;
  static constexpr base::TimeDelta kAmbientLightHorizon =
      base::TimeDelta::FromSeconds(kAmbientLightHorizonSeconds);

  // Size of |data_cache_|.
  static constexpr int kNumberAmbientValuesToTrack =
      kAmbientLightHorizonSeconds * AlsReader::kAlsPollFrequency;

  static constexpr char kModelDir[] = "autobrightness";
  static constexpr char kCurveFileName[] = "curve";

  // ModellerImpl has weak dependencies on all parameters except |trainer|.
  ModellerImpl(const Profile* profile,
               AlsReader* als_reader,
               BrightnessMonitor* brightness_monitor,
               ui::UserActivityDetector* user_activity_detector,
               std::unique_ptr<Trainer> trainer);
  ~ModellerImpl() override;

  // Modeller overrides:
  void AddObserver(Modeller::Observer* observer) override;
  void RemoveObserver(Modeller::Observer* observer) override;

  // AlsReader::Observer overrides:
  void OnAmbientLightUpdated(int lux) override;
  void OnAlsReaderInitialized(AlsReader::AlsInitStatus status) override;

  // BrightnessMonitor::Observer overrides:
  void OnBrightnessMonitorInitialized(bool success) override;
  void OnUserBrightnessChanged(double old_brightness_percent,
                               double new_brightness_percent) override;
  void OnUserBrightnessChangeRequested() override;

  // ui::UserActivityObserver overrides:
  void OnUserActivity(const ui::Event* event) override;

  // ModellerImpl has weak dependencies on all parameters except |trainer|.
  static std::unique_ptr<ModellerImpl> CreateForTesting(
      const Profile* profile,
      AlsReader* als_reader,
      BrightnessMonitor* brightness_monitor,
      ui::UserActivityDetector* user_activity_detector,
      std::unique_ptr<Trainer> trainer,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const base::TickClock* tick_clock);

  // Current average ambient light.
  double AverageAmbientForTesting() const;

  // Current number of training data points stored, which will be used for next
  // training.
  size_t NumberTrainingDataPointsForTesting() const;

  // Returns |global_curve_| for unit tests.
  MonotoneCubicSpline GetGlobalCurveForTesting() const;

  // Returns |max_training_data_points_| for unit tests.
  size_t GetMaxTrainingDataPointsForTesting() const;

  base::TimeDelta GetTrainingDelayForTesting() const;

  // Returns the path that will be used to store curves. It also creates
  // intermediate directories if they do not exist. Returns an empty path on
  // failures.
  static base::FilePath GetCurvePathFromProfile(const Profile* profile);

 private:
  // ModellerImpl has weak dependencies on all parameters except |trainer|.
  ModellerImpl(const Profile* profile,
               AlsReader* als_reader,
               BrightnessMonitor* brightness_monitor,
               ui::UserActivityDetector* user_activity_detector,
               std::unique_ptr<Trainer> trainer,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               const base::TickClock* tick_clock,
               bool is_testing = false);

  // Updates |model_status_| by checking |als_init_status_| and
  // |brightness_monitor_status_| and optionally loads a curve.
  // 1. |model_status_| is |kDisabled| if either |als_init_status_| is not
  // |kSuccess|, or |brightness_monitor_success_| is false. The modeller will
  // notify its observers as soon as |model_status_| is |kDisabled|.
  // 2. If |als_init_status_| is |kSuccess| and |brightness_monitor_success_| is
  // true, then this method loads a curve from the disk and sets |model_status_|
  // to |kPersonal|. If no curve is found from the disk a default curve will be
  // created and |model_status_| is set to |kGlobal|. All observers will be
  // notified about the status and the curve.
  void HandleStatusUpdate();

  // Notifies its observers on the status of the model. It will be called either
  // when HandleStatusUpdate is called and |model_status_| is no longer
  // |kInitializing|, or when an observer is added to the modeller, and
  // |model_status_| is not |kInitializing|.
  void OnInitializationComplete();

  // Called when the modeller is initialized. It notifies its observers about
  // constructed global curve and personal curve (loaded from the disk). Both
  // curves will be nullopt if model is disabled, and personal curve will be
  // nullopt if no curve is loaded from the disk.
  void NotifyObserverInitStatus(Modeller::Observer& observer);

  // Called after we've attempted to construct a |curve| from data saved on
  // disk. |curve| will be assigned to |current_curve_| if |curve| is not
  // nullopt. Otherwise, |current_curve_| will have the same value as
  // |global_curve_|.
  void OnCurveLoadedFromDisk(const base::Optional<MonotoneCubicSpline>& curve);

  void OnCurveSavedToDisk(bool is_successful);

  // Called after we've set trainer's initial curves.
  void OnSetInitialCurves(
      const base::Optional<MonotoneCubicSpline>& loaded_curve,
      bool is_personal_curve_valid);

  // Either starts training immediately or delays it for |training_delay_|.
  // Training starts immediately if |training_delay_| is 0 or number of training
  // points reached |max_training_data_points_|.
  // This function is called after a user brightness change signal is received
  // (that will be used as an example), and when a user activity is detected.
  // It's also called after initial curves are set.
  // Nothing will happen if model is not enabled.
  void ScheduleTrainerStart();

  // Starts model training and runs it in non UI thread. Also clears
  // |data_cache_|.
  void StartTraining();

  // Called after training is complete with a new curve.
  void OnTrainingFinished(const MonotoneCubicSpline& curve);

  // If |is_testing_| is false, we check curve saving/loading and training jobs
  // are running on non-UI thread.
  const bool is_testing_ = false;

  // If number of recorded training data has reached |max_training_data_points_|
  // we start training immediately, without waiting for user to become idle for
  // |training_delay_|. This can be overridden by experiment flag
  // "max_training_data_points".
  size_t max_training_data_points_ = 100;

  // Once user remains idle for |training_delay_|, we start training the model.
  // If this value is 0, we will not need to wait for user to remain inactive.
  // This can be overridden by experiment flag "training_delay_in_seconds".
  base::TimeDelta training_delay_ = base::TimeDelta::FromSeconds(60);

  ScopedObserver<AlsReader, AlsReader::Observer> als_reader_observer_;

  ScopedObserver<BrightnessMonitor, BrightnessMonitor::Observer>
      brightness_monitor_observer_;

  ScopedObserver<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_;

  // Background task runner for IO work (loading a curve from disk and writing a
  // curve to disk) and training jobs.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<Trainer, base::OnTaskRunnerDeleter> trainer_;

  // This will be replaced by a mock tick clock during tests.
  const base::TickClock* const tick_clock_;

  base::OneShotTimer model_timer_;

  base::Optional<AlsReader::AlsInitStatus> als_init_status_;
  base::Optional<bool> brightness_monitor_success_;

  // Whether this modeller has initialized successfully, including connecting
  // to AlsReader, BrightnessMonitor and loading a Trainer.
  // Initially has no value. Guaranteed to have a value after the completion of
  // |OnCurveLoadedFromDisk|.
  base::Optional<bool> is_modeller_enabled_;

  base::FilePath curve_path_;

  // True if a personal curve was successfully loaded from disk and passed to
  // Trainer and Trainer reported it was valid.
  bool has_initial_personal_curve_ = false;

  // Global curve constructed from predefined params.
  const MonotoneCubicSpline global_curve_;

  // Recent |kNumberAmbientValuesToTrack| ambient values.
  base::RingBuffer<AmbientLightSample, kNumberAmbientValuesToTrack>
      ambient_light_values_;

  std::vector<TrainingDataPoint> data_cache_;

  base::ObserverList<Modeller::Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ModellerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModellerImpl);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_IMPL_H_
