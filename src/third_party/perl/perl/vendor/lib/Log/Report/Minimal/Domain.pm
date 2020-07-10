# Copyrights 2013-2018 by [Mark Overmeer <mark@overmeer.net>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report-Optional. Meta-POD processed
# with OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report::Minimal::Domain;
use vars '$VERSION';
$VERSION = '1.06';


use warnings;
use strict;

use String::Print        'oo';


sub new(@)  { my $class = shift; (bless {}, $class)->init({@_}) }
sub init($)
{   my ($self, $args) = @_;
    $self->{LRMD_name} = $args->{name} or Log::Report::panic();
    $self;
}

#----------------


sub name() {shift->{LRMD_name}}
sub isConfigured() {shift->{LRMD_where}}


sub configure(%)
{   my ($self, %args) = @_;

    my $here = $args{where} || [caller];
    if(my $s = $self->{LRMD_where})
    {   my $domain = $self->name;
        die "only one package can contain configuration; for $domain already in $s->[0] in file $s->[1] line $s->[2].  Now also found at $here->[1] line $here->[2]\n";
    }
    my $where = $self->{LRMD_where} = $here;

    # documented in the super-class, the more useful man-page
    my $format = $args{formatter} || 'PRINTI';
    $format    = {} if $format eq 'PRINTI';

    if(ref $format eq 'HASH')
    {   my $class  = delete $format->{class}  || 'String::Print';
        my $method = delete $format->{method} || 'sprinti';
		my $sp     = $class->new(%$format);
        $self->{LRMD_format} = sub { $sp->$method(@_) };
    }
    elsif(ref $format eq 'CODE')
    {   $self->{LRMD_format} = $format;
    }
    else
    {   error __x"illegal formatter `{name}' at {fn} line {line}"
          , name => $format, fn => $where->[1], line => $where->[2];
    }

    $self;
}

#-------------------

sub interpolate(@)
{   my ($self, $msgid, $args) = @_;
    $args->{_expand} or return $msgid;
    my $f = $self->{LRMD_format} || $self->configure->{LRMD_format};
    $f->($msgid, $args);
}

1;
