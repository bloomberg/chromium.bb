// Copyright 2019 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Thread.hpp"

#include "Context.hpp"
#include "EventListener.hpp"
#include "File.hpp"

namespace vk {
namespace dbg {

Thread::Thread(ID id, Context *ctx)
    : id(id)
    , broadcast(ctx->broadcast())
{}

void Thread::setName(const std::string &name)
{
	marl::lock lock(mutex);
	name_ = name;
}

std::string Thread::name() const
{
	marl::lock lock(mutex);
	return name_;
}

void Thread::onLocationUpdate(marl::lock &lock)
{
	auto location = frames.back()->location;

	if(state_ == State::Running)
	{
		if(location.file->hasBreakpoint(location.line))
		{
			broadcast->onLineBreakpointHit(id);
			state_ = State::Paused;
		}
	}

	switch(state_)
	{
		case State::Paused:
		{
			lock.wait(stateCV, [this]() REQUIRES(mutex) { return state_ != State::Paused; });
			break;
		}

		case State::Stepping:
		{
			if(!pauseAtFrame || pauseAtFrame == frames.back())
			{
				broadcast->onThreadStepped(id);
				state_ = State::Paused;
				lock.wait(stateCV, [this]() REQUIRES(mutex) { return state_ != State::Paused; });
				pauseAtFrame = 0;
			}
			break;
		}

		case State::Running:
			break;
	}
}

void Thread::enter(Context::Lock &ctxlck, const std::shared_ptr<File> &file, const std::string &function)
{
	auto frame = ctxlck.createFrame(file, function);
	auto isFunctionBreakpoint = ctxlck.isFunctionBreakpoint(function);

	marl::lock lock(mutex);
	frames.push_back(frame);
	if(isFunctionBreakpoint)
	{
		broadcast->onFunctionBreakpointHit(id);
		state_ = State::Paused;
	}
}

void Thread::exit()
{
	marl::lock lock(mutex);
	frames.pop_back();
}

void Thread::update(bool isStep, std::function<void(Frame &)> f)
{
	marl::lock lock(mutex);
	auto &frame = *frames.back();
	if(isStep)
	{
		auto oldLocation = frame.location;
		f(frame);
		if(frame.location != oldLocation)
		{
			onLocationUpdate(lock);
		}
	}
	else
	{
		f(frame);
	}
}

Frame Thread::frame() const
{
	marl::lock lock(mutex);
	return *frames.back();
}

std::vector<Frame> Thread::stack() const
{
	marl::lock lock(mutex);
	std::vector<Frame> out;
	out.reserve(frames.size());
	for(auto frame : frames)
	{
		out.push_back(*frame);
	}
	return out;
}

size_t Thread::depth() const
{
	marl::lock lock(mutex);
	return frames.size();
}

Thread::State Thread::state() const
{
	marl::lock lock(mutex);
	return state_;
}

void Thread::resume()
{
	{
		marl::lock lock(mutex);
		state_ = State::Running;
	}
	stateCV.notify_all();
}

void Thread::pause()
{
	marl::lock lock(mutex);
	state_ = State::Paused;
}

void Thread::stepIn()
{
	marl::lock lock(mutex);
	state_ = State::Stepping;
	pauseAtFrame.reset();
	stateCV.notify_all();
}

void Thread::stepOver()
{
	marl::lock lock(mutex);
	state_ = State::Stepping;
	pauseAtFrame = frames.back();
	stateCV.notify_all();
}

void Thread::stepOut()
{
	marl::lock lock(mutex);
	state_ = State::Stepping;
	pauseAtFrame = (frames.size() > 1) ? frames[frames.size() - 1] : nullptr;
	stateCV.notify_all();
}

}  // namespace dbg
}  // namespace vk