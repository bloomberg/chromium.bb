# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Dispatcher::Perl;
use vars '$VERSION';
$VERSION = '1.28';

use base 'Log::Report::Dispatcher';

use warnings;
use strict;

use Log::Report 'log-report';
use IO::File;

my $singleton = 0;   # can be only one (per thread)


sub log($$$$)
{   my ($self, $opts, $reason, $message, $domain) = @_;
    print STDERR $self->translate($opts, $reason, $message);
}

1;
