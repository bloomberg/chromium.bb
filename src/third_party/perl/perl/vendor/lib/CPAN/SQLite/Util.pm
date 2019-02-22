# $Id: Util.pm 35 2011-06-17 01:34:42Z stro $

package CPAN::SQLite::Util;
use strict;
use warnings;
our $VERSION = '0.202';

use base qw(Exporter);
our (@EXPORT_OK, %chaps, %modes,
     $table_id, $query_info, $mode_info, $full_id, $dslip);
@EXPORT_OK = qw(%chaps $repositories %modes
                vcmp $table_id $query_info $mode_info $full_id
                has_hash_data has_array_data $dslip
                expand_dslip download chap_desc print_debug);

make_ids();

$mode_info = {
              module => {id => 'mod_id',
                         table => 'mods',
                         name => 'mod_name',
                         text => 'mod_abs',
                        },
              dist => {id => 'dist_id',
                       table => 'dists',
                       name => 'dist_name',
                       text => 'dist_abs',
                      },
              author => {id => 'auth_id',
                         table => 'auths',
                         name => 'cpanid',
                         text => 'fullname',
                        },
              chapter => {id => 'chapterid',
                          table => 'chaps',
                          name => 'subchapter',
                          text => 'subchapter',
                         },
             };

%modes = map {$_ => 1} keys %$mode_info;

$query_info = { module => {mode => 'module', type => 'name'},
                mod_id => {mode => 'module', type => 'id'},
                dist => {mode => 'dist', type => 'name'},
                dist_id => {mode => 'dist', type => 'id'},
                cpanid => {mode => 'author', type => 'name'},
                author => {mode => 'author', type => 'name'},
                auth_id => {mode => 'author', type => 'id'},
              };

%chaps = (
          2 => 'Perl Core Modules',
          3 => 'Development Support',
          4 => 'Operating System Interfaces',
          5 => 'Networking Devices IPC',
          6 => 'Data Type Utilities',
          7 => 'Database Interfaces',
          8 => 'User Interfaces',
          9 => 'Language Interfaces',
          10 => 'File Names Systems Locking',
          11 => 'String Lang Text Proc',
          12 => 'Opt Arg Param Proc',
          13 => 'Internationalization Locale',
          14 => 'Security and Encryption',
          15 => 'World Wide Web HTML HTTP CGI',
          16 => 'Server and Daemon Utilities',
          17 => 'Archiving and Compression',
          18 => 'Images Pixmaps Bitmaps',
          19 => 'Mail and Usenet News',
          20 => 'Control Flow Utilities',
          21 => 'File Handle Input Output',
          22 => 'Microsoft Windows Modules',
          23 => 'Miscellaneous Modules',
          24 => 'Commercial Software Interfaces',
          26 => 'Documentation',
          27 => 'Pragma',
          28 => 'Perl6',
          99 => 'Not In Modulelist',
         );

$dslip = {
    d => {
      M => q{Mature (no rigorous definition)},
      R => q{Released},
      S => q{Standard, supplied with Perl 5},
      a => q{Alpha testing},
      b => q{Beta testing},
      c => q{Under construction but pre-alpha (not yet released)},
      desc => q{Development Stage (Note: *NO IMPLIED TIMESCALES*)},
      i => q{Idea, listed to gain consensus or as a placeholder},
    },
    s => {
      a => q{Abandoned, the module has been abandoned by its author},
      d => q{Developer},
      desc => q{Support Level},
      m => q{Mailing-list},
      n => q{None known, try comp.lang.perl.modules},
      u => q{Usenet newsgroup comp.lang.perl.modules},
    },
    l => {
      '+' => q{C++ and perl, a C++ compiler will be needed},
      c => q{C and perl, a C compiler will be needed},
      desc => q{Language Used},
      h => q{Hybrid, written in perl with optional C code, no compiler needed},
      o => q{perl and another language other than C or C++},
      p => q{Perl-only, no compiler needed, should be platform independent},
    },
    i => {
      O => q{Object oriented using blessed references and/or inheritance},
      desc => q{Interface Style},
      f => q{plain Functions, no references used},
      h => q{hybrid, object and function interfaces available},
      n => q{no interface at all (huh?)},
      r => q{some use of unblessed References or ties},
    },
    p => {
      a => q{Artistic license alone},
      b => q{BSD: The BSD License},
      desc => q{Public License},
      g => q{GPL: GNU General Public License},
      l => q{LGPL: "GNU Lesser General Public License" (previously known as "GNU Library General Public License")},
      o => q{other (but distribution allowed without restrictions)},
      p => q{Standard-Perl: user may choose between GPL and Artistic},
    },
};


sub make_ids {
  my @tables = qw(mods dists auths);
  foreach my $table (@tables) {
    (my $id = $table) =~ s!(\w+)s$!$1_id!;
    $table_id->{$table} = $id;
    $full_id->{$id} = $table . '.' . $id;
  }
#    $full_id->{chapterid} = 'chaps.chapterid';
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
  my $data  = shift;
  return unless (defined $data and ref($data) eq 'HASH');
  return (scalar keys %$data > 0) ? 1 : 0;
}

sub has_array_data {
  my $data  = shift;
  return unless (defined $data and ref($data) eq 'ARRAY');
  return (scalar @$data > 0) ? 1 : 0;
}

sub expand_dslip {
  my ($string) = @_;
  my $entries = [];
  my @info = split '', $string;
  my @given = qw(d s l i p);
  for (0 .. 4) {
    my $entry;
    my $given = $given[$_];
    my $info = $info[$_];
    $entry->{desc} = $dslip->{$given}->{desc};
    $entry->{what} = (not $info or $info eq '?') ?
      'not specified' : $dslip->{$given}->{$info};
    push @$entries, $entry;
  }
  return $entries;
}

sub download {
  my ($cpanid, $dist_file) = @_;
  return unless ($cpanid and $dist_file);
  (my $fullid = $cpanid) =~ s!^(\w)(\w)(.*)!$1/$1$2/$1$2$3!;
  my $download = $fullid . '/' . $dist_file;
  return $download;
}

sub chap_desc {
  my ($id) = @_;
  return $chaps{$id};
}

sub print_debug {
  return unless $ENV{CPAN_SQLITE_DEBUG};
  return print @_;
}

sub vcmp {
  my ($v1, $v2) = @_;
  return CPAN::SQLite::Version->vcmp($v1, $v2);
}


# This is borrowed essentially verbatim from CPAN::Version
# It's included here so as to not demand a CPAN.pm upgrade

package CPAN::SQLite::Version;

use strict;
our $VERSION = 0.1;
no warnings;

# CPAN::Version::vcmp courtesy Jost Krieger
sub vcmp {
  my ($self, $l, $r) = @_;

  return 0 if $l eq $r; # short circuit for quicker success

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
          ($l ne "undef") <=> ($r ne "undef") ||
          (
           $] >= 5.006 &&
           $l =~ /^v/ &&
           $r =~ /^v/ &&
           $self->vstring($l) cmp $self->vstring($r)
          ) ||
          $l <=> $r ||
          $l cmp $r
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
    my ($mantissa) = $n =~ /\.(\d{1,12})/; # limit to 12 digits to limit
                                          # architecture influence
    $mantissa ||= 0;
    $mantissa .= "0" while length($mantissa)%3;
    my $ret = "v" . $rev;
    while ($mantissa) {
        $mantissa =~ s/(\d{1,3})// or
            die "Panic: length>0 but not a digit? mantissa[$mantissa]";
        $ret .= ".".int($1);
    }
    # warn "n[$n]ret[$ret]";
    return $ret;
}

sub readable {
  my($self,$n) = @_;
  $n =~ /^([\w\-\+\.]+)/;

  return $1 if defined $1 && length($1)>0;
  # if the first user reaches version v43, he will be treated as "+".
  # We'll have to decide about a new rule here then, depending on what
  # will be the prevailing versioning behavior then.

  if ($] < 5.006) { # or whenever v-strings were introduced
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

=head1 DESCRIPTION

This module exports some common data structures used by other
I<CPAN::Search::Lite::*> modules. At present these are

=over 3

=item * C<%chaps>

This is hash whose keys are the CPAN chapter ids with associated
values being the corresponding chapter descriptions.

=item * C<$dslip>

This contains a description of the meaning of the
various C<dslip> codes.

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
