package # hide from PAUSE
    DateTime::Conflicts;

use strict;
use warnings;

# this module was generated with Dist::Zilla::Plugin::Conflicts 0.19

use Dist::CheckConflicts
    -dist      => 'DateTime',
    -conflicts => {
        'DateTime::Format::Mail' => '0.402',
    },
    -also => [ qw(
        Carp
        DateTime::Locale
        DateTime::TimeZone
        Dist::CheckConflicts
        POSIX
        Params::ValidationCompiler
        Scalar::Util
        Specio
        Specio::Declare
        Specio::Exporter
        Specio::Library::Builtins
        Specio::Library::Numeric
        Specio::Library::String
        Try::Tiny
        XSLoader
        base
        integer
        namespace::autoclean
        overload
        parent
        strict
        warnings
        warnings::register
    ) ],

;

1;

# ABSTRACT: Provide information on conflicts for DateTime
# Dist::Zilla: -PodWeaver
