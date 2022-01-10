// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// test-runner runs tint against a number of test shaders checking for expected behavior
package main

import (
	"context"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strings"
	"time"
	"unicode/utf8"

	"dawn.googlesource.com/tint/tools/src/fileutils"
	"dawn.googlesource.com/tint/tools/src/glob"
	"github.com/fatih/color"
	"github.com/sergi/go-diff/diffmatchpatch"
)

type outputFormat string

const (
	testTimeout = 30 * time.Second

	glsl   = outputFormat("glsl")
	hlsl   = outputFormat("hlsl")
	msl    = outputFormat("msl")
	spvasm = outputFormat("spvasm")
	wgsl   = outputFormat("wgsl")
)

func main() {
	if err := run(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

func showUsage() {
	fmt.Println(`
test-runner runs tint against a number of test shaders checking for expected behavior

usage:
  test-runner [flags...] <executable> [<directory>]

  <executable> the path to the tint executable
  <directory>  the root directory of the test files

optional flags:`)
	flag.PrintDefaults()
	fmt.Println(``)
	os.Exit(1)
}

func run() error {
	var formatList, filter, dxcPath, xcrunPath string
	var maxFilenameColumnWidth int
	numCPU := runtime.NumCPU()
	fxc, verbose, generateExpected, generateSkip := false, false, false, false
	flag.StringVar(&formatList, "format", "wgsl,spvasm,msl,hlsl", "comma separated list of formats to emit. Possible values are: all, wgsl, spvasm, msl, hlsl, glsl")
	flag.StringVar(&filter, "filter", "**.wgsl, **.spvasm, **.spv", "comma separated list of glob patterns for test files")
	flag.StringVar(&dxcPath, "dxc", "", "path to DXC executable for validating HLSL output")
	flag.StringVar(&xcrunPath, "xcrun", "", "path to xcrun executable for validating MSL output")
	flag.BoolVar(&fxc, "fxc", false, "validate with FXC instead of DXC")
	flag.BoolVar(&verbose, "verbose", false, "print all run tests, including rows that all pass")
	flag.BoolVar(&generateExpected, "generate-expected", false, "create or update all expected outputs")
	flag.BoolVar(&generateSkip, "generate-skip", false, "create or update all expected outputs that fail with SKIP")
	flag.IntVar(&numCPU, "j", numCPU, "maximum number of concurrent threads to run tests")
	flag.IntVar(&maxFilenameColumnWidth, "filename-column-width", 0, "maximum width of the filename column")
	flag.Usage = showUsage
	flag.Parse()

	args := flag.Args()
	if len(args) == 0 {
		showUsage()
	}

	// executable path is the first argument
	exe, args := args[0], args[1:]

	// (optional) target directory is the second argument
	dir := "."
	if len(args) > 0 {
		dir, args = args[0], args[1:]
	}

	// Check the executable can be found and actually is executable
	if !fileutils.IsExe(exe) {
		return fmt.Errorf("'%s' not found or is not executable", exe)
	}
	exe, err := filepath.Abs(exe)
	if err != nil {
		return err
	}

	// Allow using '/' in the filter on Windows
	filter = strings.ReplaceAll(filter, "/", string(filepath.Separator))

	// Split the --filter flag up by ',', trimming any whitespace at the start and end
	globIncludes := strings.Split(filter, ",")
	for i, s := range globIncludes {
		s = filepath.ToSlash(s) // Replace '\' with '/'
		globIncludes[i] = `"` + strings.TrimSpace(s) + `"`
	}

	// Glob the files to test
	files, err := glob.Scan(dir, glob.MustParseConfig(`{
		"paths": [
			{
				"include": [ `+strings.Join(globIncludes, ",")+` ]
			},
			{
				"exclude": [
					"**.expected.wgsl",
					"**.expected.spvasm",
					"**.expected.msl",
					"**.expected.hlsl"
				]
			}
		]
	}`))
	if err != nil {
		return fmt.Errorf("Failed to glob files: %w", err)
	}

	// Ensure the files are sorted (globbing should do this, but why not)
	sort.Strings(files)

	// Parse --format into a list of outputFormat
	formats := []outputFormat{}
	if formatList == "all" {
		formats = []outputFormat{wgsl, spvasm, msl, hlsl, glsl}
	} else {
		for _, f := range strings.Split(formatList, ",") {
			switch strings.TrimSpace(f) {
			case "wgsl":
				formats = append(formats, wgsl)
			case "spvasm":
				formats = append(formats, spvasm)
			case "msl":
				formats = append(formats, msl)
			case "hlsl":
				formats = append(formats, hlsl)
			case "glsl":
				formats = append(formats, glsl)
			default:
				return fmt.Errorf("unknown format '%s'", f)
			}
		}
	}

	defaultMSLExe := "xcrun"
	if runtime.GOOS == "windows" {
		defaultMSLExe = "metal.exe"
	}

	// If explicit verification compilers have been specified, check they exist.
	// Otherwise, look on PATH for them, but don't error if they cannot be found.
	for _, tool := range []struct {
		name string
		lang string
		path *string
	}{
		{"dxc", "hlsl", &dxcPath},
		{defaultMSLExe, "msl", &xcrunPath},
	} {
		if *tool.path == "" {
			p, err := exec.LookPath(tool.name)
			if err == nil && fileutils.IsExe(p) {
				*tool.path = p
			}
		} else if !fileutils.IsExe(*tool.path) {
			return fmt.Errorf("%v not found at '%v'", tool.name, *tool.path)
		}

		color.Set(color.FgCyan)
		fmt.Printf("%-4s", tool.lang)
		color.Unset()
		fmt.Printf(" validation ")
		if *tool.path != "" || (fxc && tool.lang == "hlsl") {
			color.Set(color.FgGreen)
			tool_path := *tool.path
			if fxc && tool.lang == "hlsl" {
				tool_path = "Tint will use FXC dll in PATH"
			}
			fmt.Printf("ENABLED (" + tool_path + ")")
		} else {
			color.Set(color.FgRed)
			fmt.Printf("DISABLED")
		}
		color.Unset()
		fmt.Println()
	}
	fmt.Println()

	// Build the list of results.
	// These hold the chans used to report the job results.
	results := make([]map[outputFormat]chan status, len(files))
	for i := range files {
		fileResults := map[outputFormat]chan status{}
		for _, format := range formats {
			fileResults[format] = make(chan status, 1)
		}
		results[i] = fileResults
	}

	pendingJobs := make(chan job, 256)

	// Spawn numCPU job runners...
	for cpu := 0; cpu < numCPU; cpu++ {
		go func() {
			for job := range pendingJobs {
				job.run(dir, exe, fxc, dxcPath, xcrunPath, generateExpected, generateSkip)
			}
		}()
	}

	// Issue the jobs...
	go func() {
		for i, file := range files { // For each test file...
			file := filepath.Join(dir, file)
			flags := parseFlags(file)
			for _, format := range formats { // For each output format...
				pendingJobs <- job{
					file:   file,
					flags:  flags,
					format: format,
					result: results[i][format],
				}
			}
		}
		close(pendingJobs)
	}()

	type failure struct {
		file   string
		format outputFormat
		err    error
	}

	type stats struct {
		numTests, numPass, numSkip, numFail int
		timeTaken                           time.Duration
	}

	// Statistics per output format
	statsByFmt := map[outputFormat]*stats{}
	for _, format := range formats {
		statsByFmt[format] = &stats{}
	}

	// Print the table of file x format and gather per-format stats
	failures := []failure{}
	filenameColumnWidth := maxStringLen(files)
	if maxFilenameColumnWidth > 0 {
		filenameColumnWidth = maxFilenameColumnWidth
	}

	red := color.New(color.FgRed)
	green := color.New(color.FgGreen)
	yellow := color.New(color.FgYellow)
	cyan := color.New(color.FgCyan)

	printFormatsHeader := func() {
		fmt.Printf(strings.Repeat(" ", filenameColumnWidth))
		fmt.Printf(" ┃ ")
		for _, format := range formats {
			cyan.Printf(alignCenter(format, formatWidth(format)))
			fmt.Printf(" │ ")
		}
		fmt.Println()
	}
	printHorizontalLine := func() {
		fmt.Printf(strings.Repeat("━", filenameColumnWidth))
		fmt.Printf("━╋━")
		for _, format := range formats {
			fmt.Printf(strings.Repeat("━", formatWidth(format)))
			fmt.Printf("━┿━")
		}
		fmt.Println()
	}

	fmt.Println()

	printFormatsHeader()
	printHorizontalLine()

	for i, file := range files {
		results := results[i]

		row := &strings.Builder{}
		rowAllPassed := true

		filenameLength := utf8.RuneCountInString(file)
		shortFile := file
		if filenameLength > filenameColumnWidth {
			shortFile = "..." + file[filenameLength-filenameColumnWidth+3:]
		}

		fmt.Fprintf(row, alignRight(shortFile, filenameColumnWidth))
		fmt.Fprintf(row, " ┃ ")
		for _, format := range formats {
			columnWidth := formatWidth(format)
			result := <-results[format]
			stats := statsByFmt[format]
			stats.numTests++
			stats.timeTaken += result.timeTaken
			if err := result.err; err != nil {
				failures = append(failures, failure{
					file: file, format: format, err: err,
				})
			}
			switch result.code {
			case pass:
				green.Fprintf(row, alignCenter("PASS", columnWidth))
				stats.numPass++
			case fail:
				red.Fprintf(row, alignCenter("FAIL", columnWidth))
				rowAllPassed = false
				stats.numFail++
			case skip:
				yellow.Fprintf(row, alignCenter("SKIP", columnWidth))
				rowAllPassed = false
				stats.numSkip++
			default:
				fmt.Fprintf(row, alignCenter(result.code, columnWidth))
				rowAllPassed = false
			}
			fmt.Fprintf(row, " │ ")
		}

		if verbose || !rowAllPassed {
			fmt.Fprintln(color.Output, row)
		}
	}

	printHorizontalLine()
	printFormatsHeader()
	printHorizontalLine()
	printStat := func(col *color.Color, name string, num func(*stats) int) {
		row := &strings.Builder{}
		anyNonZero := false
		for _, format := range formats {
			columnWidth := formatWidth(format)
			count := num(statsByFmt[format])
			if count > 0 {
				col.Fprintf(row, alignLeft(count, columnWidth))
				anyNonZero = true
			} else {
				fmt.Fprintf(row, alignLeft(count, columnWidth))
			}
			fmt.Fprintf(row, " │ ")
		}

		if !anyNonZero {
			return
		}
		col.Printf(alignRight(name, filenameColumnWidth))
		fmt.Printf(" ┃ ")
		fmt.Fprintln(color.Output, row)

		col.Printf(strings.Repeat(" ", filenameColumnWidth))
		fmt.Printf(" ┃ ")
		for _, format := range formats {
			columnWidth := formatWidth(format)
			stats := statsByFmt[format]
			count := num(stats)
			percent := percentage(count, stats.numTests)
			if count > 0 {
				col.Print(alignRight(percent, columnWidth))
			} else {
				fmt.Print(alignRight(percent, columnWidth))
			}
			fmt.Printf(" │ ")
		}
		fmt.Println()
	}
	printStat(green, "PASS", func(s *stats) int { return s.numPass })
	printStat(yellow, "SKIP", func(s *stats) int { return s.numSkip })
	printStat(red, "FAIL", func(s *stats) int { return s.numFail })

	cyan.Printf(alignRight("TIME", filenameColumnWidth))
	fmt.Printf(" ┃ ")
	for _, format := range formats {
		timeTaken := printDuration(statsByFmt[format].timeTaken)
		cyan.Printf(alignLeft(timeTaken, formatWidth(format)))
		fmt.Printf(" │ ")
	}
	fmt.Println()

	for _, f := range failures {
		color.Set(color.FgBlue)
		fmt.Printf("%s ", f.file)
		color.Set(color.FgCyan)
		fmt.Printf("%s ", f.format)
		color.Set(color.FgRed)
		fmt.Println("FAIL")
		color.Unset()
		fmt.Println(indent(f.err.Error(), 4))
	}
	if len(failures) > 0 {
		fmt.Println()
	}

	allStats := stats{}
	for _, format := range formats {
		stats := statsByFmt[format]
		allStats.numTests += stats.numTests
		allStats.numPass += stats.numPass
		allStats.numSkip += stats.numSkip
		allStats.numFail += stats.numFail
	}

	fmt.Printf("%d tests run", allStats.numTests)
	if allStats.numPass > 0 {
		fmt.Printf(", ")
		color.Set(color.FgGreen)
		fmt.Printf("%d tests pass", allStats.numPass)
		color.Unset()
	} else {
		fmt.Printf(", %d tests pass", allStats.numPass)
	}
	if allStats.numSkip > 0 {
		fmt.Printf(", ")
		color.Set(color.FgYellow)
		fmt.Printf("%d tests skipped", allStats.numSkip)
		color.Unset()
	} else {
		fmt.Printf(", %d tests skipped", allStats.numSkip)
	}
	if allStats.numFail > 0 {
		fmt.Printf(", ")
		color.Set(color.FgRed)
		fmt.Printf("%d tests failed", allStats.numFail)
		color.Unset()
	} else {
		fmt.Printf(", %d tests failed", allStats.numFail)
	}
	fmt.Println()
	fmt.Println()

	if allStats.numFail > 0 {
		os.Exit(1)
	}

	return nil
}

// Structures to hold the results of the tests
type statusCode string

const (
	fail statusCode = "FAIL"
	pass statusCode = "PASS"
	skip statusCode = "SKIP"
)

type status struct {
	code      statusCode
	err       error
	timeTaken time.Duration
}

type job struct {
	file   string
	flags  []string
	format outputFormat
	result chan status
}

func (j job) run(wd, exe string, fxc bool, dxcPath, xcrunPath string, generateExpected, generateSkip bool) {
	j.result <- func() status {
		// Is there an expected output?
		expected := loadExpectedFile(j.file, j.format)
		skipped := false
		if strings.HasPrefix(expected, "SKIP") { // Special SKIP token
			skipped = true
		}

		expected = strings.ReplaceAll(expected, "\r\n", "\n")

		file, err := filepath.Rel(wd, j.file)
		if err != nil {
			file = j.file
		}

		// Make relative paths use forward slash separators (on Windows) so that paths in tint
		// output match expected output that contain errors
		file = strings.ReplaceAll(file, `\`, `/`)

		args := []string{
			file,
			"--format", string(j.format),
		}

		// Can we validate?
		validate := false
		switch j.format {
		case wgsl:
			validate = true
		case spvasm, glsl:
			args = append(args, "--validate") // spirv-val and glslang are statically linked, always available
			validate = true
		case hlsl:
			if fxc {
				args = append(args, "--fxc")
				validate = true
			} else if dxcPath != "" {
				args = append(args, "--dxc", dxcPath)
				validate = true
			}
		case msl:
			if xcrunPath != "" {
				args = append(args, "--xcrun", xcrunPath)
				validate = true
			}
		}

		args = append(args, j.flags...)

		// Invoke the compiler...
		start := time.Now()
		ok, out := invoke(wd, exe, args...)
		timeTaken := time.Since(start)

		out = strings.ReplaceAll(out, "\r\n", "\n")
		matched := expected == "" || expected == out

		if ok && generateExpected && (validate || !skipped) {
			saveExpectedFile(j.file, j.format, out)
			matched = true
		}

		switch {
		case ok && matched:
			// Test passed
			return status{code: pass, timeTaken: timeTaken}

			//       --- Below this point the test has failed ---

		case skipped:
			if generateSkip {
				saveExpectedFile(j.file, j.format, "SKIP: FAILED\n\n"+out)
			}
			return status{code: skip, timeTaken: timeTaken}

		case !ok:
			// Compiler returned non-zero exit code
			if generateSkip {
				saveExpectedFile(j.file, j.format, "SKIP: FAILED\n\n"+out)
			}
			err := fmt.Errorf("%s", out)
			return status{code: fail, err: err, timeTaken: timeTaken}

		default:
			// Compiler returned zero exit code, or output was not as expected
			if generateSkip {
				saveExpectedFile(j.file, j.format, "SKIP: FAILED\n\n"+out)
			}

			// Expected output did not match
			dmp := diffmatchpatch.New()
			diff := dmp.DiffPrettyText(dmp.DiffMain(expected, out, true))
			err := fmt.Errorf(`Output was not as expected

--------------------------------------------------------------------------------
-- Expected:                                                                  --
--------------------------------------------------------------------------------
%s

--------------------------------------------------------------------------------
-- Got:                                                                       --
--------------------------------------------------------------------------------
%s

--------------------------------------------------------------------------------
-- Diff:                                                                      --
--------------------------------------------------------------------------------
%s`,
				expected, out, diff)
			return status{code: fail, err: err, timeTaken: timeTaken}
		}
	}()
}

// loadExpectedFile loads the expected output file for the test file at 'path'
// and the output format 'format'. If the file does not exist, or cannot be
// read, then an empty string is returned.
func loadExpectedFile(path string, format outputFormat) string {
	content, err := ioutil.ReadFile(expectedFilePath(path, format))
	if err != nil {
		return ""
	}
	return string(content)
}

// saveExpectedFile writes the expected output file for the test file at 'path'
// and the output format 'format', with the content 'content'.
func saveExpectedFile(path string, format outputFormat, content string) error {
	// Don't generate expected results for certain directories that contain
	// large corpora of tests for which the generated code is uninteresting.
	for _, exclude := range []string{"/test/unittest/", "/test/vk-gl-cts/"} {
		if strings.Contains(path, filepath.FromSlash(exclude)) {
			return nil
		}
	}
	return ioutil.WriteFile(expectedFilePath(path, format), []byte(content), 0666)
}

// expectedFilePath returns the expected output file path for the test file at
// 'path' and the output format 'format'.
func expectedFilePath(path string, format outputFormat) string {
	return path + ".expected." + string(format)
}

// indent returns the string 's' indented with 'n' whitespace characters
func indent(s string, n int) string {
	tab := strings.Repeat(" ", n)
	return tab + strings.ReplaceAll(s, "\n", "\n"+tab)
}

// alignLeft returns the string of 'val' padded so that it is aligned left in
// a column of the given width
func alignLeft(val interface{}, width int) string {
	s := fmt.Sprint(val)
	padding := width - utf8.RuneCountInString(s)
	if padding < 0 {
		return s
	}
	return s + strings.Repeat(" ", padding)
}

// alignCenter returns the string of 'val' padded so that it is centered in a
// column of the given width.
func alignCenter(val interface{}, width int) string {
	s := fmt.Sprint(val)
	padding := width - utf8.RuneCountInString(s)
	if padding < 0 {
		return s
	}
	return strings.Repeat(" ", padding/2) + s + strings.Repeat(" ", (padding+1)/2)
}

// alignRight returns the string of 'val' padded so that it is aligned right in
// a column of the given width
func alignRight(val interface{}, width int) string {
	s := fmt.Sprint(val)
	padding := width - utf8.RuneCountInString(s)
	if padding < 0 {
		return s
	}
	return strings.Repeat(" ", padding) + s
}

// maxStringLen returns the maximum number of runes found in all the strings in
// 'l'
func maxStringLen(l []string) int {
	max := 0
	for _, s := range l {
		if c := utf8.RuneCountInString(s); c > max {
			max = c
		}
	}
	return max
}

// formatWidth returns the width in runes for the outputFormat column 'b'
func formatWidth(b outputFormat) int {
	const min = 6
	c := utf8.RuneCountInString(string(b))
	if c < min {
		return min
	}
	return c
}

// percentage returns the percentage of n out of total as a string
func percentage(n, total int) string {
	if total == 0 {
		return "-"
	}
	f := float64(n) / float64(total)
	return fmt.Sprintf("%.1f%c", f*100.0, '%')
}

// invoke runs the executable 'exe' with the provided arguments.
func invoke(wd, exe string, args ...string) (ok bool, output string) {
	ctx, cancel := context.WithTimeout(context.Background(), testTimeout)
	defer cancel()

	cmd := exec.CommandContext(ctx, exe, args...)
	cmd.Dir = wd
	out, err := cmd.CombinedOutput()
	str := string(out)
	if err != nil {
		if ctx.Err() == context.DeadlineExceeded {
			return false, fmt.Sprintf("test timed out after %v", testTimeout)
		}
		if str != "" {
			return false, str
		}
		return false, err.Error()
	}
	return true, str
}

var reFlags = regexp.MustCompile(` *\/\/ *flags:(.*)\n`)

// parseFlags looks for a `// flags:` header at the start of the file with the
// given path, returning each of the space delimited tokens that follow for the
// line
func parseFlags(path string) []string {
	content, err := ioutil.ReadFile(path)
	if err != nil {
		return nil
	}
	header := strings.SplitN(string(content), "\n", 1)[0]
	m := reFlags.FindStringSubmatch(header)
	if len(m) != 2 {
		return nil
	}
	return strings.Split(m[1], " ")
}

func printDuration(d time.Duration) string {
	sec := int(d.Seconds())
	min := int(sec) / 60
	hour := min / 60
	min -= hour * 60
	sec -= min * 60
	sb := &strings.Builder{}
	if hour > 0 {
		fmt.Fprintf(sb, "%dh", hour)
	}
	if min > 0 {
		fmt.Fprintf(sb, "%dm", min)
	}
	if sec > 0 {
		fmt.Fprintf(sb, "%ds", sec)
	}
	return sb.String()
}
