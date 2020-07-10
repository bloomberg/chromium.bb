package DBIx::Class::SQLMaker::LimitDialects;

use warnings;
use strict;

use List::Util 'first';
use namespace::clean;

# constants are used not only here, but also in comparison tests
sub __rows_bindtype () {
  +{ sqlt_datatype => 'integer' }
}
sub __offset_bindtype () {
  +{ sqlt_datatype => 'integer' }
}
sub __total_bindtype () {
  +{ sqlt_datatype => 'integer' }
}

=head1 NAME

DBIx::Class::SQLMaker::LimitDialects - SQL::Abstract::Limit-like functionality for DBIx::Class::SQLMaker

=head1 DESCRIPTION

This module replicates a lot of the functionality originally found in
L<SQL::Abstract::Limit>. While simple limits would work as-is, the more
complex dialects that require e.g. subqueries could not be reliably
implemented without taking full advantage of the metadata locked within
L<DBIx::Class::ResultSource> classes. After reimplementation of close to
80% of the L<SQL::Abstract::Limit> functionality it was deemed more
practical to simply make an independent DBIx::Class-specific limit-dialect
provider.

=head1 SQL LIMIT DIALECTS

Note that the actual implementations listed below never use C<*> literally.
Instead proper re-aliasing of selectors and order criteria is done, so that
the limit dialect are safe to use on joined resultsets with clashing column
names.

Currently the provided dialects are:

=head2 LimitOffset

 SELECT ... LIMIT $limit OFFSET $offset

Supported by B<PostgreSQL> and B<SQLite>

=cut
sub _LimitOffset {
    my ( $self, $sql, $rs_attrs, $rows, $offset ) = @_;
    $sql .= $self->_parse_rs_attrs( $rs_attrs ) . " LIMIT ?";
    push @{$self->{limit_bind}}, [ $self->__rows_bindtype => $rows ];
    if ($offset) {
      $sql .= " OFFSET ?";
      push @{$self->{limit_bind}}, [ $self->__offset_bindtype => $offset ];
    }
    return $sql;
}

=head2 LimitXY

 SELECT ... LIMIT $offset, $limit

Supported by B<MySQL> and any L<SQL::Statement> based DBD

=cut
sub _LimitXY {
    my ( $self, $sql, $rs_attrs, $rows, $offset ) = @_;
    $sql .= $self->_parse_rs_attrs( $rs_attrs ) . " LIMIT ";
    if ($offset) {
      $sql .= '?, ';
      push @{$self->{limit_bind}}, [ $self->__offset_bindtype => $offset ];
    }
    $sql .= '?';
    push @{$self->{limit_bind}}, [ $self->__rows_bindtype => $rows ];

    return $sql;
}

=head2 RowNumberOver

 SELECT * FROM (
  SELECT *, ROW_NUMBER() OVER( ORDER BY ... ) AS RNO__ROW__INDEX FROM (
   SELECT ...
  )
 ) WHERE RNO__ROW__INDEX BETWEEN ($offset+1) AND ($limit+$offset)


ANSI standard Limit/Offset implementation. Supported by B<DB2> and
B<< MSSQL >= 2005 >>.

=cut
sub _RowNumberOver {
  my ($self, $sql, $rs_attrs, $rows, $offset ) = @_;

  # get selectors, and scan the order_by (if any)
  my $sq_attrs = $self->_subqueried_limit_attrs ( $sql, $rs_attrs );

  # make up an order if none exists
  my $requested_order = (delete $rs_attrs->{order_by}) || $self->_rno_default_order;

  # the order binds (if any) will need to go at the end of the entire inner select
  local $self->{order_bind};
  my $rno_ord = $self->_order_by ($requested_order);
  push @{$self->{select_bind}}, @{$self->{order_bind}};

  # this is the order supplement magic
  my $mid_sel = $sq_attrs->{selection_outer};
  if (my $extra_order_sel = $sq_attrs->{order_supplement}) {
    for my $extra_col (sort
      { $extra_order_sel->{$a} cmp $extra_order_sel->{$b} }
      keys %$extra_order_sel
    ) {
      $sq_attrs->{selection_inner} .= sprintf (', %s AS %s',
        $extra_col,
        $extra_order_sel->{$extra_col},
      );
    }
  }

  # and this is order re-alias magic
  for my $map ($sq_attrs->{order_supplement}, $sq_attrs->{outer_renames}) {
    for my $col (sort { (length $b) <=> (length $a) } keys %{$map||{}} ) {
      my $re_col = quotemeta ($col);
      $rno_ord =~ s/$re_col/$map->{$col}/;
    }
  }

  # whatever is left of the order_by (only where is processed at this point)
  my $group_having = $self->_parse_rs_attrs($rs_attrs);

  my $qalias = $self->_quote ($rs_attrs->{alias});
  my $idx_name = $self->_quote ('rno__row__index');

  push @{$self->{limit_bind}}, [ $self->__offset_bindtype => $offset + 1], [ $self->__total_bindtype => $offset + $rows ];

  return <<EOS;

SELECT $sq_attrs->{selection_outer} FROM (
  SELECT $mid_sel, ROW_NUMBER() OVER( $rno_ord ) AS $idx_name FROM (
    SELECT $sq_attrs->{selection_inner} $sq_attrs->{query_leftover}${group_having}
  ) $qalias
) $qalias WHERE $idx_name >= ? AND $idx_name <= ?

EOS

}

# some databases are happy with OVER (), some need OVER (ORDER BY (SELECT (1)) )
sub _rno_default_order {
  return undef;
}

=head2 SkipFirst

 SELECT SKIP $offset FIRST $limit * FROM ...

Supported by B<Informix>, almost like LimitOffset. According to
L<SQL::Abstract::Limit> C<... SKIP $offset LIMIT $limit ...> is also supported.

=cut
sub _SkipFirst {
  my ($self, $sql, $rs_attrs, $rows, $offset) = @_;

  $sql =~ s/^ \s* SELECT \s+ //ix
    or $self->throw_exception("Unrecognizable SELECT: $sql");

  return sprintf ('SELECT %s%s%s%s',
    $offset
      ? do {
         push @{$self->{pre_select_bind}}, [ $self->__offset_bindtype => $offset];
         'SKIP ? '
      }
      : ''
    ,
    do {
       push @{$self->{pre_select_bind}}, [ $self->__rows_bindtype => $rows ];
       'FIRST ? '
    },
    $sql,
    $self->_parse_rs_attrs ($rs_attrs),
  );
}

=head2 FirstSkip

 SELECT FIRST $limit SKIP $offset * FROM ...

Supported by B<Firebird/Interbase>, reverse of SkipFirst. According to
L<SQL::Abstract::Limit> C<... ROWS $limit TO $offset ...> is also supported.

=cut
sub _FirstSkip {
  my ($self, $sql, $rs_attrs, $rows, $offset) = @_;

  $sql =~ s/^ \s* SELECT \s+ //ix
    or $self->throw_exception("Unrecognizable SELECT: $sql");

  return sprintf ('SELECT %s%s%s%s',
    do {
       push @{$self->{pre_select_bind}}, [ $self->__rows_bindtype => $rows ];
       'FIRST ? '
    },
    $offset
      ? do {
         push @{$self->{pre_select_bind}}, [ $self->__offset_bindtype => $offset];
         'SKIP ? '
      }
      : ''
    ,
    $sql,
    $self->_parse_rs_attrs ($rs_attrs),
  );
}


=head2 RowNum

Depending on the resultset attributes one of:

 SELECT * FROM (
  SELECT *, ROWNUM AS rownum__index FROM (
   SELECT ...
  ) WHERE ROWNUM <= ($limit+$offset)
 ) WHERE rownum__index >= ($offset+1)

or

 SELECT * FROM (
  SELECT *, ROWNUM AS rownum__index FROM (
    SELECT ...
  )
 ) WHERE rownum__index BETWEEN ($offset+1) AND ($limit+$offset)

or

 SELECT * FROM (
    SELECT ...
  ) WHERE ROWNUM <= ($limit+1)

Supported by B<Oracle>.

=cut
sub _RowNum {
  my ( $self, $sql, $rs_attrs, $rows, $offset ) = @_;

  my $sq_attrs = $self->_subqueried_limit_attrs ($sql, $rs_attrs);

  my $qalias = $self->_quote ($rs_attrs->{alias});
  my $idx_name = $self->_quote ('rownum__index');
  my $order_group_having = $self->_parse_rs_attrs($rs_attrs);


  # if no offset (e.g. first page) - we can skip one of the subqueries
  if (! $offset) {
    push @{$self->{limit_bind}}, [ $self->__rows_bindtype => $rows ];

    return <<EOS;
SELECT $sq_attrs->{selection_outer} FROM (
  SELECT $sq_attrs->{selection_inner} $sq_attrs->{query_leftover}${order_group_having}
) $qalias WHERE ROWNUM <= ?
EOS
  }

  #
  # There are two ways to limit in Oracle, one vastly faster than the other
  # on large resultsets: https://decipherinfosys.wordpress.com/2007/08/09/paging-and-countstopkey-optimization/
  # However Oracle is retarded and does not preserve stable ROWNUM() values
  # when called twice in the same scope. Therefore unless the resultset is
  # ordered by a unique set of columns, it is not safe to use the faster
  # method, and the slower BETWEEN query is used instead
  #
  # FIXME - this is quite expensive, and does not perform caching of any sort
  # as soon as some of the DQ work becomes viable consider switching this
  # over
  if (
    $rs_attrs->{order_by}
      and
    $rs_attrs->{result_source}->storage->_order_by_is_stable(
      @{$rs_attrs}{qw/from order_by where/}
    )
  ) {
    push @{$self->{limit_bind}}, [ $self->__total_bindtype => $offset + $rows ], [ $self->__offset_bindtype => $offset + 1 ];

    return <<EOS;
SELECT $sq_attrs->{selection_outer} FROM (
  SELECT $sq_attrs->{selection_outer}, ROWNUM AS $idx_name FROM (
    SELECT $sq_attrs->{selection_inner} $sq_attrs->{query_leftover}${order_group_having}
  ) $qalias WHERE ROWNUM <= ?
) $qalias WHERE $idx_name >= ?
EOS
  }
  else {
    push @{$self->{limit_bind}}, [ $self->__offset_bindtype => $offset + 1 ], [ $self->__total_bindtype => $offset + $rows ];

    return <<EOS;
SELECT $sq_attrs->{selection_outer} FROM (
  SELECT $sq_attrs->{selection_outer}, ROWNUM AS $idx_name FROM (
    SELECT $sq_attrs->{selection_inner} $sq_attrs->{query_leftover}${order_group_having}
  ) $qalias
) $qalias WHERE $idx_name BETWEEN ? AND ?
EOS
  }
}

# used by _Top and _FetchFirst below
sub _prep_for_skimming_limit {
  my ( $self, $sql, $rs_attrs ) = @_;

  # get selectors
  my $sq_attrs = $self->_subqueried_limit_attrs ($sql, $rs_attrs);

  my $requested_order = delete $rs_attrs->{order_by};
  $sq_attrs->{order_by_requested} = $self->_order_by ($requested_order);
  $sq_attrs->{grpby_having} = $self->_parse_rs_attrs ($rs_attrs);

  # without an offset things are easy
  if (! $rs_attrs->{offset}) {
    $sq_attrs->{order_by_inner} = $sq_attrs->{order_by_requested};
  }
  else {
    $sq_attrs->{quoted_rs_alias} = $self->_quote ($rs_attrs->{alias});

    # localise as we already have all the bind values we need
    local $self->{order_bind};

    # make up an order unless supplied or sanity check what we are given
    my $inner_order;
    if ($sq_attrs->{order_by_requested}) {
      $self->throw_exception (
        'Unable to safely perform "skimming type" limit with supplied unstable order criteria'
      ) unless ($rs_attrs->{result_source}->schema->storage->_order_by_is_stable(
        $rs_attrs->{from},
        $requested_order,
        $rs_attrs->{where},
      ));

      $inner_order = $requested_order;
    }
    else {
      $inner_order = [ map
        { "$rs_attrs->{alias}.$_" }
        ( @{
          $rs_attrs->{result_source}->_identifying_column_set
            ||
          $self->throw_exception(sprintf(
            'Unable to auto-construct stable order criteria for "skimming type" limit '
          . "dialect based on source '%s'", $rs_attrs->{result_source}->name) );
        } )
      ];
    }

    $sq_attrs->{order_by_inner} = $self->_order_by ($inner_order);

    my @out_chunks;
    for my $ch ($self->_order_by_chunks ($inner_order)) {
      $ch = $ch->[0] if ref $ch eq 'ARRAY';

      ($ch, my $is_desc) = $self->_split_order_chunk($ch);

      # !NOTE! outside chunks come in reverse order ( !$is_desc )
      push @out_chunks, { ($is_desc ? '-asc' : '-desc') => \$ch };
    }

    $sq_attrs->{order_by_middle} = $self->_order_by (\@out_chunks);

    # this is the order supplement magic
    $sq_attrs->{selection_middle} = $sq_attrs->{selection_outer};
    if (my $extra_order_sel = $sq_attrs->{order_supplement}) {
      for my $extra_col (sort
        { $extra_order_sel->{$a} cmp $extra_order_sel->{$b} }
        keys %$extra_order_sel
      ) {
        $sq_attrs->{selection_inner} .= sprintf (', %s AS %s',
          $extra_col,
          $extra_order_sel->{$extra_col},
        );

        $sq_attrs->{selection_middle} .= ', ' . $extra_order_sel->{$extra_col};
      }

      # Whatever order bindvals there are, they will be realiased and
      # reselected, and need to show up at end of the initial inner select
      push @{$self->{select_bind}}, @{$self->{order_bind}};
    }

    # and this is order re-alias magic
    for my $map ($sq_attrs->{order_supplement}, $sq_attrs->{outer_renames}) {
      for my $col (sort { (length $b) <=> (length $a) } keys %{$map||{}}) {
        my $re_col = quotemeta ($col);
        $_ =~ s/$re_col/$map->{$col}/
          for ($sq_attrs->{order_by_middle}, $sq_attrs->{order_by_requested});
      }
    }
  }

  $sq_attrs;
}

=head2 Top

 SELECT * FROM

 SELECT TOP $limit FROM (
  SELECT TOP $limit FROM (
   SELECT TOP ($limit+$offset) ...
  ) ORDER BY $reversed_original_order
 ) ORDER BY $original_order

Unreliable Top-based implementation, supported by B<< MSSQL < 2005 >>.

=head3 CAVEAT

Due to its implementation, this limit dialect returns B<incorrect results>
when $limit+$offset > total amount of rows in the resultset.

=cut

sub _Top {
  my ( $self, $sql, $rs_attrs, $rows, $offset ) = @_;

  my $lim = $self->_prep_for_skimming_limit($sql, $rs_attrs);

  $sql = sprintf ('SELECT TOP %u %s %s %s %s',
    $rows + ($offset||0),
    $offset ? $lim->{selection_inner} : $lim->{selection_original},
    $lim->{query_leftover},
    $lim->{grpby_having},
    $lim->{order_by_inner},
  );

  $sql = sprintf ('SELECT TOP %u %s FROM ( %s ) %s %s',
    $rows,
    $lim->{selection_middle},
    $sql,
    $lim->{quoted_rs_alias},
    $lim->{order_by_middle},
  ) if $offset;

  $sql = sprintf ('SELECT %s FROM ( %s ) %s %s',
    $lim->{selection_outer},
    $sql,
    $lim->{quoted_rs_alias},
    $lim->{order_by_requested},
  ) if $offset and (
    $lim->{order_by_requested} or $lim->{selection_middle} ne $lim->{selection_outer}
  );

  return $sql;
}

=head2 FetchFirst

 SELECT * FROM
 (
 SELECT * FROM (
  SELECT * FROM (
   SELECT * FROM ...
  ) ORDER BY $reversed_original_order
    FETCH FIRST $limit ROWS ONLY
 ) ORDER BY $original_order
   FETCH FIRST $limit ROWS ONLY
 )

Unreliable FetchFirst-based implementation, supported by B<< IBM DB2 <= V5R3 >>.

=head3 CAVEAT

Due to its implementation, this limit dialect returns B<incorrect results>
when $limit+$offset > total amount of rows in the resultset.

=cut

sub _FetchFirst {
  my ( $self, $sql, $rs_attrs, $rows, $offset ) = @_;

  my $lim = $self->_prep_for_skimming_limit($sql, $rs_attrs);

  $sql = sprintf ('SELECT %s %s %s %s FETCH FIRST %u ROWS ONLY',
    $offset ? $lim->{selection_inner} : $lim->{selection_original},
    $lim->{query_leftover},
    $lim->{grpby_having},
    $lim->{order_by_inner},
    $rows + ($offset||0),
  );

  $sql = sprintf ('SELECT %s FROM ( %s ) %s %s FETCH FIRST %u ROWS ONLY',
    $lim->{selection_middle},
    $sql,
    $lim->{quoted_rs_alias},
    $lim->{order_by_middle},
    $rows,
  ) if $offset;


  $sql = sprintf ('SELECT %s FROM ( %s ) %s %s',
    $lim->{selection_outer},
    $sql,
    $lim->{quoted_rs_alias},
    $lim->{order_by_requested},
  ) if $offset and (
    $lim->{order_by_requested} or $lim->{selection_middle} ne $lim->{selection_outer}
  );

  return $sql;
}

=head2 GenericSubQ

 SELECT * FROM (
  SELECT ...
 )
 WHERE (
  SELECT COUNT(*) FROM $original_table cnt WHERE cnt.id < $original_table.id
 ) BETWEEN $offset AND ($offset+$rows-1)

This is the most evil limit "dialect" (more of a hack) for I<really> stupid
databases. It works by ordering the set by some unique column, and calculating
the amount of rows that have a less-er value (thus emulating a L</RowNum>-like
index). Of course this implies the set can only be ordered by a single unique
column.

Also note that this technique can be and often is B<excruciatingly slow>. You
may have much better luck using L<DBIx::Class::ResultSet/software_limit>
instead.

Currently used by B<Sybase ASE>, due to lack of any other option.

=cut
sub _GenericSubQ {
  my ($self, $sql, $rs_attrs, $rows, $offset) = @_;

  my $main_rsrc = $rs_attrs->{result_source};

  # Explicitly require an order_by
  # GenSubQ is slow enough as it is, just emulating things
  # like in other cases is not wise - make the user work
  # to shoot their DBA in the foot
  $self->throw_exception (
    'Generic Subquery Limit does not work on resultsets without an order. Provide a stable, '
  . 'main-table-based order criteria.'
  ) unless $rs_attrs->{order_by};

  my $usable_order_colinfo = $main_rsrc->storage->_extract_colinfo_of_stable_main_source_order_by_portion(
    $rs_attrs
  );

  $self->throw_exception(
    'Generic Subquery Limit can not work with order criteria based on sources other than the main one'
  ) if (
    ! keys %{$usable_order_colinfo||{}}
      or
    grep
      { $_->{-source_alias} ne $rs_attrs->{alias} }
      (values %$usable_order_colinfo)
  );

###
###
### we need to know the directions after we figured out the above - reextract *again*
### this is eyebleed - trying to get it to work at first
  my $supplied_order = delete $rs_attrs->{order_by};

  my @order_bits = do {
    local $self->{quote_char};
    local $self->{order_bind};
    map { ref $_ ? $_->[0] : $_ } $self->_order_by_chunks ($supplied_order)
  };

  # truncate to what we'll use
  $#order_bits = ( (keys %$usable_order_colinfo) - 1 );

  # @order_bits likely will come back quoted (due to how the prefetch
  # rewriter operates
  # Hence supplement the column_info lookup table with quoted versions
  if ($self->quote_char) {
    $usable_order_colinfo->{$self->_quote($_)} = $usable_order_colinfo->{$_}
      for keys %$usable_order_colinfo;
  }

# calculate the condition
  my $count_tbl_alias = 'rownum__emulation';
  my $main_alias = $rs_attrs->{alias};
  my $main_tbl_name = $main_rsrc->name;

  my (@unqualified_names, @qualified_names, @is_desc, @new_order_by);

  for my $bit (@order_bits) {

    ($bit, my $is_desc) = $self->_split_order_chunk($bit);

    push @is_desc, $is_desc;
    push @unqualified_names, $usable_order_colinfo->{$bit}{-colname};
    push @qualified_names, $usable_order_colinfo->{$bit}{-fq_colname};

    push @new_order_by, { ($is_desc ? '-desc' : '-asc') => $usable_order_colinfo->{$bit}{-fq_colname} };
  };

  my (@where_cond, @skip_colpair_stack);
  for my $i (0 .. $#order_bits) {
    my $ci = $usable_order_colinfo->{$order_bits[$i]};

    my ($subq_col, $main_col) = map { "$_.$ci->{-colname}" } ($count_tbl_alias, $main_alias);
    my $cur_cond = { $subq_col => { ($is_desc[$i] ? '>' : '<') => { -ident => $main_col } } };

    push @skip_colpair_stack, [
      { $main_col => { -ident => $subq_col } },
    ];

    # we can trust the nullability flag because
    # we already used it during _id_col_set resolution
    #
    if ($ci->{is_nullable}) {
      push @{$skip_colpair_stack[-1]}, { $main_col => undef, $subq_col=> undef };

      $cur_cond = [
        {
          ($is_desc[$i] ? $subq_col : $main_col) => { '!=', undef },
          ($is_desc[$i] ? $main_col : $subq_col) => undef,
        },
        {
          $subq_col => { '!=', undef },
          $main_col => { '!=', undef },
          -and => $cur_cond,
        },
      ];
    }

    push @where_cond, { '-and', => [ @skip_colpair_stack[0..$i-1], $cur_cond ] };
  }

# reuse the sqlmaker WHERE, this will not be returning binds
  my $counted_where = do {
    local $self->{where_bind};
    $self->where(\@where_cond);
  };

# construct the rownum condition by hand
  my $rownum_cond;
  if ($offset) {
    $rownum_cond = 'BETWEEN ? AND ?';
    push @{$self->{limit_bind}},
      [ $self->__offset_bindtype => $offset ],
      [ $self->__total_bindtype => $offset + $rows - 1]
    ;
  }
  else {
    $rownum_cond = '< ?';
    push @{$self->{limit_bind}},
      [ $self->__rows_bindtype => $rows ]
    ;
  }

# and what we will order by inside
  my $inner_order_sql = do {
    local $self->{order_bind};

    my $s = $self->_order_by (\@new_order_by);

    $self->throw_exception('Inner gensubq order may not contain binds... something went wrong')
      if @{$self->{order_bind}};

    $s;
  };

### resume originally scheduled programming
###
###

  # we need to supply the order for the supplements to be properly calculated
  my $sq_attrs = $self->_subqueried_limit_attrs (
    $sql, { %$rs_attrs, order_by => \@new_order_by }
  );

  my $in_sel = $sq_attrs->{selection_inner};

  # add the order supplement (if any) as this is what will be used for the outer WHERE
  $in_sel .= ", $_" for sort keys %{$sq_attrs->{order_supplement}};

  my $group_having_sql = $self->_parse_rs_attrs($rs_attrs);


  return sprintf ("
SELECT $sq_attrs->{selection_outer}
  FROM (
    SELECT $in_sel $sq_attrs->{query_leftover}${group_having_sql}
  ) %s
WHERE ( SELECT COUNT(*) FROM %s %s $counted_where ) $rownum_cond
$inner_order_sql
  ", map { $self->_quote ($_) } (
    $rs_attrs->{alias},
    $main_tbl_name,
    $count_tbl_alias,
  ));
}


# !!! THIS IS ALSO HORRIFIC !!! /me ashamed
#
# Generates inner/outer select lists for various limit dialects
# which result in one or more subqueries (e.g. RNO, Top, RowNum)
# Any non-main-table columns need to have their table qualifier
# turned into a column alias (otherwise names in subqueries clash
# and/or lose their source table)
#
# Returns mangled proto-sql, inner/outer strings of SQL QUOTED selectors
# with aliases (to be used in whatever select statement), and an alias
# index hashref of QUOTED SEL => QUOTED ALIAS pairs (to maybe be used
# for string-subst higher up).
# If an order_by is supplied, the inner select needs to bring out columns
# used in implicit (non-selected) orders, and the order condition itself
# needs to be realiased to the proper names in the outer query. Thus we
# also return a hashref (order doesn't matter) of QUOTED EXTRA-SEL =>
# QUOTED ALIAS pairs, which is a list of extra selectors that do *not*
# exist in the original select list
sub _subqueried_limit_attrs {
  my ($self, $proto_sql, $rs_attrs) = @_;

  $self->throw_exception(
    'Limit dialect implementation usable only in the context of DBIC (missing $rs_attrs)'
  ) unless ref ($rs_attrs) eq 'HASH';

  # mangle the input sql as we will be replacing the selector entirely
  unless (
    $rs_attrs->{_selector_sql}
      and
    $proto_sql =~ s/^ \s* SELECT \s* \Q$rs_attrs->{_selector_sql}//ix
  ) {
    $self->throw_exception("Unrecognizable SELECT: $proto_sql");
  }

  my ($re_sep, $re_alias) = map { quotemeta $_ } ( $self->{name_sep}, $rs_attrs->{alias} );

  # correlate select and as, build selection index
  my (@sel, $in_sel_index);
  for my $i (0 .. $#{$rs_attrs->{select}}) {

    my $s = $rs_attrs->{select}[$i];
    my $sql_alias = (ref $s) eq 'HASH' ? $s->{-as} : undef;

    # we throw away the @bind here deliberately
    my ($sql_sel) = $self->_recurse_fields ($s);

    push @sel, {
      arg => $s,
      sql => $sql_sel,
      unquoted_sql => do {
        local $self->{quote_char};
        ($self->_recurse_fields ($s))[0]; # ignore binds again
      },
      as =>
        $sql_alias
          ||
        $rs_attrs->{as}[$i]
          ||
        $self->throw_exception("Select argument $i ($s) without corresponding 'as'")
      ,
    };

    # anything with a placeholder in it needs re-selection
    $in_sel_index->{$sql_sel}++ unless $sql_sel =~ / (?: ^ | \W ) \? (?: \W | $ ) /x;

    $in_sel_index->{$self->_quote ($sql_alias)}++ if $sql_alias;

    # record unqualified versions too, so we do not have
    # to reselect the same column twice (in qualified and
    # unqualified form)
    if (! ref $s && $sql_sel =~ / $re_sep (.+) $/x) {
      $in_sel_index->{$1}++;
    }
  }


  # re-alias and remove any name separators from aliases,
  # unless we are dealing with the current source alias
  # (which will transcend the subqueries as it is necessary
  # for possible further chaining)
  # same for anything we do not recognize
  my ($sel, $renamed);
  for my $node (@sel) {
    push @{$sel->{original}}, $node->{sql};

    if (
      ! $in_sel_index->{$node->{sql}}
        or
      $node->{as} =~ / (?<! ^ $re_alias ) \. /x
        or
      $node->{unquoted_sql} =~ / (?<! ^ $re_alias ) $re_sep /x
    ) {
      $node->{as} = $self->_unqualify_colname($node->{as});
      my $quoted_as = $self->_quote($node->{as});
      push @{$sel->{inner}}, sprintf '%s AS %s', $node->{sql}, $quoted_as;
      push @{$sel->{outer}}, $quoted_as;
      $renamed->{$node->{sql}} = $quoted_as;
    }
    else {
      push @{$sel->{inner}}, $node->{sql};
      push @{$sel->{outer}}, $self->_quote (ref $node->{arg} ? $node->{as} : $node->{arg});
    }
  }

  # see if the order gives us anything
  my $extra_order_sel;
  for my $chunk ($self->_order_by_chunks ($rs_attrs->{order_by})) {
    # order with bind
    $chunk = $chunk->[0] if (ref $chunk) eq 'ARRAY';
    ($chunk) = $self->_split_order_chunk($chunk);

    next if $in_sel_index->{$chunk};

    $extra_order_sel->{$chunk} ||= $self->_quote (
      'ORDER__BY__' . sprintf '%03d', scalar keys %{$extra_order_sel||{}}
    );
  }

  return {
    query_leftover => $proto_sql,
    (map {( "selection_$_" => join (', ', @{$sel->{$_}} ) )} keys %$sel ),
    outer_renames => $renamed,
    order_supplement => $extra_order_sel,
  };
}

sub _unqualify_colname {
  my ($self, $fqcn) = @_;
  $fqcn =~ s/ \. /__/xg;
  return $fqcn;
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
