############################
package SQL::Statement::RAM;
############################

######################################################################
#
# This module is copyright (c), 2001,2005 by Jeff Zucker.
# This module is copyright (c), 2007-2017 by Jens Rehsack.
# All rights reserved.
#
# It may be freely distributed under the same terms as Perl itself.
# See below for help and copyright information (search for SYNOPSIS).
#
######################################################################

use strict;
use warnings FATAL => "all";

use vars qw($VERSION);
$VERSION = '1.412';

####################################
package SQL::Statement::RAM::Table;
####################################

use strict;
use warnings FATAL => "all";

use SQL::Eval ();

use vars qw(@ISA);
@ISA = qw(SQL::Eval::Table);

use Carp qw(croak);

sub new
{
    my ( $class, $tname, $col_names, $data_tbl ) = @_;
    my %table = (
        NAME         => $tname,
        index        => 0,
        records      => $data_tbl,
        col_names    => $col_names,
        capabilities => {
            inplace_update => 1,
            inplace_delete => 1,
        },
    );
    my $self = $class->SUPER::new( \%table );
}

##################################
# fetch_row()
##################################
sub fetch_row
{
    my ( $self, $data ) = @_;

    return $self->{row} =
        ( $self->{records} and ( $self->{index} < scalar( @{ $self->{records} } ) ) )
      ? [ @{ $self->{records}->[ $self->{index}++ ] } ]
      : undef;
}

####################################
# insert_new_row()
####################################
sub insert_new_row
{
    my ( $self, $data, $fields ) = @_;
    push @{ $self->{records} }, [ @{$fields} ];
    return 1;
}

##################################
# delete_current_row()
##################################
sub delete_current_row
{
    my ( $self, $data, $fields ) = @_;
    my $currentRow = $self->{index} - 1;
    croak "No current row" unless ( $currentRow >= 0 );
    splice @{ $self->{records} }, $currentRow, 1;
    --$self->{index};
    return 1;
}

##################################
# update_current_row()
##################################
sub update_current_row
{
    my ( $self, $data, $fields ) = @_;
    my $currentRow = $self->{index} - 1;
    croak "No current row" unless ( $currentRow >= 0 );
    $self->{records}->[$currentRow] = [ @{$fields} ];
    return 1;
}

##################################
# truncate()
##################################
sub truncate
{
    return splice @{ $_[0]->{records} }, $_[0]->{index};
}

#####################################
# push_names()
#####################################
sub push_names
{
    my ( $self, $data, $names ) = @_;
    $self->{col_names}     = $names;
    $self->{org_col_names} = [ @{$names} ];
    $self->{col_nums}      = SQL::Eval::Table::_map_colnums($names);
}

#####################################
# drop()
#####################################
sub drop
{
    my ( $self, $data ) = @_;
    my $tname = $self->{NAME};
    delete $data->{Database}->{sql_ram_tables}->{$tname};
    return 1;
}

#####################################
# seek()
#####################################
sub seek
{
    my ( $self, $data, $pos, $whence ) = @_;
    return unless defined $self->{records};
    my ($currentRow) = $self->{index};
    if ( $whence == 0 )
    {
        $currentRow = $pos;
    }
    elsif ( $whence == 1 )
    {
        $currentRow += $pos;
    }
    elsif ( $whence == 2 )
    {
        $currentRow = @{ $self->{records} } + $pos;
    }
    else
    {
        croak $self . "->seek: Illegal whence argument ($whence)";
    }
    if ( $currentRow < 0 )
    {
        croak "Illegal row number: $currentRow";
    }
    $self->{index} = $currentRow;
}

1;

=pod

=head1 NAME

SQL::Statement::RAM

=head1 SYNOPSIS

  SQL::Statement::RAM

=head1 DESCRIPTION

This package contains support for the internally used
SQL::Statement::RAM::Table.

=head1 INHERITANCE

  SQL::Statement::RAM

  SQL::Statement::RAM::Table
  ISA SQL::Eval::Table

=head1 SQL::Statement::RAM::Table

=head2 METHODS

=over 8

=item new

Instantiates a new C<SQL::Statement::RAM::Table> object, used for temporary
tables.

    CREATE TEMP TABLE foo ....

=item fetch_row

Fetches the next row

=item push_row

As fetch_row except for writing

=item delete_current_row

Deletes the last fetched/pushed row

=item update_current_row

Updates the last fetched/pushed row

=item truncate

Truncates the table at the current position

=item push_names

Set the column names of the table

=item drop

Discards the table

=item seek

Seek the row pointer

=back

=head2 CAPABILITIES

This table has following capabilities:

=over 8

=item update_current_row

Using provided method C<update_current_row> and capability C<inplace_update>.

=item rowwise_update

By providing capability C<update_current_row>.

=item inplace_update

By definition (appropriate flag set in constructor).

=item delete_current_row

Using provided method C<delete_current_row> and capability C<inplace_delete>.

=item rowwise_delete

By providing capability C<delete_current_row>.

=item inplace_delete

By definition (appropriate flag set in constructor).

=back

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc SQL::Statement

You can also look for information at:

=over 4

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=SQL-Statement>

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/SQL-Statement>

=item * CPAN Ratings

L<http://cpanratings.perl.org/s/SQL-Statement>

=item * Search CPAN

L<http://search.cpan.org/dist/SQL-Statement/>

=back

=head1 AUTHOR AND COPYRIGHT

Copyright (c) 2001,2005 by Jeff Zucker: jzuckerATcpan.org
Copyright (c) 2007-2017 by Jens Rehsack: rehsackATcpan.org

All rights reserved.

You may distribute this module under the terms of either the GNU
General Public License or the Artistic License, as specified in
the Perl README file.

=cut
