package SQL::Statement::TermFactory;

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

use SQL::Statement::Term        ();
use SQL::Statement::Operation   ();
use SQL::Statement::Placeholder ();
use SQL::Statement::Function    ();

use Data::Dumper;
use Params::Util qw(_HASH _ARRAY0 _INSTANCE);
use Scalar::Util qw(blessed weaken);

our $VERSION = '1.412';

my %oplist = (
    '='       => 'Equal',
    '<>'      => 'NotEqual',
    'AND'     => 'And',
    'OR'      => 'Or',
    '<='      => 'LowerEqual',
    '>='      => 'GreaterEqual',
    '<'       => 'Lower',
    '>'       => 'Greater',
    'LIKE'    => 'Like',
    'RLIKE'   => 'Rlike',
    'CLIKE'   => 'Clike',
    'IN'      => 'Contains',
    'BETWEEN' => 'Between',
    'IS'      => 'Is',
);

sub new
{
    my ( $class, $owner ) = @_;
    my $self = bless(
        {
            OWNER => $owner,
        },
        $class
    );

    weaken( $self->{OWNER} );

    return $self;
}

my %opClasses;

sub _getOpClass($)
{
    my ( $self, $op ) = @_;
    unless ( defined( $opClasses{$op} ) )
    {
        my $opBase = 'SQL::Statement::Operation';
        my $opDialect = join( '::', $opBase, $self->{OWNER}->{dialect}, $oplist{$op} );
        $opClasses{$op} =
          $opDialect->isa($opBase) ? $opDialect : join( '::', $opBase, $oplist{$op} );
    }

    return $opClasses{$op};
}

sub buildCondition
{
    my ( $self, $pred ) = @_;
    my $term;

    if ( _ARRAY0($pred) )
    {
        $term = [ map { $self->buildCondition($_) } @{$pred} ];
    }
    elsif ( defined( $pred->{op} ) )
    {
        my $op = uc( $pred->{op} );
        if ( $op eq 'USER_DEFINED' && !$pred->{arg2} )
        {
            $term = SQL::Statement::ConstantTerm->new( $self->{OWNER}, $pred->{arg1}->{value} );
        }
        elsif ( defined( $oplist{$op} ) )
        {
            my $cn    = $self->_getOpClass($op);
            my $left  = $self->buildCondition( $pred->{arg1} );
            my $right = $self->buildCondition( $pred->{arg2} );
            $term = $cn->new( $self->{OWNER}, $op, $left, $right );
        }
        elsif ( defined( $self->{OWNER}->{opts}->{function_names}->{$op} ) )
        {
            my $left  = $self->buildCondition( $pred->{arg1} );
            my $right = $self->buildCondition( $pred->{arg2} );

            $term = SQL::Statement::Function::UserFunc->new(
                $self->{OWNER}, $op,
                $self->{OWNER}->{opts}->{function_names}->{$op},
                [ $left, $right ]
            );
        }
        else
        {
            return $self->{OWNER}->do_err( sprintf( q{Unknown operation '%s'}, $pred->{op} ) );
        }

        if ( $pred->{neg} )
        {
            $term = SQL::Statement::Operation::Neg->new( $self->{OWNER}, 'NOT', $term );
        }
    }
    elsif ( defined( $pred->{type} ) )
    {
        my $type = uc( $pred->{type} );
        if ( $type =~ m/^(?:STRING|NUMBER|BOOLEAN)$/ )
        {
            $term = SQL::Statement::ConstantTerm->new( $self->{OWNER}, $pred->{value} );
        }
        elsif ( $type eq 'NULL' )
        {
            $term = SQL::Statement::ConstantTerm->new( $self->{OWNER}, undef );
        }
        elsif ( $type eq 'COLUMN' )
        {
            $term = SQL::Statement::ColumnValue->new( $self->{OWNER}, $pred->{value} );
        }
        elsif ( $type eq 'PLACEHOLDER' )
        {
            $term = SQL::Statement::Placeholder->new( $self->{OWNER}, $pred->{argnum} );
        }
        elsif ( $type eq 'FUNCTION' )
        {
            my @params = map { blessed($_) ? $_ : $self->buildCondition($_) } @{ $pred->{value} };

            if ( $pred->{name} eq 'numeric_exp' )
            {
                $term = SQL::Statement::Function::NumericEval->new( $self->{OWNER}, $pred->{str}, \@params );
            }
            elsif ( $pred->{name} eq 'str_concat' )
            {
                $term = SQL::Statement::Function::StrConcat->new( $self->{OWNER}, \@params );
            }
            elsif ( $pred->{name} eq 'TRIM' )
            {
                $term = SQL::Statement::Function::Trim->new( $self->{OWNER}, $pred->{trim_spec}, $pred->{trim_char}, \@params );
            }
            elsif ( $pred->{name} eq 'SUBSTRING' )
            {
                my $start  = $self->buildCondition( $pred->{start} );
                my $length = $self->buildCondition( $pred->{length} )
                  if ( _HASH( $pred->{length} ) );
                $term = SQL::Statement::Function::SubString->new( $self->{OWNER}, $start, $length, \@params );
            }
            else
            {
                $term = SQL::Statement::Function::UserFunc->new( $self->{OWNER}, $pred->{name}, $pred->{subname}, \@params );
            }
        }
        else
        {
            return $self->{OWNER}->do_err( sprintf( q{Unknown type '%s'}, $pred->{type} ) );
        }
    }
    elsif ( defined( _INSTANCE( $pred, 'SQL::Statement::Term' ) ) )
    {
        return $pred;
    }
    else
    {
        return $self->{OWNER}->do_err( sprintf( q~Unknown predicate '{%s}'~, Dumper($pred) ) );
    }

    return $term;
}

sub DESTROY
{
    my $self = $_[0];
    undef $self->{OWNER};
}

=pod

=head1 NAME

SQL::Statement::TermFactory - Factory for SQL::Statement::Term instances

=head1 SYNOPSIS

  my $termFactory = SQL::Statement::TermFactory->new($stmt);
  my $whereTerms = $termFactory->buildCondition( $stmt->{where_clause} );
  my $col = $termFactory->buildCondition( $stmt->{col_obj}->{$name}->{content} );

=head1 DESCRIPTION

This package implements a factory to create type and operation based terms.
Those terms are used to access data from the table(s) - either when evaluating
the where clause or returning column data.

The concept of a factory can be studied in I<Design Patterns> by the Gang of
Four. The concept of using polymorphism instead of conditions is suggested by
Martin Fowler in his book I<Refactoring>.

=head1 METHODS

=head2 buildCondition

Builds a condition object from a given (part of a) where clause. This method
calls itself recursively for I<predicates>.

=head1 AUTHOR AND COPYRIGHT

Copyright (c) 2001,2005 by Jeff Zucker: jzuckerATcpan.org
Copyright (c) 2009-2017 by Jens Rehsack: rehsackATcpan.org

All rights reserved.

You may distribute this module under the terms of either the GNU
General Public License or the Artistic License, as specified in
the Perl README file.

=cut

1;
