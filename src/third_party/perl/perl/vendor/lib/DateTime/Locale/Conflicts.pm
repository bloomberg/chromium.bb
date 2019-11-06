package # hide from PAUSE
    DateTime::Locale::Conflicts;

use strict;
use warnings;

# this module was generated with Dist::Zilla::Plugin::Conflicts 0.19

use Dist::CheckConflicts
    -dist      => 'DateTime::Locale',
    -conflicts => {
        'DateTime::Format::Strptime' => '1.1000',
    },
    -also => [ qw(
        Carp
        Dist::CheckConflicts
        Exporter
        File::ShareDir
        List::Util
        Params::ValidationCompiler
        Specio::Declare
        Specio::Library::String
        namespace::autoclean
        strict
        warnings
    ) ],

;

1;

# ABSTRACT: Provide information on conflicts for DateTime::Locale
# Dist::Zilla: -PodWeaver
