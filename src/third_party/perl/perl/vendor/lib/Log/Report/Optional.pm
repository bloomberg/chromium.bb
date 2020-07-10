# Copyrights 2013-2018 by [Mark Overmeer <mark@overmeer.net>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report-Optional. Meta-POD processed
# with OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Optional;
use vars '$VERSION';
$VERSION = '1.06';

use base 'Exporter';

use warnings;
use strict;


my ($supported, @used_by);

BEGIN {
   if($INC{'Log/Report.pm'})
   {   $supported  = 'Log::Report';
       my $version = $Log::Report::VERSION;
       die "Log::Report too old for ::Optional, need at least 1.00"
           if $version && $version le '1.00';
   }
   else
   {   require Log::Report::Minimal;
       $supported = 'Log::Report::Minimal';
   }
}

sub import(@)
{   my $class = shift;
    push @used_by, (caller)[0];
    $supported->import('+1', @_);
}


sub usedBy() { @used_by }

1;
