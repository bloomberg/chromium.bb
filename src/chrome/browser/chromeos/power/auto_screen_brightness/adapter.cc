// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/adapter.h"

#include <string>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

constexpr double kTol = 1e-10;

constexpr double kMaxBrightnessAdjustment = 100;
constexpr double kMinBrightnessAdjustment = 1;
constexpr size_t kNumBrightnessAdjustmentBuckets = 10;

const char* BrightnessChangeCauseToString(
    Adapter::BrightnessChangeCause cause) {
  switch (cause) {
    case Adapter::BrightnessChangeCause::kInitialAlsReceived:
      return "InitialAlsReceived";
    case Adapter::BrightnessChangeCause::kBrightneningThresholdExceeded:
      return "BrightneningThresholdExceeded";
    case Adapter::BrightnessChangeCause::kDarkeningThresholdExceeded:
      return "DarkeningThresholdExceeded";
    // |kImmediateBrightneningThresholdExceeded| and
    // |kImmediateDarkeningThresholdExceeded| are deprecated, and shouldn't show
    // up.
    case Adapter::BrightnessChangeCause::
        kImmediateBrightneningThresholdExceeded:
    case Adapter::BrightnessChangeCause::kImmediateDarkeningThresholdExceeded:
      return "UnexpectedImmediateTransition";
  }
  return "Unknown";
}

// Multiplies input |x| by a factor of 100 and round to the nearest int.
int ScaleAndConvertToInt(double x) {
  return static_cast<int>(x * 100 + 0.5);
}

// Returns the bucket number from |value| following the exponential bucketing
// scheme. Each bucket is inclusive from the left and exclusive from the right.
int ExponentialBucketing(double value) {
  static const double kExponentialBrightnessAdjustmentStepSize =
      std::log(kMaxBrightnessAdjustment / kMinBrightnessAdjustment) /
      (kNumBrightnessAdjustmentBuckets - 1);
  DCHECK_LE(kMinBrightnessAdjustment, value);
  DCHECK_LE(value, kMaxBrightnessAdjustment);
  return static_cast<int>(std::log(value / kMinBrightnessAdjustment) /
                          kExponentialBrightnessAdjustmentStepSize);
}

}  // namespace

Adapter::Params::Params() = default;

Adapter::AdapterDecision::AdapterDecision() = default;

Adapter::AdapterDecision::AdapterDecision(const AdapterDecision& decision) =
    default;

Adapter::Adapter(Profile* profile,
                 AlsReader* als_reader,
                 BrightnessMonitor* brightness_monitor,
                 Modeller* modeller,
                 ModelConfigLoader* model_config_loader,
                 MetricsReporter* metrics_reporter,
                 chromeos::PowerManagerClient* power_manager_client)
    : Adapter(profile,
              als_reader,
              brightness_monitor,
              modeller,
              model_config_loader,
              metrics_reporter,
              power_manager_client,
              base::DefaultTickClock::GetInstance()) {}

Adapter::~Adapter() = default;

void Adapter::OnAmbientLightUpdated(int lux) {
  // Ambient light data is only used when adapter is initialized to success.
  // |log_als_values_| may not be available to use when adapter is being
  // initialized.
  if (adapter_status_ != Status::kSuccess)
    return;

  DCHECK(log_als_values_);

  const base::TimeTicks now = tick_clock_->NowTicks();

  log_als_values_->SaveToBuffer({ConvertToLog(lux), now});

  const AdapterDecision& decision = CanAdjustBrightness(now);

  if (decision.no_brightness_change_cause)
    return;

  DCHECK(decision.brightness_change_cause);
  DCHECK(decision.log_als_avg_stddev);

  AdjustBrightness(*decision.brightness_change_cause,
                   decision.log_als_avg_stddev->avg);
}

void Adapter::OnAlsReaderInitialized(AlsReader::AlsInitStatus status) {
  DCHECK(!als_init_status_);

  als_init_status_ = status;
  als_init_time_ = tick_clock_->NowTicks();
  UpdateStatus();
}

void Adapter::OnBrightnessMonitorInitialized(bool success) {
  DCHECK(!brightness_monitor_success_.has_value());

  brightness_monitor_success_ = success;
  UpdateStatus();
}

void Adapter::OnUserBrightnessChanged(double old_brightness_percent,
                                      double new_brightness_percent) {
  const auto first_recent_user_brightness_request_time =
      first_recent_user_brightness_request_time_;
  const auto decision_at_first_recent_user_brightness_request =
      decision_at_first_recent_user_brightness_request_;

  first_recent_user_brightness_request_time_ = base::nullopt;
  decision_at_first_recent_user_brightness_request_ = base::nullopt;

  // We skip this notification if adapter hasn't been initialised because its
  // |params_| may change. We need to log even if adapter is initialized to
  // disabled.
  if (adapter_status_ == Status::kInitializing) {
    return;
  }

  // |latest_brightness_change_time_|, |current_brightness_|,
  // |average_log_ambient_lux_| and thresholds are only needed if adapter is
  // |kSuccess|.
  if (adapter_status_ == Status::kSuccess) {
    if (!decision_at_first_recent_user_brightness_request) {
      // This should not happen frequently.
      UMA_HISTOGRAM_BOOLEAN(
          "AutoScreenBrightness.MissingPriorUserBrightnessRequest", true);
      return;
    }
    DCHECK(first_recent_user_brightness_request_time);
    LogAdapterDecision(*first_recent_user_brightness_request_time,
                       *decision_at_first_recent_user_brightness_request,
                       old_brightness_percent, new_brightness_percent);

    const base::Optional<AlsAvgStdDev> log_als_avg_stddev =
        decision_at_first_recent_user_brightness_request->log_als_avg_stddev;

    OnBrightnessChanged(
        *first_recent_user_brightness_request_time, new_brightness_percent,
        log_als_avg_stddev ? base::Optional<double>(log_als_avg_stddev->avg)
                           : base::nullopt);
  }

  if (!metrics_reporter_)
    return;

  DCHECK(als_init_status_);

  switch (*als_init_status_) {
    case AlsReader::AlsInitStatus::kSuccess:
      DCHECK(!params_.metrics_key.empty());
      if (params_.metrics_key == "eve") {
        metrics_reporter_->OnUserBrightnessChangeRequested(
            MetricsReporter::UserAdjustment::kEve);
        return;
      }
      if (params_.metrics_key == "atlas") {
        metrics_reporter_->OnUserBrightnessChangeRequested(
            MetricsReporter::UserAdjustment::kAtlas);
        return;
      }
      metrics_reporter_->OnUserBrightnessChangeRequested(
          MetricsReporter::UserAdjustment::kSupportedAls);
      return;
    case AlsReader::AlsInitStatus::kDisabled:
    case AlsReader::AlsInitStatus::kMissingPath:
      metrics_reporter_->OnUserBrightnessChangeRequested(
          MetricsReporter::UserAdjustment::kNoAls);
      return;
    case AlsReader::AlsInitStatus::kIncorrectConfig:
      metrics_reporter_->OnUserBrightnessChangeRequested(
          MetricsReporter::UserAdjustment::kUnsupportedAls);
      return;
    case AlsReader::AlsInitStatus::kInProgress:
      NOTREACHED() << "ALS should have been initialized with a valid value.";
  }
}

void Adapter::OnUserBrightnessChangeRequested() {
  const base::TimeTicks now = tick_clock_->NowTicks();
  // We skip this notification if adapter hasn't been initialised (because its
  // |params_| may change), or, if adapter is disabled (because adapter won't
  // change brightness anyway).
  if (adapter_status_ != Status::kSuccess) {
    // Set |first_recent_user_brightness_request_time_| if not already set, so
    // that it won't be reset.
    if (!first_recent_user_brightness_request_time_)
      first_recent_user_brightness_request_time_ = now;
    return;
  }

  if (!first_recent_user_brightness_request_time_) {
    DCHECK(log_als_values_);
    // Check what model would say and also get latest AlsAvgStdDev.
    decision_at_first_recent_user_brightness_request_ =
        CanAdjustBrightness(now);
    first_recent_user_brightness_request_time_ = now;
  }

  if (params_.user_adjustment_effect != UserAdjustmentEffect::kContinueAuto) {
    // Adapter will stop making brightness adjustment until suspend/resume or
    // when browser restarts.
    adapter_disabled_by_user_adjustment_ = true;
  }
}

void Adapter::OnModelTrained(const MonotoneCubicSpline& brightness_curve) {
  // It's ok to record brightness curve even when adapter is not completely
  // initialized. But we stop recording curves if we know adapter is disabled.
  if (adapter_status_ == Status::kDisabled)
    return;

  personal_curve_.emplace(brightness_curve);
}

void Adapter::OnModelInitialized(
    const base::Optional<MonotoneCubicSpline>& global_curve,
    const base::Optional<MonotoneCubicSpline>& personal_curve) {
  DCHECK(!model_initialized_);

  model_initialized_ = true;

  if (global_curve)
    global_curve_.emplace(*global_curve);
  if (personal_curve)
    personal_curve_.emplace(*personal_curve);

  UpdateStatus();
}

void Adapter::OnModelConfigLoaded(base::Optional<ModelConfig> model_config) {
  DCHECK(!model_config_exists_.has_value());

  model_config_exists_ = model_config.has_value();

  if (model_config_exists_.value()) {
    InitParams(model_config.value());
  }

  UpdateStatus();
}

void Adapter::SuspendDone(const base::TimeDelta& /* sleep_duration */) {
  // We skip this notification if adapter hasn't been initialised (because its
  // |params_| may change), or, if adapter is disabled (because adapter won't
  // change brightness anyway).
  if (adapter_status_ != Status::kSuccess)
    return;

  if (params_.user_adjustment_effect == UserAdjustmentEffect::kPauseAuto)
    adapter_disabled_by_user_adjustment_ = false;
}

Adapter::Status Adapter::GetStatusForTesting() const {
  return adapter_status_;
}

bool Adapter::IsAppliedForTesting() const {
  return (adapter_status_ == Status::kSuccess &&
          !adapter_disabled_by_user_adjustment_);
}

base::Optional<MonotoneCubicSpline> Adapter::GetGlobalCurveForTesting() const {
  return global_curve_;
}

base::Optional<MonotoneCubicSpline> Adapter::GetPersonalCurveForTesting()
    const {
  return personal_curve_;
}

base::Optional<AlsAvgStdDev> Adapter::GetAverageAmbientWithStdDevForTesting(
    base::TimeTicks now) {
  DCHECK(log_als_values_);
  return log_als_values_->AverageAmbientWithStdDev(now);
}

double Adapter::GetBrighteningThresholdForTesting() const {
  return *brightening_threshold_;
}

double Adapter::GetDarkeningThresholdForTesting() const {
  return *darkening_threshold_;
}

base::Optional<double> Adapter::GetCurrentAvgLogAlsForTesting() const {
  return average_log_ambient_lux_;
}

std::unique_ptr<Adapter> Adapter::CreateForTesting(
    Profile* profile,
    AlsReader* als_reader,
    BrightnessMonitor* brightness_monitor,
    Modeller* modeller,
    ModelConfigLoader* model_config_loader,
    MetricsReporter* metrics_reporter,
    chromeos::PowerManagerClient* power_manager_client,
    const base::TickClock* tick_clock) {
  return base::WrapUnique(new Adapter(
      profile, als_reader, brightness_monitor, modeller, model_config_loader,
      metrics_reporter, power_manager_client, tick_clock));
}

Adapter::Adapter(Profile* profile,
                 AlsReader* als_reader,
                 BrightnessMonitor* brightness_monitor,
                 Modeller* modeller,
                 ModelConfigLoader* model_config_loader,
                 MetricsReporter* metrics_reporter,
                 chromeos::PowerManagerClient* power_manager_client,
                 const base::TickClock* tick_clock)
    : profile_(profile),
      als_reader_observer_(this),
      brightness_monitor_observer_(this),
      modeller_observer_(this),
      model_config_loader_observer_(this),
      power_manager_client_observer_(this),
      metrics_reporter_(metrics_reporter),
      power_manager_client_(power_manager_client),
      tick_clock_(tick_clock),
      weak_ptr_factory_(this) {
  DCHECK(profile);
  DCHECK(als_reader);
  DCHECK(brightness_monitor);
  DCHECK(modeller);
  DCHECK(model_config_loader);
  DCHECK(power_manager_client);

  als_reader_observer_.Add(als_reader);
  brightness_monitor_observer_.Add(brightness_monitor);
  modeller_observer_.Add(modeller);
  model_config_loader_observer_.Add(model_config_loader);
  power_manager_client_observer_.Add(power_manager_client);

  power_manager_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&Adapter::OnPowerManagerServiceAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void Adapter::InitParams(const ModelConfig& model_config) {
  if (!base::FeatureList::IsEnabled(features::kAutoScreenBrightness)) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  params_.metrics_key = model_config.metrics_key;
  params_.brightening_log_lux_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightening_log_lux_threshold",
      params_.brightening_log_lux_threshold);

  params_.darkening_log_lux_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "darkening_log_lux_threshold",
      params_.darkening_log_lux_threshold);

  params_.stabilization_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "stabilization_threshold",
      params_.stabilization_threshold);

  const int model_curve = base::GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "model_curve", 2);
  if (model_curve < 0 || model_curve > 2) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }
  params_.model_curve = static_cast<ModelCurve>(model_curve);

  const int auto_brightness_als_horizon_seconds =
      GetFieldTrialParamByFeatureAsInt(
          features::kAutoScreenBrightness,
          "auto_brightness_als_horizon_seconds",
          model_config.auto_brightness_als_horizon_seconds);

  if (auto_brightness_als_horizon_seconds <= 0) {
    adapter_status_ = Status::kDisabled;
    LogParameterError(ParameterError::kAdapterError);
    return;
  }

  params_.auto_brightness_als_horizon =
      base::TimeDelta::FromSeconds(auto_brightness_als_horizon_seconds);
  log_als_values_ = std::make_unique<AmbientLightSampleBuffer>(
      params_.auto_brightness_als_horizon);

  // TODO(jiameng): move this to device config once we complete experiments.
  if (model_config.metrics_key == "atlas") {
    params_.user_adjustment_effect = UserAdjustmentEffect::kContinueAuto;
  }

  const int user_adjustment_effect_as_int = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "user_adjustment_effect",
      static_cast<int>(params_.user_adjustment_effect));
  if (user_adjustment_effect_as_int < 0 || user_adjustment_effect_as_int > 2) {
    LogParameterError(ParameterError::kAdapterError);
    return;
  }
  params_.user_adjustment_effect =
      static_cast<UserAdjustmentEffect>(user_adjustment_effect_as_int);

  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.UserAdjustmentEffect",
                            params_.user_adjustment_effect);
}

void Adapter::OnPowerManagerServiceAvailable(bool service_is_ready) {
  power_manager_service_available_ = service_is_ready;
  UpdateStatus();
}

void Adapter::UpdateStatus() {
  if (adapter_status_ != Status::kInitializing)
    return;

  if (!als_init_status_)
    return;

  const bool als_success =
      *als_init_status_ == AlsReader::AlsInitStatus::kSuccess;
  if (!als_success) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  if (!brightness_monitor_success_.has_value())
    return;

  if (!*brightness_monitor_success_) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  if (!model_initialized_)
    return;

  if (!global_curve_) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  if (!power_manager_service_available_.has_value())
    return;

  if (!*power_manager_service_available_) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  if (!model_config_exists_.has_value())
    return;

  if (!model_config_exists_.value()) {
    adapter_status_ = Status::kDisabled;
    return;
  }

  adapter_status_ = Status::kSuccess;
}

Adapter::AdapterDecision Adapter::CanAdjustBrightness(base::TimeTicks now) {
  DCHECK_EQ(adapter_status_, Status::kSuccess);
  DCHECK(log_als_values_);
  DCHECK(!als_init_time_.is_null());

  AdapterDecision decision;
  const base::Optional<AlsAvgStdDev> log_als_avg_stddev =
      log_als_values_->AverageAmbientWithStdDev(now);
  decision.log_als_avg_stddev = log_als_avg_stddev;

  // User has previously manually changed brightness and it (at least
  // temporarily) stopped the adapter from operating.
  if (adapter_disabled_by_user_adjustment_) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kDisabledByUser;
    return decision;
  }

  // Do not change brightness if it's set by the policy, but do not completely
  // disable the model as the policy could change.
  if (profile_->GetPrefs()->GetInteger(
          ash::prefs::kPowerAcScreenBrightnessPercent) >= 0 ||
      profile_->GetPrefs()->GetInteger(
          ash::prefs::kPowerBatteryScreenBrightnessPercent) >= 0) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kBrightnessSetByPolicy;
    return decision;
  }

  if (params_.model_curve == ModelCurve::kPersonal && !personal_curve_) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kMissingPersonalCurve;
    return decision;
  }

  // Wait until we've had enough ALS data to calc avg.
  if (now - als_init_time_ < params_.auto_brightness_als_horizon) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kWaitingForInitialAls;
    return decision;
  }

  // Check if we've waited long enough from previous brightness change (either
  // by user or by model).
  if (!latest_brightness_change_time_.is_null() &&
      now - latest_brightness_change_time_ <
          params_.auto_brightness_als_horizon) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kWaitingForAvgHorizon;
    return decision;
  }

  if (!log_als_avg_stddev) {
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kMissingAlsData;
    return decision;
  }

  if (!average_log_ambient_lux_) {
    // Either
    // 1. brightness hasn't been changed, or,
    // 2. brightness was changed by the user but there wasn't any ALS data. This
    //    case should be rare.
    // In either case, we change brightness as soon as we have brightness.
    decision.brightness_change_cause =
        BrightnessChangeCause::kInitialAlsReceived;
    return decision;
  }

  // The following thresholds should have been set last time when brightness was
  // changed.
  DCHECK(brightening_threshold_);
  DCHECK(darkening_threshold_);

  if (log_als_avg_stddev->avg > *brightening_threshold_) {
    if (log_als_avg_stddev->stddev <= params_.brightening_log_lux_threshold *
                                          params_.stabilization_threshold) {
      decision.brightness_change_cause =
          BrightnessChangeCause::kBrightneningThresholdExceeded;
      return decision;
    }
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kFluctuatingAlsIncrease;
    return decision;
  }

  if (log_als_avg_stddev->avg < *darkening_threshold_) {
    if (log_als_avg_stddev->stddev <=
        params_.darkening_log_lux_threshold * params_.stabilization_threshold) {
      decision.brightness_change_cause =
          BrightnessChangeCause::kDarkeningThresholdExceeded;
      return decision;
    }
    decision.no_brightness_change_cause =
        NoBrightnessChangeCause::kFluctuatingAlsDecrease;
    return decision;
  }

  decision.no_brightness_change_cause =
      NoBrightnessChangeCause::kMinimalAlsChange;
  return decision;
}

void Adapter::AdjustBrightness(BrightnessChangeCause cause,
                               double log_als_avg) {
  const double brightness = GetBrightnessBasedOnAmbientLogLux(log_als_avg);

  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(brightness);
  request.set_transition(
      power_manager::SetBacklightBrightnessRequest_Transition_GRADUAL);
  request.set_cause(power_manager::SetBacklightBrightnessRequest_Cause_MODEL);
  power_manager_client_->SetScreenBrightness(request);

  const base::TimeTicks brightness_change_time = tick_clock_->NowTicks();
  if (!latest_model_brightness_change_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "AutoScreenBrightness.BrightnessChange.ElapsedTime",
        brightness_change_time - latest_model_brightness_change_time_);
  }
  latest_model_brightness_change_time_ = brightness_change_time;
  if (current_brightness_) {
    model_brightness_change_ = brightness - *current_brightness_;
  }

  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.BrightnessChange.Cause",
                            cause);

  WriteLogMessages(log_als_avg, brightness, cause);
  model_brightness_change_counter_++;

  OnBrightnessChanged(brightness_change_time, brightness, log_als_avg);
}

double Adapter::GetBrightnessBasedOnAmbientLogLux(
    double ambient_log_lux) const {
  DCHECK_EQ(adapter_status_, Status::kSuccess);
  switch (params_.model_curve) {
    case ModelCurve::kGlobal:
      return global_curve_->Interpolate(ambient_log_lux);
    case ModelCurve::kPersonal:
      DCHECK(personal_curve_);
      return personal_curve_->Interpolate(ambient_log_lux);
    default:
      // We use the latest curve available.
      if (personal_curve_)
        return personal_curve_->Interpolate(ambient_log_lux);
      return global_curve_->Interpolate(ambient_log_lux);
  }
}

void Adapter::OnBrightnessChanged(base::TimeTicks now,
                                  double new_brightness_percent,
                                  base::Optional<double> new_log_als) {
  DCHECK_NE(adapter_status_, Status::kInitializing);

  current_brightness_ = new_brightness_percent;
  latest_brightness_change_time_ = now;

  UMA_HISTOGRAM_BOOLEAN("AutoScreenBrightness.MissingAlsWhenBrightnessChanged",
                        !new_log_als);

  if (!new_log_als)
    return;

  // Update |average_log_ambient_lux_| with the new reference value. Brightness
  // will be changed by the model if next log-avg ALS value goes outside of the
  // range
  // [|darkening_threshold_|, |brightening_threshold_|].
  // Thresholds in |params_| are absolute values to be added/subtracted from
  // the reference values. Log-avg can be negative.
  average_log_ambient_lux_ = new_log_als;
  brightening_threshold_ = *new_log_als + params_.brightening_log_lux_threshold;
  darkening_threshold_ = *new_log_als - params_.darkening_log_lux_threshold;
}

void Adapter::WriteLogMessages(double new_log_als,
                               double new_brightness,
                               BrightnessChangeCause cause) const {
  DCHECK_EQ(adapter_status_, Status::kSuccess);
  const std::string old_log_als =
      average_log_ambient_lux_
          ? base::StringPrintf("%.4f", average_log_ambient_lux_.value()) + "->"
          : "";

  const std::string old_brightness =
      current_brightness_
          ? base::StringPrintf("%.4f", current_brightness_.value()) + "%->"
          : "";

  VLOG(1) << "Screen brightness change #" << model_brightness_change_counter_
          << ": "
          << "brightness=" << old_brightness
          << base::StringPrintf("%.4f", new_brightness) << "%"
          << " cause=" << BrightnessChangeCauseToString(cause)
          << " log_als=" << old_log_als
          << base::StringPrintf("%.4f", new_log_als);
}

void Adapter::LogAdapterDecision(
    base::TimeTicks first_recent_user_brightness_request_time,
    const AdapterDecision& decision,
    double old_brightness_percent,
    double new_brightness_percent) const {
  const bool was_previous_change_by_model =
      !latest_model_brightness_change_time_.is_null() &&
      latest_brightness_change_time_ == latest_model_brightness_change_time_;

  const bool is_decision_brightness_change =
      decision.brightness_change_cause.has_value();
  DCHECK_NE(is_decision_brightness_change,
            decision.no_brightness_change_cause.has_value());

  const std::string histogram_prefix =
      std::string("AutoScreenBrightness.AdapterDecisionAtUserChange.") +
      (is_decision_brightness_change ? "" : "No") + "BrightnessChange.";

  // What we log depends on whether the decision is to change or not to change.
  if (is_decision_brightness_change) {
    base::UmaHistogramEnumeration(histogram_prefix + "Cause",
                                  *decision.brightness_change_cause);
  } else {
    const NoBrightnessChangeCause no_brightness_change_cause =
        *decision.no_brightness_change_cause;
    base::UmaHistogramEnumeration(histogram_prefix + "Cause",
                                  no_brightness_change_cause);

    // If previous change was triggered by the model, we log additional metrics.
    if (was_previous_change_by_model) {
      UMA_HISTOGRAM_LONG_TIMES(
          "AutoScreenBrightness.ElapsedTimeBetweenModelAndUserAdjustments",
          first_recent_user_brightness_request_time -
              latest_model_brightness_change_time_);

      // If we have a recorded model change, compare it with user change and
      // log.
      if (model_brightness_change_ &&
          (no_brightness_change_cause ==
               NoBrightnessChangeCause::kMinimalAlsChange ||
           no_brightness_change_cause ==
               NoBrightnessChangeCause::kWaitingForAvgHorizon)) {
        const double user_brightness_adj =
            new_brightness_percent - old_brightness_percent;
        // Whether they are both +ve or negative.
        const bool user_model_same_direction =
            *model_brightness_change_ * user_brightness_adj >= 0;

        const int user_brightness_adj_bucket =
            ExponentialBucketing(std::max(1.0, std::abs(user_brightness_adj)));

        const int model_brightness_adj_bucket = ExponentialBucketing(
            std::max(1.0, std::abs(*model_brightness_change_)));

        const int logged_value =
            user_brightness_adj_bucket * kNumBrightnessAdjustmentBuckets +
            model_brightness_adj_bucket;

        const std::string prefix = "AutoScreenBrightness.";
        base::UmaHistogramExactLinear(
            prefix + (user_model_same_direction ? "Same" : "Opposite") +
                ".UserModelBrightnessAdjustments",
            logged_value, 100 /* value_max */);
      }
    }
  }

  // Log ALS delta and std-dev if they exist.
  if (decision.log_als_avg_stddev) {
    const int logged_stddev =
        ScaleAndConvertToInt(decision.log_als_avg_stddev->stddev);
    if (average_log_ambient_lux_) {
      const double log_als_diff =
          decision.log_als_avg_stddev->avg - *average_log_ambient_lux_;
      if (std::abs(log_als_diff) < kTol)
        return;

      const std::string dir = log_als_diff > 0 ? "Brighten" : "Darken";

      const int logged_value = ScaleAndConvertToInt(std::abs(log_als_diff));
      base::UmaHistogramCounts1000(histogram_prefix + dir + ".AlsDelta",
                                   logged_value);
      base::UmaHistogramCounts1000(histogram_prefix + dir + ".AlsStd",
                                   logged_stddev);
      return;
    }

    base::UmaHistogramCounts1000(histogram_prefix + "Unknown.AlsStd",
                                 logged_stddev);
  }
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
