# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Dispatcher::LogDispatch;
use vars '$VERSION';
$VERSION = '1.28';

use base 'Log::Report::Dispatcher';

use warnings;
use strict;

use Log::Report 'log-report', syntax => 'SHORT';
use Log::Report::Util  qw/@reasons expand_reasons/;

use Log::Dispatch 2.00;

my %default_reasonToLevel =
 ( TRACE   => 'debug'
 , ASSERT  => 'debug'
 , INFO    => 'info'
 , NOTICE  => 'notice'
 , WARNING => 'warning'
 , MISTAKE => 'warning'
 , ERROR   => 'error'
 , FAULT   => 'error'
 , ALERT   => 'alert'
 , FAILURE => 'emergency'
 , PANIC   => 'critical'
 );

@reasons != keys %default_reasonToLevel
    and panic __"Not all reasons have a default translation";


sub init($)
{   my ($self, $args) = @_;
    $self->SUPER::init($args);

    $args->{name}        = $self->name;
    $args->{min_level} ||= 'debug';

    $self->{level}  = { %default_reasonToLevel };
    if(my $to_level = delete $args->{to_level})
    {   my @to = @$to_level;
        while(@to)
        {   my ($reasons, $level) = splice @to, 0, 2;
            my @reasons = expand_reasons $reasons;

            Log::Dispatch->level_is_valid($level)
                or error __x"Log::Dispatch level '{level}' not understood"
                     , level => $level;

            $self->{level}{$_} = $level for @reasons;
        }
    }

    $self->{backend} = $self->type->new(%$args);
    $self;
}

sub close()
{   my $self = shift;
    $self->SUPER::close or return;
    delete $self->{backend};
    $self;
}


sub backend() {shift->{backend}}


sub log($$$$$)
{   my $self  = shift;
    my $text  = $self->translate(@_) or return;
    my $level = $self->reasonToLevel($_[1]);

    $self->backend->log(level => $level, message => $text);
    $self;
}


sub reasonToLevel($) { $_[0]->{level}{$_[1]} }

1;
