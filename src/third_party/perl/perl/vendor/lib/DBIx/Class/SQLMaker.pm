package DBIx::Class::SQLMaker;

use strict;
use warnings;

=head1 NAME

DBIx::Class::SQLMaker - An SQL::Abstract-based SQL maker class

=head1 DESCRIPTION

This module is a subclass of L<SQL::Abstract> and includes a number of
DBIC-specific workarounds, not yet suitable for inclusion into the
L<SQL::Abstract> core. It also provides all (and more than) the functionality
of L<SQL::Abstract::Limit>, see L<DBIx::Class::SQLMaker::LimitDialects> for
more info.

Currently the enhancements to L<SQL::Abstract> are:

=over

=item * Support for C<JOIN> statements (via extended C<table/from> support)

=item * Support of functions in C<SELECT> lists

=item * C<GROUP BY>/C<HAVING> support (via extensions to the order_by parameter)

=item * Support of C<...FOR UPDATE> type of select statement modifiers

=back

=cut

use base qw/
  DBIx::Class::SQLMaker::LimitDialects
  SQL::Abstract
  DBIx::Class
/;
use mro 'c3';

use Sub::Name 'subname';
use DBIx::Class::Carp;
use namespace::clean;

__PACKAGE__->mk_group_accessors (simple => qw/quote_char name_sep limit_dialect/);

sub _quoting_enabled {
  ( defined $_[0]->{quote_char} and length $_[0]->{quote_char} ) ? 1 : 0
}

# for when I need a normalized l/r pair
sub _quote_chars {

  # in case we are called in the old !!$sm->_quote_chars fashion
  return () if !wantarray and ( ! defined $_[0]->{quote_char} or ! length $_[0]->{quote_char} );

  map
    { defined $_ ? $_ : '' }
    ( ref $_[0]->{quote_char} ? (@{$_[0]->{quote_char}}) : ( ($_[0]->{quote_char}) x 2 ) )
  ;
}

# FIXME when we bring in the storage weaklink, check its schema
# weaklink and channel through $schema->throw_exception
sub throw_exception { DBIx::Class::Exception->throw($_[1]) }

BEGIN {
  # reinstall the belch()/puke() functions of SQL::Abstract with custom versions
  # that use DBIx::Class::Carp/DBIx::Class::Exception instead of plain Carp
  no warnings qw/redefine/;

  *SQL::Abstract::belch = subname 'SQL::Abstract::belch' => sub (@) {
    my($func) = (caller(1))[3];
    carp "[$func] Warning: ", @_;
  };

  *SQL::Abstract::puke = subname 'SQL::Abstract::puke' => sub (@) {
    my($func) = (caller(1))[3];
    __PACKAGE__->throw_exception("[$func] Fatal: " . join ('',  @_));
  };
}

# the "oh noes offset/top without limit" constant
# limited to 31 bits for sanity (and consistency,
# since it may be handed to the like of sprintf %u)
#
# Also *some* builds of SQLite fail the test
#   some_column BETWEEN ? AND ?: 1, 4294967295
# with the proper integer bind attrs
#
# Implemented as a method, since ::Storage::DBI also
# refers to it (i.e. for the case of software_limit or
# as the value to abuse with MSSQL ordered subqueries)
sub __max_int () { 0x7FFFFFFF };

# we ne longer need to check this - DBIC has ways of dealing with it
# specifically ::Storage::DBI::_resolve_bindattrs()
sub _assert_bindval_matches_bindtype () { 1 };

# poor man's de-qualifier
sub _quote {
  $_[0]->next::method( ( $_[0]{_dequalify_idents} and ! ref $_[1] )
    ? $_[1] =~ / ([^\.]+) $ /x
    : $_[1]
  );
}

sub _where_op_NEST {
  carp_unique ("-nest in search conditions is deprecated, you most probably wanted:\n"
      .q|{..., -and => [ \%cond0, \@cond1, \'cond2', \[ 'cond3', [ col => bind ] ], etc. ], ... }|
  );

  shift->next::method(@_);
}

# Handle limit-dialect selection
sub select {
  my ($self, $table, $fields, $where, $rs_attrs, $limit, $offset) = @_;


  ($fields, @{$self->{select_bind}}) = $self->_recurse_fields($fields);

  if (defined $offset) {
    $self->throw_exception('A supplied offset must be a non-negative integer')
      if ( $offset =~ /\D/ or $offset < 0 );
  }
  $offset ||= 0;

  if (defined $limit) {
    $self->throw_exception('A supplied limit must be a positive integer')
      if ( $limit =~ /\D/ or $limit <= 0 );
  }
  elsif ($offset) {
    $limit = $self->__max_int;
  }


  my ($sql, @bind);
  if ($limit) {
    # this is legacy code-flow from SQLA::Limit, it is not set in stone

    ($sql, @bind) = $self->next::method ($table, $fields, $where);

    my $limiter;

    if( $limiter = $self->can ('emulate_limit') ) {
      carp_unique(
        'Support for the legacy emulate_limit() mechanism inherited from '
      . 'SQL::Abstract::Limit has been deprecated, and will be removed when '
      . 'DBIC transitions to Data::Query. If your code uses this type of '
      . 'limit specification please file an RT and provide the source of '
      . 'your emulate_limit() implementation, so an acceptable upgrade-path '
      . 'can be devised'
      );
    }
    else {
      my $dialect = $self->limit_dialect
        or $self->throw_exception( "Unable to generate SQL-limit - no limit dialect specified on $self" );

      $limiter = $self->can ("_$dialect")
        or $self->throw_exception(__PACKAGE__ . " does not implement the requested dialect '$dialect'");
    }

    $sql = $self->$limiter (
      $sql,
      { %{$rs_attrs||{}}, _selector_sql => $fields },
      $limit,
      $offset
    );
  }
  else {
    ($sql, @bind) = $self->next::method ($table, $fields, $where, $rs_attrs);
  }

  push @{$self->{where_bind}}, @bind;

# this *must* be called, otherwise extra binds will remain in the sql-maker
  my @all_bind = $self->_assemble_binds;

  $sql .= $self->_lock_select ($rs_attrs->{for})
    if $rs_attrs->{for};

  return wantarray ? ($sql, @all_bind) : $sql;
}

sub _assemble_binds {
  my $self = shift;
  return map { @{ (delete $self->{"${_}_bind"}) || [] } } (qw/pre_select select from where group having order limit/);
}

my $for_syntax = {
  update => 'FOR UPDATE',
  shared => 'FOR SHARE',
};
sub _lock_select {
  my ($self, $type) = @_;

  my $sql;
  if (ref($type) eq 'SCALAR') {
    $sql = "FOR $$type";
  }
  else {
    $sql = $for_syntax->{$type} || $self->throw_exception( "Unknown SELECT .. FOR type '$type' requested" );
  }

  return " $sql";
}

# Handle default inserts
sub insert {
# optimized due to hotttnesss
#  my ($self, $table, $data, $options) = @_;

  # SQLA will emit INSERT INTO $table ( ) VALUES ( )
  # which is sadly understood only by MySQL. Change default behavior here,
  # until SQLA2 comes with proper dialect support
  if (! $_[2] or (ref $_[2] eq 'HASH' and !keys %{$_[2]} ) ) {
    my @bind;
    my $sql = sprintf(
      'INSERT INTO %s DEFAULT VALUES', $_[0]->_quote($_[1])
    );

    if ( ($_[3]||{})->{returning} ) {
      my $s;
      ($s, @bind) = $_[0]->_insert_returning ($_[3]);
      $sql .= $s;
    }

    return ($sql, @bind);
  }

  next::method(@_);
}

sub _recurse_fields {
  my ($self, $fields) = @_;
  my $ref = ref $fields;
  return $self->_quote($fields) unless $ref;
  return $$fields if $ref eq 'SCALAR';

  if ($ref eq 'ARRAY') {
    my (@select, @bind);
    for my $field (@$fields) {
      my ($select, @new_bind) = $self->_recurse_fields($field);
      push @select, $select;
      push @bind, @new_bind;
    }
    return (join(', ', @select), @bind);
  }
  elsif ($ref eq 'HASH') {
    my %hash = %$fields;  # shallow copy

    my $as = delete $hash{-as};   # if supplied

    my ($func, $rhs, @toomany) = %hash;

    # there should be only one pair
    if (@toomany) {
      $self->throw_exception( "Malformed select argument - too many keys in hash: " . join (',', keys %$fields ) );
    }

    if (lc ($func) eq 'distinct' && ref $rhs eq 'ARRAY' && @$rhs > 1) {
      $self->throw_exception (
        'The select => { distinct => ... } syntax is not supported for multiple columns.'
       .' Instead please use { group_by => [ qw/' . (join ' ', @$rhs) . '/ ] }'
       .' or { select => [ qw/' . (join ' ', @$rhs) . '/ ], distinct => 1 }'
      );
    }

    my ($rhs_sql, @rhs_bind) = $self->_recurse_fields($rhs);
    my $select = sprintf ('%s( %s )%s',
      $self->_sqlcase($func),
      $rhs_sql,
      $as
        ? sprintf (' %s %s', $self->_sqlcase('as'), $self->_quote ($as) )
        : ''
    );

    return ($select, @rhs_bind);
  }
  elsif ( $ref eq 'REF' and ref($$fields) eq 'ARRAY' ) {
    return @{$$fields};
  }
  else {
    $self->throw_exception( $ref . qq{ unexpected in _recurse_fields()} );
  }
}


# this used to be a part of _order_by but is broken out for clarity.
# What we have been doing forever is hijacking the $order arg of
# SQLA::select to pass in arbitrary pieces of data (first the group_by,
# then pretty much the entire resultset attr-hash, as more and more
# things in the SQLA space need to have more info about the $rs they
# create SQL for. The alternative would be to keep expanding the
# signature of _select with more and more positional parameters, which
# is just gross. All hail SQLA2!
sub _parse_rs_attrs {
  my ($self, $arg) = @_;

  my $sql = '';

  if ($arg->{group_by}) {
    if ( my ($group_sql, @group_bind) = $self->_recurse_fields($arg->{group_by}) ) {
      $sql .= $self->_sqlcase(' group by ') . $group_sql;
      push @{$self->{group_bind}}, @group_bind;
    }
  }

  if (defined $arg->{having}) {
    my ($frag, @bind) = $self->_recurse_where($arg->{having});
    push(@{$self->{having_bind}}, @bind);
    $sql .= $self->_sqlcase(' having ') . $frag;
  }

  if (defined $arg->{order_by}) {
    $sql .= $self->_order_by ($arg->{order_by});
  }

  return $sql;
}

sub _order_by {
  my ($self, $arg) = @_;

  # check that we are not called in legacy mode (order_by as 4th argument)
  if (ref $arg eq 'HASH' and not grep { $_ =~ /^-(?:desc|asc)/i } keys %$arg ) {
    return $self->_parse_rs_attrs ($arg);
  }
  else {
    my ($sql, @bind) = $self->next::method($arg);
    push @{$self->{order_bind}}, @bind;
    return $sql;
  }
}

sub _split_order_chunk {
  my ($self, $chunk) = @_;

  # strip off sort modifiers, but always succeed, so $1 gets reset
  $chunk =~ s/ (?: \s+ (ASC|DESC) )? \s* $//ix;

  return (
    $chunk,
    ( $1 and uc($1) eq 'DESC' ) ? 1 : 0,
  );
}

sub _table {
# optimized due to hotttnesss
#  my ($self, $from) = @_;
  if (my $ref = ref $_[1] ) {
    if ($ref eq 'ARRAY') {
      return $_[0]->_recurse_from(@{$_[1]});
    }
    elsif ($ref eq 'HASH') {
      return $_[0]->_recurse_from($_[1]);
    }
    elsif ($ref eq 'REF' && ref ${$_[1]} eq 'ARRAY') {
      my ($sql, @bind) = @{ ${$_[1]} };
      push @{$_[0]->{from_bind}}, @bind;
      return $sql
    }
  }
  return $_[0]->next::method ($_[1]);
}

sub _generate_join_clause {
    my ($self, $join_type) = @_;

    $join_type = $self->{_default_jointype}
      if ! defined $join_type;

    return sprintf ('%s JOIN ',
      $join_type ?  $self->_sqlcase($join_type) : ''
    );
}

sub _recurse_from {
  my $self = shift;
  return join (' ', $self->_gen_from_blocks(@_) );
}

sub _gen_from_blocks {
  my ($self, $from, @joins) = @_;

  my @fchunks = $self->_from_chunk_to_sql($from);

  for (@joins) {
    my ($to, $on) = @$_;

    # check whether a join type exists
    my $to_jt = ref($to) eq 'ARRAY' ? $to->[0] : $to;
    my $join_type;
    if (ref($to_jt) eq 'HASH' and defined($to_jt->{-join_type})) {
      $join_type = $to_jt->{-join_type};
      $join_type =~ s/^\s+ | \s+$//xg;
    }

    my @j = $self->_generate_join_clause( $join_type );

    if (ref $to eq 'ARRAY') {
      push(@j, '(', $self->_recurse_from(@$to), ')');
    }
    else {
      push(@j, $self->_from_chunk_to_sql($to));
    }

    my ($sql, @bind) = $self->_join_condition($on);
    push(@j, ' ON ', $sql);
    push @{$self->{from_bind}}, @bind;

    push @fchunks, join '', @j;
  }

  return @fchunks;
}

sub _from_chunk_to_sql {
  my ($self, $fromspec) = @_;

  return join (' ', do {
    if (! ref $fromspec) {
      $self->_quote($fromspec);
    }
    elsif (ref $fromspec eq 'SCALAR') {
      $$fromspec;
    }
    elsif (ref $fromspec eq 'REF' and ref $$fromspec eq 'ARRAY') {
      push @{$self->{from_bind}}, @{$$fromspec}[1..$#$$fromspec];
      $$fromspec->[0];
    }
    elsif (ref $fromspec eq 'HASH') {
      my ($as, $table, $toomuch) = ( map
        { $_ => $fromspec->{$_} }
        ( grep { $_ !~ /^\-/ } keys %$fromspec )
      );

      $self->throw_exception( "Only one table/as pair expected in from-spec but an exra '$toomuch' key present" )
        if defined $toomuch;

      ($self->_from_chunk_to_sql($table), $self->_quote($as) );
    }
    else {
      $self->throw_exception('Unsupported from refkind: ' . ref $fromspec );
    }
  });
}

sub _join_condition {
  my ($self, $cond) = @_;

  # Backcompat for the old days when a plain hashref
  # { 't1.col1' => 't2.col2' } meant ON t1.col1 = t2.col2
  if (
    ref $cond eq 'HASH'
      and
    keys %$cond == 1
      and
    (keys %$cond)[0] =~ /\./
      and
    ! ref ( (values %$cond)[0] )
  ) {
    carp_unique(
      "ResultSet {from} structures with conditions not conforming to the "
    . "SQL::Abstract syntax are deprecated: you either need to stop abusing "
    . "{from} altogether, or express the condition properly using the "
    . "{ -ident => ... } operator"
    );
    $cond = { keys %$cond => { -ident => values %$cond } }
  }
  elsif ( ref $cond eq 'ARRAY' ) {
    # do our own ORing so that the hashref-shim above is invoked
    my @parts;
    my @binds;
    foreach my $c (@$cond) {
      my ($sql, @bind) = $self->_join_condition($c);
      push @binds, @bind;
      push @parts, $sql;
    }
    return join(' OR ', @parts), @binds;
  }

  return $self->_recurse_where($cond);
}

# This is hideously ugly, but SQLA does not understand multicol IN expressions
# FIXME TEMPORARY - DQ should have native syntax for this
# moved here to raise API questions
#
# !!! EXPERIMENTAL API !!! WILL CHANGE !!!
sub _where_op_multicolumn_in {
  my ($self, $lhs, $rhs) = @_;

  if (! ref $lhs or ref $lhs eq 'ARRAY') {
    my (@sql, @bind);
    for (ref $lhs ? @$lhs : $lhs) {
      if (! ref $_) {
        push @sql, $self->_quote($_);
      }
      elsif (ref $_ eq 'SCALAR') {
        push @sql, $$_;
      }
      elsif (ref $_ eq 'REF' and ref $$_ eq 'ARRAY') {
        my ($s, @b) = @$$_;
        push @sql, $s;
        push @bind, @b;
      }
      else {
        $self->throw_exception("ARRAY of @{[ ref $_ ]}es unsupported for multicolumn IN lhs...");
      }
    }
    $lhs = \[ join(', ', @sql), @bind];
  }
  elsif (ref $lhs eq 'SCALAR') {
    $lhs = \[ $$lhs ];
  }
  elsif (ref $lhs eq 'REF' and ref $$lhs eq 'ARRAY' ) {
    # noop
  }
  else {
    $self->throw_exception( ref($lhs) . "es unsupported for multicolumn IN lhs...");
  }

  # is this proper...?
  $rhs = \[ $self->_recurse_where($rhs) ];

  for ($lhs, $rhs) {
    $$_->[0] = "( $$_->[0] )"
      unless $$_->[0] =~ /^ \s* \( .* \) \s* $/xs;
  }

  \[ join( ' IN ', shift @$$lhs, shift @$$rhs ), @$$lhs, @$$rhs ];
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
