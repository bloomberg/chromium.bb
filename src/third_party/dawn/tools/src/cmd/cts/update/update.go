// Copyright 2022 The Dawn Authors
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

package update

import (
	"context"
	"flag"
	"fmt"

	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
	"go.chromium.org/luci/auth/client/authcli"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
		results      common.ResultSource
		expectations string
		auth         authcli.Flags
	}
}

func (cmd) Name() string {
	return "update"
}

func (cmd) Desc() string {
	return "updates a CTS expectations file"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	defaultExpectations := common.DefaultExpectationsPath()
	c.flags.results.RegisterFlags(cfg)
	c.flags.auth.Register(flag.CommandLine, common.DefaultAuthOptions())
	flag.StringVar(&c.flags.expectations, "expectations", defaultExpectations, "path to CTS expectations file to update")
	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	// Validate command line arguments
	auth, err := c.flags.auth.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	// Fetch the results
	results, err := c.flags.results.GetResults(ctx, cfg, auth)
	if err != nil {
		return err
	}

	// Load the expectations file
	ex, err := expectations.Load(c.flags.expectations)
	if err != nil {
		return err
	}

	// Update the expectations file with the results
	msgs, err := ex.Update(results)
	if err != nil {
		return err
	}

	// Print any diagnostics
	for _, msg := range msgs {
		fmt.Printf("%v:%v %v\n", c.flags.expectations, msg.Line, msg.Message)
	}

	// Save the updated expectations file
	return ex.Save(c.flags.expectations)
}
