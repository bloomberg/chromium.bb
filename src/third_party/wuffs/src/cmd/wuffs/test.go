// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	cf "github.com/google/wuffs/cmd/commonflags"
)

func doBench(wuffsRoot string, args []string) error { return doBenchTest(wuffsRoot, args, true) }
func doTest(wuffsRoot string, args []string) error  { return doBenchTest(wuffsRoot, args, false) }

func doBenchTest(wuffsRoot string, args []string, bench bool) error {
	flags := flag.NewFlagSet("test", flag.ExitOnError)
	ccompilersFlag := flags.String("ccompilers", cf.CcompilersDefault, cf.CcompilersUsage)
	cformatterFlag := flags.String("cformatter", cf.CformatterDefault, cf.CformatterUsage)
	focusFlag := flags.String("focus", cf.FocusDefault, cf.FocusUsage)
	iterscaleFlag := flags.Int("iterscale", cf.IterscaleDefault, cf.IterscaleUsage)
	langsFlag := flags.String("langs", langsDefault, langsUsage)
	mimicFlag := flags.Bool("mimic", cf.MimicDefault, cf.MimicUsage)
	repsFlag := flags.Int("reps", cf.RepsDefault, cf.RepsUsage)
	skipgenFlag := flags.Bool("skipgen", skipgenDefault, skipgenUsage)
	skipgendepsFlag := flags.Bool("skipgendeps", skipgendepsDefault, skipgendepsUsage)

	if err := flags.Parse(args); err != nil {
		return err
	}

	langs, err := parseLangs(*langsFlag)
	if err != nil {
		return err
	}
	if !cf.IsAlphaNumericIsh(*ccompilersFlag) {
		return fmt.Errorf("bad -ccompilers flag value %q", *ccompilersFlag)
	}
	if !cf.IsAlphaNumericIsh(*cformatterFlag) {
		return fmt.Errorf("bad -cformatter flag value %q", *cformatterFlag)
	}
	if !cf.IsAlphaNumericIsh(*focusFlag) {
		return fmt.Errorf("bad -focus flag value %q", *focusFlag)
	}
	if *iterscaleFlag < cf.IterscaleMin || cf.IterscaleMax < *iterscaleFlag {
		return fmt.Errorf("bad -iterscale flag value %d, outside the range [%d ..= %d]",
			*iterscaleFlag, cf.IterscaleMin, cf.IterscaleMax)
	}
	if *repsFlag < cf.RepsMin || cf.RepsMax < *repsFlag {
		return fmt.Errorf("bad -reps flag value %d, outside the range [%d ..= %d]",
			*repsFlag, cf.RepsMin, cf.RepsMax)
	}

	args = flags.Args()
	if len(args) == 0 {
		args = []string{"base", "std/..."}
	}

	cmdArgs := []string(nil)
	if bench {
		cmdArgs = append(cmdArgs, "bench",
			fmt.Sprintf("-iterscale=%d", *iterscaleFlag),
			fmt.Sprintf("-reps=%d", *repsFlag),
		)
	} else {
		cmdArgs = append(cmdArgs, "test")
	}
	if *focusFlag != "" {
		cmdArgs = append(cmdArgs, fmt.Sprintf("-focus=%s", *focusFlag))
	}
	if *mimicFlag {
		cmdArgs = append(cmdArgs, "-mimic")
	}

	h := testHelper{
		wuffsRoot:  wuffsRoot,
		langs:      langs,
		cmdArgs:    cmdArgs,
		ccompilers: *ccompilersFlag,
	}

	// Ensure that we are testing the latest version of the generated code.
	if !*skipgenFlag {
		gh := genHelper{
			wuffsRoot:   wuffsRoot,
			langs:       langs,
			cformatter:  *cformatterFlag,
			skipgen:     *skipgenFlag,
			skipgendeps: *skipgendepsFlag,
		}
		for _, arg := range args {
			recursive := strings.HasSuffix(arg, "/...")
			if recursive {
				arg = arg[:len(arg)-4]
			}
			if arg == "" {
				continue
			}

			if err := gh.gen(arg, recursive); err != nil {
				return err
			}
		}
		if err := genrelease(wuffsRoot, langs, cf.Version{}); err != nil {
			return err
		}
	}

	failed := false
	for _, arg := range args {
		recursive := strings.HasSuffix(arg, "/...")
		if recursive {
			arg = arg[:len(arg)-4]
		}
		if arg == "" {
			continue
		}

		// Proceed with benching / testing the generated code.
		f, err := h.benchTest(arg, recursive)
		if err != nil {
			return err
		}
		failed = failed || f
	}
	if failed {
		s0, s1 := "test", "tests"
		if bench {
			s0, s1 = "bench", "benchmarks"
		}
		return fmt.Errorf("wuffs %s: some %s failed", s0, s1)
	}
	return nil
}

type testHelper struct {
	wuffsRoot  string
	langs      []string
	cmdArgs    []string
	ccompilers string
}

func (h *testHelper) benchTest(dirname string, recursive bool) (failed bool, err error) {
	if dirname == "base" {
		return false, nil
	}
	qualFilenames, dirnames, err := listDir(
		filepath.Join(h.wuffsRoot, filepath.FromSlash(dirname)), ".wuffs", recursive)
	if err != nil {
		return false, err
	}
	if len(qualFilenames) > 0 {
		f, err := h.benchTestDir(dirname)
		if err != nil {
			return false, err
		}
		failed = failed || f
	}
	if len(dirnames) > 0 {
		for _, d := range dirnames {
			f, err := h.benchTest(filepath.Join(dirname, d), recursive)
			if err != nil {
				return false, err
			}
			failed = failed || f
		}
	}
	return failed, nil
}

func (h *testHelper) benchTestDir(dirname string) (failed bool, err error) {
	if packageName := filepath.Base(dirname); !validName(packageName) {
		return false, fmt.Errorf(`invalid package %q, not in [a-z0-9]+`, packageName)
	}

	for _, lang := range h.langs {
		command := "wuffs-" + lang
		args := []string(nil)
		args = append(args, h.cmdArgs...)
		if lang == "c" {
			args = append(args, fmt.Sprintf("-ccompilers=%s", h.ccompilers))
		}
		args = append(args, filepath.Join(h.wuffsRoot, "test", lang, filepath.FromSlash(dirname)))
		cmd := exec.Command(command, args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err == nil {
			// No-op.
		} else if _, ok := err.(*exec.ExitError); ok {
			failed = true
		} else {
			return false, err
		}
	}
	return failed, nil
}
