package Text::Patch;
use Exporter;
our @ISA = qw( Exporter );
our @EXPORT = qw( patch );
our $VERSION = '1.8';
use strict;
use warnings;
use Carp;

use constant NO_NEWLINE => '\\ No newline at end of file';

sub patch
{
  my $text = shift;
  my $diff = shift;
  my %options = ref $_[0] eq 'HASH' ? %{ $_[0] } : @_;

  my %handler = ('unified'  => \&patch_unified,
                 'context'  => \&patch_context,
                 'oldstyle' => \&patch_oldstyle,
                 );
  my $style = $options{STYLE};
  croak "required STYLE option is missing" unless $style;
  croak "source required" unless defined $text;
  croak "diff required" unless defined $diff;
  my $code = $handler{lc($style)} || croak "unrecognised STYLE '$style'";

  my @text = split /^/m, $text;
  my @diff = split /^/m, $diff;

  # analyse source/diff to determine line ending used.
  # (if source is only 1 line, can't use it to determine line endings)
  my $line1 = @text > 1 ? $text[0] : $diff[0];
  my($line1c, $sep) = _chomp($line1);
  $sep ||= "\n";  # default to unix line ending

  # apply patch
  DUMP("got patch", \@diff);
  my $out = $code->(\@text, \@diff, $sep);

  my $lastline = _chomp($diff[-1], $sep);
  $out = _chomp($out, $sep) if $lastline eq NO_NEWLINE;
  return $out;
}

sub patch_unified
{
  my($text, $diff, $sep) = @_;
  my @hunks;
  my %hunk;

  for( @$diff )
    {
    #print STDERR ">>> ... [$_]";
    if( /^\@\@\s*-([\d,]+)/ )
      {
      #print STDERR ">>> *** HUNK!\n";
      my($pos1, $count1) = split /,/, $1;
      push @hunks, { %hunk };
      %hunk = ();
      $hunk{ FROM } = $pos1 - 1; # diff is 1-based
      # Modification by Ben L., patches may have @@ -0,0 if the source is empty.
      $hunk{ FROM } = 0 if $hunk{ FROM } < 0;
      $hunk{ LEN  } = defined $count1 ? $count1 : $pos1 == 0 ? 0 : 1;
      $hunk{ DATA } = [];
      }
    push @{ $hunk{ DATA } }, $_;
    }
  push @hunks, { %hunk }; # push last hunk
  shift @hunks; # first is always empty

  return _patch($text, \@hunks, $sep);
}

sub patch_oldstyle {
    my($text, $diff, $sep) = @_;
    my @hunks;
    my $i = 0;

    my $hunk_head = qr/^([\d,]+)([acd])([\d,]+)$/;
    while($i < @$diff) {
        my $l = $diff->[$i];
        my($r1, $type, $r2) = $l =~ $hunk_head;
        die "Malformed patch at line ".($i + 1)."\n"
            unless defined $r1 && $type && defined $r2;
        my($pos1, $count1) = _range($r1);
        my($pos2, $count2) = _range($r2);

        # parse chunk data
        my @data;
        my $j = $i + 1;
        for(; $j < @$diff; $j++) {
            $l = $diff->[$j];
            last if $l =~ $hunk_head;
            next if $l =~ /^---/;  # separator
            push @data, $l;
        }
        my $datalen = $j - $i - 1;

        if($type eq 'a') { # add
            $count1 = 0;   # don't remove any lines
            $pos1++;       # add to line after pos1
        }

        # convert data to a format _patch() will understand
        for(@data) {
            $_ =~ s/^< /-/;
            $_ =~ s/^> /+/;
        }

        push @hunks, { FROM => $pos1 - 1,
                       LEN  => $count1,
                       DATA => \@data,
                     };
        $i += $datalen + 1;
    }
    return _patch($text, \@hunks, $sep);
}

# NB: this works by converting hunks into a kind of unified format
sub patch_context {
    my($text, $diff, $sep) = @_;
    my $i = 0;
    my @hunks;

    # skip past header
    for(@$diff) {
        $i++;
        last if /^\Q***************\E$/;  # end header marker
    }

    # this sub reads one half of a hunk (from/to part)
    my $read_part = sub {
        my $l = $diff->[$i++];
        TRACE("got line: $l");
        die "Malformed patch at line $i\n"
            unless $l =~ /^(?:\*\*\*|---)\s+([\d,]+)\s+(?:\*\*\*|---)/;
        my($pos, $count) = _range($1);
        my @part;
        while($i < @$diff) {
            my $l = $diff->[$i];
            last if $l =~ /^(\*\*\*|---)/;
            push @part, $l;
            $i++;
        }
        DUMP("got part", \@part);
        return (\@part, $pos, $count);
    };

    while($i < @$diff) {
        # read the from and to part of this hunk
        my($part1, $pos1, $count1) = $read_part->();
        my($part2, $pos2, $count2) = $read_part->();
        $i++;  # skip chunk separator

        # convert operations to unified style ones
        $_ =~ s/^(.)\s/$1/ for @$part1, @$part2;
        $_ =~ s/^\!/-/ for @$part1;  # remove
        $_ =~ s/^\!/+/ for @$part2;  # add

        # merge the parts to create a unified style chunk
        my @data;
        for(;;) {
            my $c1 = $part1->[0];
            my $c2 = $part2->[0];
            last unless defined $c1 || defined $c2;

            if(defined $c1 && $c1 =~ /^-/) {
                push @data, shift @$part1;  # remove line
            } elsif(defined $c2 && $c2 =~ /^\+/) {
                push @data, shift @$part2;  # add line
            } else {                        # context
                my($x1, $x2) = (shift @$part1, shift @$part2);
                push @data, defined $x1 ? $x1 : $x2;
            }
        }
        push @hunks, { FROM => $pos1 - 1,
                       LEN  => $count1,
                       DATA => \@data,
                     };
        DUMP("merged data", \@data);
    }
    return _patch($text, \@hunks, $sep);
}

######################################################################
# private

# returns (start line, line count)
sub _range {
    my($range) = @_;
    my($pos1, $pos2) = split /,/, $range;
    return ($pos1, defined $pos2 ? $pos2 - $pos1 + 1 : 1);
}

sub _patch {
  my($text, $hunks, $sep) = @_;
  my $hunknum = scalar @$hunks + 1;
  die "No hunks found\n" unless @$hunks;
  for my $hunk ( reverse @$hunks )
    {
    $hunknum--;
    DUMP("hunk", $hunk);
    my @pdata;
    my $num = $hunk->{FROM};
    for( @{ $hunk->{ DATA } } )
      {
      next unless s/^([ \-\+])//;
      #print STDERR ">>> ($1) $_";
      if($1 ne '+') {
          # not an addition, check line for match against existing text.
          # ignore line endings for comparison
          my $orig   = _chomp($text->[$num++], $sep); # num 0 based here
          my $expect = _chomp($_, $sep);
          TRACE("checking >>$orig<<");
          TRACE(" against >>$expect<<");
          die "Hunk #$hunknum failed at line $num.\n" # actual line number
              unless $orig eq $expect;
      }
      next if $1 eq '-';  # removals
      push @pdata, $_;    # add/replace line
      }
    splice @$text, $hunk->{ FROM }, $hunk->{ LEN }, @pdata;
    }

  return join '', @$text;
}

# chomp $sep from the end of line
# if $sep is not given, chomp unix or dos line ending
sub _chomp {
    my($text, $sep) = @_;
    if($sep) {
        $text =~ s/($sep)$//;
    } else {
        $text =~ s/(\r\n|\n)$//;
    }
    return wantarray ? ($text, $1) : $text;
}

sub DUMP {}
sub TRACE {}

#sub DUMP {
#use Data::Dumper;
#print STDERR Dumper(@_);
#}
#sub TRACE {
#use Data::Dumper;
#print STDERR Dumper(@_);
#}


=pod

=head1 NAME

Text::Patch - Patches text with given patch

=head1 SYNOPSIS

    use Text::Patch;

    $output = patch( $source, $diff, STYLE => "Unified" );

    use Text::Diff;

    $src  = ...
    $dst  = ...

    $diff = diff( \$src, \$dst, { STYLE => 'Unified' } );

    $out  = patch( $src, $diff, { STYLE => 'Unified' } );

    print "Patch successful" if $out eq $dst;

=head1 DESCRIPTION

Text::Patch combines source text with given diff (difference) data.
Diff data is produced by Text::Diff module or by the standard diff
utility (man diff, see -u option).

=over 4

=item patch( $source, $diff, options... )

First argument is source (original) text. Second is the diff data.
Third argument can be either hash reference with options or all the
rest arguments will be considered patch options:

    $output = patch( $source, $diff, STYLE => "Unified", ... );

    $output = patch( $source, $diff, { STYLE => "Unified", ... } );

Options are:

  STYLE => 'Unified'

STYLE can be "Unified", "Context" or "OldStyle".

The 'Unified' diff format looks like this:

  @@ -1,7 +1,6 @@
  -The Way that can be told of is not the eternal Way;
  -The name that can be named is not the eternal name.
   The Nameless is the origin of Heaven and Earth;
  -The Named is the mother of all things.
  +The named is the mother of all things.
  +
   Therefore let there always be non-being,
     so we may see their subtlety,
   And let there always be being,
  @@ -9,3 +8,6 @@
   The two are the same,
   But after they are produced,
     they have different names.
  +They both may be called deep and profound.
  +Deeper and more profound,
  +The door of all subtleties!


=back

=head1 TODO

  Interfaces with files, arrays, etc.

=head1 AUTHOR

  Vladi Belperchinov-Shabanski "Cade"

  <cade@biscom.net> <cade@datamax.bg> <cade@cpan.org>

  http://cade.datamax.bg

=head1 VERSION

  $Id: Patch.pm,v 1.6 2007/04/07 19:57:41 cade Exp $

=cut
