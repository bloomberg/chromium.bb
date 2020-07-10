# Chromite Signer

[TOC]

## Objective

This doc tries to give an overview of how the signer interacts with its world.

## Background

It is assumed that you have read
[go/cros-domain-model](https://goto.corp.google.com/cros-domain-model) and are
familiar with the contents.

## Directory Overview

You can use [Code Search](https://cs.corp.google.com/) to lookup things in
Chromite or ChromeOS in general. You can add a ChromeOS filter to only show
files from CrOS repositories by going to
[CS Settings](https://cs.corp.google.com/settings/) and adding a new Saved
query: “`package:^chromeos`” named “chromeos”.

### `signing/lib`

Code here is expected to be imported whenever needed throughout
chromite.signing.

### `signing/signer_instructions`

See that README.md.
