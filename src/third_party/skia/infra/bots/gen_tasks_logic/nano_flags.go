// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package gen_tasks_logic

import (
	"fmt"
	"sort"

	"go.skia.org/infra/task_scheduler/go/specs"
)

// nanobenchFlags generates flags to Nanobench based on the given task properties.
func (b *taskBuilder) nanobenchFlags(doUpload bool) {
	args := []string{
		"nanobench",
		"--pre_log",
	}

	if b.gpu() {
		args = append(args, "--gpuStatsDump", "true")
	}

	args = append(args, "--scales", "1.0", "1.1")

	configs := []string{}
	if b.cpu() {
		args = append(args, "--nogpu")
		configs = append(configs, "8888", "nonrendering")

		if b.extraConfig("BonusConfigs") {
			configs = []string{
				"f16",
				"srgb",
				"esrgb",
				"narrow",
				"enarrow",
			}
		}

		if b.model("Nexus7") {
			args = append(args, "--purgeBetweenBenches") // Debugging skia:8929
		}

	} else if b.gpu() {
		args = append(args, "--nocpu")

		glPrefix := "gl"
		sampleCount := 8
		if b.os("Android", "iOS") {
			sampleCount = 4
			// The NVIDIA_Shield has a regular OpenGL implementation. We bench that
			// instead of ES.
			if !b.model("NVIDIA_Shield") {
				glPrefix = "gles"
			}
			// iOS crashes with MSAA (skia:6399)
			// Nexus7 (Tegra3) does not support MSAA.
			// MSAA is disabled on Pixel3a (https://b.corp.google.com/issues/143074513).
			if b.os("iOS") || b.model("Nexus7", "Pixel3a") {
				sampleCount = 0
			}
		} else if b.matchGpu("Intel") {
			// MSAA doesn't work well on Intel GPUs chromium:527565, chromium:983926
			sampleCount = 0
		} else if b.os("ChromeOS") {
			glPrefix = "gles"
		}

		configs = append(configs, glPrefix, glPrefix+"srgb")
		if sampleCount > 0 {
			configs = append(configs, fmt.Sprintf("%smsaa%d", glPrefix, sampleCount))
		}

		// We want to test both the OpenGL config and the GLES config on Linux Intel:
		// GL is used by Chrome, GLES is used by ChromeOS.
		if b.matchGpu("Intel") && b.isLinux() {
			configs = append(configs, "gles", "glessrgb")
		}

		if b.extraConfig("CommandBuffer") {
			configs = []string{"commandbuffer"}
		}

		if b.extraConfig("Vulkan") {
			configs = []string{"vk"}
			if b.os("Android") {
				// skbug.com/9274
				if !b.model("Pixel2XL") {
					configs = append(configs, "vkmsaa4")
				}
			} else {
				// MSAA doesn't work well on Intel GPUs chromium:527565, chromium:983926, skia:9023
				if !b.matchGpu("Intel") {
					configs = append(configs, "vkmsaa8")
				}
			}
		}
		if b.extraConfig("Metal") {
			configs = []string{"mtl"}
			if b.os("iOS") {
				configs = append(configs, "mtlmsaa4")
			} else {
				configs = append(configs, "mtlmsaa8")
			}
		}

		if b.extraConfig("ANGLE") {
			// Test only ANGLE configs.
			configs = []string{"angle_d3d11_es2"}
			if sampleCount > 0 {
				configs = append(configs, fmt.Sprintf("angle_d3d11_es2_msaa%d", sampleCount))
			}
			if b.gpu("QuadroP400") {
				// See skia:7823 and chromium:693090.
				configs = append(configs, "angle_gl_es2")
				if sampleCount > 0 {
					configs = append(configs, fmt.Sprintf("angle_gl_es2_msaa%d", sampleCount))
				}
			}
		}
		if b.os("ChromeOS") {
			// Just run GLES for now - maybe add gles_msaa4 in the future
			configs = []string{"gles"}
		}
	}

	args = append(args, "--config")
	args = append(args, configs...)

	// By default, we test with GPU threading enabled, unless specifically
	// disabled.
	if b.extraConfig("NoGPUThreads") {
		args = append(args, "--gpuThreads", "0")
	}

	if b.debug() || b.extraConfig("ASAN") || b.extraConfig("Valgrind") {
		args = append(args, "--loops", "1")
		args = append(args, "--samples", "1")
		// Ensure that the bot framework does not think we have timed out.
		args = append(args, "--keepAlive", "true")
	}

	// skia:9036
	if b.model("NVIDIA_Shield") {
		args = append(args, "--dontReduceOpsTaskSplitting")
	}

	// Some people don't like verbose output.
	verbose := false

	match := []string{}
	if b.os("Android") {
		// Segfaults when run as GPU bench. Very large texture?
		match = append(match, "~blurroundrect")
		match = append(match, "~patch_grid") // skia:2847
		match = append(match, "~desk_carsvg")
	}
	if b.matchModel("Nexus5") {
		match = append(match, "~keymobi_shop_mobileweb_ebay_com.skp") // skia:5178
	}
	if b.os("iOS") {
		match = append(match, "~blurroundrect")
		match = append(match, "~patch_grid") // skia:2847
		match = append(match, "~desk_carsvg")
		match = append(match, "~keymobi")
		match = append(match, "~path_hairline")
		match = append(match, "~GLInstancedArraysBench") // skia:4714
	}
	if b.os("iOS") && b.extraConfig("Metal") {
		// skia:9799
		match = append(match, "~compositing_images_tile_size")
	}
	if b.matchGpu("Intel") && b.isLinux() && !b.extraConfig("Vulkan") {
		// TODO(dogben): Track down what's causing bots to die.
		verbose = true
	}
	if b.gpu("IntelHD405") && b.isLinux() && b.extraConfig("Vulkan") {
		// skia:7322
		match = append(match, "~desk_carsvg.skp_1")
		match = append(match, "~desk_googlehome.skp")
		match = append(match, "~desk_tiger8svg.skp_1")
		match = append(match, "~desk_wowwiki.skp")
		match = append(match, "~desk_ynevsvg.skp_1.1")
		match = append(match, "~desk_nostroke_tiger8svg.skp")
		match = append(match, "~keymobi_booking_com.skp_1")
		match = append(match, "~keymobi_booking_com.skp_1_mpd")
		match = append(match, "~keymobi_cnn_article.skp_1")
		match = append(match, "~keymobi_cnn_article.skp_1_mpd")
		match = append(match, "~keymobi_forecast_io.skp_1")
		match = append(match, "~keymobi_forecast_io.skp_1_mpd")
		match = append(match, "~keymobi_sfgate.skp_1")
		match = append(match, "~keymobi_techcrunch_com.skp_1.1")
		match = append(match, "~keymobi_techcrunch.skp_1.1")
		match = append(match, "~keymobi_techcrunch.skp_1.1_mpd")
		match = append(match, "~svgparse_Seal_of_California.svg_1.1")
		match = append(match, "~svgparse_NewYork-StateSeal.svg_1.1")
		match = append(match, "~svgparse_Vermont_state_seal.svg_1")
		match = append(match, "~tabl_gamedeksiam.skp_1.1")
		match = append(match, "~tabl_pravda.skp_1")
		match = append(match, "~top25desk_ebay_com.skp_1.1")
		match = append(match, "~top25desk_ebay.skp_1.1")
		match = append(match, "~top25desk_ebay.skp_1.1_mpd")
	}
	if b.extraConfig("Vulkan") && b.gpu("GTX660") {
		// skia:8523 skia:9271
		match = append(match, "~compositing_images")
	}
	if b.model("MacBook10.1") && b.extraConfig("CommandBuffer") {
		match = append(match, "~^desk_micrographygirlsvg.skp_1.1$")
	}
	if b.extraConfig("ASAN") && b.cpu() {
		// floor2int_undef benches undefined behavior, so ASAN correctly complains.
		match = append(match, "~^floor2int_undef$")
	}
	if b.model("AcerChromebook13_CB5_311") && b.gpu() {
		// skia:7551
		match = append(match, "~^shapes_rrect_inner_rrect_50_500x500$")
	}
	if b.model("Pixel3a") {
		// skia:9413
		match = append(match, "~^path_text$")
		match = append(match, "~^path_text_clipped_uncached$")
	}
	if b.model("Pixel3") && b.extraConfig("Vulkan") {
		// skia:9972
		match = append(match, "~^path_text_clipped_uncached$")
	}

	// We do not need or want to benchmark the decodes of incomplete images.
	// In fact, in nanobench we assert that the full image decode succeeds.
	match = append(match, "~inc0.gif")
	match = append(match, "~inc1.gif")
	match = append(match, "~incInterlaced.gif")
	match = append(match, "~inc0.jpg")
	match = append(match, "~incGray.jpg")
	match = append(match, "~inc0.wbmp")
	match = append(match, "~inc1.wbmp")
	match = append(match, "~inc0.webp")
	match = append(match, "~inc1.webp")
	match = append(match, "~inc0.ico")
	match = append(match, "~inc1.ico")
	match = append(match, "~inc0.png")
	match = append(match, "~inc1.png")
	match = append(match, "~inc2.png")
	match = append(match, "~inc12.png")
	match = append(match, "~inc13.png")
	match = append(match, "~inc14.png")
	match = append(match, "~inc0.webp")
	match = append(match, "~inc1.webp")

	if len(match) > 0 {
		args = append(args, "--match")
		args = append(args, match...)
	}

	if verbose {
		args = append(args, "--verbose")
	}

	// Add properties indicating which assets the task should use.
	b.recipeProp("do_upload", fmt.Sprintf("%t", doUpload))
	if !b.gpu() {
		b.asset("skimage")
		b.recipeProp("images", "true")
	}
	b.recipeProp("resources", "true")
	if !b.os("iOS") {
		b.asset("skp")
		b.recipeProp("skps", "true")
	}
	if !b.extraConfig("Valgrind") {
		b.asset("svg")
		b.recipeProp("svgs", "true")
	}
	if b.cpu() && b.os("Android") {
		// TODO(borenet): Where do these come from?
		b.recipeProp("textTraces", "true")
	}

	// These properties are plumbed through nanobench and into Perf results.
	nanoProps := map[string]string{
		"gitHash":          specs.PLACEHOLDER_REVISION,
		"issue":            specs.PLACEHOLDER_ISSUE,
		"patchset":         specs.PLACEHOLDER_PATCHSET,
		"patch_storage":    specs.PLACEHOLDER_PATCH_STORAGE,
		"swarming_bot_id":  "${SWARMING_BOT_ID}",
		"swarming_task_id": "${SWARMING_TASK_ID}",
	}

	if doUpload {
		keysBlacklist := map[string]bool{
			"configuration": true,
			"role":          true,
			"test_filter":   true,
		}
		keys := make([]string, 0, len(b.parts))
		for k := range b.parts {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		args = append(args, "--key")
		for _, k := range keys {
			if !keysBlacklist[k] {
				args = append(args, k, b.parts[k])
			}
		}
	}

	// Finalize the nanobench flags and properties.
	b.recipeProp("nanobench_flags", marshalJson(args))
	b.recipeProp("nanobench_properties", marshalJson(nanoProps))
}
