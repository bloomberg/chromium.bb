// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <list>

extern "C" {
#include <libevdev/libevdev.h>
}

#include <gflags/gflags.h>

using std::list;
using std::string;
using std::stringstream;

DEFINE_string(dev_name, "", "Name of the the device to be monitored.");
DEFINE_bool(foreground, false, "Don't daemon()ize; run in foreground.");
DEFINE_int32(max_burst_touch_num, 2,
             "Max number of burst touches considered non-noise.");
DEFINE_int32(burst_length, 500,
             "Time interval (ms) during which burst touches are calculated.");
DEFINE_int32(max_touch_num, 3,
             "Max number of touch-down contacts considered non-noise.");
DEFINE_int32(report_min_interval_sec, 3600 * 4,
             "Two reports must be apart by this interval");
DEFINE_string(crash_cmd, "/sbin/crash_reporter --udev=SUBSYSTEM=TouchNoise",
              "Script to run when noise pattern detected");

struct TouchRecord {
  int track_id;
  struct timeval created_time;
  struct timeval removed_time;
};

void SynReportHandler(void* udata, EventStatePtr evstate, struct timeval* tv);

static inline double timeval_diff_ms(struct timeval *newer,
                                     struct timeval *older) {
  return (newer->tv_sec - older->tv_sec) * 1000 +
      (newer->tv_usec - older->tv_usec) / 1000.0;
}

static void RunCommandASync(const char* command) {
  printf("Run async command %s\n", command);
  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    exit(fork() == 0 ? system(command) : 0);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
}

static void SimpleLog(void* udata, int level, const char* format, ...) {
  /* Can be turned on for debugging
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  printf("%s\n", buffer);
  */
}

class MainLoop {
 public:
  MainLoop(int fd) : has_reported_(false) {
    memset(&evdev_, 0, sizeof(Evdev));
    memset(&evstate_, 0, sizeof(EventStateRec));
    evdev_.log = &SimpleLog;
    evdev_.fd = fd;
    evdev_.evstate = &evstate_;
    evdev_.syn_report = &SynReportHandler;
    evdev_.syn_report_udata = this;
  }

  void Run();
  void ApplyNewState(EventStatePtr evstate, struct timeval* tv);

 private:
  void CheckNoise(struct timeval time);
  void DoReport(const string& noise_desc, struct timeval time);
  bool IsNewTouch(int id);
  bool IsReleasedTouch(int id, EventStatePtr state);

 private:
  struct timeval last_report_time_;
  bool has_reported_;
  list<TouchRecord> current_touches_;
  list<TouchRecord> removed_touches_;
  Evdev evdev_;
  EventStateRec evstate_;
};

void SynReportHandler(void* udata, EventStatePtr evstate, struct timeval* tv) {
  static_cast<MainLoop*>(udata)->ApplyNewState(evstate, tv);
}

void MainLoop::Run() {
  int rc = Event_Init(&evdev_);
  if (rc != Success) {
    fprintf(stderr, "Event_Init() fails\n");
    return;
  }

  while (true) {
    int ret = EvdevRead(&evdev_);
    if (ret != Success) {
      if (ret == EAGAIN || ret == EWOULDBLOCK)
        continue;
      break;
    }
  }

  fprintf(stderr, "EvdevRead() fails\n");
}

// Check if the id does not exist in the current touches
bool MainLoop::IsNewTouch(int id) {
  list<TouchRecord>::iterator it = current_touches_.begin();
  for(; it != current_touches_.end(); it++) {
    if (it->track_id == id)
      return false;
  }
  return true;
}

// Check if the id does not exist in the new event state
bool MainLoop::IsReleasedTouch(int id, EventStatePtr state) {
  for (int i = 0; i < state->slot_count; i++) {
    if (state->slots[i].tracking_id == id)
      return false;
  }
  return true;
}

// Given the new event state, adjust the current touches (new touch)
// and removed touches (released touch), and check noise pattern.
void MainLoop::ApplyNewState(EventStatePtr state, struct timeval* tv) {
  bool need_check_noise = false;

  // Check if there is new touch
  for (int i = 0; i < state->slot_count; i++) {
    int id = state->slots[i].tracking_id;
    if (id != -1 && IsNewTouch(id)) {
        TouchRecord tr = {id, *tv, *tv};
        current_touches_.push_back(tr);
        need_check_noise = true;
    }
  }

  // Check if there is released touch
  list<TouchRecord>::iterator it = current_touches_.begin();
  while (it != current_touches_.end()) {
    if (IsReleasedTouch(it->track_id, state)) {
      it->removed_time = *tv;
      removed_touches_.push_back(*it);
      it = current_touches_.erase(it);
      need_check_noise = true;
    } else
      it++;
  }

  if (need_check_noise)
    CheckNoise(*tv);
}

void MainLoop::CheckNoise(struct timeval time) {
  // Check if there are more than max_touch_num touches at the same time.
  int touch_num = current_touches_.size();
  if (touch_num > FLAGS_max_touch_num) {
    stringstream oss;
    oss << touch_num << " touches detected ";
    DoReport(oss.str(), time);
  }

  list<TouchRecord>::iterator it = removed_touches_.begin();
  // Go through all the recently removed touches to see if any pattern of
  // noise touches are detected.
  while (it != removed_touches_.end()) {
    double diff = timeval_diff_ms(&time, &(*it).created_time);
    // The touch is too old to be considered as burst.
    if (diff > FLAGS_burst_length) {
      it = removed_touches_.erase(it);
      continue;
    }

    list<TouchRecord>::iterator it_next = it;
    it_next++;
    if (it_next != removed_touches_.end()) {
        double diff2 = timeval_diff_ms(&(*it).removed_time,
                                       &(*it_next).created_time);
        // We only consider successive touches with current touch's
        // down time after previous touch's release time as valid burst.
        if (diff2 > 0) {
          it = removed_touches_.erase(it);
          continue;
        }
    }
    it++;
  }

  int burst_touch_num = removed_touches_.size();
  // Check if there are more than max_burst_touch_num touches created
  // during burst_length.
  if (burst_touch_num > FLAGS_max_burst_touch_num) {
    stringstream oss;
    oss << burst_touch_num << " burst touches during last "
        << FLAGS_burst_length << "ms";
    DoReport(oss.str(), time);
  }
}

void MainLoop::DoReport(const string& noise_desc, struct timeval time) {
  printf("Touch noise detected: %s\n", noise_desc.c_str());
  if (has_reported_) {
    // Only one report is allowed within report_min_interval_sec.
    long diff_sec = timeval_diff_ms(&time, &last_report_time_) / 1000;
    if (diff_sec >= 0 && diff_sec < FLAGS_report_min_interval_sec)
      return;
  }
  RunCommandASync(FLAGS_crash_cmd.c_str());
  has_reported_ = true;
  last_report_time_ = time;
}

bool MatchDevName(const char* fname, const string& dev_name) {
  char name[256] = "Unknown";
  int fd = open(fname, O_RDONLY);
  if (fd < 0)
    return false;
  ioctl(fd, EVIOCGNAME(sizeof(name)), name);
  close(fd);
  return string(name) == dev_name;
}


// Scan the input subsystem to find the wanted devnode.
int OpenDevFdScan(const string& dev_name, struct udev* udev,
                  struct udev_monitor* udev_mon) {
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "input");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(dev_list_entry, devices) {
    const char* path = udev_list_entry_get_name(dev_list_entry);
    udev_device* dev = udev_device_new_from_syspath(udev, path);
    if (!dev)
      continue;
    const char* devnode = udev_device_get_devnode(dev);
    if (devnode && MatchDevName(devnode, dev_name)) {
      printf("Udev scan found devnode %s\n", devnode);
      int fd = open(devnode, O_RDONLY);
      udev_enumerate_unref(enumerate);
      udev_device_unref(dev);
      return fd;
    }
    udev_device_unref(dev);
  }
  udev_enumerate_unref(enumerate);
  return -1;
}

int OpenDevFd(const string& dev_name, struct udev* udev,
              struct udev_monitor* udev_mon) {
  int fd = -1;
  // Wait on the udev signal indicating the devnode is "added"/"changed"
  while (true) {
    struct udev_device* dev = udev_monitor_receive_device(udev_mon);
    if (!dev)
      continue;
    const char* devnode = udev_device_get_devnode(dev);
    if (devnode && MatchDevName(devnode, dev_name)) {
      const char* action = udev_device_get_action(dev);
      if (action && (string(action) == "add" || string(action) == "change")) {
        printf("Udev signal devnode %s action %s\n", devnode, action);
        fd = open(devnode, O_RDONLY);
        udev_device_unref(dev);
        break;
      }
    }
    udev_device_unref(dev);
  }
  return fd;
}

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (!FLAGS_foreground && daemon(0, 0) < 0) {
    printf("Can't daemonize\n");
    return -1;
  }

  if (FLAGS_dev_name.empty()) {
    printf("No device name provided\n");
    return -1;
  }

  struct udev *udev = udev_new();
  if(!udev) {
    fprintf(stderr, "Failed to initialize udev\n");
    return -1;
  }

  struct udev_monitor *udev_mon;
  udev_mon = udev_monitor_new_from_netlink(udev, "udev");
  if(!udev_mon) {
    fprintf(stderr, "Failed to create a udev monitor\n");
    return -1;
  }

  udev_monitor_filter_add_match_subsystem_devtype(udev_mon, "input", NULL);
  udev_monitor_enable_receiving(udev_mon);
  int udev_fd = udev_monitor_get_fd(udev_mon);
  int saved_flags = fcntl(udev_fd, F_GETFL);
  // We want to block at receiving udev events.
  fcntl(udev_fd, F_SETFL, saved_flags & ~O_NONBLOCK);

  // We only do udev scan once to see if we find the dev node is already there.
  int fd = OpenDevFdScan(FLAGS_dev_name, udev, udev_mon);
  while (true) {
    while (fd < 0)
      fd = OpenDevFd(FLAGS_dev_name, udev, udev_mon);

    MainLoop looper(fd);
    looper.Run();

    // Events reading fails.
    close(fd);
    fd = -1;
  }

  return 0;
}
