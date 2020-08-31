package SQL::Statement::Term;

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

our $VERSION = '1.412';

use Scalar::Util qw(weaken);
use Carp ();

=pod

=head1 NAME

SQL::Statement::Term - base class for all terms

=head1 SYNOPSIS

  # create a term with an SQL::Statement object as owner
  my $term = SQL::Statement::Term->new( $owner );
  # access the value of that term
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::Term is an abstract base class providing the interface
for all terms.

=head1 INHERITANCE

  SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates new term and stores a weak reference to the owner.

=head2 value

I<Abstract> method which will return the value of the term. Must be
overridden by derived classes.

=head2 DESTROY

Destroys the term and undefines the weak reference to the owner.

=cut

sub new
{
    my $class = $_[0];
    my $owner = $_[1];

    my $self = bless( { OWNER => $owner }, $class );
    weaken( $self->{OWNER} );

    return $self;
}

sub DESTROY
{
    my $self = $_[0];
    undef $self->{OWNER};
}

sub value($)
{
    Carp::confess( sprintf( q{pure virtual function '%s->value' called}, ref( $_[0] ) || __PACKAGE__ ) );
}

package SQL::Statement::ConstantTerm;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Term);

=pod

=head1 NAME

SQL::Statement::ConstantTerm - term for constant values

=head1 SYNOPSIS

  # create a term with an SQL::Statement object as owner
  my $term = SQL::Statement::ConstantTerm->new( $owner, 'foo' );
  # access the value of that term - returns 'foo'
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::ConstantTerm implements a term which will always return the
same constant value.

=head1 INHERITANCE

  SQL::Statement::ConstantTerm
  ISA SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates new term and stores the constant to deliver and a weak
reference to the owner.

=head2 value

Returns the specified constant.

=cut

sub new
{
    my ( $class, $owner, $value ) = @_;

    my $self = $class->SUPER::new($owner);
    $self->{VALUE} = $value;

    return $self;
}

sub value($$) { return $_[0]->{VALUE}; }

package SQL::Statement::ColumnValue;

use vars qw(@ISA);
@ISA = qw(SQL::Statement::Term);

use Carp qw(croak);
use Params::Util qw(_INSTANCE _ARRAY0 _SCALAR);
use Scalar::Util qw(looks_like_number);

=pod

=head1 NAME

SQL::Statement::ColumnValue - term for column values

=head1 SYNOPSIS

  # create a term with an SQL::Statement object as owner
  my $term = SQL::Statement::ColumnValue->new( $owner, 'id' );
  # access the value of that term - returns the value of the column 'id'
  # of the currently active row in $eval
  $term->value( $eval );

=head1 DESCRIPTION

SQL::Statement::ColumnValue implements a term which will return the specified
column of the active row.

=head1 INHERITANCE

  SQL::Statement::ColumnValue
  ISA SQL::Statement::Term

=head1 METHODS

=head2 new

Instantiates new term and stores the column name to deliver and a weak
reference to the owner.

=head2 value

Returns the specified column value.

=cut

sub new
{
    my ( $class, $owner, $value ) = @_;

    my $self = $class->SUPER::new($owner);
    $self->{VALUE} = $value;

    return $self;
}

sub value($)
{
    my ( $self, $eval ) = @_;
    unless ( defined( $self->{TMPVAL} ) )
    {
        my ( $tbl, $col ) = $self->{OWNER}->full_qualified_column_name( $self->{VALUE} );
        defined($tbl) or croak("Can't find table containing column named '$self->{VALUE}'");
        defined($col) or croak("Unknown column: '$self->{VALUE}'");
        $self->{TMPVAL}      = $tbl . $self->{OWNER}->{dlm} . $col;
        $self->{TABLE_NAME}  = $tbl;
        $self->{COLUMN_NAME} = $col;
    }

    # XXX - can TMPVAL being defined without TABLE_NAME?
    unless ( defined( $self->{TABLE_NAME} ) )
    {
        croak( "No table specified: '" . $self->{OWNER}->{original_string} . "'" );
    }

    # with TempEval: return $eval->column($self->{TABLE_NAME}, $self->{COLUMN_NAME});
    my $fp;
    defined( $fp = $self->{fastpath}->{ "${eval}." . $self->{TABLE_NAME} } )
      and return &$fp( $self->{COLUMN_NAME} );

    defined( $fp = $self->{fastpath}->{ "${eval}." . $self->{TMPVAL} } )
      and return &$fp( $self->{TMPVAL} );

    if ( defined( _INSTANCE( $eval, 'SQL::Eval' ) ) )
    {
        $self->{fastpath}->{ "${eval}." . $self->{TABLE_NAME} } =
          $eval->_gen_access_fastpath( $self->{TABLE_NAME} );
        return &{ $self->{fastpath}->{ "${eval}." . $self->{TABLE_NAME} } }( $self->{COLUMN_NAME} );
    }
    elsif ( defined( _INSTANCE( $eval, 'SQL::Eval::Table' ) ) )
    {
        $self->{fastpath}->{ "${eval}." . $self->{TMPVAL} } =
          $eval->_gen_access_fastpath( $self->{TMPVAL} );
        return &{ $self->{fastpath}->{ "${eval}." . $self->{TMPVAL} } }( $self->{TMPVAL} );
        # return $eval->column( $self->{TMPVAL} );
    }
    else
    {
        croak( "Unsupported table storage: '" . ref($eval) . "'" );
    }
}

=head1 AUTHOR AND COPYRIGHT

Copyright (c) 2009-2017 by Jens Rehsack: rehsackATcpan.org

All rights reserved.

You may distribute this module under the terms of either the GNU
General Public License or the Artistic License, as specified in
the Perl README file.

=cut

1;
