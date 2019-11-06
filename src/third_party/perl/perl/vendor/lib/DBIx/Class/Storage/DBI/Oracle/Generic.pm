package DBIx::Class::Storage::DBI::Oracle::Generic;

use strict;
use warnings;
use base qw/DBIx::Class::Storage::DBI/;
use mro 'c3';
use DBIx::Class::Carp;
use Scope::Guard ();
use Context::Preserve 'preserve_context';
use Try::Tiny;
use List::Util 'first';
use namespace::clean;

__PACKAGE__->sql_limit_dialect ('RowNum');
__PACKAGE__->sql_quote_char ('"');
__PACKAGE__->sql_maker_class('DBIx::Class::SQLMaker::Oracle');
__PACKAGE__->datetime_parser_type('DateTime::Format::Oracle');

sub __cache_queries_with_max_lob_parts { 2 }

=head1 NAME

DBIx::Class::Storage::DBI::Oracle::Generic - Oracle Support for DBIx::Class

=head1 SYNOPSIS

  # In your result (table) classes
  use base 'DBIx::Class::Core';
  __PACKAGE__->add_columns({ id => { sequence => 'mysequence', auto_nextval => 1 } });
  __PACKAGE__->set_primary_key('id');

  # Somewhere in your Code
  # add some data to a table with a hierarchical relationship
  $schema->resultset('Person')->create ({
        firstname => 'foo',
        lastname => 'bar',
        children => [
            {
                firstname => 'child1',
                lastname => 'bar',
                children => [
                    {
                        firstname => 'grandchild',
                        lastname => 'bar',
                    }
                ],
            },
            {
                firstname => 'child2',
                lastname => 'bar',
            },
        ],
    });

  # select from the hierarchical relationship
  my $rs = $schema->resultset('Person')->search({},
    {
      'start_with' => { 'firstname' => 'foo', 'lastname' => 'bar' },
      'connect_by' => { 'parentid' => { '-prior' => { -ident => 'personid' } },
      'order_siblings_by' => { -asc => 'name' },
    };
  );

  # this will select the whole tree starting from person "foo bar", creating
  # following query:
  # SELECT
  #     me.persionid me.firstname, me.lastname, me.parentid
  # FROM
  #     person me
  # START WITH
  #     firstname = 'foo' and lastname = 'bar'
  # CONNECT BY
  #     parentid = prior personid
  # ORDER SIBLINGS BY
  #     firstname ASC

=head1 DESCRIPTION

This class implements base Oracle support. The subclass
L<DBIx::Class::Storage::DBI::Oracle::WhereJoins> is for C<(+)> joins in Oracle
versions before 9.0.

=head1 METHODS

=cut

sub _determine_supports_insert_returning {
  my $self = shift;

# TODO find out which version supports the RETURNING syntax
# 8i has it and earlier docs are a 404 on oracle.com

  return 1
    if $self->_server_info->{normalized_dbms_version} >= 8.001;

  return 0;
}

__PACKAGE__->_use_insert_returning_bound (1);

sub deployment_statements {
  my $self = shift;;
  my ($schema, $type, $version, $dir, $sqltargs, @rest) = @_;

  $sqltargs ||= {};

  if (
    ! exists $sqltargs->{producer_args}{oracle_version}
      and
    my $dver = $self->_server_info->{dbms_version}
  ) {
    $sqltargs->{producer_args}{oracle_version} = $dver;
  }

  $self->next::method($schema, $type, $version, $dir, $sqltargs, @rest);
}

sub _dbh_last_insert_id {
  my ($self, $dbh, $source, @columns) = @_;
  my @ids = ();
  foreach my $col (@columns) {
    my $seq = ($source->column_info($col)->{sequence} ||= $self->get_autoinc_seq($source,$col));
    my $id = $self->_sequence_fetch( 'CURRVAL', $seq );
    push @ids, $id;
  }
  return @ids;
}

sub _dbh_get_autoinc_seq {
  my ($self, $dbh, $source, $col) = @_;

  my $sql_maker = $self->sql_maker;
  my ($ql, $qr) = map { $_ ? (quotemeta $_) : '' } $sql_maker->_quote_chars;

  my $source_name;
  if ( ref $source->name eq 'SCALAR' ) {
    $source_name = ${$source->name};

    # the ALL_TRIGGERS match further on is case sensitive - thus uppercase
    # stuff unless it is already quoted
    $source_name = uc ($source_name) if $source_name !~ /\"/;
  }
  else {
    $source_name = $source->name;
    $source_name = uc($source_name) unless $ql;
  }

  # trigger_body is a LONG
  local $dbh->{LongReadLen} = 64 * 1024 if ($dbh->{LongReadLen} < 64 * 1024);

  # disable default bindtype
  local $sql_maker->{bindtype} = 'normal';

  # look up the correct sequence automatically
  my ( $schema, $table ) = $source_name =~ /( (?:${ql})? \w+ (?:${qr})? ) \. ( (?:${ql})? \w+ (?:${qr})? )/x;

  # if no explicit schema was requested - use the default schema (which in the case of Oracle is the db user)
  $schema ||= \'= USER';

  my ($sql, @bind) = $sql_maker->select (
    'ALL_TRIGGERS',
    [qw/TRIGGER_BODY TABLE_OWNER TRIGGER_NAME/],
    {
      OWNER => $schema,
      TABLE_NAME => $table || $source_name,
      TRIGGERING_EVENT => { -like => '%INSERT%' },  # this will also catch insert_or_update
      TRIGGER_TYPE => { -like => '%BEFORE%' },      # we care only about 'before' triggers
      STATUS => 'ENABLED',
     },
  );

  # to find all the triggers that mention the column in question a simple
  # regex grep since the trigger_body above is a LONG and hence not searchable
  # via -like
  my @triggers = ( map
    { my %inf; @inf{qw/body schema name/} = @$_; \%inf }
    ( grep
      { $_->[0] =~ /\:new\.${ql}${col}${qr} | \:new\.$col/xi }
      @{ $dbh->selectall_arrayref( $sql, {}, @bind ) }
    )
  );

  # extract all sequence names mentioned in each trigger, throw away
  # triggers without apparent sequences
  @triggers = map {
    my @seqs = $_->{body} =~ / ( [\.\w\"\-]+ ) \. nextval /xig;
    @seqs
      ? { %$_, sequences => \@seqs }
      : ()
    ;
  } @triggers;

  my $chosen_trigger;

  # if only one trigger matched things are easy
  if (@triggers == 1) {

    if ( @{$triggers[0]{sequences}} == 1 ) {
      $chosen_trigger = $triggers[0];
    }
    else {
      $self->throw_exception( sprintf (
        "Unable to introspect trigger '%s' for column '%s.%s' (references multiple sequences). "
      . "You need to specify the correct 'sequence' explicitly in '%s's column_info.",
        $triggers[0]{name},
        $source_name,
        $col,
        $col,
      ) );
    }
  }
  # got more than one matching trigger - see if we can narrow it down
  elsif (@triggers > 1) {

    my @candidates = grep
      { $_->{body} =~ / into \s+ \:new\.$col /xi }
      @triggers
    ;

    if (@candidates == 1 && @{$candidates[0]{sequences}} == 1) {
      $chosen_trigger = $candidates[0];
    }
    else {
      $self->throw_exception( sprintf (
        "Unable to reliably select a BEFORE INSERT trigger for column '%s.%s' (possibilities: %s). "
      . "You need to specify the correct 'sequence' explicitly in '%s's column_info.",
        $source_name,
        $col,
        ( join ', ', map { "'$_->{name}'" } @triggers ),
        $col,
      ) );
    }
  }

  if ($chosen_trigger) {
    my $seq_name = $chosen_trigger->{sequences}[0];

    $seq_name = "$chosen_trigger->{schema}.$seq_name"
      unless $seq_name =~ /\./;

    return \$seq_name if $seq_name =~ /\"/; # may already be quoted in-trigger
    return $seq_name;
  }

  $self->throw_exception( sprintf (
    "No suitable BEFORE INSERT triggers found for column '%s.%s'. "
  . "You need to specify the correct 'sequence' explicitly in '%s's column_info.",
    $source_name,
    $col,
    $col,
  ));
}

sub _sequence_fetch {
  my ( $self, $type, $seq ) = @_;

  # use the maker to leverage quoting settings
  my $sth = $self->_dbh->prepare_cached(
    $self->sql_maker->select('DUAL', [ ref $seq ? \"$$seq.$type" : "$seq.$type" ] )
  );
  $sth->execute;
  my ($id) = $sth->fetchrow_array;
  $sth->finish;
  return $id;
}

sub _ping {
  my $self = shift;

  my $dbh = $self->_dbh or return 0;

  local $dbh->{RaiseError} = 1;
  local $dbh->{PrintError} = 0;

  return try {
    $dbh->do('select 1 from dual');
    1;
  } catch {
    0;
  };
}

sub _dbh_execute {
  #my ($self, $dbh, $sql, $bind, $bind_attrs) = @_;
  my ($self, $sql, $bind) = @_[0,2,3];

  # Turn off sth caching for multi-part LOBs. See _prep_for_execute below
  local $self->{disable_sth_caching} = 1 if first {
    ($_->[0]{_ora_lob_autosplit_part}||0)
      >
    (__cache_queries_with_max_lob_parts - 1)
  } @$bind;

  my $next = $self->next::can;

  # if we are already in a txn we can't retry anything
  return shift->$next(@_)
    if $self->transaction_depth;

  # cheat the blockrunner we are just about to create
  # we do want to rerun things regardless of outer state
  local $self->{_in_do_block};

  return DBIx::Class::Storage::BlockRunner->new(
    storage => $self,
    wrap_txn => 0,
    retry_handler => sub {
      # ORA-01003: no statement parsed (someone changed the table somehow,
      # invalidating your cursor.)
      if (
        $_[0]->failed_attempt_count == 1
          and
        $_[0]->last_exception =~ /ORA-01003/
          and
        my $dbh = $_[0]->storage->_dbh
      ) {
        delete $dbh->{CachedKids}{$sql};
        return 1;
      }
      else {
        return 0;
      }
    },
  )->run( $next, @_ );
}

sub _dbh_execute_for_fetch {
  #my ($self, $sth, $tuple_status, @extra) = @_;

  # DBD::Oracle warns loudly on partial execute_for_fetch failures
  local $_[1]->{PrintWarn} = 0;

  shift->next::method(@_);
}

=head2 get_autoinc_seq

Returns the sequence name for an autoincrement column

=cut

sub get_autoinc_seq {
  my ($self, $source, $col) = @_;

  $self->dbh_do('_dbh_get_autoinc_seq', $source, $col);
}

=head2 datetime_parser_type

This sets the proper DateTime::Format module for use with
L<DBIx::Class::InflateColumn::DateTime>.

=head2 connect_call_datetime_setup

Used as:

    on_connect_call => 'datetime_setup'

In L<connect_info|DBIx::Class::Storage::DBI/connect_info> to set the session nls
date, and timestamp values for use with L<DBIx::Class::InflateColumn::DateTime>
and the necessary environment variables for L<DateTime::Format::Oracle>, which
is used by it.

Maximum allowable precision is used, unless the environment variables have
already been set.

These are the defaults used:

  $ENV{NLS_DATE_FORMAT}         ||= 'YYYY-MM-DD HH24:MI:SS';
  $ENV{NLS_TIMESTAMP_FORMAT}    ||= 'YYYY-MM-DD HH24:MI:SS.FF';
  $ENV{NLS_TIMESTAMP_TZ_FORMAT} ||= 'YYYY-MM-DD HH24:MI:SS.FF TZHTZM';

To get more than second precision with L<DBIx::Class::InflateColumn::DateTime>
for your timestamps, use something like this:

  use Time::HiRes 'time';
  my $ts = DateTime->from_epoch(epoch => time);

=cut

sub connect_call_datetime_setup {
  my $self = shift;

  my $date_format = $ENV{NLS_DATE_FORMAT} ||= 'YYYY-MM-DD HH24:MI:SS';
  my $timestamp_format = $ENV{NLS_TIMESTAMP_FORMAT} ||=
    'YYYY-MM-DD HH24:MI:SS.FF';
  my $timestamp_tz_format = $ENV{NLS_TIMESTAMP_TZ_FORMAT} ||=
    'YYYY-MM-DD HH24:MI:SS.FF TZHTZM';

  $self->_do_query(
    "alter session set nls_date_format = '$date_format'"
  );
  $self->_do_query(
    "alter session set nls_timestamp_format = '$timestamp_format'"
  );
  $self->_do_query(
    "alter session set nls_timestamp_tz_format='$timestamp_tz_format'"
  );
}

### Note originally by Ron "Quinn" Straight <quinnfazigu@gmail.org>
### https://github.com/Perl5/DBIx-Class/commit/5db2758de6
#
# Handle LOB types in Oracle.  Under a certain size (4k?), you can get away
# with the driver assuming your input is the deprecated LONG type if you
# encode it as a hex string.  That ain't gonna fly at larger values, where
# you'll discover you have to do what this does.
#
# This method had to be overridden because we need to set ora_field to the
# actual column, and that isn't passed to the call (provided by Storage) to
# bind_attribute_by_data_type.
#
# According to L<DBD::Oracle>, the ora_field isn't always necessary, but
# adding it doesn't hurt, and will save your bacon if you're modifying a
# table with more than one LOB column.
#
sub _dbi_attrs_for_bind {
  my ($self, $ident, $bind) = @_;

  my $attrs = $self->next::method($ident, $bind);

  # Push the column name into all bind attrs, make sure to *NOT* write into
  # the existing $attrs->[$idx]{..} hashref, as it is cached by the call to
  # next::method above.
  $attrs->[$_]
    and
  keys %{ $attrs->[$_] }
    and
  $bind->[$_][0]{dbic_colname}
    and
  $attrs->[$_] = { %{$attrs->[$_]}, ora_field => $bind->[$_][0]{dbic_colname} }
    for 0 .. $#$attrs;

  $attrs;
}

sub bind_attribute_by_data_type {
  my ($self, $dt) = @_;

  if ($self->_is_lob_type($dt)) {

    # this is a hot-ish codepath, store an escape-flag in the DBD namespace, so that
    # things like Class::Unload work (unlikely but possible)
    unless ($DBD::Oracle::__DBIC_DBD_VERSION_CHECK_OK__) {

      # no earlier - no later
      if ($DBD::Oracle::VERSION eq '1.23') {
        $self->throw_exception(
          "BLOB/CLOB support in DBD::Oracle == 1.23 is broken, use an earlier or later ".
          "version (https://rt.cpan.org/Public/Bug/Display.html?id=46016)"
        );
      }

      $DBD::Oracle::__DBIC_DBD_VERSION_CHECK_OK__ = 1;
    }

    return {
      ora_type => $self->_is_text_lob_type($dt)
        ? DBD::Oracle::ORA_CLOB()
        : DBD::Oracle::ORA_BLOB()
    };
  }
  else {
    return undef;
  }
}

# Handle blob columns in WHERE.
#
# For equality comparisons:
#
# We split data intended for comparing to a LOB into 2000 character chunks and
# compare them using dbms_lob.substr on the LOB column.
#
# We turn off DBD::Oracle LOB binds for these partial LOB comparisons by passing
# dbd_attrs => undef, because these are regular varchar2 comparisons and
# otherwise the query will fail.
#
# Since the most common comparison size is likely to be under 4000 characters
# (TEXT comparisons previously deployed to other RDBMSes) we disable
# prepare_cached for queries with more than two part comparisons to a LOB
# column. This is done in _dbh_execute (above) which was previously overridden
# to gracefully recover from an Oracle error. This is to be careful to not
# exhaust your application's open cursor limit.
#
# See:
# http://itcareershift.com/blog1/2011/02/21/oracle-max-number-of-open-cursors-complete-reference-for-the-new-oracle-dba/
# on the open_cursor limit.
#
# For everything else:
#
# We assume that everything that is not a LOB comparison, will most likely be a
# LIKE query or some sort of function invocation. This may prove to be a naive
# assumption in the future, but for now it should cover the two most likely
# things users would want to do with a BLOB or CLOB, an equality test or a LIKE
# query (on a CLOB.)
#
# For these expressions, the bind must NOT have the attributes of a LOB bind for
# DBD::Oracle, otherwise the query will fail. This is done by passing
# dbd_attrs => undef.

sub _prep_for_execute {
  my $self = shift;
  my ($op) = @_;

  return $self->next::method(@_)
    if $op eq 'insert';

  my ($sql, $bind) = $self->next::method(@_);

  my $lob_bind_indices = { map {
    (
      $bind->[$_][0]{sqlt_datatype}
        and
      $self->_is_lob_type($bind->[$_][0]{sqlt_datatype})
    ) ? ( $_ => 1 ) : ()
  } ( 0 .. $#$bind ) };

  return ($sql, $bind) unless %$lob_bind_indices;

  my ($final_sql, @final_binds);
  if ($op eq 'update') {
    $self->throw_exception('Update with complex WHERE clauses involving BLOB columns currently not supported')
      if $sql =~ /\bWHERE\b .+ \bWHERE\b/xs;

    my $where_sql;
    ($final_sql, $where_sql) = $sql =~ /^ (.+?) ( \bWHERE\b .+) /xs;

    if (my $set_bind_count = $final_sql =~ y/?//) {

      delete $lob_bind_indices->{$_} for (0 .. ($set_bind_count - 1));

      # bail if only the update part contains blobs
      return ($sql, $bind) unless %$lob_bind_indices;

      @final_binds = splice @$bind, 0, $set_bind_count;
      $lob_bind_indices = { map
        { $_ - $set_bind_count => $lob_bind_indices->{$_} }
        keys %$lob_bind_indices
      };
    }

    # if we got that far - assume the where SQL is all we got
    # (the first part is already shoved into $final_sql)
    $sql = $where_sql;
  }
  elsif ($op ne 'select' and $op ne 'delete') {
    $self->throw_exception("Unsupported \$op: $op");
  }

  my @sql_parts = split /\?/, $sql;

  my $col_equality_re = qr/ (?<=\s) ([\w."]+) (\s*=\s*) $/x;

  for my $b_idx (0 .. $#$bind) {
    my $bound = $bind->[$b_idx];

    if (
      $lob_bind_indices->{$b_idx}
        and
      my ($col, $eq) = $sql_parts[0] =~ $col_equality_re
    ) {
      my $data = $bound->[1];

      $data = "$data" if ref $data;

      my @parts = unpack '(a2000)*', $data;

      my @sql_frag;

      for my $idx (0..$#parts) {
        push @sql_frag, sprintf (
          'UTL_RAW.CAST_TO_VARCHAR2(RAWTOHEX(DBMS_LOB.SUBSTR(%s, 2000, %d))) = ?',
          $col, ($idx*2000 + 1),
        );
      }

      my $sql_frag = '( ' . (join ' AND ', @sql_frag) . ' )';

      $sql_parts[0] =~ s/$col_equality_re/$sql_frag/;

      $final_sql .= shift @sql_parts;

      for my $idx (0..$#parts) {
        push @final_binds, [
          {
            %{ $bound->[0] },
            _ora_lob_autosplit_part => $idx,
            dbd_attrs => undef,
          },
          $parts[$idx]
        ];
      }
    }
    else {
      $final_sql .= shift(@sql_parts) . '?';
      push @final_binds, $lob_bind_indices->{$b_idx}
        ? [
          {
            %{ $bound->[0] },
            dbd_attrs => undef,
          },
          $bound->[1],
        ] : $bound
      ;
    }
  }

  if (@sql_parts > 1) {
    carp "There are more placeholders than binds, this should not happen!";
    @sql_parts = join ('?', @sql_parts);
  }

  $final_sql .= $sql_parts[0];

  return ($final_sql, \@final_binds);
}

# Savepoints stuff.

sub _exec_svp_begin {
  my ($self, $name) = @_;
  $self->_dbh->do("SAVEPOINT $name");
}

# Oracle automatically releases a savepoint when you start another one with the
# same name.
sub _exec_svp_release { 1 }

sub _exec_svp_rollback {
  my ($self, $name) = @_;
  $self->_dbh->do("ROLLBACK TO SAVEPOINT $name")
}

=head2 relname_to_table_alias

L<DBIx::Class> uses L<DBIx::Class::Relationship> names as table aliases in
queries.

Unfortunately, Oracle doesn't support identifiers over 30 chars in length, so
the L<DBIx::Class::Relationship> name is shortened and appended with half of an
MD5 hash.

See L<DBIx::Class::Storage::DBI/relname_to_table_alias>.

=cut

sub relname_to_table_alias {
  my $self = shift;
  my ($relname, $join_count) = @_;

  my $alias = $self->next::method(@_);

  # we need to shorten here in addition to the shortening in SQLA itself,
  # since the final relnames are crucial for the join optimizer
  return $self->sql_maker->_shorten_identifier($alias);
}

=head2 with_deferred_fk_checks

Runs a coderef between:

  alter session set constraints = deferred
  ...
  alter session set constraints = immediate

to defer foreign key checks.

Constraints must be declared C<DEFERRABLE> for this to work.

=cut

sub with_deferred_fk_checks {
  my ($self, $sub) = @_;

  my $txn_scope_guard = $self->txn_scope_guard;

  $self->_do_query('alter session set constraints = deferred');

  my $sg = Scope::Guard->new(sub {
    $self->_do_query('alter session set constraints = immediate');
  });

  return
    preserve_context { $sub->() } after => sub { $txn_scope_guard->commit };
}

=head1 ATTRIBUTES

Following additional attributes can be used in resultsets.

=head2 connect_by or connect_by_nocycle

=over 4

=item Value: \%connect_by

=back

A hashref of conditions used to specify the relationship between parent rows
and child rows of the hierarchy.


  connect_by => { parentid => 'prior personid' }

  # adds a connect by statement to the query:
  # SELECT
  #     me.persionid me.firstname, me.lastname, me.parentid
  # FROM
  #     person me
  # CONNECT BY
  #     parentid = prior persionid


  connect_by_nocycle => { parentid => 'prior personid' }

  # adds a connect by statement to the query:
  # SELECT
  #     me.persionid me.firstname, me.lastname, me.parentid
  # FROM
  #     person me
  # CONNECT BY NOCYCLE
  #     parentid = prior persionid


=head2 start_with

=over 4

=item Value: \%condition

=back

A hashref of conditions which specify the root row(s) of the hierarchy.

It uses the same syntax as L<DBIx::Class::ResultSet/search>

  start_with => { firstname => 'Foo', lastname => 'Bar' }

  # SELECT
  #     me.persionid me.firstname, me.lastname, me.parentid
  # FROM
  #     person me
  # START WITH
  #     firstname = 'foo' and lastname = 'bar'
  # CONNECT BY
  #     parentid = prior persionid

=head2 order_siblings_by

=over 4

=item Value: ($order_siblings_by | \@order_siblings_by)

=back

Which column(s) to order the siblings by.

It uses the same syntax as L<DBIx::Class::ResultSet/order_by>

  'order_siblings_by' => 'firstname ASC'

  # SELECT
  #     me.persionid me.firstname, me.lastname, me.parentid
  # FROM
  #     person me
  # CONNECT BY
  #     parentid = prior persionid
  # ORDER SIBLINGS BY
  #     firstname ASC

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
