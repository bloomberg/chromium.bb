package # hide from the pauses
  DBIx::Class::ResultSource::RowParser::Util;

use strict;
use warnings;

use List::Util 'first';
use DBIx::Class::_Util 'perlstring';

use constant HAS_DOR => ( $] < 5.010 ? 0 : 1 );

use base 'Exporter';
our @EXPORT_OK = qw(
  assemble_simple_parser
  assemble_collapsing_parser
);

# working title - we are hoping to extract this eventually...
our $null_branch_class = 'DBIx::ResultParser::RelatedNullBranch';

sub __wrap_in_strictured_scope {
  "  { use strict; use warnings; use warnings FATAL => 'uninitialized';\n$_[0]\n  }"
}

sub assemble_simple_parser {
  #my ($args) = @_;

  # the non-collapsing assembler is easy
  # FIXME SUBOPTIMAL there could be a yet faster way to do things here, but
  # need to try an actual implementation and benchmark it:
  #
  # <timbunce_> First setup the nested data structure you want for each row
  #   Then call bind_col() to alias the row fields into the right place in
  #   the data structure, then to fetch the data do:
  # push @rows, dclone($row_data_struct) while ($sth->fetchrow);
  #
  my $parser_src = sprintf('$_ = %s for @{$_[0]}', __visit_infmap_simple($_[0]) );

  # change the quoted placeholders to unquoted alias-references
  $parser_src =~ s/ \' \xFF__VALPOS__(\d+)__\xFF \' /"\$_->[$1]"/gex;

  __wrap_in_strictured_scope($parser_src);
}

# the simple non-collapsing nested structure recursor
sub __visit_infmap_simple {
  my $args = shift;

  my $my_cols = {};
  my $rel_cols;
  for (keys %{$args->{val_index}}) {
    if ($_ =~ /^ ([^\.]+) \. (.+) /x) {
      $rel_cols->{$1}{$2} = $args->{val_index}{$_};
    }
    else {
      $my_cols->{$_} = $args->{val_index}{$_};
    }
  }

  my @relperl;
  for my $rel (sort keys %$rel_cols) {

    my $rel_struct = __visit_infmap_simple({ %$args,
      val_index => $rel_cols->{$rel},
    });

    if (keys %$my_cols) {

      my $branch_null_checks = join ' && ', map
        { "( ! defined '\xFF__VALPOS__${_}__\xFF' )" }
        sort { $a <=> $b } values %{$rel_cols->{$rel}}
      ;

      if ($args->{prune_null_branches}) {
        $rel_struct = sprintf ( '( (%s) ? undef : %s )',
          $branch_null_checks,
          $rel_struct,
        );
      }
      else {
        $rel_struct = sprintf ( '( (%s) ? bless( (%s), %s ) : %s )',
          $branch_null_checks,
          $rel_struct,
          perlstring($null_branch_class),
          $rel_struct,
        );
      }
    }

    push @relperl, sprintf '( %s => %s )',
      perlstring($rel),
      $rel_struct,
    ;

  }

  my $me_struct;
  $me_struct = __result_struct_to_source($my_cols) if keys %$my_cols;

  if ($args->{hri_style}) {
    $me_struct =~ s/^ \s* \{ | \} \s* $//gx
      if $me_struct;

    return sprintf '{ %s }', join (', ', $me_struct||(), @relperl);
  }
  else {
    return sprintf '[%s]', join (',',
      $me_struct || 'undef',
      @relperl ? sprintf ('{ %s }', join (',', @relperl)) : (),
    );
  }
}

sub assemble_collapsing_parser {
  my $args = shift;

  # it may get unset further down
  my $no_rowid_container = $args->{prune_null_branches};

  my ($top_node_key, $top_node_key_assembler);

  if (scalar @{$args->{collapse_map}{-identifying_columns}}) {
    $top_node_key = join ('', map
      { "{'\xFF__IDVALPOS__${_}__\xFF'}" }
      @{$args->{collapse_map}{-identifying_columns}}
    );
  }
  elsif( my @variants = @{$args->{collapse_map}{-identifying_columns_variants}} ) {

    my @path_parts = map { sprintf
      "( ( defined '\xFF__VALPOS__%d__\xFF' ) && (join qq(\xFF), '', %s, '') )",
      $_->[0],  # checking just first is enough - one ID defined, all defined
      ( join ', ', map { "'\xFF__VALPOS__${_}__\xFF'" } @$_ ),
    } @variants;

    my $virtual_column_idx = (scalar keys %{$args->{val_index}} ) + 1;

    $top_node_key = "{'\xFF__IDVALPOS__${virtual_column_idx}__\xFF'}";

    $top_node_key_assembler = sprintf "'\xFF__IDVALPOS__%d__\xFF' = (%s);",
      $virtual_column_idx,
      "\n" . join( "\n  or\n", @path_parts, qq{"\0\$rows_pos\0"} )
    ;

    $args->{collapse_map} = {
      %{$args->{collapse_map}},
      -custom_node_key => $top_node_key,
    };

    $no_rowid_container = 0;
  }
  else {
    die('Unexpected collapse map contents');
  }

  my ($data_assemblers, $stats) = __visit_infmap_collapse ($args);

  my @idcol_args = $no_rowid_container ? ('', '') : (
    ', %cur_row_ids', # only declare the variable if we'll use it
    join ("\n", map {
      my $quoted_null_val = qq( "\0NULL\xFF\${rows_pos}\xFF${_}\0" );
      qq(\$cur_row_ids{$_} = ) . (
        # in case we prune - we will never hit these undefs
        $args->{prune_null_branches} ? qq( \$cur_row_data->[$_]; )
        : HAS_DOR                    ? qq( \$cur_row_data->[$_] // $quoted_null_val; )
        :                              qq( defined(\$cur_row_data->[$_]) ? \$cur_row_data->[$_] : $quoted_null_val; )
      )
    } sort { $a <=> $b } keys %{ $stats->{idcols_seen} } ),
  );

  my $parser_src = sprintf (<<'EOS', @idcol_args, $top_node_key_assembler||'', $top_node_key, join( "\n", @{$data_assemblers||[]} ) );
### BEGIN LITERAL STRING EVAL
  my $rows_pos = 0;
  my ($result_pos, @collapse_idx, $cur_row_data %1$s);

  # this loop is a bit arcane - the rationale is that the passed in
  # $_[0] will either have only one row (->next) or will have all
  # rows already pulled in (->all and/or unordered). Given that the
  # result can be rather large - we reuse the same already allocated
  # array, since the collapsed prefetch is smaller by definition.
  # At the end we cut the leftovers away and move on.
  while ($cur_row_data = (
    ( $rows_pos >= 0 and $_[0][$rows_pos++] )
      or
    ( $_[1] and $rows_pos = -1 and $_[1]->() )
  ) ) {

    # this code exists only when we are using a cur_row_ids
    # furthermore the undef checks may or may not be there
    # depending on whether we prune or not
    #
    # due to left joins some of the ids may be NULL/undef, and
    # won't play well when used as hash lookups
    # we also need to differentiate NULLs on per-row/per-col basis
    # (otherwise folding of optional 1:1s will be greatly confused
%2$s

    # in the case of an underdefined root - calculate the virtual id (otherwise no code at all)
%3$s

    # if we were supplied a coderef - we are collapsing lazily (the set
    # is ordered properly)
    # as long as we have a result already and the next result is new we
    # return the pre-read data and bail
$_[1] and $result_pos and ! $collapse_idx[0]%4$s and (unshift @{$_[2]}, $cur_row_data) and last;

    # the rel assemblers
%5$s

  }

  $#{$_[0]} = $result_pos - 1; # truncate the passed in array to where we filled it with results
### END LITERAL STRING EVAL
EOS

  # !!! note - different var than the one above
  # change the quoted placeholders to unquoted alias-references
  $parser_src =~ s/ \' \xFF__VALPOS__(\d+)__\xFF \' /"\$cur_row_data->[$1]"/gex;
  $parser_src =~ s/
    \' \xFF__IDVALPOS__(\d+)__\xFF \'
  /
    $no_rowid_container ? "\$cur_row_data->[$1]" : "\$cur_row_ids{$1}"
  /gex;

  __wrap_in_strictured_scope($parser_src);
}


# the collapsing nested structure recursor
sub __visit_infmap_collapse {
  my $args = {%{ shift() }};

  my $cur_node_idx = ${ $args->{-node_idx_counter} ||= \do { my $x = 0} }++;

  my ($my_cols, $rel_cols) = {};
  for ( keys %{$args->{val_index}} ) {
    if ($_ =~ /^ ([^\.]+) \. (.+) /x) {
      $rel_cols->{$1}{$2} = $args->{val_index}{$_};
    }
    else {
      $my_cols->{$_} = $args->{val_index}{$_};
    }
  }


  if ($args->{hri_style}) {
    delete $my_cols->{$_} for grep { $rel_cols->{$_} } keys %$my_cols;
  }

  my $me_struct;
  $me_struct = __result_struct_to_source($my_cols) if keys %$my_cols;

  $me_struct = sprintf( '[ %s ]', $me_struct||'' )
    unless $args->{hri_style};


  my $node_key = $args->{collapse_map}->{-custom_node_key} || join ('', map
    { "{'\xFF__IDVALPOS__${_}__\xFF'}" }
    @{$args->{collapse_map}->{-identifying_columns}}
  );
  my $node_idx_slot = sprintf '$collapse_idx[%d]%s', $cur_node_idx, $node_key;


  my @src;

  if ($cur_node_idx == 0) {
    push @src, sprintf( '%s %s $_[0][$result_pos++] = %s;',
      $node_idx_slot,
      (HAS_DOR ? '//=' : '||='),
      $me_struct || '{}',
    );
  }
  else {
    my $parent_attach_slot = sprintf( '$collapse_idx[%d]%s%s{%s}',
      @{$args}{qw/-parent_node_idx -parent_node_key/},
      $args->{hri_style} ? '' : '[1]',
      perlstring($args->{-node_rel_name}),
    );

    if ($args->{collapse_map}->{-is_single}) {
      push @src, sprintf ( '%s %s %s%s;',
        $parent_attach_slot,
        (HAS_DOR ? '//=' : '||='),
        $node_idx_slot,
        $me_struct ? " = $me_struct" : '',
      );
    }
    else {
      push @src, sprintf('(! %s) and push @{%s}, %s%s;',
        $node_idx_slot,
        $parent_attach_slot,
        $node_idx_slot,
        $me_struct ? " = $me_struct" : '',
      );
    }
  }

  my $known_present_ids = { map { $_ => 1 } @{$args->{collapse_map}{-identifying_columns}} };
  my ($stats, $rel_src);

  for my $rel (sort keys %$rel_cols) {

    my $relinfo = $args->{collapse_map}{$rel};

    ($rel_src, $stats->{$rel}) = __visit_infmap_collapse({ %$args,
      val_index => $rel_cols->{$rel},
      collapse_map => $relinfo,
      -parent_node_idx => $cur_node_idx,
      -parent_node_key => $node_key,
      -node_rel_name => $rel,
    });

    my $rel_src_pos = $#src + 1;
    push @src, @$rel_src;

    if (
      $relinfo->{-is_optional}
        and
      defined ( my $first_distinct_child_idcol = first
        { ! $known_present_ids->{$_} }
        @{$relinfo->{-identifying_columns}}
      )
    ) {

      if ($args->{prune_null_branches}) {

        # start of wrap of the entire chain in a conditional
        splice @src, $rel_src_pos, 0, sprintf "( ! defined %s )\n  ? %s%s{%s} = %s\n  : do {",
          "'\xFF__VALPOS__${first_distinct_child_idcol}__\xFF'",
          $node_idx_slot,
          $args->{hri_style} ? '' : '[1]',
          perlstring($rel),
          ($args->{hri_style} && $relinfo->{-is_single}) ? 'undef' : '[]'
        ;

        # end of wrap
        push @src, '};'
      }
      else {

        splice @src, $rel_src_pos + 1, 0, sprintf ( '(defined %s) or bless (%s[1]{%s}, %s);',
          "'\xFF__VALPOS__${first_distinct_child_idcol}__\xFF'",
          $node_idx_slot,
          perlstring($rel),
          perlstring($null_branch_class),
        );
      }
    }
  }

  return (
    \@src,
    {
      idcols_seen => {
        ( map { %{ $_->{idcols_seen} } } values %$stats ),
        ( map { $_ => 1 } @{$args->{collapse_map}->{-identifying_columns}} ),
      }
    }
  );
}

sub __result_struct_to_source {
  sprintf( '{ %s }', join (', ', map
    { sprintf "%s => '\xFF__VALPOS__%d__\xFF'", perlstring($_), $_[0]{$_} }
    sort keys %{$_[0]}
  ));
}

1;
