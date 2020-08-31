// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package gen_tasks_logic

import (
	"log"

	"go.skia.org/infra/task_scheduler/go/specs"
)

// jobBuilder provides helpers for creating a job.
type jobBuilder struct {
	*builder
	parts
	Name string
	Spec *specs.JobSpec
}

// newJobBuilder returns a jobBuilder for the given job name.
func newJobBuilder(b *builder, name string) *jobBuilder {
	p, err := b.jobNameSchema.ParseJobName(name)
	if err != nil {
		log.Fatal(err)
	}
	return &jobBuilder{
		builder: b,
		parts:   p,
		Name:    name,
		Spec:    &specs.JobSpec{},
	}
}

// priority sets the priority of the job.
func (b *jobBuilder) priority(p float64) {
	b.Spec.Priority = p
}

// trigger dictates when the job should be triggered.
func (b *jobBuilder) trigger(trigger string) {
	b.Spec.Trigger = trigger
}

// Create a taskBuilder and run the given function for it.
func (b *jobBuilder) addTask(name string, fn func(*taskBuilder)) {
	tb := newTaskBuilder(b, name)
	fn(tb)
	b.MustAddTask(tb.Name, tb.Spec)
	// Add the task to the Job's dependency set, removing any which are
	// accounted for by the new task's dependencies.
	b.Spec.TaskSpecs = append(b.Spec.TaskSpecs, tb.Name)
	newSpecs := make([]string, 0, len(b.Spec.TaskSpecs))
	for _, t := range b.Spec.TaskSpecs {
		if !In(t, tb.Spec.Dependencies) {
			newSpecs = append(newSpecs, t)
		}
	}
	b.Spec.TaskSpecs = newSpecs
}

// isolateCIPDAsset generates a task to isolate the given CIPD asset. Returns
// the name of the task.
func (b *jobBuilder) isolateCIPDAsset(asset string) string {
	cfg, ok := ISOLATE_ASSET_MAPPING[asset]
	if !ok {
		log.Fatalf("No isolate task for asset %q", asset)
	}
	b.addTask(cfg.isolateTaskName, func(b *taskBuilder) {
		b.cipd(b.MustGetCipdPackageFromAsset(asset))
		b.cmd("/bin/cp", "-rL", cfg.path, "${ISOLATED_OUTDIR}")
		b.linuxGceDimensions(MACHINE_TYPE_SMALL)
		b.idempotent()
		b.isolate("empty.isolate")
	})
	return cfg.isolateTaskName
}

// genTasksForJob generates the tasks needed by this job.
func (b *jobBuilder) genTasksForJob() {
	// Bundle Recipes.
	if b.Name == BUNDLE_RECIPES_NAME {
		b.bundleRecipes()
		return
	}
	if b.Name == BUILD_TASK_DRIVERS_NAME {
		b.buildTaskDrivers()
		return
	}

	// Isolate CIPD assets.
	if b.matchExtraConfig("Isolate") {
		for asset, cfg := range ISOLATE_ASSET_MAPPING {
			if cfg.isolateTaskName == b.Name {
				b.isolateCIPDAsset(asset)
				return
			}
		}
	}

	// RecreateSKPs.
	if b.extraConfig("RecreateSKPs") {
		b.recreateSKPs()
		return
	}

	// Update Go Dependencies.
	if b.extraConfig("UpdateGoDeps") {
		b.updateGoDeps()
		return
	}

	// Create docker image.
	if b.extraConfig("CreateDockerImage") {
		b.createDockerImage(b.extraConfig("WASM"))
		return
	}

	// Push apps from docker image.
	if b.extraConfig("PushAppsFromSkiaDockerImage") {
		b.createPushAppsFromSkiaDockerImage()
		return
	} else if b.extraConfig("PushAppsFromWASMDockerImage") {
		b.createPushAppsFromWASMDockerImage()
		return
	} else if b.extraConfig("PushAppsFromSkiaWASMDockerImages") {
		b.createPushAppsFromSkiaWASMDockerImages()
		return
	}

	// Infra tests.
	if b.extraConfig("InfraTests") {
		b.infra()
		return
	}

	// Housekeepers.
	if b.Name == "Housekeeper-PerCommit" {
		b.housekeeper()
		return
	}
	if b.Name == "Housekeeper-PerCommit-CheckGeneratedFiles" {
		b.checkGeneratedFiles()
		return
	}
	if b.Name == "Housekeeper-OnDemand-Presubmit" {
		b.priority(1)
		b.presubmit()
		return
	}

	// Compile bots.
	if b.role("Build") {
		if b.extraConfig("Android") && b.extraConfig("Framework") {
			// Android Framework compile tasks use a different recipe.
			b.androidFrameworkCompile()
			return
		} else if b.extraConfig("G3") && b.extraConfig("Framework") {
			// G3 compile tasks use a different recipe.
			b.g3FrameworkCompile()
			return
		} else {
			b.compile()
			return
		}
	}

	// BuildStats bots. This computes things like binary size.
	if b.role("BuildStats") {
		b.buildstats()
		return
	}

	// Valgrind runs at a low priority so that it doesn't occupy all the bots.
	if b.extraConfig("Valgrind") {
		// Priority of 0.085 should result in Valgrind tasks with a blamelist of ~10 commits having the
		// same score as other tasks with a blamelist of 1 commit, when we have insufficient bot
		// capacity to run more frequently.
		b.priority(0.085)
	}

	// Test bots.
	if b.role("Test") {
		b.dm()
		return
	}
	if b.role("FM") {
		b.fm()
		return
	}

	if b.extraConfig("Puppeteer") {
		// TODO(kjlubick) make this a new role
		b.puppeteer()
		return
	}

	// Perf bots.
	if b.role("Perf") {
		b.perf()
		return
	}

	log.Fatalf("Don't know how to handle job %q", b.Name)
}

func (b *jobBuilder) finish() {
	// Add the Job spec.
	if b.frequency("Nightly") {
		b.trigger(specs.TRIGGER_NIGHTLY)
	} else if b.frequency("Weekly") {
		b.trigger(specs.TRIGGER_WEEKLY)
	} else if b.extraConfig("Flutter", "CommandBuffer") {
		b.trigger(specs.TRIGGER_MASTER_ONLY)
	} else if b.frequency("OnDemand") || (b.extraConfig("Framework") && b.extraConfig("Android", "G3")) {
		b.trigger(specs.TRIGGER_ON_DEMAND)
	} else {
		b.trigger(specs.TRIGGER_ANY_BRANCH)
	}
	b.MustAddJob(b.Name, b.Spec)
}
