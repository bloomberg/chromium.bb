# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::DebugDirectory;

use strict;
use base 'Win32::Exe::Base';
use constant FORMAT => (
    Flags	    => 'V',
    TimeStamp	    => 'V',
    VersionMajor    => 'v',
    VersionMinor    => 'v',
    Type	    => 'V',
    Size	    => 'V',
    VirtualAddress  => 'V',
    Offset	    => 'V',
);

1;
