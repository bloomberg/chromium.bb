# $Id: Util.pm 79 2019-01-30 02:35:31Z stro $

package CPAN::SQLite::Util;
use strict;
use warnings;

our $VERSION = '0.217';

use English qw/-no_match_vars/;

use parent 'Exporter';
our (@EXPORT_OK, %modes, $table_id, $query_info, $mode_info, $full_id);
@EXPORT_OK = qw($repositories %modes
  vcmp $table_id $query_info $mode_info $full_id
  has_hash_data has_array_data
  download print_debug);

make_ids();

$mode_info = {
  module => {
    id    => 'mod_id',
    table => 'mods',
    name  => 'mod_name',
    text  => 'mod_abs',
  },
  dist => {
    id    => 'dist_id',
    table => 'dists',
    name  => 'dist_name',
    text  => 'dist_abs',
  },
  author => {
    id    => 'auth_id',
    table => 'auths',
    name  => 'cpanid',
    text  => 'fullname',
  },
};

%modes = map { $_ => 1 } keys %$mode_info;

$query_info = {
  module  => { mode => 'module', type => 'name' },
  mod_id  => { mode => 'module', type => 'id' },
  dist    => { mode => 'dist',   type => 'name' },
  dist_id => { mode => 'dist',   type => 'id' },
  cpanid  => { mode => 'author', type => 'name' },
  author  => { mode => 'author', type => 'name' },
  auth_id => { mode => 'author', type => 'id' },
};

sub make_ids {
  my @tables = qw(mods dists auths);
  foreach my $table (@tables) {
    (my $id = $table) =~ s!(\w+)s$!$1_id!;
    $table_id->{$table} = $id;
    $full_id->{$id}     = $table . '.' . $id;
  }
  return;
}

#my $num_re = qr{^0*\.\d+$};
#sub vcmp {
#    my ($v1, $v2) = @_;
#    return unless (defined $v1 and defined $v2);
#    if ($v1 =~ /$num_re/ and $v2 =~ /$num_re/) {
#        return $v1 <=> $v2;
#    }
#    return Sort::Versions::versioncmp($v1, $v2);
#}

sub has_hash_data {
  my $data = shift;
  return unless (defined $data and ref($data) eq 'HASH');
  return (scalar keys %$data > 0) ? 1 : 0;
}

sub has_array_data {
  my $data = shift;
  return unless (defined $data and ref($data) eq 'ARRAY');
  return (scalar @$data > 0) ? 1 : 0;
}

sub download {
  my ($cpanid, $dist_file) = @_;
  return unless ($cpanid and $dist_file);
  (my $fullid = $cpanid) =~ s!^(\w)(\w)(.*)!$1/$1$2/$1$2$3!;
  my $download = $fullid . '/' . $dist_file;
  return $download;
}

sub print_debug {
  return unless $ENV{CPAN_SQLITE_DEBUG};
  $CPAN::FrontEnd->myprint(@_);
}

sub vcmp {
  my ($v1, $v2) = @_;
  return CPAN::SQLite::Version->vcmp($v1, $v2);
}

# This is borrowed essentially verbatim from CPAN::Version
# It's included here so as to not demand a CPAN.pm upgrade

package CPAN::SQLite::Version;

use strict;
our $VERSION = '0.217';
no warnings;

# CPAN::Version::vcmp courtesy Jost Krieger
sub vcmp {
  my ($self, $l, $r) = @_;

  return 0 if $l eq $r;    # short circuit for quicker success

  for ($l, $r) {
    next unless tr/.// > 1;
    s/^v?/v/;
    1 while s/\.0+(\d)/.$1/;
  }
  if ($l =~ /^v/ <=> $r =~ /^v/) {
    for ($l, $r) {
      next if /^v/;
      $_ = $self->float2vv($_);
    }
  }

  return (
    ($l ne "undef") <=> ($r ne "undef")
      || ($] >= 5.006
      && $l =~ /^v/
      && $r =~ /^v/
      && $self->vstring($l) cmp $self->vstring($r))
      || $l <=> $r
      || $l cmp $r
  );
}

sub vgt {
  my ($self, $l, $r) = @_;
  return $self->vcmp($l, $r) > 0;
}

sub vlt {
  my ($self, $l, $r) = @_;
  return 0 + ($self->vcmp($l, $r) < 0);
}

sub vstring {
  my ($self, $n) = @_;
  $n =~ s/^v//
    or die "CPAN::Search::Lite::Version::vstring() called with invalid arg [$n]";
  {
    no warnings;
    return pack "U*", split /\./, $n;
  }
}

# vv => visible vstring
sub float2vv {
  my ($self, $n) = @_;
  my ($rev) = int($n);
  $rev ||= 0;
  my ($mantissa) = $n =~ /\.(\d{1,12})/;    # limit to 12 digits to limit
                                            # architecture influence
  $mantissa ||= 0;
  $mantissa .= "0" while length($mantissa) % 3;
  my $ret = "v" . $rev;

  while ($mantissa) {
    $mantissa =~ s/(\d{1,3})//
      or die "Panic: length>0 but not a digit? mantissa[$mantissa]";
    $ret .= "." . int($1);
  }

  # warn "n[$n]ret[$ret]";
  return $ret;
}

sub readable {
  my ($self, $n) = @_;
  $n =~ /^([\w\-\+\.]+)/;

  return $1 if defined $1 && length($1) > 0;

  # if the first user reaches version v43, he will be treated as "+".
  # We'll have to decide about a new rule here then, depending on what
  # will be the prevailing versioning behavior then.

  if ($] < 5.006) {    # or whenever v-strings were introduced
                       # we get them wrong anyway, whatever we do, because 5.005 will
                       # have already interpreted 0.2.4 to be "0.24". So even if he
                       # indexer sends us something like "v0.2.4" we compare wrongly.

    # And if they say v1.2, then the old perl takes it as "v12"

    warn("Suspicious version string seen [$n]\n");
    return $n;
  }
  my $better = sprintf "v%vd", $n;
  return $better;
}

1;

__END__

=head1 NAME

CPAN::SQLite::Util - export some common data structures used by CPAN::SQLite::*

=head1 VERSION

version 0.217

=head1 DESCRIPTION

This module exports some common data structures used by other
I<CPAN::Search::Lite::*> modules. At present these are

=over 3

=item * C<$table_id>

This is a hash reference whose keys are the tables used
and whose values are the associated primary keys.

=item * C<$full_id>

This is a hash reference whose keys are the primary keys
of the tables and whose values are the associated fully qualified
primary keys (ie, with the table name prepended).

=item * C<$mode_info>

This is a hash reference whose keys are the allowed
modes of I<CPAN::Search::Lite::Query> and whose associated values
are hash references with keys C<id>, C<name>, and C<text> describing
what columns to use for that key.

=item * C<$query_info>

This is a hash reference whose purpose is to provide
shortcuts to making queries using I<CPAN::Search::Lite::Query>. The
keys of this reference is the shortcut name, and the associated
value is a hash reference specifying the required I<mode> and
I<type> keys.

=item * C<vcmp>

This routine, used as

  if (vcmp($v1, $v2) > 0) {
    print "$v1 is higher than $v2\n";
  }

is used to compare two versions, and returns 1/0/-1 if
the first argument is considered higher/equal/lower than
the second. It uses C<Sort::Versions>.

=back

=cut
