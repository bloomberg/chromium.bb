// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tool is used to benchmark the render model used by the compositor

// Most of this file is derived from the source of the tile_render_bench tool,
// and has been changed to  support running a sequence of independent
// simulations for our different render models and test cases.

#include <stdio.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "gpu/tools/compositor_model_bench/render_model_utils.h"
#include "gpu/tools/compositor_model_bench/render_models.h"
#include "gpu/tools/compositor_model_bench/render_tree.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/glx.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/gl/glx_util.h"
#include "ui/gl/init/gl_factory.h"

using base::DirectoryExists;
using base::PathExists;
using base::TimeTicks;
using std::string;

struct SimulationSpecification {
  string simulation_name;
  base::FilePath input_path;
  RenderModel model_under_test;
  TimeTicks simulation_start_time;
  int frames_rendered;
};

// Forward declarations
class Simulator;
void _process_events(Simulator* sim);
void _update_loop(Simulator* sim);

class Simulator {
 public:
  Simulator(int seconds_per_test, const base::FilePath& output_path)
      : output_path_(output_path),
        seconds_per_test_(seconds_per_test),
        gl_context_(nullptr),
        window_width_(WINDOW_WIDTH),
        window_height_(WINDOW_HEIGHT) {}

  ~Simulator() {
    // Cleanup GL.
    auto display = connection_->GetXlibDisplay(x11::XlibDisplayType::kFlushing);
    glXMakeCurrent(display, 0, nullptr);
    glXDestroyContext(display, gl_context_);

    // The window and X11 connection will be cleaned up when connection_ is
    // destroyed.
  }

  void QueueTest(const base::FilePath& path) {
    SimulationSpecification spec;

    // To get a std::string, we'll try to get an ASCII simulation name.
    // If the name of the file wasn't ASCII, this will give an empty simulation
    //  name, but that's not really harmful (we'll still warn about it though.)
    spec.simulation_name = path.BaseName().RemoveExtension().MaybeAsASCII();
    if (spec.simulation_name.empty()) {
      LOG(WARNING) << "Simulation for path " << path.LossyDisplayName()
                   << " will have a blank simulation name, since the file name "
                      "isn't ASCII";
    }
    spec.input_path = path;
    spec.model_under_test = ForwardRenderModel;
    spec.frames_rendered = 0;

    sims_remaining_.push(spec);

    // The following lines are commented out pending the addition
    // of the new render model once this version gets fully checked in.
    //
    //  spec.model_under_test = KDTreeRenderModel;
    //  sims_remaining_.push(spec);
  }

  void Run() {
    if (sims_remaining_.empty()) {
      LOG(WARNING) << "No configuration files loaded.";
      return;
    }

    base::AtExitManager at_exit;
    if (!InitX11() || !InitGLContext()) {
      LOG(FATAL) << "Failed to set up GUI.";
    }

    InitBuffers();

    LOG(INFO) << "Running " << sims_remaining_.size() << " simulations.";

    single_thread_task_executor_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&Simulator::ProcessEvents, weak_factory_.GetWeakPtr()));
    run_loop_.Run();
  }

  void ProcessEvents() {
    // Consume all the X events.
    connection_->Flush();
    connection_->ReadResponses();
    auto& events = connection_->events();
    while (!events.empty()) {
      auto event = std::move(events.front());
      events.pop_front();
      if (event.As<x11::ExposeEvent>())
        UpdateLoop();
      else if (auto* configure = event.As<x11::ConfigureNotifyEvent>())
        Resize(configure->width, configure->height);
    }
  }

  void UpdateLoop() {
    if (UpdateTestStatus())
      UpdateCurrentTest();
  }

 private:
  // Initialize X11. Returns true if successful. This method creates the
  // X11 window. Further initialization is done in X11VideoRenderer.
  bool InitX11() {
    connection_ = std::make_unique<x11::Connection>();
    if (!connection_->Ready()) {
      LOG(FATAL) << "Cannot open X11 connection";
      return false;
    }

    // Creates the window.
    auto black_pixel = connection_->default_screen().black_pixel;
    window_ = connection_->GenerateId<x11::Window>();
    connection_->CreateWindow({
        .wid = window_,
        .parent = connection_->default_root(),
        .x = 1,
        .y = 1,
        .width = static_cast<uint16_t>(window_width_),
        .height = static_cast<uint16_t>(window_height_),
        .background_pixel = black_pixel,
        .border_pixel = black_pixel,
        .event_mask = x11::EventMask::Exposure | x11::EventMask::KeyPress |
                      x11::EventMask::StructureNotify,
    });
    x11::SetStringProperty(window_, x11::Atom::WM_NAME, x11::Atom::STRING,
                           "Compositor Model Bench");

    connection_->MapWindow({window_});

    connection_->ConfigureWindow({
        .window = window_,
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
    });

    return true;
  }

  // Initialize the OpenGL context.
  bool InitGLContext() {
    if (!gl::init::InitializeGLOneOff()) {
      LOG(FATAL) << "gl::init::InitializeGLOneOff failed";
      return false;
    }

    auto* glx_config = gl::GetFbConfigForWindow(connection_.get(), window_);
    if (!glx_config)
      return false;
    auto* visual =
        glXGetVisualFromFBConfig(connection_->GetXlibDisplay(), glx_config);
    DCHECK(visual);

    gl_context_ = glXCreateContext(
        connection_->GetXlibDisplay(x11::XlibDisplayType::kSyncing), visual,
        nullptr, true /* Direct rendering */);

    if (!gl_context_)
      return false;

    auto display = connection_->GetXlibDisplay(x11::XlibDisplayType::kFlushing);
    if (!glXMakeCurrent(display, static_cast<uint32_t>(window_), gl_context_)) {
      glXDestroyContext(display, gl_context_);
      gl_context_ = nullptr;
      return false;
    }

    return true;
  }

  bool InitializeNextTest() {
    SimulationSpecification& spec = sims_remaining_.front();
    LOG(INFO) << "Initializing test for " << spec.simulation_name << "("
              << ModelToString(spec.model_under_test) << ")";
    const base::FilePath& path = spec.input_path;

    std::unique_ptr<RenderNode> root = BuildRenderTreeFromFile(path);
    if (!root) {
      LOG(ERROR) << "Couldn't parse test configuration file "
                 << path.LossyDisplayName();
      return false;
    }

    current_sim_ = ConstructSimulationModel(
        spec.model_under_test, std::move(root), window_width_, window_height_);
    return !!current_sim_;
  }

  void CleanupCurrentTest() {
    LOG(INFO) << "Finished test " << sims_remaining_.front().simulation_name;

    current_sim_.reset();
  }

  void UpdateCurrentTest() {
    ++sims_remaining_.front().frames_rendered;

    if (current_sim_)
      current_sim_->Update();

    glXSwapBuffers(connection_->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
                   static_cast<uint32_t>(window_));

    auto window = static_cast<x11::Window>(window_);
    x11::ExposeEvent ev{
        .window = window,
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
    };
    x11::SendEvent(ev, window, x11::EventMask::Exposure);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Simulator::UpdateLoop, weak_factory_.GetWeakPtr()));
  }

  void DumpOutput() {
    LOG(INFO) << "Successfully ran " << sims_completed_.size() << " tests";

    FILE* f = base::OpenFile(output_path_, "w");

    if (!f) {
      LOG(ERROR) << "Failed to open output file "
                 << output_path_.LossyDisplayName();
      exit(-1);
    }

    LOG(INFO) << "Writing results to " << output_path_.LossyDisplayName();

    fputs("{\n\t\"results\": [\n", f);

    while (sims_completed_.size()) {
      SimulationSpecification i = sims_completed_.front();
      fprintf(f,
              "\t\t{\"simulation_name\":\"%s\",\n"
              "\t\t\t\"render_model\":\"%s\",\n"
              "\t\t\t\"frames_drawn\":%d\n"
              "\t\t},\n",
              i.simulation_name.c_str(), ModelToString(i.model_under_test),
              i.frames_rendered);
      sims_completed_.pop();
    }

    fputs("\t]\n}", f);
    base::CloseFile(f);
  }

  bool UpdateTestStatus() {
    TimeTicks& current_start = sims_remaining_.front().simulation_start_time;
    base::TimeDelta d = TimeTicks::Now() - current_start;
    if (!current_start.is_null() && d.InSeconds() > seconds_per_test_) {
      CleanupCurrentTest();
      sims_completed_.push(sims_remaining_.front());
      sims_remaining_.pop();
    }

    if (sims_remaining_.size() &&
        sims_remaining_.front().simulation_start_time.is_null()) {
      while (sims_remaining_.size() && !InitializeNextTest()) {
        sims_remaining_.pop();
      }
      if (sims_remaining_.size()) {
        sims_remaining_.front().simulation_start_time = TimeTicks::Now();
      }
    }

    if (sims_remaining_.empty()) {
      DumpOutput();
      run_loop_.QuitWhenIdle();
      return false;
    }

    return true;
  }

  void Resize(int width, int height) {
    window_width_ = width;
    window_height_ = height;
    if (current_sim_)
      current_sim_->Resize(window_width_, window_height_);
  }

  base::SingleThreadTaskExecutor single_thread_task_executor_;
  base::RunLoop run_loop_;

  // Simulation task list for this execution
  std::unique_ptr<RenderModelSimulator> current_sim_;
  base::queue<SimulationSpecification> sims_remaining_;
  base::queue<SimulationSpecification> sims_completed_;
  base::FilePath output_path_;
  // Amount of time to run each simulation
  int seconds_per_test_;
  // GUI data
  std::unique_ptr<x11::Connection> connection_;
  x11::Window window_ = x11::Window::None;
  GLXContext gl_context_;
  int window_width_;
  int window_height_;
  base::WeakPtrFactory<Simulator> weak_factory_{this};
};

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (argc != 3 && argc != 4) {
    LOG(INFO)
        << "Usage: \n"
        << cl->GetProgram().BaseName().LossyDisplayName()
        << "--in=[input path] --out=[output path] (duration=[seconds])\n"
           "The input path specifies either a JSON configuration file or\n"
           "a directory containing only these files\n"
           "(if a directory is specified, simulations will be run for\n"
           "all files in that directory and subdirectories)\n"
           "The optional duration parameter specifies the (integer)\n"
           "number of seconds to be spent on each simulation.\n"
           "Performance measurements for the specified simulation(s) are\n"
           "written to the output path.";
    return -1;
  }

  int seconds_per_test = 1;
  if (cl->HasSwitch("duration")) {
    seconds_per_test = atoi(cl->GetSwitchValueASCII("duration").c_str());
  }

  Simulator sim(seconds_per_test, cl->GetSwitchValuePath("out"));
  base::FilePath inPath = cl->GetSwitchValuePath("in");

  if (!PathExists(inPath)) {
    LOG(FATAL) << "Path does not exist: " << inPath.LossyDisplayName();
    return -1;
  }

  if (DirectoryExists(inPath)) {
    LOG(INFO) << "(input path is a directory)";
    base::FileEnumerator dirItr(inPath, true, base::FileEnumerator::FILES);
    for (base::FilePath f = dirItr.Next(); !f.empty(); f = dirItr.Next()) {
      sim.QueueTest(f);
    }
  } else {
    LOG(INFO) << "(input path is a file)";
    sim.QueueTest(inPath);
  }

  sim.Run();

  return 0;
}
