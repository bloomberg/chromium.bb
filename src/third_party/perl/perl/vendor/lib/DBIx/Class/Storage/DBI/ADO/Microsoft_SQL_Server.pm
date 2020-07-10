package DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server;

use strict;
use warnings;

use base qw/
  DBIx::Class::Storage::DBI::ADO
  DBIx::Class::Storage::DBI::MSSQL
/;
use mro 'c3';
use DBIx::Class::Carp;
use DBIx::Class::Storage::DBI::ADO::CursorUtils qw/_normalize_guids _strip_trailing_binary_nulls/;
use namespace::clean;

__PACKAGE__->cursor_class(
  'DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server::Cursor'
);

__PACKAGE__->datetime_parser_type (
  'DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server::DateTime::Format'
);

__PACKAGE__->new_guid(sub {
    my $self = shift;
    my $guid = $self->_get_dbh->selectrow_array('SELECT NEWID()');
    $guid =~ s/\A \{ (.+) \} \z/$1/xs;
    return $guid;
});

=head1 NAME

DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server - Support for Microsoft
SQL Server via DBD::ADO

=head1 SYNOPSIS

This subclass supports MSSQL server connections via L<DBD::ADO>.

=head1 DESCRIPTION

The MSSQL specific functionality is provided by
L<DBIx::Class::Storage::DBI::MSSQL>.

=head1 EXAMPLE DSN

  dbi:ADO:provider=sqlncli10;server=EEEBOX\SQLEXPRESS

=head1 CAVEATS

=head2 identities

C<_identity_method> is set to C<@@identity>, as C<SCOPE_IDENTITY()> doesn't work
with L<DBD::ADO>. See L<DBIx::Class::Storage::DBI::MSSQL/IMPLEMENTATION NOTES>
for caveats regarding this.

=head2 truncation bug

There is a bug with MSSQL ADO providers where data gets truncated based on the
size of the bind sizes in the first prepare call:

L<https://rt.cpan.org/Ticket/Display.html?id=52048>

The C<ado_size> workaround is used (see L<DBD::ADO/ADO providers>) with the
approximate maximum size of the data_type of the bound column, or 8000 (maximum
VARCHAR size) if the data_type is not available.

Please report problems with this driver and send patches.

=head2 LongReadLen

C<LongReadLen> is set to C<LongReadLen * 2 + 1> on connection as it is necessary
for some LOB types. Be aware of this if you localize this value on the C<$dbh>
directly.

=head2 binary data

Due perhaps to the ado_size workaround we use, and/or other reasons, binary data
such as C<varbinary> column data comes back padded with trailing C<NULL> chars.
The Cursor class for this driver
(L<DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server::Cursor>) removes them,
of course if your binary data is actually C<NULL> padded that may be an issue to
keep in mind when using this driver.

=head2 uniqueidentifier columns

uniqueidentifier columns come back from ADO wrapped in braces and must be
submitted to the MSSQL ADO driver wrapped in braces. We take care of this
transparently in this driver and the associated Cursor class
(L<DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server::Cursor>) so that you
don't have to use braces in most cases (except in literal SQL, in those cases
you will have to add the braces yourself.)

=head2 fractional seconds

Fractional seconds with L<DBIx::Class::InflateColumn::DateTime> are not
currently supported, datetimes are truncated at the second.

=cut

sub _init {
  my $self = shift;

# SCOPE_IDENTITY() doesn't work
  $self->_identity_method('@@identity');
  $self->_no_scope_identity_query(1);

  return $self->next::method(@_);
}

sub _run_connection_actions {
  my $self = shift;

# make transactions work
  require DBD::ADO::Const;
  $self->_dbh->{ado_conn}{CursorLocation} =
    DBD::ADO::Const->Enums->{CursorLocationEnum}{adUseClient};

# set LongReadLen = LongReadLen * 2 + 1
# this may need to be in ADO.pm, being conservative for now...
  my $long_read_len = $self->_dbh->{LongReadLen};

# This is the DBD::ADO default.
  if ($long_read_len != 2147483647) {
    $self->_dbh->{LongReadLen} = $long_read_len * 2 + 1;
  }

  return $self->next::method(@_);
}


# Fix up binary data and GUIDs for ->find, for cursors see the cursor_class
# above.
sub select_single {
  my $self = shift;
  my ($ident, $select) = @_;

  my @row = $self->next::method(@_);

  return @row unless $self->cursor_class->isa(
    'DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server::Cursor'
  );

  my $col_infos = $self->_resolve_column_info($ident);

  _normalize_guids($select, $col_infos, \@row, $self);

  _strip_trailing_binary_nulls($select, $col_infos, \@row, $self);

  return @row;
}

# We need to catch VARCHAR(max) before bind_attribute_by_data_type because it
# could be specified by size, also if bind_attribute_by_data_type fails we want
# to specify the default ado_size of 8000.
# Also make sure GUID binds have braces on them or else ADO throws an "Invalid
# character value for cast specification"

sub _dbi_attrs_for_bind {
  my $self = shift;
  my ($ident, $bind) = @_;

  my $lob_max = $self->_get_dbh->{LongReadLen} || 32768;

  foreach my $bind (@$bind) {
    my $attrs     = $bind->[0];
    my $data_type = $attrs->{sqlt_datatype};
    my $size      = $attrs->{sqlt_size};

    if ($size && lc($size) eq 'max') {
      if ($data_type =~ /^(?:varchar|character varying|nvarchar|national char varying|national character varying|varbinary)\z/i) {
        $attrs->{dbd_attrs} = { ado_size => $lob_max };
      }
      else {
        carp_unique "bizarre data_type '$data_type' with size => 'max'";
      }
    }

    if ($self->_is_guid_type($data_type) && substr($bind->[1], 0, 1) ne '{') {
      $bind->[1] = '{' . $bind->[1] . '}';
    }
  }

  my $attrs = $self->next::method(@_);

  foreach my $attr (@$attrs) {
    $attr->{ado_size} ||= 8000 if $attr;
  }

  return $attrs;
}

# Can't edit all the binds in _dbi_attrs_for_bind for _insert_bulk, so we take
# care of those GUIDs here.
sub _insert_bulk {
  my $self = shift;
  my ($source, $cols, $data) = @_;

  my $columns_info = $source->columns_info($cols);

  my $col_idx = 0;
  foreach my $col (@$cols) {
    if ($self->_is_guid_type($columns_info->{$col}{data_type})) {
      foreach my $data_row (@$data) {
        if (substr($data_row->[$col_idx], 0, 1) ne '{') {
          $data_row->[$col_idx] = '{' . $data_row->[$col_idx] . '}';
        }
      }
    }
    $col_idx++;
  }

  return $self->next::method(@_);
}

sub bind_attribute_by_data_type {
  my ($self, $data_type) = @_;

  $data_type = lc $data_type;

  my $max_size =
    $self->_mssql_max_data_type_representation_size_in_bytes->{$data_type};

  my $res = {};

  if ($max_size) {
    $res->{ado_size} = $max_size;
  }
  else {
    carp_unique "could not map data_type '$data_type' to a max size for ado_size: defaulting to 8000";
  }

  return $res;
}

# FIXME This list is an abomination. We need a way to do this outside
# of the scope of DBIC, as it is right now nobody will ever think to
# even look here to diagnose some sort of misbehavior.
sub _mssql_max_data_type_representation_size_in_bytes {
  my $self = shift;

  my $lob_max = $self->_get_dbh->{LongReadLen} || 32768;

  return +{
# MSSQL types
    char => 8000,
    character => 8000,
    varchar => 8000,
    'varchar(max)' => $lob_max,
    'character varying' => 8000,
    binary => 8000,
    varbinary => 8000,
    'varbinary(max)' => $lob_max,
    nchar => 16000,
    'national character' => 16000,
    'national char' => 16000,
    nvarchar => 16000,
    'nvarchar(max)' => ($lob_max*2),
    'national character varying' => 16000,
    'national char varying' => 16000,
    numeric => 100,
    smallint => 100,
    tinyint => 100,
    smallmoney => 100,
    bigint => 100,
    bit => 100,
    decimal => 100,
    dec => 100,
    integer => 100,
    int => 100,
    'int identity' => 100,
    'integer identity' => 100,
    money => 100,
    float => 100,
    double => 100,
    'double precision' => 100,
    real => 100,
    uniqueidentifier => 100,
    ntext => $lob_max,
    text => $lob_max,
    image => $lob_max,
    date => 100,
    datetime => 100,
    datetime2 => 100,
    datetimeoffset => 100,
    smalldatetime => 100,
    time => 100,
    timestamp => 100,
    cursor => 100,
    hierarchyid => 100,
    rowversion => 100,
    sql_variant => 100,
    table => $lob_max,
    xml => $lob_max,

# mysql types
    bool => 100,
    boolean => 100,
    'tinyint unsigned' => 100,
    'smallint unsigned' => 100,
    'mediumint unsigned' => 100,
    'int unsigned' => 100,
    'integer unsigned' => 100,
    'bigint unsigned' => 100,
    'float unsigned' => 100,
    'double unsigned' => 100,
    'double precision unsigned' => 100,
    'decimal unsigned' => 100,
    'fixed' => 100,
    'year' => 100,
    tinyblob => $lob_max,
    tinytext => $lob_max,
    blob => $lob_max,
    text => $lob_max,
    mediumblob => $lob_max,
    mediumtext => $lob_max,
    longblob => $lob_max,
    longtext => $lob_max,
    enum => 100,
    set => 8000,

# Pg types
    serial => 100,
    bigserial => 100,
    int8 => 100,
    integer8 => 100,
    serial8 => 100,
    int4 => 100,
    integer4 => 100,
    serial4 => 100,
    int2 => 100,
    integer2 => 100,
    float8 => 100,
    float4 => 100,
    'bit varying' => 8000,
    'varbit' => 8000,
    inet => 100,
    cidr => 100,
    macaddr => 100,
    'time without time zone' => 100,
    'time with time zone' => 100,
    'timestamp without time zone' => 100,
    'timestamp with time zone' => 100,
    bytea => $lob_max,

# DB2 types
    graphic => 8000,
    vargraphic => 8000,
    'long vargraphic' => $lob_max,
    dbclob => $lob_max,
    clob => $lob_max,
    'char for bit data' => 8000,
    'varchar for bit data' => 8000,
    'long varchar for bit data' => $lob_max,

# oracle types
    varchar2 => 8000,
    binary_float => 100,
    binary_double => 100,
    raw => 8000,
    nclob => $lob_max,
    long => $lob_max,
    'long raw' => $lob_max,
    'timestamp with local time zone' => 100,

# Sybase ASE types
    unitext => $lob_max,
    unichar => 16000,
    univarchar => 16000,

# SQL Anywhere types
    'long varbit' => $lob_max,
    'long bit varying' => $lob_max,
    uniqueidentifierstr => 100,
    'long binary' => $lob_max,
    'long varchar' => $lob_max,
    'long nvarchar' => $lob_max,

# Firebird types
    'char(x) character set unicode_fss' => 16000,
    'varchar(x) character set unicode_fss' => 16000,
    'blob sub_type text' => $lob_max,
    'blob sub_type text character set unicode_fss' => $lob_max,

# Informix types
    smallfloat => 100,
    byte => $lob_max,
    lvarchar => 8000,
    'datetime year to fraction(5)' => 100,
    # FIXME add other datetime types

# MS Access types
    autoincrement => 100,
    long => 100,
    integer4 => 100,
    integer2 => 100,
    integer1 => 100,
    logical => 100,
    logical1 => 100,
    yesno => 100,
    currency => 100,
    single => 100,
    ieeesingle => 100,
    ieeedouble => 100,
    number => 100,
    string => 8000,
    guid => 100,
    longchar => $lob_max,
    memo => $lob_max,
    longbinary => $lob_max,
  }
}

package # hide from PAUSE
  DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server::DateTime::Format;

my $datetime_format = '%m/%d/%Y %I:%M:%S %p';
my $datetime_parser;

sub parse_datetime {
  shift;
  require DateTime::Format::Strptime;
  $datetime_parser ||= DateTime::Format::Strptime->new(
    pattern  => $datetime_format,
    on_error => 'croak',
  );
  return $datetime_parser->parse_datetime(shift);
}

sub format_datetime {
  shift;
  require DateTime::Format::Strptime;
  $datetime_parser ||= DateTime::Format::Strptime->new(
    pattern  => $datetime_format,
    on_error => 'croak',
  );
  return $datetime_parser->format_datetime(shift);
}

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;

# vim:sts=2 sw=2:
