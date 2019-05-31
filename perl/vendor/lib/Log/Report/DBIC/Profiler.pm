# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::DBIC::Profiler;
use vars '$VERSION';
$VERSION = '1.28';

use base 'DBIx::Class::Storage::Statistics';

use strict;
use warnings;

use Log::Report  'log-report', import => 'trace';
use Time::HiRes  qw(time);


my $start;

sub print($) { trace $_[1] }

sub query_start(@)
{   my $self = shift;
    $self->SUPER::query_start(@_);
    $start   = time;
}

sub query_end(@)
{   my $self = shift;
    $self->SUPER::query_end(@_);
    trace sprintf "execution took %0.4f seconds elapse", time-$start;
}

1;

