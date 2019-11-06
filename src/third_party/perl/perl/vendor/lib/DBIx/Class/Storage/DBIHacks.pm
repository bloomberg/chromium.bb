package   #hide from PAUSE
  DBIx::Class::Storage::DBIHacks;

#
# This module contains code that should never have seen the light of day,
# does not belong in the Storage, or is otherwise unfit for public
# display. The arrival of SQLA2 should immediately obsolete 90% of this
#

use strict;
use warnings;

use base 'DBIx::Class::Storage';
use mro 'c3';

use List::Util 'first';
use Scalar::Util 'blessed';
use DBIx::Class::_Util qw(UNRESOLVABLE_CONDITION serialize);
use SQL::Abstract qw(is_plain_value is_literal_value);
use DBIx::Class::Carp;
use namespace::clean;

#
# This code will remove non-selecting/non-restricting joins from
# {from} specs, aiding the RDBMS query optimizer
#
sub _prune_unused_joins {
  my ($self, $attrs) = @_;

  # only standard {from} specs are supported, and we could be disabled in general
  return ($attrs->{from}, {})  unless (
    ref $attrs->{from} eq 'ARRAY'
      and
    @{$attrs->{from}} > 1
      and
    ref $attrs->{from}[0] eq 'HASH'
      and
    ref $attrs->{from}[1] eq 'ARRAY'
      and
    $self->_use_join_optimizer
  );

  my $orig_aliastypes = $self->_resolve_aliastypes_from_select_args($attrs);

  my $new_aliastypes = { %$orig_aliastypes };

  # we will be recreating this entirely
  my @reclassify = 'joining';

  # a grouped set will not be affected by amount of rows. Thus any
  # purely multiplicator classifications can go
  # (will be reintroduced below if needed by something else)
  push @reclassify, qw(multiplying premultiplied)
    if $attrs->{_force_prune_multiplying_joins} or $attrs->{group_by};

  # nuke what will be recalculated
  delete @{$new_aliastypes}{@reclassify};

  my @newfrom = $attrs->{from}[0]; # FROM head is always present

  # recalculate what we need once the multipliers are potentially gone
  # ignore premultiplies, since they do not add any value to anything
  my %need_joins;
  for ( @{$new_aliastypes}{grep { $_ ne 'premultiplied' } keys %$new_aliastypes }) {
    # add all requested aliases
    $need_joins{$_} = 1 for keys %$_;

    # add all their parents (as per joinpath which is an AoH { table => alias })
    $need_joins{$_} = 1 for map { values %$_ } map { @{$_->{-parents}} } values %$_;
  }

  for my $j (@{$attrs->{from}}[1..$#{$attrs->{from}}]) {
    push @newfrom, $j if (
      (! defined $j->[0]{-alias}) # legacy crap
        ||
      $need_joins{$j->[0]{-alias}}
    );
  }

  # we have a new set of joiners - for everything we nuked pull the classification
  # off the original stack
  for my $ctype (@reclassify) {
    $new_aliastypes->{$ctype} = { map
      { $need_joins{$_} ? ( $_ => $orig_aliastypes->{$ctype}{$_} ) : () }
      keys %{$orig_aliastypes->{$ctype}}
    }
  }

  return ( \@newfrom, $new_aliastypes );
}

#
# This is the code producing joined subqueries like:
# SELECT me.*, other.* FROM ( SELECT me.* FROM ... ) JOIN other ON ...
#
sub _adjust_select_args_for_complex_prefetch {
  my ($self, $attrs) = @_;

  $self->throw_exception ('Complex prefetches are not supported on resultsets with a custom from attribute') unless (
    ref $attrs->{from} eq 'ARRAY'
      and
    @{$attrs->{from}} > 1
      and
    ref $attrs->{from}[0] eq 'HASH'
      and
    ref $attrs->{from}[1] eq 'ARRAY'
  );

  my $root_alias = $attrs->{alias};

  # generate inner/outer attribute lists, remove stuff that doesn't apply
  my $outer_attrs = { %$attrs };
  delete @{$outer_attrs}{qw(from bind rows offset group_by _grouped_by_distinct having)};

  my $inner_attrs = { %$attrs, _simple_passthrough_construction => 1 };
  delete @{$inner_attrs}{qw(for collapse select as)};

  # there is no point of ordering the insides if there is no limit
  delete $inner_attrs->{order_by} if (
    delete $inner_attrs->{_order_is_artificial}
      or
    ! $inner_attrs->{rows}
  );

  # generate the inner/outer select lists
  # for inside we consider only stuff *not* brought in by the prefetch
  # on the outside we substitute any function for its alias
  $outer_attrs->{select} = [ @{$attrs->{select}} ];

  my ($root_node, $root_node_offset);

  for my $i (0 .. $#{$inner_attrs->{from}}) {
    my $node = $inner_attrs->{from}[$i];
    my $h = (ref $node eq 'HASH')                                ? $node
          : (ref $node  eq 'ARRAY' and ref $node->[0] eq 'HASH') ? $node->[0]
          : next
    ;

    if ( ($h->{-alias}||'') eq $root_alias and $h->{-rsrc} ) {
      $root_node = $h;
      $root_node_offset = $i;
      last;
    }
  }

  $self->throw_exception ('Complex prefetches are not supported on resultsets with a custom from attribute')
    unless $root_node;

  # use the heavy duty resolver to take care of aliased/nonaliased naming
  my $colinfo = $self->_resolve_column_info($inner_attrs->{from});
  my $selected_root_columns;

  for my $i (0 .. $#{$outer_attrs->{select}}) {
    my $sel = $outer_attrs->{select}->[$i];

    next if (
      $colinfo->{$sel} and $colinfo->{$sel}{-source_alias} ne $root_alias
    );

    if (ref $sel eq 'HASH' ) {
      $sel->{-as} ||= $attrs->{as}[$i];
      $outer_attrs->{select}->[$i] = join ('.', $root_alias, ($sel->{-as} || "inner_column_$i") );
    }
    elsif (! ref $sel and my $ci = $colinfo->{$sel}) {
      $selected_root_columns->{$ci->{-colname}} = 1;
    }

    push @{$inner_attrs->{select}}, $sel;

    push @{$inner_attrs->{as}}, $attrs->{as}[$i];
  }

  # We will need to fetch all native columns in the inner subquery, which may
  # be a part of an *outer* join condition, or an order_by (which needs to be
  # preserved outside), or wheres. In other words everything but the inner
  # selector
  # We can not just fetch everything because a potential has_many restricting
  # join collapse *will not work* on heavy data types.
  my $connecting_aliastypes = $self->_resolve_aliastypes_from_select_args({
    %$inner_attrs,
    select => [],
  });

  for (sort map { keys %{$_->{-seen_columns}||{}} } map { values %$_ } values %$connecting_aliastypes) {
    my $ci = $colinfo->{$_} or next;
    if (
      $ci->{-source_alias} eq $root_alias
        and
      ! $selected_root_columns->{$ci->{-colname}}++
    ) {
      # adding it to both to keep limits not supporting dark selectors happy
      push @{$inner_attrs->{select}}, $ci->{-fq_colname};
      push @{$inner_attrs->{as}}, $ci->{-fq_colname};
    }
  }

  # construct the inner {from} and lock it in a subquery
  # we need to prune first, because this will determine if we need a group_by below
  # throw away all non-selecting, non-restricting multijoins
  # (since we def. do not care about multiplication of the contents of the subquery)
  my $inner_subq = do {

    # must use it here regardless of user requests (vastly gentler on optimizer)
    local $self->{_use_join_optimizer} = 1;

    # throw away multijoins since we def. do not care about those inside the subquery
    ($inner_attrs->{from}, my $inner_aliastypes) = $self->_prune_unused_joins ({
      %$inner_attrs, _force_prune_multiplying_joins => 1
    });

    # uh-oh a multiplier (which is not us) left in, this is a problem for limits
    # we will need to add a group_by to collapse the resultset for proper counts
    if (
      grep { $_ ne $root_alias } keys %{ $inner_aliastypes->{multiplying} || {} }
        and
      # if there are user-supplied groups - assume user knows wtf they are up to
      ( ! $inner_aliastypes->{grouping} or $inner_attrs->{_grouped_by_distinct} )
    ) {

      my $cur_sel = { map { $_ => 1 } @{$inner_attrs->{select}} };

      # *possibly* supplement the main selection with pks if not already
      # there, as they will have to be a part of the group_by to collapse
      # things properly
      my $inner_select_with_extras;
      my @pks = map { "$root_alias.$_" } $root_node->{-rsrc}->primary_columns
        or $self->throw_exception( sprintf
          'Unable to perform complex limited prefetch off %s without declared primary key',
          $root_node->{-rsrc}->source_name,
        );
      for my $col (@pks) {
        push @{ $inner_select_with_extras ||= [ @{$inner_attrs->{select}} ] }, $col
          unless $cur_sel->{$col}++;
      }

      ($inner_attrs->{group_by}, $inner_attrs->{order_by}) = $self->_group_over_selection({
        %$inner_attrs,
        $inner_select_with_extras ? ( select => $inner_select_with_extras ) : (),
        _aliastypes => $inner_aliastypes,
      });
    }

    # we already optimized $inner_attrs->{from} above
    # and already local()ized
    $self->{_use_join_optimizer} = 0;

    # generate the subquery
    $self->_select_args_to_query (
      @{$inner_attrs}{qw(from select where)},
      $inner_attrs,
    );
  };

  # Generate the outer from - this is relatively easy (really just replace
  # the join slot with the subquery), with a major caveat - we can not
  # join anything that is non-selecting (not part of the prefetch), but at
  # the same time is a multi-type relationship, as it will explode the result.
  #
  # There are two possibilities here
  # - either the join is non-restricting, in which case we simply throw it away
  # - it is part of the restrictions, in which case we need to collapse the outer
  #   result by tackling yet another group_by to the outside of the query

  # work on a shallow copy
  my @orig_from = @{$attrs->{from}};


  $outer_attrs->{from} = \ my @outer_from;

  # we may not be the head
  if ($root_node_offset) {
    # first generate the outer_from, up to the substitution point
    @outer_from = splice @orig_from, 0, $root_node_offset;

    # substitute the subq at the right spot
    push @outer_from, [
      {
        -alias => $root_alias,
        -rsrc => $root_node->{-rsrc},
        $root_alias => $inner_subq,
      },
      # preserve attrs from what is now the head of the from after the splice
      @{$orig_from[0]}[1 .. $#{$orig_from[0]}],
    ];
  }
  else {
    @outer_from = {
      -alias => $root_alias,
      -rsrc => $root_node->{-rsrc},
      $root_alias => $inner_subq,
    };
  }

  shift @orig_from; # what we just replaced above

  # scan the *remaining* from spec against different attributes, and see which joins are needed
  # in what role
  my $outer_aliastypes = $outer_attrs->{_aliastypes} =
    $self->_resolve_aliastypes_from_select_args({ %$outer_attrs, from => \@orig_from });

  # unroll parents
  my ($outer_select_chain, @outer_nonselecting_chains) = map { +{
    map { $_ => 1 } map { values %$_} map { @{$_->{-parents}} } values %{ $outer_aliastypes->{$_} || {} }
  } } qw/selecting restricting grouping ordering/;

  # see what's left - throw away if not selecting/restricting
  my $may_need_outer_group_by;
  while (my $j = shift @orig_from) {
    my $alias = $j->[0]{-alias};

    if (
      $outer_select_chain->{$alias}
    ) {
      push @outer_from, $j
    }
    elsif (first { $_->{$alias} } @outer_nonselecting_chains ) {
      push @outer_from, $j;
      $may_need_outer_group_by ||= $outer_aliastypes->{multiplying}{$alias} ? 1 : 0;
    }
  }

  # also throw in a synthetic group_by if a non-selecting multiplier,
  # to guard against cross-join explosions
  # the logic is somewhat fragile, but relies on the idea that if a user supplied
  # a group by on their own - they know what they were doing
  if ( $may_need_outer_group_by and $attrs->{_grouped_by_distinct} ) {
    ($outer_attrs->{group_by}, $outer_attrs->{order_by}) = $self->_group_over_selection ({
      %$outer_attrs,
      from => \@outer_from,
    });
  }

  # This is totally horrific - the {where} ends up in both the inner and outer query
  # Unfortunately not much can be done until SQLA2 introspection arrives, and even
  # then if where conditions apply to the *right* side of the prefetch, you may have
  # to both filter the inner select (e.g. to apply a limit) and then have to re-filter
  # the outer select to exclude joins you didn't want in the first place
  #
  # OTOH it can be seen as a plus: <ash> (notes that this query would make a DBA cry ;)
  return $outer_attrs;
}

#
# I KNOW THIS SUCKS! GET SQLA2 OUT THE DOOR SO THIS CAN DIE!
#
# Due to a lack of SQLA2 we fall back to crude scans of all the
# select/where/order/group attributes, in order to determine what
# aliases are needed to fulfill the query. This information is used
# throughout the code to prune unnecessary JOINs from the queries
# in an attempt to reduce the execution time.
# Although the method is pretty horrific, the worst thing that can
# happen is for it to fail due to some scalar SQL, which in turn will
# result in a vocal exception.
sub _resolve_aliastypes_from_select_args {
  my ( $self, $attrs ) = @_;

  $self->throw_exception ('Unable to analyze custom {from}')
    if ref $attrs->{from} ne 'ARRAY';

  # what we will return
  my $aliases_by_type;

  # see what aliases are there to work with
  # and record who is a multiplier and who is premultiplied
  my $alias_list;
  for my $node (@{$attrs->{from}}) {

    my $j = $node;
    $j = $j->[0] if ref $j eq 'ARRAY';
    my $al = $j->{-alias}
      or next;

    $alias_list->{$al} = $j;

    $aliases_by_type->{multiplying}{$al} ||= { -parents => $j->{-join_path}||[] }
      # not array == {from} head == can't be multiplying
      if ref($node) eq 'ARRAY' and ! $j->{-is_single};

    $aliases_by_type->{premultiplied}{$al} ||= { -parents => $j->{-join_path}||[] }
      # parts of the path that are not us but are multiplying
      if grep { $aliases_by_type->{multiplying}{$_} }
          grep { $_ ne $al }
           map { values %$_ }
            @{ $j->{-join_path}||[] }
  }

  # get a column to source/alias map (including unambiguous unqualified ones)
  my $colinfo = $self->_resolve_column_info ($attrs->{from});

  # set up a botched SQLA
  my $sql_maker = $self->sql_maker;

  # these are throw away results, do not pollute the bind stack
  local $sql_maker->{where_bind};
  local $sql_maker->{group_bind};
  local $sql_maker->{having_bind};
  local $sql_maker->{from_bind};

  # we can't scan properly without any quoting (\b doesn't cut it
  # everywhere), so unless there is proper quoting set - use our
  # own weird impossible character.
  # Also in the case of no quoting, we need to explicitly disable
  # name_sep, otherwise sorry nasty legacy syntax like
  # { 'count(foo.id)' => { '>' => 3 } } will stop working >:(
  local $sql_maker->{quote_char} = $sql_maker->{quote_char};
  local $sql_maker->{name_sep} = $sql_maker->{name_sep};

  unless (defined $sql_maker->{quote_char} and length $sql_maker->{quote_char}) {
    $sql_maker->{quote_char} = ["\x00", "\xFF"];
    # if we don't unset it we screw up retarded but unfortunately working
    # 'MAX(foo.bar)' => { '>', 3 }
    $sql_maker->{name_sep} = '';
  }

  my ($lquote, $rquote, $sep) = map { quotemeta $_ } ($sql_maker->_quote_chars, $sql_maker->name_sep);

  # generate sql chunks
  my $to_scan = {
    restricting => [
      ($sql_maker->_recurse_where ($attrs->{where}))[0],
      $sql_maker->_parse_rs_attrs ({ having => $attrs->{having} }),
    ],
    grouping => [
      $sql_maker->_parse_rs_attrs ({ group_by => $attrs->{group_by} }),
    ],
    joining => [
      $sql_maker->_recurse_from (
        ref $attrs->{from}[0] eq 'ARRAY' ? $attrs->{from}[0][0] : $attrs->{from}[0],
        @{$attrs->{from}}[1 .. $#{$attrs->{from}}],
      ),
    ],
    selecting => [
      map { ($sql_maker->_recurse_fields($_))[0] } @{$attrs->{select}},
    ],
    ordering => [
      map { $_->[0] } $self->_extract_order_criteria ($attrs->{order_by}, $sql_maker),
    ],
  };

  # throw away empty chunks and all 2-value arrayrefs: the thinking is that these are
  # bind value specs left in by the sloppy renderer above. It is ok to do this
  # at this point, since we are going to end up rewriting this crap anyway
  for my $v (values %$to_scan) {
    my @nv;
    for (@$v) {
      next if (
        ! defined $_
          or
        (
          ref $_ eq 'ARRAY'
            and
          ( @$_ == 0 or @$_ == 2 )
        )
      );

      if (ref $_) {
        require Data::Dumper::Concise;
        $self->throw_exception("Unexpected ref in scan-plan: " . Data::Dumper::Concise::Dumper($v) );
      }

      push @nv, $_;
    }

    $v = \@nv;
  }

  # kill all selectors which look like a proper subquery
  # this is a sucky heuristic *BUT* - if we get it wrong the query will simply
  # fail to run, so we are relatively safe
  $to_scan->{selecting} = [ grep {
    $_ !~ / \A \s* \( \s* SELECT \s+ .+? \s+ FROM \s+ .+? \) \s* \z /xsi
  } @{ $to_scan->{selecting} || [] } ];

  # first see if we have any exact matches (qualified or unqualified)
  for my $type (keys %$to_scan) {
    for my $piece (@{$to_scan->{$type}}) {
      if ($colinfo->{$piece} and my $alias = $colinfo->{$piece}{-source_alias}) {
        $aliases_by_type->{$type}{$alias} ||= { -parents => $alias_list->{$alias}{-join_path}||[] };
        $aliases_by_type->{$type}{$alias}{-seen_columns}{$colinfo->{$piece}{-fq_colname}} = $piece;
      }
    }
  }

  # now loop through all fully qualified columns and get the corresponding
  # alias (should work even if they are in scalarrefs)
  for my $alias (keys %$alias_list) {
    my $al_re = qr/
      $lquote $alias $rquote $sep (?: $lquote ([^$rquote]+) $rquote )?
        |
      \b $alias \. ([^\s\)\($rquote]+)?
    /x;

    for my $type (keys %$to_scan) {
      for my $piece (@{$to_scan->{$type}}) {
        if (my @matches = $piece =~ /$al_re/g) {
          $aliases_by_type->{$type}{$alias} ||= { -parents => $alias_list->{$alias}{-join_path}||[] };
          $aliases_by_type->{$type}{$alias}{-seen_columns}{"$alias.$_"} = "$alias.$_"
            for grep { defined $_ } @matches;
        }
      }
    }
  }

  # now loop through unqualified column names, and try to locate them within
  # the chunks
  for my $col (keys %$colinfo) {
    next if $col =~ / \. /x;   # if column is qualified it was caught by the above

    my $col_re = qr/ $lquote ($col) $rquote /x;

    for my $type (keys %$to_scan) {
      for my $piece (@{$to_scan->{$type}}) {
        if ( my @matches = $piece =~ /$col_re/g) {
          my $alias = $colinfo->{$col}{-source_alias};
          $aliases_by_type->{$type}{$alias} ||= { -parents => $alias_list->{$alias}{-join_path}||[] };
          $aliases_by_type->{$type}{$alias}{-seen_columns}{"$alias.$_"} = $_
            for grep { defined $_ } @matches;
        }
      }
    }
  }

  # Add any non-left joins to the restriction list (such joins are indeed restrictions)
  for my $j (values %$alias_list) {
    my $alias = $j->{-alias} or next;
    $aliases_by_type->{restricting}{$alias} ||= { -parents => $j->{-join_path}||[] } if (
      (not $j->{-join_type})
        or
      ($j->{-join_type} !~ /^left (?: \s+ outer)? $/xi)
    );
  }

  for (keys %$aliases_by_type) {
    delete $aliases_by_type->{$_} unless keys %{$aliases_by_type->{$_}};
  }

  return $aliases_by_type;
}

# This is the engine behind { distinct => 1 } and the general
# complex prefetch grouper
sub _group_over_selection {
  my ($self, $attrs) = @_;

  my $colinfos = $self->_resolve_column_info ($attrs->{from});

  my (@group_by, %group_index);

  # the logic is: if it is a { func => val } we assume an aggregate,
  # otherwise if \'...' or \[...] we assume the user knows what is
  # going on thus group over it
  for (@{$attrs->{select}}) {
    if (! ref($_) or ref ($_) ne 'HASH' ) {
      push @group_by, $_;
      $group_index{$_}++;
      if ($colinfos->{$_} and $_ !~ /\./ ) {
        # add a fully qualified version as well
        $group_index{"$colinfos->{$_}{-source_alias}.$_"}++;
      }
    }
  }

  my @order_by = $self->_extract_order_criteria($attrs->{order_by})
    or return (\@group_by, $attrs->{order_by});

  # add any order_by parts that are not already present in the group_by
  # to maintain SQL cross-compatibility and general sanity
  #
  # also in case the original selection is *not* unique, or in case part
  # of the ORDER BY refers to a multiplier - we will need to replace the
  # skipped order_by elements with their MIN/MAX equivalents as to maintain
  # the proper overall order without polluting the group criteria (and
  # possibly changing the outcome entirely)

  my ($leftovers, $sql_maker, @new_order_by, $order_chunks, $aliastypes);

  my $group_already_unique = $self->_columns_comprise_identifying_set($colinfos, \@group_by);

  for my $o_idx (0 .. $#order_by) {

    # if the chunk is already a min/max function - there is nothing left to touch
    next if $order_by[$o_idx][0] =~ /^ (?: min | max ) \s* \( .+ \) $/ix;

    # only consider real columns (for functions the user got to do an explicit group_by)
    my $chunk_ci;
    if (
      @{$order_by[$o_idx]} != 1
        or
      # only declare an unknown *plain* identifier as "leftover" if we are called with
      # aliastypes to examine. If there are none - we are still in _resolve_attrs, and
      # can just assume the user knows what they want
      ( ! ( $chunk_ci = $colinfos->{$order_by[$o_idx][0]} ) and $attrs->{_aliastypes} )
    ) {
      push @$leftovers, $order_by[$o_idx][0];
    }

    next unless $chunk_ci;

    # no duplication of group criteria
    next if $group_index{$chunk_ci->{-fq_colname}};

    $aliastypes ||= (
      $attrs->{_aliastypes}
        or
      $self->_resolve_aliastypes_from_select_args({
        from => $attrs->{from},
        order_by => $attrs->{order_by},
      })
    ) if $group_already_unique;

    # check that we are not ordering by a multiplier (if a check is requested at all)
    if (
      $group_already_unique
        and
      ! $aliastypes->{multiplying}{$chunk_ci->{-source_alias}}
        and
      ! $aliastypes->{premultiplied}{$chunk_ci->{-source_alias}}
    ) {
      push @group_by, $chunk_ci->{-fq_colname};
      $group_index{$chunk_ci->{-fq_colname}}++
    }
    else {
      # We need to order by external columns without adding them to the group
      # (eiehter a non-unique selection, or a multi-external)
      #
      # This doesn't really make sense in SQL, however from DBICs point
      # of view is rather valid (e.g. order the leftmost objects by whatever
      # criteria and get the offset/rows many). There is a way around
      # this however in SQL - we simply tae the direction of each piece
      # of the external order and convert them to MIN(X) for ASC or MAX(X)
      # for DESC, and group_by the root columns. The end result should be
      # exactly what we expect
      #

      # both populated on the first loop over $o_idx
      $sql_maker ||= $self->sql_maker;
      $order_chunks ||= [
        map { ref $_ eq 'ARRAY' ? $_ : [ $_ ] } $sql_maker->_order_by_chunks($attrs->{order_by})
      ];

      my ($chunk, $is_desc) = $sql_maker->_split_order_chunk($order_chunks->[$o_idx][0]);

      $new_order_by[$o_idx] = \[
        sprintf( '%s( %s )%s',
          $self->_minmax_operator_for_datatype($chunk_ci->{data_type}, $is_desc),
          $chunk,
          ($is_desc ? ' DESC' : ''),
        ),
        @ {$order_chunks->[$o_idx]} [ 1 .. $#{$order_chunks->[$o_idx]} ]
      ];
    }
  }

  $self->throw_exception ( sprintf
    'Unable to programatically derive a required group_by from the supplied '
  . 'order_by criteria. To proceed either add an explicit group_by, or '
  . 'simplify your order_by to only include plain columns '
  . '(supplied order_by: %s)',
    join ', ', map { "'$_'" } @$leftovers,
  ) if $leftovers;

  # recreate the untouched order parts
  if (@new_order_by) {
    $new_order_by[$_] ||= \ $order_chunks->[$_] for ( 0 .. $#$order_chunks );
  }

  return (
    \@group_by,
    (@new_order_by ? \@new_order_by : $attrs->{order_by} ),  # same ref as original == unchanged
  );
}

sub _minmax_operator_for_datatype {
  #my ($self, $datatype, $want_max) = @_;

  $_[2] ? 'MAX' : 'MIN';
}

sub _resolve_ident_sources {
  my ($self, $ident) = @_;

  my $alias2source = {};

  # the reason this is so contrived is that $ident may be a {from}
  # structure, specifying multiple tables to join
  if ( blessed $ident && $ident->isa("DBIx::Class::ResultSource") ) {
    # this is compat mode for insert/update/delete which do not deal with aliases
    $alias2source->{me} = $ident;
  }
  elsif (ref $ident eq 'ARRAY') {

    for (@$ident) {
      my $tabinfo;
      if (ref $_ eq 'HASH') {
        $tabinfo = $_;
      }
      if (ref $_ eq 'ARRAY' and ref $_->[0] eq 'HASH') {
        $tabinfo = $_->[0];
      }

      $alias2source->{$tabinfo->{-alias}} = $tabinfo->{-rsrc}
        if ($tabinfo->{-rsrc});
    }
  }

  return $alias2source;
}

# Takes $ident, \@column_names
#
# returns { $column_name => \%column_info, ... }
# also note: this adds -result_source => $rsrc to the column info
#
# If no columns_names are supplied returns info about *all* columns
# for all sources
sub _resolve_column_info {
  my ($self, $ident, $colnames) = @_;

  return {} if $colnames and ! @$colnames;

  my $alias2src = $self->_resolve_ident_sources($ident);

  my (%seen_cols, @auto_colnames);

  # compile a global list of column names, to be able to properly
  # disambiguate unqualified column names (if at all possible)
  for my $alias (keys %$alias2src) {
    my $rsrc = $alias2src->{$alias};
    for my $colname ($rsrc->columns) {
      push @{$seen_cols{$colname}}, $alias;
      push @auto_colnames, "$alias.$colname" unless $colnames;
    }
  }

  $colnames ||= [
    @auto_colnames,
    grep { @{$seen_cols{$_}} == 1 } (keys %seen_cols),
  ];

  my (%return, $colinfos);
  foreach my $col (@$colnames) {
    my ($source_alias, $colname) = $col =~ m/^ (?: ([^\.]+) \. )? (.+) $/x;

    # if the column was seen exactly once - we know which rsrc it came from
    $source_alias ||= $seen_cols{$colname}[0]
      if ($seen_cols{$colname} and @{$seen_cols{$colname}} == 1);

    next unless $source_alias;

    my $rsrc = $alias2src->{$source_alias}
      or next;

    $return{$col} = {
      %{
          ( $colinfos->{$source_alias} ||= $rsrc->columns_info )->{$colname}
            ||
          $self->throw_exception(
            "No such column '$colname' on source " . $rsrc->source_name
          );
      },
      -result_source => $rsrc,
      -source_alias => $source_alias,
      -fq_colname => $col eq $colname ? "$source_alias.$col" : $col,
      -colname => $colname,
    };

    $return{"$source_alias.$colname"} = $return{$col} if $col eq $colname;
  }

  return \%return;
}

# The DBIC relationship chaining implementation is pretty simple - every
# new related_relationship is pushed onto the {from} stack, and the {select}
# window simply slides further in. This means that when we count somewhere
# in the middle, we got to make sure that everything in the join chain is an
# actual inner join, otherwise the count will come back with unpredictable
# results (a resultset may be generated with _some_ rows regardless of if
# the relation which the $rs currently selects has rows or not). E.g.
# $artist_rs->cds->count - normally generates:
# SELECT COUNT( * ) FROM artist me LEFT JOIN cd cds ON cds.artist = me.artistid
# which actually returns the number of artists * (number of cds || 1)
#
# So what we do here is crawl {from}, determine if the current alias is at
# the top of the stack, and if not - make sure the chain is inner-joined down
# to the root.
#
sub _inner_join_to_node {
  my ($self, $from, $alias) = @_;

  my $switch_branch = $self->_find_join_path_to_node($from, $alias);

  return $from unless @{$switch_branch||[]};

  # So it looks like we will have to switch some stuff around.
  # local() is useless here as we will be leaving the scope
  # anyway, and deep cloning is just too fucking expensive
  # So replace the first hashref in the node arrayref manually
  my @new_from = ($from->[0]);
  my $sw_idx = { map { (values %$_), 1 } @$switch_branch }; #there's one k/v per join-path

  for my $j (@{$from}[1 .. $#$from]) {
    my $jalias = $j->[0]{-alias};

    if ($sw_idx->{$jalias}) {
      my %attrs = %{$j->[0]};
      delete $attrs{-join_type};
      push @new_from, [
        \%attrs,
        @{$j}[ 1 .. $#$j ],
      ];
    }
    else {
      push @new_from, $j;
    }
  }

  return \@new_from;
}

sub _find_join_path_to_node {
  my ($self, $from, $target_alias) = @_;

  # subqueries and other oddness are naturally not supported
  return undef if (
    ref $from ne 'ARRAY'
      ||
    ref $from->[0] ne 'HASH'
      ||
    ! defined $from->[0]{-alias}
  );

  # no path - the head is the alias
  return [] if $from->[0]{-alias} eq $target_alias;

  for my $i (1 .. $#$from) {
    return $from->[$i][0]{-join_path} if ( ($from->[$i][0]{-alias}||'') eq $target_alias );
  }

  # something else went quite wrong
  return undef;
}

sub _extract_order_criteria {
  my ($self, $order_by, $sql_maker) = @_;

  my $parser = sub {
    my ($sql_maker, $order_by, $orig_quote_chars) = @_;

    return scalar $sql_maker->_order_by_chunks ($order_by)
      unless wantarray;

    my ($lq, $rq, $sep) = map { quotemeta($_) } (
      ($orig_quote_chars ? @$orig_quote_chars : $sql_maker->_quote_chars),
      $sql_maker->name_sep
    );

    my @chunks;
    for ($sql_maker->_order_by_chunks ($order_by) ) {
      my $chunk = ref $_ ? [ @$_ ] : [ $_ ];
      ($chunk->[0]) = $sql_maker->_split_order_chunk($chunk->[0]);

      # order criteria may have come back pre-quoted (literals and whatnot)
      # this is fragile, but the best we can currently do
      $chunk->[0] =~ s/^ $lq (.+?) $rq $sep $lq (.+?) $rq $/"$1.$2"/xe
        or $chunk->[0] =~ s/^ $lq (.+) $rq $/$1/x;

      push @chunks, $chunk;
    }

    return @chunks;
  };

  if ($sql_maker) {
    return $parser->($sql_maker, $order_by);
  }
  else {
    $sql_maker = $self->sql_maker;

    # pass these in to deal with literals coming from
    # the user or the deep guts of prefetch
    my $orig_quote_chars = [$sql_maker->_quote_chars];

    local $sql_maker->{quote_char};
    return $parser->($sql_maker, $order_by, $orig_quote_chars);
  }
}

sub _order_by_is_stable {
  my ($self, $ident, $order_by, $where) = @_;

  my @cols = (
    ( map { $_->[0] } $self->_extract_order_criteria($order_by) ),
    ( $where ? keys %{ $self->_extract_fixed_condition_columns($where) } : () ),
  ) or return 0;

  my $colinfo = $self->_resolve_column_info($ident, \@cols);

  return keys %$colinfo
    ? $self->_columns_comprise_identifying_set( $colinfo,  \@cols )
    : 0
  ;
}

sub _columns_comprise_identifying_set {
  my ($self, $colinfo, $columns) = @_;

  my $cols_per_src;
  $cols_per_src -> {$_->{-source_alias}} -> {$_->{-colname}} = $_
    for grep { defined $_ } @{$colinfo}{@$columns};

  for (values %$cols_per_src) {
    my $src = (values %$_)[0]->{-result_source};
    return 1 if $src->_identifying_column_set($_);
  }

  return 0;
}

# this is almost similar to _order_by_is_stable, except it takes
# a single rsrc, and will succeed only if the first portion of the order
# by is stable.
# returns that portion as a colinfo hashref on success
sub _extract_colinfo_of_stable_main_source_order_by_portion {
  my ($self, $attrs) = @_;

  my $nodes = $self->_find_join_path_to_node($attrs->{from}, $attrs->{alias});

  return unless defined $nodes;

  my @ord_cols = map
    { $_->[0] }
    ( $self->_extract_order_criteria($attrs->{order_by}) )
  ;
  return unless @ord_cols;

  my $valid_aliases = { map { $_ => 1 } (
    $attrs->{from}[0]{-alias},
    map { values %$_ } @$nodes,
  ) };

  my $colinfos = $self->_resolve_column_info($attrs->{from});

  my ($colinfos_to_return, $seen_main_src_cols);

  for my $col (@ord_cols) {
    # if order criteria is unresolvable - there is nothing we can do
    my $colinfo = $colinfos->{$col} or last;

    # if we reached the end of the allowed aliases - also nothing we can do
    last unless $valid_aliases->{$colinfo->{-source_alias}};

    $colinfos_to_return->{$col} = $colinfo;

    $seen_main_src_cols->{$colinfo->{-colname}} = 1
      if $colinfo->{-source_alias} eq $attrs->{alias};
  }

  # FIXME the condition may be singling out things on its own, so we
  # conceivable could come back wi "stable-ordered by nothing"
  # not confient enough in the parser yet, so punt for the time being
  return unless $seen_main_src_cols;

  my $main_src_fixed_cols_from_cond = [ $attrs->{where}
    ? (
      map
      {
        ( $colinfos->{$_} and $colinfos->{$_}{-source_alias} eq $attrs->{alias} )
          ? $colinfos->{$_}{-colname}
          : ()
      }
      keys %{ $self->_extract_fixed_condition_columns($attrs->{where}) }
    )
    : ()
  ];

  return $attrs->{result_source}->_identifying_column_set([
    keys %$seen_main_src_cols,
    @$main_src_fixed_cols_from_cond,
  ]) ? $colinfos_to_return : ();
}

# Attempts to flatten a passed in SQLA condition as much as possible towards
# a plain hashref, *without* altering its semantics. Required by
# create/populate being able to extract definitive conditions from preexisting
# resultset {where} stacks
#
# FIXME - while relatively robust, this is still imperfect, one of the first
# things to tackle with DQ
sub _collapse_cond {
  my ($self, $where, $where_is_anded_array) = @_;

  my $fin;

  if (! $where) {
    return;
  }
  elsif ($where_is_anded_array or ref $where eq 'HASH') {

    my @pairs;

    my @pieces = $where_is_anded_array ? @$where : $where;
    while (@pieces) {
      my $chunk = shift @pieces;

      if (ref $chunk eq 'HASH') {
        for (sort keys %$chunk) {

          # Match SQLA 1.79 behavior
          if ($_ eq '') {
            is_literal_value($chunk->{$_})
              ? carp 'Hash-pairs consisting of an empty string with a literal are deprecated, use -and => [ $literal ] instead'
              : $self->throw_exception("Supplying an empty left hand side argument is not supported in hash-pairs")
            ;
          }

          push @pairs, $_ => $chunk->{$_};
        }
      }
      elsif (ref $chunk eq 'ARRAY') {
        push @pairs, -or => $chunk
          if @$chunk;
      }
      elsif ( ! length ref $chunk) {

        # Match SQLA 1.79 behavior
        $self->throw_exception("Supplying an empty left hand side argument is not supported in array-pairs")
          if $where_is_anded_array and (! defined $chunk or $chunk eq '');

        push @pairs, $chunk, shift @pieces;
      }
      else {
        push @pairs, '', $chunk;
      }
    }

    return unless @pairs;

    my @conds = $self->_collapse_cond_unroll_pairs(\@pairs)
      or return;

    # Consolidate various @conds back into something more compact
    for my $c (@conds) {
      if (ref $c ne 'HASH') {
        push @{$fin->{-and}}, $c;
      }
      else {
        for my $col (sort keys %$c) {

          # consolidate all -and nodes
          if ($col =~ /^\-and$/i) {
            push @{$fin->{-and}},
              ref $c->{$col} eq 'ARRAY' ? @{$c->{$col}}
            : ref $c->{$col} eq 'HASH' ? %{$c->{$col}}
            : { $col => $c->{$col} }
            ;
          }
          elsif ($col =~ /^\-/) {
            push @{$fin->{-and}}, { $col => $c->{$col} };
          }
          elsif (exists $fin->{$col}) {
            $fin->{$col} = [ -and => map {
              (ref $_ eq 'ARRAY' and ($_->[0]||'') =~ /^\-and$/i )
                ? @{$_}[1..$#$_]
                : $_
              ;
            } ($fin->{$col}, $c->{$col}) ];
          }
          else {
            $fin->{$col} = $c->{$col};
          }
        }
      }
    }
  }
  elsif (ref $where eq 'ARRAY') {
    # we are always at top-level here, it is safe to dump empty *standalone* pieces
    my $fin_idx;

    for (my $i = 0; $i <= $#$where; $i++ ) {

      # Match SQLA 1.79 behavior
      $self->throw_exception(
        "Supplying an empty left hand side argument is not supported in array-pairs"
      ) if (! defined $where->[$i] or ! length $where->[$i]);

      my $logic_mod = lc ( ($where->[$i] =~ /^(\-(?:and|or))$/i)[0] || '' );

      if ($logic_mod) {
        $i++;
        $self->throw_exception("Unsupported top-level op/arg pair: [ $logic_mod => $where->[$i] ]")
          unless ref $where->[$i] eq 'HASH' or ref $where->[$i] eq 'ARRAY';

        my $sub_elt = $self->_collapse_cond({ $logic_mod => $where->[$i] })
          or next;

        my @keys = keys %$sub_elt;
        if ( @keys == 1 and $keys[0] !~ /^\-/ ) {
          $fin_idx->{ "COL_$keys[0]_" . serialize $sub_elt } = $sub_elt;
        }
        else {
          $fin_idx->{ "SER_" . serialize $sub_elt } = $sub_elt;
        }
      }
      elsif (! length ref $where->[$i] ) {
        my $sub_elt = $self->_collapse_cond({ @{$where}[$i, $i+1] })
          or next;

        $fin_idx->{ "COL_$where->[$i]_" . serialize $sub_elt } = $sub_elt;
        $i++;
      }
      else {
        $fin_idx->{ "SER_" . serialize $where->[$i] } = $self->_collapse_cond( $where->[$i] ) || next;
      }
    }

    if (! $fin_idx) {
      return;
    }
    elsif ( keys %$fin_idx == 1 ) {
      $fin = (values %$fin_idx)[0];
    }
    else {
      my @or;

      # at this point everything is at most one level deep - unroll if needed
      for (sort keys %$fin_idx) {
        if ( ref $fin_idx->{$_} eq 'HASH' and keys %{$fin_idx->{$_}} == 1 ) {
          my ($l, $r) = %{$fin_idx->{$_}};

          if (
            ref $r eq 'ARRAY'
              and
            (
              ( @$r == 1 and $l =~ /^\-and$/i )
                or
              $l =~ /^\-or$/i
            )
          ) {
            push @or, @$r
          }

          elsif (
            ref $r eq 'HASH'
              and
            keys %$r == 1
              and
            $l =~ /^\-(?:and|or)$/i
          ) {
            push @or, %$r;
          }

          else {
            push @or, $l, $r;
          }
        }
        else {
          push @or, $fin_idx->{$_};
        }
      }

      $fin->{-or} = \@or;
    }
  }
  else {
    # not a hash not an array
    $fin = { -and => [ $where ] };
  }

  # unroll single-element -and's
  while (
    $fin->{-and}
      and
    @{$fin->{-and}} < 2
  ) {
    my $and = delete $fin->{-and};
    last if @$and == 0;

    # at this point we have @$and == 1
    if (
      ref $and->[0] eq 'HASH'
        and
      ! grep { exists $fin->{$_} } keys %{$and->[0]}
    ) {
      $fin = {
        %$fin, %{$and->[0]}
      };
    }
    else {
      $fin->{-and} = $and;
      last;
    }
  }

  # compress same-column conds found in $fin
  for my $col ( grep { $_ !~ /^\-/ } keys %$fin ) {
    next unless ref $fin->{$col} eq 'ARRAY' and ($fin->{$col}[0]||'') =~ /^\-and$/i;
    my $val_bag = { map {
      (! defined $_ )                          ? ( UNDEF => undef )
    : ( ! length ref $_ or is_plain_value $_ ) ? ( "VAL_$_" => $_ )
    : ( ( 'SER_' . serialize $_ ) => $_ )
    } @{$fin->{$col}}[1 .. $#{$fin->{$col}}] };

    if (keys %$val_bag == 1 ) {
      ($fin->{$col}) = values %$val_bag;
    }
    else {
      $fin->{$col} = [ -and => map { $val_bag->{$_} } sort keys %$val_bag ];
    }
  }

  return keys %$fin ? $fin : ();
}

sub _collapse_cond_unroll_pairs {
  my ($self, $pairs) = @_;

  my @conds;

  while (@$pairs) {
    my ($lhs, $rhs) = splice @$pairs, 0, 2;

    if ($lhs eq '') {
      push @conds, $self->_collapse_cond($rhs);
    }
    elsif ( $lhs =~ /^\-and$/i ) {
      push @conds, $self->_collapse_cond($rhs, (ref $rhs eq 'ARRAY'));
    }
    elsif ( $lhs =~ /^\-or$/i ) {
      push @conds, $self->_collapse_cond(
        (ref $rhs eq 'HASH') ? [ map { $_ => $rhs->{$_} } sort keys %$rhs ] : $rhs
      );
    }
    else {
      if (ref $rhs eq 'HASH' and ! keys %$rhs) {
        # FIXME - SQLA seems to be doing... nothing...?
      }
      # normalize top level -ident, for saner extract_fixed_condition_columns code
      elsif (ref $rhs eq 'HASH' and keys %$rhs == 1 and exists $rhs->{-ident}) {
        push @conds, { $lhs => { '=', $rhs } };
      }
      elsif (ref $rhs eq 'HASH' and keys %$rhs == 1 and exists $rhs->{-value} and is_plain_value $rhs->{-value}) {
        push @conds, { $lhs => $rhs->{-value} };
      }
      elsif (ref $rhs eq 'HASH' and keys %$rhs == 1 and exists $rhs->{'='}) {
        if ( length ref $rhs->{'='} and is_literal_value $rhs->{'='} ) {
          push @conds, { $lhs => $rhs };
        }
        else {
          for my $p ($self->_collapse_cond_unroll_pairs([ $lhs => $rhs->{'='} ])) {

            # extra sanity check
            if (keys %$p > 1) {
              require Data::Dumper::Concise;
              local $Data::Dumper::Deepcopy = 1;
              $self->throw_exception(
                "Internal error: unexpected collapse unroll:"
              . Data::Dumper::Concise::Dumper { in => { $lhs => $rhs }, out => $p }
              );
            }

            my ($l, $r) = %$p;

            push @conds, (
              ! length ref $r
                or
              # the unroller recursion may return a '=' prepended value already
              ref $r eq 'HASH' and keys %$rhs == 1 and exists $rhs->{'='}
                or
              is_plain_value($r)
            )
              ? { $l => $r }
              : { $l => { '=' => $r } }
            ;
          }
        }
      }
      elsif (ref $rhs eq 'ARRAY') {
        # some of these conditionals encounter multi-values - roll them out using
        # an unshift, which will cause extra looping in the while{} above
        if (! @$rhs ) {
          push @conds, { $lhs => [] };
        }
        elsif ( ($rhs->[0]||'') =~ /^\-(?:and|or)$/i ) {
          $self->throw_exception("Value modifier not followed by any values: $lhs => [ $rhs->[0] ] ")
            if  @$rhs == 1;

          if( $rhs->[0] =~ /^\-and$/i ) {
            unshift @$pairs, map { $lhs => $_ } @{$rhs}[1..$#$rhs];
          }
          # if not an AND then it's an OR
          elsif(@$rhs == 2) {
            unshift @$pairs, $lhs => $rhs->[1];
          }
          else {
            push @conds, { $lhs => [ @{$rhs}[1..$#$rhs] ] };
          }
        }
        elsif (@$rhs == 1) {
          unshift @$pairs, $lhs => $rhs->[0];
        }
        else {
          push @conds, { $lhs => $rhs };
        }
      }
      # unroll func + { -value => ... }
      elsif (
        ref $rhs eq 'HASH'
          and
        ( my ($subop) = keys %$rhs ) == 1
          and
        length ref ((values %$rhs)[0])
          and
        my $vref = is_plain_value( (values %$rhs)[0] )
      ) {
        push @conds, { $lhs => { $subop => $$vref } }
      }
      else {
        push @conds, { $lhs => $rhs };
      }
    }
  }

  return @conds;
}

# Analyzes a given condition and attempts to extract all columns
# with a definitive fixed-condition criteria. Returns a hashref
# of k/v pairs suitable to be passed to set_columns(), with a
# MAJOR CAVEAT - multi-value (contradictory) equalities are still
# represented as a reference to the UNRESOVABLE_CONDITION constant
# The reason we do this is that some codepaths only care about the
# codition being stable, as opposed to actually making sense
#
# The normal mode is used to figure out if a resultset is constrained
# to a column which is part of a unique constraint, which in turn
# allows us to better predict how ordering will behave etc.
#
# With the optional "consider_nulls" boolean argument, the function
# is instead used to infer inambiguous values from conditions
# (e.g. the inheritance of resultset conditions on new_result)
#
sub _extract_fixed_condition_columns {
  my ($self, $where, $consider_nulls) = @_;
  my $where_hash = $self->_collapse_cond($_[1]);

  my $res = {};
  my ($c, $v);
  for $c (keys %$where_hash) {
    my $vals;

    if (!defined ($v = $where_hash->{$c}) ) {
      $vals->{UNDEF} = $v if $consider_nulls
    }
    elsif (
      ref $v eq 'HASH'
        and
      keys %$v == 1
    ) {
      if (exists $v->{-value}) {
        if (defined $v->{-value}) {
          $vals->{"VAL_$v->{-value}"} = $v->{-value}
        }
        elsif( $consider_nulls ) {
          $vals->{UNDEF} = $v->{-value};
        }
      }
      # do not need to check for plain values - _collapse_cond did it for us
      elsif(
        length ref $v->{'='}
          and
        (
          ( ref $v->{'='} eq 'HASH' and keys %{$v->{'='}} == 1 and exists $v->{'='}{-ident} )
            or
          is_literal_value($v->{'='})
        )
       ) {
        $vals->{ 'SER_' . serialize $v->{'='} } = $v->{'='};
      }
    }
    elsif (
      ! length ref $v
        or
      is_plain_value ($v)
    ) {
      $vals->{"VAL_$v"} = $v;
    }
    elsif (ref $v eq 'ARRAY' and ($v->[0]||'') eq '-and') {
      for ( @{$v}[1..$#$v] ) {
        my $subval = $self->_extract_fixed_condition_columns({ $c => $_ }, 'consider nulls');  # always fish nulls out on recursion
        next unless exists $subval->{$c};  # didn't find anything
        $vals->{
          ! defined $subval->{$c}                                        ? 'UNDEF'
        : ( ! length ref $subval->{$c} or is_plain_value $subval->{$c} ) ? "VAL_$subval->{$c}"
        : ( 'SER_' . serialize $subval->{$c} )
        } = $subval->{$c};
      }
    }

    if (keys %$vals == 1) {
      ($res->{$c}) = (values %$vals)
        unless !$consider_nulls and exists $vals->{UNDEF};
    }
    elsif (keys %$vals > 1) {
      $res->{$c} = UNRESOLVABLE_CONDITION;
    }
  }

  $res;
}

1;
