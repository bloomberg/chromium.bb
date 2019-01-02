# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::DataDirectory;

use strict;
use base 'Win32::Exe::Base';
use constant FORMAT => (
    VirtualAddress  => 'V',
    Size	    => 'V',
);

1;
