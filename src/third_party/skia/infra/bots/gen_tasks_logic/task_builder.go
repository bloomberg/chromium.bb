// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package gen_tasks_logic

import (
	"log"
	"reflect"
	"time"

	"go.skia.org/infra/task_scheduler/go/specs"
)

// taskBuilder is a helper for creating a task.
type taskBuilder struct {
	*jobBuilder
	parts
	Name             string
	Spec             *specs.TaskSpec
	recipeProperties map[string]string
}

// newTaskBuilder returns a taskBuilder instance.
func newTaskBuilder(b *jobBuilder, name string) *taskBuilder {
	parts, err := b.jobNameSchema.ParseJobName(name)
	if err != nil {
		log.Fatal(err)
	}
	return &taskBuilder{
		jobBuilder:       b,
		parts:            parts,
		Name:             name,
		Spec:             &specs.TaskSpec{},
		recipeProperties: map[string]string{},
	}
}

// attempts sets the desired MaxAttempts for this task.
func (b *taskBuilder) attempts(a int) {
	b.Spec.MaxAttempts = a
}

// cache adds the given caches to the task.
func (b *taskBuilder) cache(caches ...*specs.Cache) {
	for _, c := range caches {
		alreadyHave := false
		for _, exist := range b.Spec.Caches {
			if c.Name == exist.Name {
				if !reflect.DeepEqual(c, exist) {
					log.Fatalf("Already have cache %s with a different definition!", c.Name)
				}
				alreadyHave = true
				break
			}
		}
		if !alreadyHave {
			b.Spec.Caches = append(b.Spec.Caches, c)
		}
	}
}

// cmd sets the command for the task.
func (b *taskBuilder) cmd(c ...string) {
	b.Spec.Command = c
}

// dimension adds the given dimensions to the task.
func (b *taskBuilder) dimension(dims ...string) {
	for _, dim := range dims {
		if !In(dim, b.Spec.Dimensions) {
			b.Spec.Dimensions = append(b.Spec.Dimensions, dim)
		}
	}
}

// expiration sets the expiration of the task.
func (b *taskBuilder) expiration(e time.Duration) {
	b.Spec.Expiration = e
}

// idempotent marks the task as idempotent.
func (b *taskBuilder) idempotent() {
	b.Spec.Idempotent = true
}

// isolate sets the isolate file used by the task.
func (b *taskBuilder) isolate(i string) {
	b.Spec.Isolate = b.relpath(i)
}

// env appends the given values to the given environment variable for the task.
func (b *taskBuilder) env(key string, values ...string) {
	if b.Spec.EnvPrefixes == nil {
		b.Spec.EnvPrefixes = map[string][]string{}
	}
	for _, value := range values {
		if !In(value, b.Spec.EnvPrefixes[key]) {
			b.Spec.EnvPrefixes[key] = append(b.Spec.EnvPrefixes[key], value)
		}
	}
}

// addToPATH adds the given locations to PATH for the task.
func (b *taskBuilder) addToPATH(loc ...string) {
	b.env("PATH", loc...)
}

// output adds the given paths as outputs to the task, which results in their
// contents being uploaded to the isolate server.
func (b *taskBuilder) output(paths ...string) {
	for _, path := range paths {
		if !In(path, b.Spec.Outputs) {
			b.Spec.Outputs = append(b.Spec.Outputs, path)
		}
	}
}

// serviceAccount sets the service account for this task.
func (b *taskBuilder) serviceAccount(sa string) {
	b.Spec.ServiceAccount = sa
}

// timeout sets the timeout(s) for this task.
func (b *taskBuilder) timeout(timeout time.Duration) {
	b.Spec.ExecutionTimeout = timeout
	b.Spec.IoTimeout = timeout // With kitchen, step logs don't count toward IoTimeout.
}

// dep adds the given tasks as dependencies of this task.
func (b *taskBuilder) dep(tasks ...string) {
	for _, task := range tasks {
		if !In(task, b.Spec.Dependencies) {
			b.Spec.Dependencies = append(b.Spec.Dependencies, task)
		}
	}
}

// cipd adds the given CIPD packages to the task.
func (b *taskBuilder) cipd(pkgs ...*specs.CipdPackage) {
	for _, pkg := range pkgs {
		alreadyHave := false
		for _, exist := range b.Spec.CipdPackages {
			if pkg.Name == exist.Name {
				if !reflect.DeepEqual(pkg, exist) {
					log.Fatalf("Already have package %s with a different definition!", pkg.Name)
				}
				alreadyHave = true
				break
			}
		}
		if !alreadyHave {
			b.Spec.CipdPackages = append(b.Spec.CipdPackages, pkg)
		}
	}
}

// useIsolatedAssets returns true if this task should use assets which are
// isolated rather than downloading directly from CIPD.
func (b *taskBuilder) useIsolatedAssets() bool {
	// Only do this on the RPIs for now. Other, faster machines shouldn't
	// see much benefit and we don't need the extra complexity, for now.
	if b.os("Android", "ChromeOS", "iOS") {
		return true
	}
	return false
}

// isolateAssetConfig represents a task which copies a CIPD package into
// isolate.
type isolateAssetCfg struct {
	alwaysIsolate   bool
	isolateTaskName string
	path            string
}

// asset adds the given assets to the task as CIPD packages.
func (b *taskBuilder) asset(assets ...string) {
	shouldIsolate := b.useIsolatedAssets()
	pkgs := make([]*specs.CipdPackage, 0, len(assets))
	for _, asset := range assets {
		if cfg, ok := ISOLATE_ASSET_MAPPING[asset]; ok && (cfg.alwaysIsolate || shouldIsolate) {
			b.dep(b.isolateCIPDAsset(asset))
		} else {
			pkgs = append(pkgs, b.MustGetCipdPackageFromAsset(asset))
		}
	}
	b.cipd(pkgs...)
}

// usesCCache adds attributes to tasks which use ccache.
func (b *taskBuilder) usesCCache() {
	b.cache(CACHES_CCACHE...)
}

// usesGit adds attributes to tasks which use git.
func (b *taskBuilder) usesGit() {
	b.cache(CACHES_GIT...)
	if b.matchOs("Win") {
		b.cipd(specs.CIPD_PKGS_GIT_WINDOWS_AMD64...)
	} else if b.matchOs("Mac") {
		b.cipd(specs.CIPD_PKGS_GIT_MAC_AMD64...)
	} else {
		b.cipd(specs.CIPD_PKGS_GIT_LINUX_AMD64...)
	}
}

// usesGo adds attributes to tasks which use go. Recipes should use
// "with api.context(env=api.infra.go_env)".
func (b *taskBuilder) usesGo() {
	b.usesGit() // Go requires Git.
	b.cache(CACHES_GO...)
	pkg := b.MustGetCipdPackageFromAsset("go")
	if b.matchOs("Win") || b.matchExtraConfig("Win") {
		pkg = b.MustGetCipdPackageFromAsset("go_win")
		pkg.Path = "go"
	}
	b.cipd(pkg)
}

// usesDocker adds attributes to tasks which use docker.
func (b *taskBuilder) usesDocker() {
	b.dimension("docker_installed:true")
}

// recipeProp adds the given recipe property key/value pair. Panics if
// getRecipeProps() was already called.
func (b *taskBuilder) recipeProp(key, value string) {
	if b.recipeProperties == nil {
		log.Fatal("taskBuilder.recipeProp() cannot be called after taskBuilder.getRecipeProps()!")
	}
	b.recipeProperties[key] = value
}

// recipeProps calls recipeProp for every key/value pair in the given map.
// Panics if getRecipeProps() was already called.
func (b *taskBuilder) recipeProps(props map[string]string) {
	for k, v := range props {
		b.recipeProp(k, v)
	}
}

// getRecipeProps returns JSON-encoded recipe properties. Subsequent calls to
// recipeProp[s] will panic, to prevent accidentally adding recipe properties
// after they have been added to the task.
func (b *taskBuilder) getRecipeProps() string {
	props := make(map[string]interface{}, len(b.recipeProperties)+2)
	props["buildername"] = b.Name
	props["$kitchen"] = struct {
		DevShell bool `json:"devshell"`
		GitAuth  bool `json:"git_auth"`
	}{
		DevShell: true,
		GitAuth:  true,
	}
	for k, v := range b.recipeProperties {
		props[k] = v
	}
	b.recipeProperties = nil
	return marshalJson(props)
}

// usesPython adds attributes to tasks which use python.
func (b *taskBuilder) usesPython() {
	// TODO(borenet): This is hacky and bad.
	b.cipd(specs.CIPD_PKGS_PYTHON[1])
	if (b.matchOs("Win") || b.matchExtraConfig("Win")) && !b.model("LenovoYogaC630") {
		b.cipd(CIPD_PKG_CPYTHON)
	} else if b.os("Mac10.15") && b.model("VMware7.1") {
		b.cipd(CIPD_PKG_CPYTHON)
	}
	b.cache(&specs.Cache{
		Name: "vpython",
		Path: "cache/vpython",
	})
	b.env("VPYTHON_VIRTUALENV_ROOT", "cache/vpython")
}

func (b *taskBuilder) usesNode() {
	// It is very important when including node via CIPD to also add it to the PATH of the
	// taskdriver or mysterious things can happen when subprocesses try to resolve node/npm.
	b.asset("node")
	b.addToPATH("node/node/bin")
}
