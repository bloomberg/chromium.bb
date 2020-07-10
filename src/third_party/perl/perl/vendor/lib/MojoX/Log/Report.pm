# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package MojoX::Log::Report;
use vars '$VERSION';
$VERSION = '1.28';

use Mojo::Base 'Mojo::Log';  # implies use strict etc

use Log::Report 'log-report', import => 'report';


sub new(@) {
    my $class = shift;
    my $self  = $class->SUPER::new(@_);

    # issue with Mojo, where the base-class registers a function --not
    # a method-- to handle the message.
    $self->unsubscribe('message');    # clean all listeners
    $self->on(message => '_message'); # call it OO
    $self;
}

my %level2reason = qw/
 debug  TRACE
 info   INFO
 warn   WARNING
 error  ERROR
 fatal  ALERT
/;

sub _message($$@)
{   my ($self, $level) = (shift, shift);
 
    report +{is_fatal => 0}    # do not die on errors
      , $level2reason{$level}, join('', @_);
}

1;
