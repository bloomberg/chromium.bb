package DBIx::Class::ResultClass::HashRefInflator;

use strict;
use warnings;

=head1 NAME

DBIx::Class::ResultClass::HashRefInflator - Get raw hashrefs from a resultset

=head1 SYNOPSIS

 use DBIx::Class::ResultClass::HashRefInflator;

 my $rs = $schema->resultset('CD');
 $rs->result_class('DBIx::Class::ResultClass::HashRefInflator');
 while (my $hashref = $rs->next) {
   ...
 }

  OR as an attribute:

 my $rs = $schema->resultset('CD')->search({}, {
   result_class => 'DBIx::Class::ResultClass::HashRefInflator',
 });
 while (my $hashref = $rs->next) {
   ...
 }

=head1 DESCRIPTION

DBIx::Class is faster than older ORMs like Class::DBI but it still isn't
designed primarily for speed. Sometimes you need to quickly retrieve the data
from a massive resultset, while skipping the creation of fancy result objects.
Specifying this class as a C<result_class> for a resultset will change C<< $rs->next >>
to return a plain data hash-ref (or a list of such hash-refs if C<< $rs->all >> is used).

There are two ways of applying this class to a resultset:

=over

=item *

Specify C<< $rs->result_class >> on a specific resultset to affect only that
resultset (and any chained off of it); or

=item *

Specify C<< __PACKAGE__->result_class >> on your source object to force all
uses of that result source to be inflated to hash-refs - this approach is not
recommended.

=back

=cut

##############
# NOTE
#
# Generally people use this to gain as much speed as possible. If a new &mk_hash is
# implemented, it should be benchmarked using the maint/benchmark_hashrefinflator.pl
# script (in addition to passing all tests of course :)

# This coderef is a simple recursive function
# Arguments: ($me, $prefetch, $is_root) from inflate_result() below
my $mk_hash;
$mk_hash = sub {

  my $hash = {

    # the main hash could be an undef if we are processing a skipped-over join
    $_[0] ? %{$_[0]} : (),

    # the second arg is a hash of arrays for each prefetched relation
    map { $_ => (

      # null-branch or not
      ref $_[1]->{$_} eq $DBIx::Class::ResultSource::RowParser::Util::null_branch_class

        ? ref $_[1]->{$_}[0] eq 'ARRAY' ? [] : undef

        : ref $_[1]->{$_}[0] eq 'ARRAY'
          ? [ map { $mk_hash->( @$_ ) || () } @{$_[1]->{$_}} ]
          : $mk_hash->( @{$_[1]->{$_}} )

    ) } ($_[1] ? keys %{$_[1]} : ())
  };

  ($_[2] || keys %$hash) ? $hash : undef;
};

=head1 METHODS

=head2 inflate_result

Inflates the result and prefetched data into a hash-ref (invoked by L<DBIx::Class::ResultSet>)

=cut

##################################################################################
# inflate_result is invoked as:
# HRI->inflate_result ($resultsource_instance, $main_data_hashref, $prefetch_data_hashref)
sub inflate_result {
  return $mk_hash->($_[2], $_[3], 'is_root');
}

1;

__END__

=head1 CAVEATS

=over

=item *

This will not work for relationships that have been prefetched. Consider the
following:

 my $artist = $artitsts_rs->search({}, {prefetch => 'cds' })->first;

 my $cds = $artist->cds;
 $cds->result_class('DBIx::Class::ResultClass::HashRefInflator');
 my $first = $cds->first;

C<$first> will B<not> be a hashref, it will be a normal CD row since
HashRefInflator only affects resultsets at inflation time, and prefetch causes
relations to be inflated when the master C<$artist> row is inflated.

=item *

Column value inflation, e.g., using modules like
L<DBIx::Class::InflateColumn::DateTime>, is not performed.
The returned hash contains the raw database values.

=back

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
