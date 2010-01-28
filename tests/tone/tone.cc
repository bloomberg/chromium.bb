/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// NaCl audio tone demo
//   render a simple tone to the audio device for a couple seconds

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#if !defined(STANDALONE)
#include <nacl/nacl_av.h>
#endif
#if defined(STANDALONE)
#include "native_client/common/standalone.h"
#endif

const int kMaxDuration = 60;
const int kMaxFrequency = 24000;
const int kMaxAmplitude = 32000;
const int kSampling = 48000;
const int kDesiredSampleRate = 4096;
const int kMaxDemoLoop = 500;
const int kMinAmplitudeCutoff = 200;
const float kRampTime = 0.25f;
const float kPI = 3.14159265f;
int g_duration = 3;
int g_frequency = 1000;
int g_amplitude = 16000;
int g_loopcount = 1;

struct AudioControl {
  int             uid;
  volatile bool   running;
  volatile bool   enable_output;
  pthread_t       thread_id;
  int             sample_rate;
  pthread_mutex_t start_barrier;
};

int global_audio_thread_exit = 0;


double gettimed() {
  timeval tv;
  double sec, usec;
  const float kUSec = 1.e6f;
  gettimeofday(&tv, NULL);
  sec = static_cast<double>(tv.tv_sec);
  usec = static_cast<double>(tv.tv_usec);
  return (sec * kUSec + usec) / kUSec;
}


// pumps the audio via nacl_audio_update
// this is a very simple sine wave generator, with no mixing
void* AudioThread(void *userdata) {
  AudioControl *ac = reinterpret_cast<AudioControl *>(userdata);
  int16_t sbuffer[kNaClAudioBufferLength];
  size_t count = 0;
  float delta = static_cast<float>(g_frequency) * (2.0f * kPI / kSampling);
  bool to_silence = false;
  bool from_silence = false;
  float grad = 0.0f, fclock = 0.0f, amplitude = 0.0f;

  printf("Audio thread: Starting up...\n");
  printf("Audio thread: uid = 0x%0X\n", ac->uid);
  printf("Audio thread: running = %s\n", ac->running ? "yes" : "no");
  // This lock/unlock will stall until main thread releases lock.
  // We do this because we can't call nacl_audio_update until
  // after the audio system has been successfully initialized.
  pthread_mutex_lock(&ac->start_barrier);
  pthread_mutex_unlock(&ac->start_barrier);
  printf("Audio thread: Entering output loop\n");
  while (ac->running) {
    bool last_enable = ac->enable_output;
    if (0 != nacl_audio_stream(sbuffer, &count))
      break;
    bool now_enable = ac->enable_output;
    // eliminate audio pops at post-startup and pre-shutdown
    if ((last_enable) && (!now_enable)) {
      // shutting down to silence
      to_silence = true;
      from_silence = false;
      grad = 1.0f;
    } else if ((!last_enable) && (now_enable)) {
      // starting up, reset sine wave clock
      fclock = 0.0f;
      from_silence = true;
      to_silence = false;
      grad = 1.0f;
    }
    // output to buffer
    count = count / 2;
    for (size_t i = 0; i < count; i += 2) {
      // sine wave
      int16_t wave = static_cast<int16_t>(sinf(fclock) * amplitude);
      // left channel
      sbuffer[i+0] = wave;
      // right channel
      sbuffer[i+1] = wave;
      // increment fclock by delta
      fclock += delta;
      // keep fclock within -2PI..2PI
      if (fclock > 2.0f * kPI)
        fclock = fclock - (2.0f * kPI);
      // ramp up / ramp down amplitude in transitions
      if (to_silence) {
        grad *= 0.99f;
        // ramp down amplitude
        amplitude = grad * g_amplitude;
        // if the wave gets close to 0, cut it
        if (abs(wave) < kMinAmplitudeCutoff)
          grad = 0.0f;
      } else if (from_silence) {
        grad *= 0.99f;
        // ramp up amplitude
        amplitude = (1.0f - grad) * g_amplitude;
      }
    }
  }
  printf("Audio thread: Shutting down...\n");
  global_audio_thread_exit++;
  return NULL;
}


// Initializes the audio system with the desired sample rate.
// (The audio system might choose a different sample rate.)
int InitAudioDemo(AudioControl *ac, int desired_samples) {
  int r;
  void *thread_result;

  // init basic audio controller
  ac->uid = 0xC0DE5EED;
  ac->running = true;
  ac->enable_output = false;
  pthread_mutex_init(&ac->start_barrier, NULL);
  pthread_mutex_lock(&ac->start_barrier);

  // create audio thread
  printf("Main thread: Spawning audio thread...\n");
  int p = pthread_create(&ac->thread_id, NULL, AudioThread, ac);
  if (0 != p) {
    printf("Main thread: Unable to create audio thread!\n");
    pthread_mutex_unlock(&ac->start_barrier);
    nacl_audio_shutdown();
    nacl_multimedia_shutdown();
    return -1;
  }

  // initialize multimedia
  r = nacl_multimedia_init(NACL_SUBSYSTEM_AUDIO);
  if (r != 0) {
    printf("Main thread: Couldn't init multimedia w/ audio\n");
    ac->running = false;
    pthread_mutex_unlock(&ac->start_barrier);
    pthread_join(ac->thread_id, &thread_result);
    return -1;
  }

  // Open the audio device
  printf("Main thread: Desired samples: %d\n", desired_samples);
  r = nacl_audio_init(NACL_AUDIO_FORMAT_STEREO_48K,
                      desired_samples, &ac->sample_rate);
  if (r != 0) {
    printf("Main thread: Couldn't init nacl audio\n");
    nacl_multimedia_shutdown();
    pthread_mutex_unlock(&ac->start_barrier);
    ac->running = false;
    pthread_join(ac->thread_id, &thread_result);
    return -1;
  }
  printf("Main thread: Obtained samples: %d\n", ac->sample_rate);

  // Audio thread can go ahead and start now
  pthread_mutex_unlock(&ac->start_barrier);

  return 0;
}


void Sleep(double x) {
  double start, current;
  start = gettimed();
  while (true) {
    current = gettimed();
    if ((current - start) > x) break;
  }
}


// Runs the demo loop for a few seconds...
void RunAudioDemo(AudioControl *ac) {
  // the audio is being continuously output by the AudioThread.
  // So we can just wait g_duration seconds here and do nothing...
  // (we'll be rude and ask for timeofday over and over
  // to exercise the cpu and memory while audio is playing.)
  printf("Main thread: Doing something for a few seconds\n");
  Sleep(0.2f);
  ac->enable_output = true;
  Sleep(static_cast<float>(g_duration));
  ac->enable_output = false;
  Sleep(0.2f);
}


void ShutdownAudioDemo(AudioControl *ac) {
  void *thread_result;
  // tell audio thread to exit
  ac->running = false;
  pthread_join(ac->thread_id, &thread_result);
  nacl_audio_shutdown();
  nacl_multimedia_shutdown();
  printf("Main thread: Exited gracefully\n");
}


int ToneTest() {
  AudioControl ac;
  if (0 == InitAudioDemo(&ac, kDesiredSampleRate)) {
    RunAudioDemo(&ac);
    ShutdownAudioDemo(&ac);
    printf("TEST PASSED\n");
    return 0;
  } else {
    printf("Main thread: Unable to init audio\n");
    return -1;
  }
}


// If user specifies options on cmd line, parse them
// here and update global settings as needed.
void ParseCmdLineArgs(int argc, char **argv) {
  // look for cmd line args
  if (argc > 1) {
    for (int i = 1; i < argc; ++i) {
      if (argv[i] == strstr(argv[i], "-d")) {
        int d = atoi(&argv[i][2]);
        if ((d > 0) && (d < kMaxDuration)) {
          g_duration = d;
        }
      } else if (argv[i] == strstr(argv[i], "-a")) {
        int a = atoi(&argv[i][2]);
        if ((a > 0) && (a < kMaxAmplitude)) {
          g_amplitude = a;
        }
      } else if (argv[i] == strstr(argv[i], "-f")) {
        int f = atoi(&argv[i][2]);
        if ((f > 0) && (f < kMaxFrequency)) {
          g_frequency = f;
        }
      } else if (argv[i] == strstr(argv[i], "-c")) {
        int c = atoi(&argv[i][2]);
        if ((c > 0) && (c < kMaxDemoLoop)) {
          g_loopcount = c;
        }
      } else {
        printf("Tone SDL Demo\n");
        printf("usage: -f<n>   output tone at frequency n.\n");
        printf("       -a<n>   amplitude\n");
        printf("       -d<n>   duration of n seconds.\n");
        printf("       -c<n>   demo loop count.\n");
        printf("       --help  show this screen.\n");
        exit(0);
      }
    }
  }
}


// Parses cmd line options, initializes surface, runs the demo & shuts down.
int main(int argc, char **argv) {
  ParseCmdLineArgs(argc, argv);
  for (int i = 0; i < g_loopcount; ++i) {
    printf("Test %d, %d:\n", i, global_audio_thread_exit);
    ToneTest();
  }
  return 0;
}
