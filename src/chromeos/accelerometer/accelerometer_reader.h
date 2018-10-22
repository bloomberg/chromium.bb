// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ACCELEROMETER_ACCELEROMETER_READER_H_
#define CHROMEOS_ACCELEROMETER_ACCELEROMETER_READER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "chromeos/chromeos_export.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;

class SequencedTaskRunner;
}

namespace chromeos {

class AccelerometerFileReader;

// Reads an accelerometer device and reports data back to an
// AccelerometerDelegate.
class CHROMEOS_EXPORT AccelerometerReader {
 public:
  // The time to wait between reading the accelerometer.
  static const int kDelayBetweenReadsMs;

  // An interface to receive data from the AccelerometerReader.
  class Observer {
   public:
    virtual void OnAccelerometerUpdated(
        scoped_refptr<const AccelerometerUpdate> update) = 0;

   protected:
    virtual ~Observer() {}
  };

  static AccelerometerReader* GetInstance();

  void Initialize(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  AccelerometerReader();
  virtual ~AccelerometerReader();

 private:
  friend struct base::DefaultSingletonTraits<AccelerometerReader>;

  // Worker that will run on the base::SequencedTaskRunner provided to
  // Initialize. It will determine accelerometer configuration, read the data,
  // and notify observers.
  scoped_refptr<AccelerometerFileReader> accelerometer_file_reader_;

  DISALLOW_COPY_AND_ASSIGN(AccelerometerReader);
};

}  // namespace chromeos

#endif  // CHROMEOS_ACCELEROMETER_ACCELEROMETER_READER_H_
