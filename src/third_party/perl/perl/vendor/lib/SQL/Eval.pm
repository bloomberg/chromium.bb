package SQL::Eval;

#########################################################################
#
# This module is copyright (c), 2001,2005 by Jeff Zucker.
# This module is copyright (c), 2007-2017 by Jens Rehsack.
# All rights reserved.
#
# It may be freely distributed under the same terms as Perl itself.
#
# See below for help (search for SYNOPSIS)
#########################################################################

require 5.008;
use strict;
use warnings FATAL => "all";
use vars qw($VERSION);

$VERSION = '1.412';

use Carp qw(croak);

sub new($)
{
    my ( $proto, $attr ) = @_;
    my ($self) = {%$attr};
    bless( $self, ( ref($proto) || $proto ) );
}

sub param($;$)
{
    $_[1] < 0 and croak "Illegal parameter number: $_[1]";
    @_ == 3 and return $_[0]->{params}->[ $_[1] ] = $_[2];
    $_[0]->{params}->[ $_[1] ];
}

sub params(;$)
{
    @_ == 2 and return $_[0]->{params} = $_[1];
    $_[0]->{params};
}

sub table($) { $_[0]->{tables}->{ $_[1] } }

sub column($$) { $_[0]->table( $_[1] )->column( $_[2] ) }

sub _gen_access_fastpath($) { $_[0]->table( $_[1] )->_gen_access_fastpath() }

package SQL::Eval::Table;

use strict;
use warnings FATAL => "all";

use Carp qw(croak);
use Params::Util qw(_ARRAY0 _HASH0);

sub new($)
{
    my ( $proto, $attr ) = @_;
    my ($self) = {%$attr};

    defined( $self->{col_names} ) and defined( _ARRAY0( $self->{col_names} ) )
      or croak("attribute 'col_names' must be defined as an array");
    exists( $self->{col_nums} ) or $self->{col_nums} = _map_colnums( $self->{col_names} );
    defined( $self->{col_nums} ) and defined( _HASH0( $self->{col_nums} ) )
      or croak("attribute 'col_nums' must be defined as a hash");

    $self->{capabilities} = {} unless ( defined( $self->{capabilities} ) );
    bless( $self, ( ref($proto) || $proto ) );
}

sub _map_colnums
{
    my $col_names = $_[0];
    my %col_nums;
    $col_nums{ $col_names->[$_] } = $_ for ( 0 .. scalar @$col_names - 1 );
    \%col_nums;
}

sub row()         { $_[0]->{row} }
sub column($)     { $_[0]->{row}->[ $_[0]->column_num( $_[1] ) ] }
sub column_num($) { $_[0]->{col_nums}->{ $_[1] }; }
sub col_nums()    { $_[0]->{col_nums} }
sub col_names()   { $_[0]->{col_names}; }

sub _gen_access_fastpath($)
{
    my ($self) = @_;

    $self->can("column") == SQL::Eval::Table->can("column")
      && $self->can("column_num") == SQL::Eval::Table->can("column_num")
      ? sub { $self->{row}->[ $self->{col_nums}->{ $_[0] } ] }
      : sub { $self->column( $_[0] ) };
}

sub capability($)
{
    my ( $self, $capname ) = @_;
    exists $self->{capabilities}->{$capname} and return $self->{capabilities}->{$capname};

    $capname eq "insert_new_row"
      and $self->{capabilities}->{insert_new_row} = $self->can("insert_new_row");
    $capname eq "delete_one_row"
      and $self->{capabilities}->{delete_one_row} = $self->can("delete_one_row");
    $capname eq "delete_current_row"
      and $self->{capabilities}->{delete_current_row} =
      ( $self->can("delete_current_row") and $self->capability("inplace_delete") );
    $capname eq "update_one_row"
      and $self->{capabilities}->{update_one_row} = $self->can("update_one_row");
    $capname eq "update_current_row"
      and $self->{capabilities}->{update_current_row} =
      ( $self->can("update_current_row") and $self->capability("inplace_update") );
    $capname eq "update_specific_row"
      and $self->{capabilities}->{update_specific_row} = $self->can("update_specific_row");

    $capname eq "rowwise_update"
      and $self->{capabilities}->{rowwise_update} = (
             $self->capability("update_one_row")
          or $self->capability("update_current_row")
          or $self->capability("update_specific_row")
      );
    $capname eq "rowwise_delete"
      and $self->{capabilities}->{rowwise_delete} = (
             $self->capability("delete_one_row")
          or $self->capability("delete_current_row")
      );

    $self->{capabilities}->{$capname};
}

sub drop ($$)        { croak "Abstract method " . ref( $_[0] ) . "::drop called" }
sub fetch_row ($$)   { croak "Abstract method " . ref( $_[0] ) . "::fetch_row called" }
sub push_row ($$$)   { croak "Abstract method " . ref( $_[0] ) . "::push_row called" }
sub push_names ($$$) { croak "Abstract method " . ref( $_[0] ) . "::push_names called" }
sub truncate ($$)    { croak "Abstract method " . ref( $_[0] ) . "::truncate called" }
sub seek ($$$$)      { croak "Abstract method " . ref( $_[0] ) . "::seek called" }

1;

__END__

=head1 NAME

SQL::Eval - Base for deriving evaluation objects for SQL::Statement


=head1 SYNOPSIS

    require SQL::Statement;
    require SQL::Eval;

    # Create an SQL statement; use a concrete subclass of
    # SQL::Statement
    my $stmt = MyStatement->new("SELECT * FROM foo, bar",
			        SQL::Parser->new('Ansi'));

    # Get an eval object by calling open_tables; this
    # will call MyStatement::open_table
    my $eval = $stmt->open_tables($data);

    # Set parameter 0 to 'Van Gogh'
    $eval->param(0, 'Van Gogh');
    # Get parameter 2
    my $param = $eval->param(2);

    # Get the SQL::Eval::Table object referring the 'foo' table
    my $fooTable = $eval->table('foo');


=head1 DESCRIPTION

This module implements two classes that can be used for deriving
subclasses to evaluate SQL::Statement objects. The SQL::Eval object
can be thought as an abstract state engine for executing SQL queries
and the SQL::Eval::Table object is a table abstraction. It implements
methods for fetching or storing rows, retrieving column names and
numbers and so on.  See the C<test.pl> script as an example for
implementing a subclass.

While reading on, keep in mind that these are abstract classes,
you *must* implement at least some of the methods described below.
In addition, you need not derive from SQL::Eval or SQL::Eval::Table,
you just need to implement the method interface.

All methods throw a Perl exception in case of errors.

=head2 Method interface of SQL::Eval

=over 8

=item new

Constructor; use it like this:

    $eval = SQL::Eval->new(\%attr);

Blesses the hash ref \%attr into the SQL::Eval class (or a subclass).

=item param

Used for getting or setting input parameters, as in the SQL query

    INSERT INTO foo VALUES (?, ?);

Example:

    $eval->param(0, $val);        # Set parameter 0
    $eval->param(0);              # Get parameter 0

=item params

Used for getting or setting the complete array of input
parameters. Example:

    $eval->params($params);       # Set the array
    $eval->params();              # Get the array

=item table

Returns or sets a table object. Example:

    $eval->table('foo', $fooTable);  # Set the 'foo' table object
    $eval->table('foo');             # Return the 'foo' table object

=item column

Return the value of a column with a given name; example:

    $col = $eval->column('foo', 'id');  # Return the 'id' column of
                                        # the current row in the
                                        # 'foo' table

This is equivalent to and a shorthand for

    $col = $eval->table('foo')->column('id');

=item _gen_access_fastpath

Return a subroutine reference for fast accessing columns for read-only
access. This routine simply returns the C<_gen_access_fastpath> of the
referenced table.

=back


=head2 Method interface of SQL::Eval::Table

=over 8

=item new

Constructor; use it like this:

    $eval = SQL::Eval::Table->new(\%attr);

Blesses the hash ref \%attr into the SQL::Eval::Table class (or a
subclass).

The following attributes are used by C<SQL::Eval::Table>:

=over 12

=item col_names

Array reference containing the names of the columns in order they appear
in the table. This attribute B<must> be provided by the derived class.

=item col_nums

Hash reference containing the column names as keys and the column
indexes as values. If this is omitted (does not exist), it will be
created from C<col_names>.

=item capabilities

Hash reference containing additional capabilities.

=item _gen_access_fastpath

Return a subroutine reference for fast accessing columns for read-only
access. When the instantiated object doesn't provide own methods for
C<column> and C<column_num> a subroutine reference is returned which
directly access the internal data structures. For all other cases a
subroutine directly calling C<< $self->column($_[0]) >> is returned.

=back

=item row

Used to get the current row as an array ref. Do not confuse
getting the current row with the fetch_row method! In fact this
method is valid only after a successful C<$table-E<gt>fetchrow()>.
Example:

    $row = $table->row();

=item column

Get the column with a given name in the current row. Valid only after
a successful C<$table-E<gt>fetchrow()>. Example:

    $col = $table->column($colName);

=item column_num

Return the number of the given column name. Column numbers start with
0. Returns undef, if a column name is not defined, so that you can use
this for verifying column names. Example:

    $colNum = $table->column_num($colNum);

=item col_nums

Returns an hash ref of column names with the column names as keys and
the column indexes as the values.

=item col_names

Returns an array ref of column names ordered by their index within the table.

=item capability

Returns a boolean value whether the table has the specified capability
or not. This method might be overridden by derived classes, but ensure
that in that case the parent capability method is called when the
derived class does not handle the requested capability.

The following capabilities are used (and requested) by SQL::Statement:

=over 12

=item update_one_row

Defines whether the table is able to update one single row. This
capability is used for backward compatibility and might have
(depending on table implementation) several limitations. Please
carefully study the documentation of the table or ask the author of
the table, if this information is not provided.

This capability is evaluated automatically on first request and must
not be handled by any derived classes.

=item update_specific_row

Defines if the table is able to update one single row, but keeps the
original content of the row to update.

This capability is evaluated automatically on first request and must not
be handled by derived classes.

=item update_current_row

Defines if the table is able to update the currently touched row. This
capability requires the capability of C<inplace_update>.

This capability is evaluated automatically on first request and must not
be handled by derived classes.

=item rowwise_update

Defines if the table is able to do row-wise updates which means one
of C<update_one_row>, C<update_specific_row> or C<update_current_row>.
The C<update_current_row> is only evaluated if the table has the
C<inplace_update> capability.

This capability is evaluated automatically on first request and must not
be handled by derived classes.

=item inplace_update

Defines if an update of a row has side effects (capability is not
available) or can be done without harming any other currently running
task on the table.

Example: The table storage is using a hash on the C<PRIMARY KEY> of
the table. Real perl hashes do not care when an item is updated while
the hash is traversed using C<each>. C<SDBM_File> 1.06 has a bug,
which does not adjust the traversal pointer when an item is deleted.

C<SQL::Statement::RAM::Table> recognizes such situations and adjusts
the traversal pointer.

This might not be possible for all implementations which can update
single rows.

This capability could be provided by a derived class only.

=item delete_one_row

Defines whether the table can delete one single row by it's content or
not.

This capability is evaluated automatically on first request and must not
be handled by derived classes.

=item delete_current_row

Defines whether a table can delete the current traversed row or
not. This capability requires the C<inplace_delete> capability.

This capability is evaluated automatically on first request and must not
be handled by derived classes.

=item rowwise_delete

Defines if any row-wise delete operation is provided by the
table. C<row-wise> delete capabilities are C<delete_one_row> and
C<delete_current_row>.

This capability is evaluated automatically on first request and must not
be handled by derived classes.

=item inplace_delete

Defines if the deletion of a row has side effects (capability is not
available) or can be done without harming any other currently running
task on the table.

This capability should be provided by a derived class only.

=item insert_new_row

Defines if a table can easily insert a new row without need to seek
or truncate. This capability is provided by defining the table class
method C<insert_new_row>.

This capability is evaluated automatically on first request and must not
be handled by derived classes.

=back

If the capabilities I<rowwise_update> and I<insert_new_row> are
provided, the table primitive C<push_row> is not required anymore and
may be omitted.

=back

The above methods are implemented by SQL::Eval::Table. The following
methods are not, so that they *must* be implemented by the
subclass. See the C<DBD::DBM::Table> or C<DBD::CSV::Table> for
example.

=over 8

=item drop

Drops the table. All resources allocated by the table must be released
after C<$table->drop($data)>.

=item fetch_row

Fetches the next row from the table. Returns C<undef>, if the last
row was already fetched. The argument $data is for private use of
the subclass. Example:

    $row = $table->fetch_row($data);

Note, that you may use

    $row = $table->row();

for retrieving the same row again, until the next call of C<fetch_row>.

C<SQL::Statement> requires that the last fetched row is available again
and again via C<$table->row()>.

=item push_row

As fetch_row except for storing rows. Example:

    $table->push_row($data, $row);

=item push_names

Used by the I<CREATE TABLE> statement to set the column names of the
new table. Receives an array ref of names. Example:

    $table->push_names($data, $names);

=item seek

Similar to the seek method of a filehandle; used for setting the number
of the next row being written. Example:

    $table->seek($data, $whence, $rowNum);

Actually the current implementation only uses C<seek($data, 0, 0)>
(first row) and C<seek($data, 2, 0)> (beyond last row, end of file).

=item truncate

Truncates a table after the current row. Example:

    $table->truncate($data);

=back


=head1 INTERNALS

The current implementation is quite simple: An SQL::Eval object is an
hash ref with only two attributes. The C<params> attribute is an array
ref of parameters. The C<tables> attribute is an hash ref of table
names (keys) and table objects (values).

SQL::Eval::Table instances are implemented as hash refs. Attributes
used are C<row> (the array ref of the current row), C<col_nums> (an
hash ref of column names as keys and column numbers as values) and
C<col_names>, an array ref of column names with the column numbers as
indexes.

=head1 MULTITHREADING

All methods are working with instance-local data only, thus the module
is reentrant and thread safe, if you either don't share handles between
threads or grant serialized use.

=head1 BUGS

Please report any bugs or feature requests to C<bug-sql-statement at
rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=SQL-Statement>.  I
will be notified, and then you will automatically be notified of
progress on your bug as I make changes.

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc SQL::Eval
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

Written by Jochen Wiedmann and currently maintained by Jens Rehsack.

This module is Copyright (C) 1998 by

    Jochen Wiedmann
    Am Eisteich 9
    72555 Metzingen
    Germany

    Email: joe@ispsoft.de
    Phone: +49 7123 14887

and Copyright (C) 2009, 2017 by

     Jens Rehsack < rehsackATcpan.org>

All rights reserved.

You may distribute this module under the terms of either the GNU
General Public License or the Artistic License, as specified in
the Perl README file.


=head1 SEE ALSO

L<SQL::Statement(3)>


=cut
