package SQL::Statement::Function;

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
use vars qw(@ISA $VERSION);

use SQL::Statement::Term ();
@ISA = qw(SQL::Statement::Term);

$VERSION = '1.412';

=pod

=head1 NAME

SQL::Statement::Function - abstract base class for all function executing terms

=head1 SYNOPSIS

  # this class doesn't have a common constructor, because all derived classes
  # have their special requirements

=head1 DESCRIPTION

SQL::Statement::Function is an abstract base class providing the interface
for all function executing terms.

=head1 INHERITANCE

  SQL::Statement::Function
  ISA SQL::Statement::Term

=head1 METHODS

=head2 DESTROY

Destroys the term and undefines the weak reference to the owner as well
as the reference to the parameter list.

=cut

sub DESTROY
{
    my $self = $_[0];

    undef $self->{PARAMS};

    $self->SUPER::DESTROY();
}

package SQL::Statement::Function::UserFunc;

use vars qw(@ISA);

use Carp ();
use Params::Util qw(_INSTANCE);

use SQL::Statement::Functions;

@ISA = qw(SQL::Statement::Function);

=pod

=head1 NAME

SQL::Statement::Function::UserFunc - implements executing a perl subroutine

=head1 SYNOPSIS

  # create an user function term with an SQL::Statement object as owner,
  # specifying the function name, the subroutine name (full qualified)
  # and the parameters to the subroutine
  my $term = SQL::Statement::Function::UserFunc->new( $owner, $name, $sub, \@params );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Function::UserFunc implements a term which returns the result
of the specified subroutine.

=head1 INHERITANCE

  SQL::Statement::Function
  ISA SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates a new C<SQL::Statement::Function::UserFunc> instance.

=head2 value

Invokes the given subroutine with the values of the params and return it's
result:

    my @params = map { $_->value($eval); } @{ $self->{PARAMS} };
    return $subpkg->$subname( $self->{OWNER}, @params );

=cut

sub new
{
    my ( $class, $owner, $name, $subnm, $params ) = @_;

    my $self = $class->SUPER::new($owner);

    my ( $pkg, $sub ) = $subnm =~ m/^(.*::)([^:]+$)/;
    if ( !$sub )
    {
        $sub = $subnm;
        $pkg = 'main';
    }
    $pkg =~ s/::$//g;
    $pkg = 'main' unless ($pkg);

    $self->{SUB}    = $sub;
    $self->{PKG}    = $pkg;
    $self->{NAME}   = $name;
    $self->{PARAMS} = $params;

    unless ( UNIVERSAL::can( $pkg, $sub ) )
    {
        unless ( 'main' eq $pkg )
        {
            my $mod = $pkg;
            $mod =~ s|::|/|g;
            $mod .= '.pm';
            eval { require $mod; } unless ( defined( $INC{$mod} ) );
            return $owner->do_err($@) if ($@);
        }

        $pkg->can($sub) or return $owner->do_err( "Can't find subroutine $pkg" . "::$sub" );
    }

    return $self;
}

sub value($)
{
    my $self   = $_[0];
    my $eval   = $_[1];
    my $pkg    = $self->{PKG};
    my $sub    = $self->{SUB};
    my @params = map { $_->value($eval); } @{ $self->{PARAMS} };
    return $pkg->$sub( $self->{OWNER}, @params );    # FIXME is $pkg just a string?
}

package SQL::Statement::Function::NumericEval;

use vars qw(@ISA);

use Params::Util qw(_NUMBER _INSTANCE);

@ISA = qw(SQL::Statement::Function);

=pod

=head1 NAME

SQL::Statement::Function::NumericEval - implements numeric evaluation of a term

=head1 SYNOPSIS

  # create an user function term with an SQL::Statement object as owner,
  # specifying the expression to evaluate and the parameters to the subroutine
  my $term = SQL::Statement::NumericEval->new( $owner, $expr, \@params );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Function::NumericEval implements the numeric evaluation of a
term. All parameters are expected to be numeric.

=head1 INHERITANCE

  SQL::Statement::Function::NumericEval
  ISA SQL::Statement::Function
    ISA SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates a new C<SQL::Statement::Function::NumericEval> instance.
Takes I<$owner>, I<$expr> and I<\@params> as arguments (in specified order).

=head2 value

Returns the result of the evaluated expression.

=cut

sub new
{
    my ( $class, $owner, $expr, $params ) = @_;

    my $self = $class->SUPER::new($owner);

    $self->{EXPR}   = $expr;
    $self->{PARAMS} = $params;

    return $self;
}

sub value($)
{
    my ( $self, $eval ) = @_;
    my @vals =
      map { _INSTANCE( $_, 'SQL::Statement::Term' ) ? $_->value($eval) : $_ } @{ $self->{PARAMS} };
    foreach my $val (@vals)
    {
        return $self->{OWNER}->do_err(qq~Bad numeric expression '$val'!~)
          unless ( defined( _NUMBER($val) ) );
    }
    my $expr = $self->{EXPR};
    $expr =~ s/\?(\d+)\?/$vals[$1]/g;
    $expr =~ s/\s//g;
    $expr =~ s/^([\)\(+\-\*\/\%0-9]+)$/$1/;    # untaint
    return eval $expr;
}

package SQL::Statement::Function::Trim;

use vars qw(@ISA);

BEGIN { @ISA = qw(SQL::Statement::Function); }

=pod

=head1 NAME

SQL::Statement::Function::Trim - implements the built-in trim function support

=head1 SYNOPSIS

  # create an trim function term with an SQL::Statement object as owner,
  # specifying the spec, char and the parameters to the subroutine
  my $term = SQL::Statement::Trim->new( $owner, $spec, $char, \@params );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Function::Trim implements string trimming.

=head1 INHERITANCE

  SQL::Statement::Function::Trim
  ISA SQL::Statement::Function
    ISA SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates a new C<SQL::Statement::Function::Trim> instance.
Takes I<$owner>, I<$spec>, I<$char> and I<\@params> as arguments
(in specified order).

Meaning of the parameters:

=over 4

=item I<$spec>

Can be on of 'LEADING', 'TRAILING' 'BOTH'. Trims the leading chars, trailing
chars or at both ends, respectively.

Defaults to 'BOTH'.

=item I<$char>

The character to trim - defaults to C<' '>

=item I<\@params>

Expected to be an array with exact 1 element (more aren't evaluated).

=back

=head2 value

Returns the trimmed value of first parameter argument.

=cut

sub new
{
    my ( $class, $owner, $spec, $char, $params ) = @_;
    $spec ||= 'BOTH';
    $char ||= ' ';

    my $self = $class->SUPER::new($owner);

    $self->{PARAMS} = $params;
    $self->{TRIMFN} = sub { my $s = $_[0]; $s =~ s/^$char*//g; return $s; }
      if ( $spec =~ m/LEADING/ );
    $self->{TRIMFN} = sub { my $s = $_[0]; $s =~ s/$char*$//g; return $s; }
      if ( $spec =~ m/TRAILING/ );
    $self->{TRIMFN} = sub { my $s = $_[0]; $s =~ s/^$char*//g; $s =~ s/$char*$//g; return $s; }
      if ( $spec =~ m/BOTH/ );

    return $self;
}

sub value($)
{
    my $val = $_[0]->{PARAMS}->[0]->value( $_[1] );
    $val = &{ $_[0]->{TRIMFN} }($val);
    return $val;
}

package SQL::Statement::Function::SubString;

use vars qw(@ISA);

@ISA = qw(SQL::Statement::Function);

=pod

=head1 NAME

SQL::Statement::Function::SubString - implements the built-in sub-string function support

=head1 SYNOPSIS

  # create an substr function term with an SQL::Statement object as owner,
  # specifying the start and length of the sub string to extract from the
  # first element of \@params
  my $term = SQL::Statement::SubString->new( $owner, $start, $length, \@params );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Function::SubString implements a sub-string extraction term.

=head1 INHERITANCE

  SQL::Statement::Function::SubString
  ISA SQL::Statement::Function
    ISA SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates a new C<SQL::Statement::Function::SubString> instance.
Takes I<$owner>, I<$start>, I<$length> and I<\@params> as arguments
(in specified order).

Meaning of the parameters:

=over 4

=item I<$start>

Specifies the start position to extract the sub-string. This is expected
to be a L<SQL::Statement::Term> instance. The first character in a string
has the position 1.

=item I<$length>

Specifies the length of the extracted sub-string. This is expected
to be a L<SQL::Statement::Term> instance.

If omitted, everything to the end of the string is returned.

=item I<\@params>

Expected to be an array with exact 1 element (more aren't evaluated).

=back

=head2 value

Returns the extracted sub-string value from first parameter argument.

=cut

sub new
{
    my ( $class, $owner, $start, $length, $params ) = @_;

    my $self = $class->SUPER::new($owner);

    $self->{START}  = $start;
    $self->{LENGTH} = $length;
    $self->{PARAMS} = $params;

    return $self;
}

sub value($)
{
    my $val   = $_[0]->{PARAMS}->[0]->value( $_[1] );
    my $start = $_[0]->{START}->value( $_[1] ) - 1;
    my $length =
      defined( $_[0]->{LENGTH} ) ? $_[0]->{LENGTH}->value( $_[1] ) : length($val) - $start;
    return substr( $val, $start, $length );
}

package SQL::Statement::Function::StrConcat;

use vars qw(@ISA);

@ISA = qw(SQL::Statement::Function);

=pod

=head1 NAME

SQL::Statement::Function::StrConcat - implements the built-in string concatenation

=head1 SYNOPSIS

  # create an substr function term with an SQL::Statement object as owner
  # and \@params to concatenate
  my $term = SQL::Statement::StrConcat->new( $owner, \@params );
  # access the result of that operation
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Function::StrConcat implements a string concatenation term.

=head1 INHERITANCE

  SQL::Statement::Function::StrConcat
  ISA SQL::Statement::Function
    ISA SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates a new C<SQL::Statement::Function::StrConcat> instance.

=head2 value

Returns the concatenated string composed of the parameter values.

=cut

sub new
{
    my ( $class, $owner, $params ) = @_;

    my $self = $class->SUPER::new($owner);

    $self->{PARAMS} = $params;

    return $self;
}

sub value($)
{
    my $rc = '';
    foreach my $val ( @{ $_[0]->{PARAMS} } )
    {
        my $catval = $val->value( $_[1] );
        $rc .= defined($catval) ? $catval : '';
    }
    return $rc;
}

=head1 AUTHOR AND COPYRIGHT

Copyright (c) 2009-2017 by Jens Rehsack: rehsackATcpan.org

All rights reserved.

You may distribute this module under the terms of either the GNU
General Public License or the Artistic License, as specified in
the Perl README file.

=cut

1;
