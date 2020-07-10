package DBIx::Class::SQLMaker::OracleJoins;

use warnings;
use strict;

use base qw( DBIx::Class::SQLMaker::Oracle );

sub select {
  my ($self, $table, $fields, $where, $rs_attrs, @rest) = @_;

  # pull out all join conds as regular WHEREs from all extra tables
  if (ref($table) eq 'ARRAY') {
    $where = $self->_oracle_joins($where, @{ $table }[ 1 .. $#$table ]);
  }

  return $self->next::method($table, $fields, $where, $rs_attrs, @rest);
}

sub _recurse_from {
  my ($self, $from, @join) = @_;

  my @sqlf = $self->_from_chunk_to_sql($from);

  for (@join) {
    my ($to, $on) = @$_;

    if (ref $to eq 'ARRAY') {
      push (@sqlf, $self->_recurse_from(@{ $to }));
    }
    else {
      push (@sqlf, $self->_from_chunk_to_sql($to));
    }
  }

  return join q{, }, @sqlf;
}

sub _oracle_joins {
  my ($self, $where, @join) = @_;
  my $join_where = $self->_recurse_oracle_joins(@join);

  if (keys %$join_where) {
    if (!defined($where)) {
      $where = $join_where;
    } else {
      if (ref($where) eq 'ARRAY') {
        $where = { -or => $where };
      }
      $where = { -and => [ $join_where, $where ] };
    }
  }
  return $where;
}

sub _recurse_oracle_joins {
  my $self = shift;

  my @where;
  for my $j (@_) {
    my ($to, $on) = @{ $j };

    push @where, $self->_recurse_oracle_joins(@{ $to })
      if (ref $to eq 'ARRAY');

    my $join_opts  = ref $to eq 'ARRAY' ? $to->[0] : $to;
    my $left_join  = q{};
    my $right_join = q{};

    if (ref $join_opts eq 'HASH' and my $jt = $join_opts->{-join_type}) {
      #TODO: Support full outer joins -- this would happen much earlier in
      #the sequence since oracle 8's full outer join syntax is best
      #described as INSANE.
      $self->throw_exception("Can't handle full outer joins in Oracle 8 yet!\n")
        if $jt =~ /full/i;

      $left_join  = q{(+)} if $jt =~ /left/i
        && $jt !~ /inner/i;

      $right_join = q{(+)} if $jt =~ /right/i
        && $jt !~ /inner/i;
    }

    # FIXME - the code below *UTTERLY* doesn't work with custom conds... sigh
    # for the time being do not do any processing with the likes of _collapse_cond
    # instead only unroll the -and hack if present
    $on = $on->{-and}[0] if (
      ref $on eq 'HASH'
        and
      keys %$on == 1
        and
      ref $on->{-and} eq 'ARRAY'
        and
      @{$on->{-and}} == 1
    );


    push @where, map { \do {
        my ($sql) = $self->_recurse_where({
          # FIXME - more borkage, more or less a copy of the kludge in ::SQLMaker::_join_condition()
          $_ => ( length ref $on->{$_}
            ? $on->{$_}
            : { -ident => $on->{$_} }
          )
        });

        $sql =~ s/\s*\=/$left_join =/
          if $left_join;

        "$sql$right_join";
      }
    } sort keys %$on;
  }

  return { -and => \@where };
}

1;

__END__

=head1 NAME

DBIx::Class::SQLMaker::OracleJoins - Pre-ANSI Joins-via-Where-Clause Syntax

=head1 PURPOSE

This module is used with Oracle < 9.0 due to lack of support for standard
ANSI join syntax.

=head1 SYNOPSIS

Not intended for use directly; used as the sql_maker_class for schemas and components.

=head1 DESCRIPTION

Implements pre-ANSI joins specified in the where clause.  Instead of:

    SELECT x FROM y JOIN z ON y.id = z.id

It will write:

    SELECT x FROM y, z WHERE y.id = z.id

It should properly support left joins, and right joins.  Full outer joins are
not possible due to the fact that Oracle requires the entire query be written
to union the results of a left and right join, and by the time this module is
called to create the where query and table definition part of the sql query,
it's already too late.

=head1 METHODS

=over

=item select

Overrides DBIx::Class::SQLMaker's select() method, which calls _oracle_joins()
to modify the column and table list before calling next::method().

=back

=head1 BUGS

Does not support full outer joins (however neither really does DBIC itself)

=head1 SEE ALSO

=over

=item L<DBIx::Class::Storage::DBI::Oracle::WhereJoins> - Storage class using this

=item L<DBIx::Class::SQLMaker> - Parent module

=item L<DBIx::Class> - Duh

=back

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
