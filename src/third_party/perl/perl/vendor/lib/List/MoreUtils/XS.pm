package List::MoreUtils::XS;

use 5.008_001;
use strict;
use warnings;
use base ('Exporter');

use vars qw{$VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS};

$VERSION = '0.428';

@EXPORT    = ();
@EXPORT_OK = qw(any all none notall one
  any_u all_u none_u notall_u one_u
  reduce_u reduce_0 reduce_1
  true false
  insert_after insert_after_string
  apply indexes
  after after_incl before before_incl
  firstidx lastidx onlyidx
  firstval lastval onlyval
  firstres lastres onlyres
  singleton duplicates frequency occurrences mode
  each_array each_arrayref
  pairwise natatime
  arrayify mesh zip6 uniq listcmp
  samples minmax minmaxstr part
  bsearch bsearchidx binsert bremove lower_bound upper_bound equal_range
  qsort);
%EXPORT_TAGS = (all => \@EXPORT_OK);

# Load the XS at compile-time so that redefinition warnings will be
# thrown correctly if the XS versions of part or indexes loaded

# PERL_DL_NONLAZY must be false, or any errors in loading will just
# cause the perl code to be tested
local $ENV{PERL_DL_NONLAZY} = 0 if $ENV{PERL_DL_NONLAZY};

use XSLoader ();
XSLoader::load("List::MoreUtils::XS", "$VERSION");

=pod

=head1 NAME

List::MoreUtils::XS - Provide compiled List::MoreUtils functions

=head1 SYNOPSIS

  use List::Moreutils::XS ();
  use List::MoreUtils ':all';

  my @procs = get_process_stats->fetchall_array;
  # sort by ppid, then pid
  qsort { $a->[3] <=> $b->[3] or $a->[2] <=> $b->[2] } @procs;
  while( @procs ) {
      my $proc = shift @procs;
      my @children = equal_range { $_->[3] <=> $proc->[2] } @procs;
  }

  my @left = qw(this is a test);
  my @right = qw(this is also a test);
  my %rlinfo = listcmp @left, @right;

  # on unsorted
  my $i = firstidx { $_ eq 'yeah' } @foo;
  # on sorted - always first, but might not be 'yeah'
  my $j = lower_bound { $_ cmp 'yeah' } @bar;
  # on sorted - any of occurrences, is surely 'yeah'
  my $k = bsearchidx { $_ cmp 'yeah' } @bar;

=head1 DESCRIPTION

List::MoreUtils::XS is a backend for List::MoreUtils. Even if it's possible
(because of user wishes) to have it practically independent from
L<List::MoreUtils>, it technically depend on C<List::MoreUtils>. Since it's
only a backend, the API is not public and can change without any warning.

=head1 SEE ALSO

L<List::Util>, L<List::AllUtils>

=head1 AUTHOR

Jens Rehsack E<lt>rehsack AT cpan.orgE<gt>

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

Tassilo von Parseval E<lt>tassilo.von.parseval@rwth-aachen.deE<gt>

=head1 COPYRIGHT AND LICENSE

Some parts copyright 2011 Aaron Crane.

Copyright 2004 - 2010 by Tassilo von Parseval

Copyright 2013 - 2017 by Jens Rehsack

All code added with 0.417 or later is licensed under the Apache License,
Version 2.0 (the "License"); you may not use this file except in compliance
with the License. You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

All code until 0.416 is licensed under the same terms as Perl itself,
either Perl version 5.8.4 or, at your option, any later version of
Perl 5 you may have available.

=cut

1;
