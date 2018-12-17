// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/manifest.h"

namespace service_manager {

Manifest::Manifest() = default;
Manifest::Manifest(const Manifest&) = default;
Manifest::Manifest(Manifest&&) = default;
Manifest::~Manifest() = default;
Manifest& Manifest::operator=(const Manifest&) = default;
Manifest& Manifest::operator=(Manifest&&) = default;

Manifest::Options::Options() = default;
Manifest::Options::Options(const Options&) = default;
Manifest::Options::Options(Options&&) = default;
Manifest::Options::~Options() = default;
Manifest::Options& Manifest::Options::operator=(Options&&) = default;
Manifest::Options& Manifest::Options::operator=(const Options&) = default;

Manifest::ExposedCapability::ExposedCapability() = default;
Manifest::ExposedCapability::ExposedCapability(const ExposedCapability&) =
    default;
Manifest::ExposedCapability::ExposedCapability(ExposedCapability&&) = default;
Manifest::ExposedCapability::ExposedCapability(
    const char* capability_name,
    std::set<const char*> interface_names)
    : capability_name(capability_name),
      interface_names(std::move(interface_names)) {}
Manifest::ExposedCapability::~ExposedCapability() = default;
Manifest::ExposedCapability& Manifest::ExposedCapability::operator=(
    const ExposedCapability&) = default;
Manifest::ExposedCapability& Manifest::ExposedCapability::operator=(
    ExposedCapability&&) = default;

Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability() =
    default;
Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability(
    ExposedInterfaceFilterCapability&&) = default;
Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability(
    const ExposedInterfaceFilterCapability&) = default;
Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability(
    const char* filter_name,
    const char* capability_name,
    std::set<const char*> interface_names)
    : filter_name(filter_name),
      capability_name(capability_name),
      interface_names(std::move(interface_names)) {}
Manifest::ExposedInterfaceFilterCapability::
    ~ExposedInterfaceFilterCapability() = default;
Manifest::ExposedInterfaceFilterCapability&
Manifest::ExposedInterfaceFilterCapability::operator=(
    const ExposedInterfaceFilterCapability&) = default;
Manifest::ExposedInterfaceFilterCapability&
Manifest::ExposedInterfaceFilterCapability::operator=(
    ExposedInterfaceFilterCapability&&) = default;

}  // namespace service_manager
