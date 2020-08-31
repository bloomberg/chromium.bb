# Copyright 2004 by Audrey Tang <cpan@audreyt.org>

package Win32::Exe::DebugTable;

use strict;
use base 'Win32::Exe::Base';
use constant FORMAT => (
    'DebugDirectory'	=> [ 'a28', '*', 1 ],
);

1;
