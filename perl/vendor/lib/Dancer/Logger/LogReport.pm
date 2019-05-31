# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Dancer::Logger::LogReport;
use vars '$VERSION';
$VERSION = '1.28';

use base 'Dancer::Logger::Abstract', 'Exporter';

use strict;
use warnings;

use Scalar::Util            qw/blessed/;
use Log::Report             'log-report', import => 'report';
use Log::Report::Dispatcher ();

our $AUTHORITY = 'cpan:MARKOV';

our @EXPORT    = qw/
    trace
    assert
    notice
    alert
    panic
    /;

my %level_dancer2lr =
  ( core  => 'TRACE'
  , debug => 'TRACE'
  );


# Add some extra 'levels'
sub trace   { goto &Dancer::Logger::debug  }
sub assert  { goto &Dancer::Logger::assert }
sub notice  { goto &Dancer::Logger::notice }
sub panic   { goto &Dancer::Logger::panic  }
sub alert   { goto &Dancer::Logger::alert  }

sub Dancer::Logger::assert
{ my $l = logger(); $l && $l->_log(assert => _serialize(@_)) }

sub Dancer::Logger::notice
{ my $l = logger(); $l && $l->_log(notice => _serialize(@_)) }

sub Dancer::Logger::alert
{ my $l = logger(); $l && $l->_log(alert  => _serialize(@_)) }

sub Dancer::Logger::panic
{ my $l = logger(); $l && $l->_log(panic  => _serialize(@_)) }
 
#sub init(@)
#{   my $self = shift;
#    $self->SUPER::init(@_);
#}
 
sub _log {
    my ($self, $level, $params) = @_;

    # all dancer levels are the same as L::R levels, except:
    my $msg;
    if(blessed $params && $params->isa('Log::Report::Message'))
    {   $msg = $params;
    }
    else
    {   $msg = $self->format_message($level => $params);
        $msg =~ s/\n+$//;
    }

    # The levels are nearly the same.
    my $reason = $level_dancer2lr{$level} // uc $level;

    # Gladly, report() does not get confused between Dancer's use of
    # Try::Tiny and Log::Report's try() which starts a new dispatcher.
    report {is_fatal => 0}, $reason => $msg;

    undef;
}
 
1;
