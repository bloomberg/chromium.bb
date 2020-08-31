package SQL::Statement::Operation;

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

SQL::Statement::Operation - base class for all operation terms

=head1 SYNOPSIS

  # create an operation with an SQL::Statement object as owner, specifying
  # the operation name (for error purposes), the left and the right
  # operand
  my $term = SQL::Statement::Operation->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation is an abstract base class providing the interface
for all operation terms.

=head1 INHERITANCE

  SQL::Statement::Operation
  ISA SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates new operation term.

=head2 value

Return the result of the operation of the term by calling L<operate>

=head2 operate

I<Abstract> method which will do the operation of the term. Must be
overridden by derived classes.

=head2 op

Returns the name of the executed operation.

=head2 left

Returns the left operand (if any).

=head2 right

Returns the right operand (if any).

=head2 DESTROY

Destroys the term and undefines the weak reference to the owner as well
as the stored operation, the left and the right operand.

=cut

sub new
{
    my ( $class, $owner, $operation, $leftTerm, $rightTerm ) = @_;

    my $self = $class->SUPER::new($owner);
    $self->{OP}    = $operation;
    $self->{LEFT}  = $leftTerm;
    $self->{RIGHT} = $rightTerm;

    return $self;
}

sub op    { return $_[0]->{OP}; }
sub left  { return $_[0]->{LEFT}; }
sub right { return $_[0]->{RIGHT}; }

sub operate($)
{
    Carp::confess( sprintf( q{pure virtual function 'operate' called on %s for %s}, ref( $_[0] ) || __PACKAGE__, $_[0]->{OP} ) );
}

sub DESTROY
{
    my $self = $_[0];

    undef $self->{OP};
    undef $self->{LEFT};
    undef $self->{RIGHT};

    $self->SUPER::DESTROY();
}

sub value($) { return $_[0]->operate( $_[1] ); }

package SQL::Statement::Operation::Neg;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation);

=pod

=head1 NAME

SQL::Statement::Operation::Neg - negate operation

=head1 SYNOPSIS

  # create an <not> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and B<no> right operand
  my $term = SQL::Statement::Neg->new( $owner, $op, $left, undef );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Neg

=head1 INHERITANCE

  SQL::Statement::Operation::Neg
  ISA SQL::Statement::Operation
    ISA SQL::Statement::Term

=head1 METHODS

=head2 operate

Return the logical negated value of the left operand.

=cut

sub operate($)
{
    return !$_[0]->{LEFT}->value( $_[1] );
}

package SQL::Statement::Operation::And;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation);

=pod

=head1 NAME

SQL::Statement::Operation::And - and operation

=head1 SYNOPSIS

  # create an C<and> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::And->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::And implements the logical C<and> operation
between two terms.

=head1 INHERITANCE

  SQL::Statement::Operation::And
  ISA SQL::Statement::Operation
    ISA SQL::Statement::Term

=head1 METHODS

=head2 operate

Return the result of the logical C<and> operation for the L<value>s of the
left and right operand.

=cut

sub operate($)
{
    my $left  = $_[0]->{LEFT}->value( $_[1] );
    my $right = $_[0]->{RIGHT}->value( $_[1] );

    return $left && $right;
}

package SQL::Statement::Operation::Or;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation);

=pod

=head1 NAME

SQL::Statement::Operation::Or - or operation

=head1 SYNOPSIS

  # create an C<or> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Or->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Or implements the logical C<or> operation
between two terms.

=head1 INHERITANCE

  SQL::Statement::Operation::Or
  ISA SQL::Statement::Operation
    ISA SQL::Statement::Term

=head1 METHODS

=head2 operate

Return the result of the logical C<or> operation for the L<value>s of the
left and right operand.

=cut

sub operate($)
{
    my $left  = $_[0]->{LEFT}->value( $_[1] );
    my $right = $_[0]->{RIGHT}->value( $_[1] );

    return $left || $right;
}

package SQL::Statement::Operation::Is;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation);

=pod

=head1 NAME

SQL::Statement::Operation::Is - is operation

=head1 SYNOPSIS

  # create an C<is> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Is->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Is supports: C<IS NULL>, C<IS TRUE> and C<IS FALSE>.
The right operand is always evaluated in boolean context in case of C<IS TRUE>
and C<IS FALSE>. C<IS NULL> returns I<true> even if the left term is an empty
string (C<''>).

=head1 INHERITANCE

  SQL::Statement::Operation::Is
  ISA SQL::Statement::Operation
    ISA SQL::Statement::Term

=head1 METHODS

=head2 operate

Returns true when the left term is null, true or false - based on the
requested right value.

=cut

sub operate($)
{
    my $self  = $_[0];
    my $left  = $self->{LEFT}->value( $_[1] );
    my $right = $self->{RIGHT}->value( $_[1] );
    my $expr;

    if ( defined($right) )
    {
        $expr = defined($left) ? $left && $right : 0;    # is true / is false
    }
    else
    {
        $expr = !defined($left) || ( $left eq '' );      # FIXME I don't like that '' IS NULL
    }

    return $expr;
}

package SQL::Statement::Operation::ANSI::Is;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation);

=pod

=head1 NAME

SQL::Statement::Operation::ANSI::Is - is operation

=head1 SYNOPSIS

  # create an C<is> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Is->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::ANSI::Is supports: C<IS NULL>, C<IS TRUE> and C<IS FALSE>.
The right operand is always evaluated in boolean context in case of C<IS TRUE>
and C<IS FALSE>. C<IS NULL> returns I<true> if the right term is not defined,
I<false> otherwise.

=head1 INHERITANCE

  SQL::Statement::Operation::Is
  ISA SQL::Statement::Operation
    ISA SQL::Statement::Term

=head1 METHODS

=head2 operate

Returns true when the left term is null, true or false - based on the
requested right value.

=cut

sub operate($)
{
    my $self  = $_[0];
    my $left  = $self->{LEFT}->value( $_[1] );
    my $right = $self->{RIGHT}->value( $_[1] );
    my $expr;

    if ( defined($right) )
    {
        $expr = defined($left) ? $left && $right : 0;    # is true / is false
    }
    else
    {
        $expr = !defined($left);
    }

    return $expr;
}

package SQL::Statement::Operation::Contains;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation);
use Scalar::Util qw(looks_like_number);

=pod

=head1 NAME

SQL::Statement::Operation::Contains - in operation

=head1 SYNOPSIS

  # create an C<in> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Contains->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Contains expects the right operand is an array
of L<SQL::Statement::Term> instances. It checks whether the left operand
is in the list of the right operands or not like:

  $left->value($eval) ~~ map { $_->value($eval) } @{$right}

=head1 INHERITANCE

  SQL::Statement::Operation::Contains
  ISA SQL::Statement::Operation
    ISA SQL::Statement::Term

=head1 METHODS

=head2 operate

Returns true when the left term is equal to any of the right terms

=cut

sub operate($)
{
    my ( $self, $eval ) = @_;
    my $left  = $self->{LEFT}->value($eval);
    my @right = map { $_->value($eval); } @{ $self->{RIGHT} };
    my $expr  = 0;

    foreach my $r (@right)
    {
        last
          if $expr |= ( looks_like_number($left) && looks_like_number($r) ) ? $left == $r : $left eq $r;
    }

    return $expr;
}

package SQL::Statement::Operation::Between;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation);
use Scalar::Util qw(looks_like_number);

=pod

=head1 NAME

SQL::Statement::Operation::Between - between operation

=head1 SYNOPSIS

  # create an C<between> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Between->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Between expects the right operand is an array
of 2 L<SQL::Statement::Term> instances. It checks whether the left operand
is between the right operands like:

     ( $left->value($eval) >= $right[0]->value($eval) )
  && ( $left->value($eval) <= $right[1]->value($eval) )

=head1 INHERITANCE

  SQL::Statement::Operation::Between
  ISA SQL::Statement::Operation
    ISA SQL::Statement::Term

=head1 METHODS

=head2 operate

Returns true when the left term is between both right terms

=cut

sub operate($)
{
    my ( $self, $eval ) = @_;
    my $left  = $self->{LEFT}->value($eval);
    my @right = map { $_->value($eval); } @{ $self->{RIGHT} };
    my $expr  = 0;

    if (   looks_like_number($left)
        && looks_like_number( $right[0] )
        && looks_like_number( $right[1] ) )
    {
        $expr = ( $left >= $right[0] ) && ( $left <= $right[1] );
    }
    else
    {
        $expr = ( $left ge $right[0] ) && ( $left le $right[1] );
    }

    return $expr;
}

package SQL::Statement::Operation::Equality;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation);

use Carp ();
use Scalar::Util qw(looks_like_number);

=pod

=head1 NAME

SQL::Statement::Operation::Equality - abstract base class for comparisons

=head1 SYNOPSIS

  # create an C<equality> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Equality->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Equality implements compare operations between
two terms - choosing either numerical comparison or string comparison,
depending whether both operands are numeric or not.

=head1 INHERITANCE

  SQL::Statement::Operation::Equality
  ISA SQL::Statement::Operation
    ISA SQL::Statement::Term

=head1 METHODS

=head2 operate

Return the result of the comparison.

=head2 numcmp

I<Abstract> method which will do the numeric comparison of both terms. Must be
overridden by derived classes.

=head2 strcmp

I<Abstract> method which will do the string comparison of both terms. Must be
overridden by derived classes.

=cut

sub operate($)
{
    my $self  = $_[0];
    my $left  = $self->{LEFT}->value( $_[1] );
    my $right = $self->{RIGHT}->value( $_[1] );
    return 0 unless ( defined($left) && defined($right) );
    return ( looks_like_number($left) && looks_like_number($right) )
      ? $self->numcmp( $left, $right )
      : $self->strcmp( $left, $right );
}

sub numcmp($)
{
    Carp::confess( sprintf( q{pure virtual function 'numcmp' called on %s for %s}, ref( $_[0] ) || __PACKAGE__, $_[0]->{OP} ) );
}

sub strcmp($)
{
    Carp::confess( sprintf( q{pure virtual function 'strcmp' called on %s for %s}, ref( $_[0] ) || __PACKAGE__, $_[0]->{OP} ) );
}

package SQL::Statement::Operation::Equal;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation::Equality);

=pod

=head1 NAME

SQL::Statement::Operation::Equal - implements equal operation

=head1 SYNOPSIS

  # create an C<equal> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Equal->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Equal implements compare operations between
two numbers and two strings.

=head1 INHERITANCE

  SQL::Statement::Operation::Equal
  ISA SQL::Statement::Operation::Equality
    ISA SQL::Statement::Operation
      ISA SQL::Statement::Term

=head1 METHODS

=head2 numcmp

Return true when C<$left == $right>

=head2 strcmp

Return true when C<$left eq $right>

=cut

sub numcmp($$) { return $_[1] == $_[2]; }
sub strcmp($$) { return $_[1] eq $_[2]; }

package SQL::Statement::Operation::NotEqual;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation::Equality);

=pod

=head1 NAME

SQL::Statement::Operation::NotEqual - implements not equal operation

=head1 SYNOPSIS

  # create an C<not equal> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::NotEqual->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::NotEqual implements negated compare operations
between two numbers and two strings.

=head1 INHERITANCE

  SQL::Statement::Operation::NotEqual
  ISA SQL::Statement::Operation::Equality
    ISA SQL::Statement::Operation
      ISA SQL::Statement::Term

=head1 METHODS

=head2 numcmp

Return true when C<$left != $right>

=head2 strcmp

Return true when C<$left ne $right>

=cut

sub numcmp($$) { return $_[1] != $_[2]; }
sub strcmp($$) { return $_[1] ne $_[2]; }

package SQL::Statement::Operation::Lower;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation::Equality);

=pod

=head1 NAME

SQL::Statement::Operation::Lower - implements lower than operation

=head1 SYNOPSIS

  # create an C<lower than> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Lower->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Lower implements lower than compare operations
between two numbers and two strings.

=head1 INHERITANCE

  SQL::Statement::Operation::Lower
  ISA SQL::Statement::Operation::Equality
    ISA SQL::Statement::Operation
      ISA SQL::Statement::Term

=head1 METHODS

=head2 numcmp

Return true when C<$left < $right>

=head2 strcmp

Return true when C<$left lt $right>

=cut

sub numcmp($$) { return $_[1] < $_[2]; }
sub strcmp($$) { return $_[1] lt $_[2]; }

package SQL::Statement::Operation::Greater;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation::Equality);

=pod

=head1 NAME

SQL::Statement::Operation::Greater - implements greater than operation

=head1 SYNOPSIS

  # create an C<greater than> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Greater->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Greater implements greater than compare operations
between two numbers and two strings.

=head1 INHERITANCE

  SQL::Statement::Operation::Greater
  ISA SQL::Statement::Operation::Equality
    ISA SQL::Statement::Operation
      ISA SQL::Statement::Term

=head1 METHODS

=head2 numcmp

Return true when C<$left > $right>

=head2 strcmp

Return true when C<$left gt $right>

=cut

sub numcmp($$) { return $_[1] > $_[2]; }
sub strcmp($$) { return $_[1] gt $_[2]; }

package SQL::Statement::Operation::LowerEqual;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation::Equality);

=pod

=head1 NAME

SQL::Statement::Operation::LowerEqual - implements lower equal operation

=head1 SYNOPSIS

  # create an C<lower equal> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::LowerEqual->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::LowerEqual implements lower equal compare operations
between two numbers and two strings.

=head1 INHERITANCE

  SQL::Statement::Operation::LowerEqual
  ISA SQL::Statement::Operation::Equality
    ISA SQL::Statement::Operation
      ISA SQL::Statement::Term

=head1 METHODS

=head2 numcmp

Return true when C<$left <= $right>

=head2 strcmp

Return true when C<$left le $right>

=cut

sub numcmp($$) { return $_[1] <= $_[2]; }
sub strcmp($$) { return $_[1] le $_[2]; }

package SQL::Statement::Operation::GreaterEqual;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation::Equality);

=pod

=head1 NAME

SQL::Statement::Operation::GreaterEqual - implements greater equal operation

=head1 SYNOPSIS

  # create an C<greater equal> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::GreaterEqual->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::GreaterEqual implements greater equal compare operations
between two numbers and two strings.

=head1 INHERITANCE

  SQL::Statement::Operation::GreaterEqual
  ISA SQL::Statement::Operation::Equality
    ISA SQL::Statement::Operation
      ISA SQL::Statement::Term

=head1 METHODS

=head2 numcmp

Return true when C<$left >= $right>

=head2 strcmp

Return true when C<$left ge $right>

=cut

sub numcmp($$) { return $_[1] >= $_[2]; }
sub strcmp($$) { return $_[1] ge $_[2]; }

package SQL::Statement::Operation::Regexp;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation);

=pod

=head1 NAME

SQL::Statement::Operation::Regexp - abstract base class for comparisons based on regular expressions

=head1 SYNOPSIS

  # create an C<regexp> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Regexp->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Regexp implements the comparisons for the C<LIKE>
operation family.

=head1 INHERITANCE

  SQL::Statement::Operation::Regexp
  ISA SQL::Statement::Operation
    ISA SQL::Statement::Term

=head1 METHODS

=head2 operate

Return the result of the comparison.

=head2 right

Returns the regular expression based on the right term. The right term
is expected to be constant - so C<a LIKE b> in not supported.

=head2 regexp

I<Abstract> method which must return a regular expression (C<qr//>) from
the given string.  Must be overridden by derived classes.

=cut

sub right($)
{
    my $self  = $_[0];
    my $right = $self->{RIGHT}->value( $_[1] );

    unless ( defined( $self->{PATTERNS}->{$right} ) )
    {
        $self->{PATTERNS}->{$right} = $right;
        $self->{PATTERNS}->{$right} =~ s/%/.*/g;
        $self->{PATTERNS}->{$right} = $self->regexp( $self->{PATTERNS}->{$right} );
    }

    return $self->{PATTERNS}->{$right};
}

sub regexp($)
{
    Carp::confess( sprintf( q{pure virtual function 'regexp' called on %s for %s}, ref( $_[0] ) || __PACKAGE__, $_[0]->{OP} ) );
}

sub operate($)
{
    my $self  = $_[0];
    my $left  = $self->{LEFT}->value( $_[1] );
    my $right = $self->right( $_[1] );

    return 0 unless ( defined($left) && defined($right) );
    return $left =~ m/^$right$/s;
}

package SQL::Statement::Operation::Like;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation::Regexp);

=pod

=head1 NAME

SQL::Statement::Operation::Like - implements the like operation

=head1 SYNOPSIS

  # create an C<like> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Like->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Like is used to the comparisons for the C<LIKE>
operation.

=head1 INHERITANCE

  SQL::Statement::Operation::Like
  ISA SQL::Statement::Operation::Regexp
    ISA SQL::Statement::Operation
      ISA SQL::Statement::Term

=head1 METHODS

=head2 regexp

Returns C<qr/^$right$/s>

=cut

sub regexp($)
{
    my $right = $_[1];
    return qr/^$right$/s;
}

package SQL::Statement::Operation::Clike;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation::Regexp);

=pod

=head1 NAME

SQL::Statement::Operation::Clike - implements the clike operation

=head1 SYNOPSIS

  # create an C<clike> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::Clike->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::Clike is used to the comparisons for the C<CLIKE>
operation.

=head1 INHERITANCE

  SQL::Statement::Operation::Clike
  ISA SQL::Statement::Operation::Regexp
    ISA SQL::Statement::Operation
      ISA SQL::Statement::Term

=head1 METHODS

=head2 regexp

Returns C<qr/^$right$/si>

=cut

sub regexp($)
{
    my $right = $_[1];
    return qr/^$right$/si;
}

package SQL::Statement::Operation::Rlike;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Operation::Regexp);

=pod

=head1 NAME

SQL::Statement::Operation::RLike - implements the rlike operation

=head1 SYNOPSIS

  # create an C<rlike> operation with an SQL::Statement object as owner,
  # specifying the operation name, the left and the right operand
  my $term = SQL::Statement::RLike->new( $owner, $op, $left, $right );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Operation::RLike is used to the comparisons for the C<RLIKE>
operation.

=head1 INHERITANCE

  SQL::Statement::Operation::RLike
  ISA SQL::Statement::Operation::Regexp
    ISA SQL::Statement::Operation
      ISA SQL::Statement::Term

=head1 METHODS

=head2 regexp

Returns C<qr/$right$/s>

=cut

sub regexp($)
{
    my $right = $_[1];
    return qr/$right$/;
}

=head1 AUTHOR AND COPYRIGHT

Copyright (c) 2009,2017 by Jens Rehsack: rehsackATcpan.org

All rights reserved.

You may distribute this module under the terms of either the GNU
General Public License or the Artistic License, as specified in
the Perl README file.

=cut

1;
