# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Dispatcher::Callback;
use vars '$VERSION';
$VERSION = '1.28';

use base 'Log::Report::Dispatcher';

use warnings;
use strict;

use Log::Report 'log-report';


sub init($)
{   my ($self, $args) = @_;
    $self->SUPER::init($args);

    $self->{callback} = $args->{callback}
        or error __x"dispatcher {name} needs a 'callback'", name => $self->name;

    $self;
}


sub callback() {shift->{callback}}


sub log($$$$)
{   my $self = shift;
    $self->{callback}->($self, @_);
}

1;
