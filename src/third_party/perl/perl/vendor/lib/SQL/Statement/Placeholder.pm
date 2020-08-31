package SQL::Statement::Placeholder;

######################################################################
#
# This module is copyright (c), 2009-2017 by Jens Rehsack.
# All rights reserved.
#
# It may be freely distributed under the same terms as Perl itself.
# See below for help and copyright information (search for SYNOPSIS).
#
######################################################################

use strict;
use warnings FATAL => "all";

use vars qw(@ISA);
use Carp ();

use SQL::Statement::Term ();

our $VERSION = '1.412';

@ISA = qw(SQL::Statement::Term);

=pod

=head1 NAME

SQL::Statement::Placeholder - implements getting the next placeholder value

=head1 SYNOPSIS

  # create an placeholder term with an SQL::Statement object as owner
  # and the $argnum of the placeholder.
  my $term = SQL::Statement::Placeholder->new( $owner, $argnum );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Placeholder implements getting the next placeholder value.
Accessing a specific placeholder is currently unimplemented and not tested.

=head1 INHERITANCE

  SQL::Statement::Placeholder
  ISA SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates a new C<SQL::Statement::Placeholder> instance.

=head2 value

Returns the value of the next placeholder.

=cut

sub new
{
    my ( $class, $owner, $argnum ) = @_;

    my $self = $class->SUPER::new($owner);
    $self->{ARGNUM} = $argnum;

    return $self;
}

sub value($)
{

    # from S::S->get_row_value():
    #        my $val = (
    #                         $self->{join}
    #                      or !$eval
    #                      or ref($eval) =~ /Statement$/
    #                  ) ? $self->params($arg_num) : $eval->param($arg_num);

    # let's see where us will lead taking from params every time
    # XXX later: return $_[0]->{OWNER}->{params}->[$_[0]->{ARGNUM}];
    return $_[0]->{OWNER}->{params}->[ $_[0]->{OWNER}->{argnum}++ ];
}

=head1 AUTHOR AND COPYRIGHT

Copyright (c) 2009-2017 by Jens Rehsack: rehsackATcpan.org

All rights reserved.

You may distribute this module under the terms of either the GNU
General Public License or the Artistic License, as specified in
the Perl README file.

=cut

1;
