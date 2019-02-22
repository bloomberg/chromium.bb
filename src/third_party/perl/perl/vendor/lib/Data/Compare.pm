# Data::Compare - compare perl data structures
# Author: Fabien Tassin <fta@sofaraway.org>
# updated by David Cantrell <david@cantrell.org.uk>
# Copyright 1999-2001 Fabien Tassin <fta@sofaraway.org>
# portions Copyright 2003 - 2010 David Cantrell

package Data::Compare;

use strict;
use warnings;

use vars qw(@ISA @EXPORT $VERSION $DEBUG %been_there);
use Exporter;
use Carp;
use Scalar::Util;

@ISA     = qw(Exporter);
@EXPORT  = qw(Compare);
$VERSION = 1.22;
$DEBUG   = $ENV{PERL_DATA_COMPARE_DEBUG} || 0;

my %handler;

use Cwd;

sub import {
  if(eval { chdir(getcwd()) }) { # chdir(getcwd()) isn't taint-safe
    register_plugins();
  }
  __PACKAGE__->export_to_level(1, @EXPORT);
}

# finds and registers plugins
sub register_plugins {
  eval 'use File::Find::Rule';
  foreach my $file (
    File::Find::Rule->file()->name('*.pm')->in(
      map { "$_/Data/Compare/Plugins" }
      grep { -d "$_/Data/Compare/Plugins" }
      @INC
    )
  ) {
    # all of this just to avoid loading the same plugin twice and
    # generating a pile of warnings. Grargh!
    $file =~ s!.*(Data/Compare/Plugins/.*)\.pm$!$1!;
    $file =~ s!/!::!g;
    # ignore badly named example from earlier version, oops
    next if($file eq 'Data::Compare::Plugins::Scalar-Properties');
    my $requires = eval "require $file";
    next if($requires eq '1'); # already loaded this plugin?

    # not an arrayref? bail
    if(ref($requires) ne 'ARRAY') {
      warn("$file isn't a valid Data::Compare plugin (didn't return arrayref)\n");
      return;
    }
    # coerce into arrayref of arrayrefs if necessary
    if(ref((@{$requires})[0]) ne 'ARRAY') { $requires = [$requires] }

    # register all the handlers
    foreach my $require (@{$requires}) {
      my($handler, $type1, $type2, $cruft) = reverse @{$require};
      $type2 = $type1 unless(defined($type2));
      ($type1, $type2) = sort($type1, $type2);
      if(!defined($type1) || ref($type1) ne '' || !defined($type2) || ref($type2) ne '') {
        warn("$file isn't a valid Data::Compare plugin (invalid type)\n");
      } elsif(defined($cruft)) {
        warn("$file isn't a valid Data::Compare plugin (extra data)\n");
      } elsif(ref($handler) ne 'CODE') {
        warn("$file isn't a valid Data::Compare plugin (no coderef)\n");
      } else {
        $handler{$type1}{$type2} = $handler;
      }
    }
  }
}

sub new {
  my $this = shift;
  my $class = ref($this) || $this;
  my $self = {};
  bless $self, $class;
  $self->{'x'} = shift;
  $self->{'y'} = shift;
  return $self;
}

sub Cmp {
  my $self = shift;

  croak "Usage: DataCompareObj->Cmp(x, y)" unless $#_ == 1 || $#_ == -1;
  my $x = shift || $self->{'x'};
  my $y = shift || $self->{'y'};

  return Compare($x, $y);
}

sub Compare {
  croak "Usage: Data::Compare::Compare(x, y, [opts])\n" unless $#_ == 1 || $#_ == 2;

  my $x = shift;
  my $y = shift;
  my $opts = shift || {};
  my($xparent, $xpos, $yparent, $ypos) = map {
    $opts->{$_} || ''
  } qw(xparent xpos yparent ypos);

  my $rval = '';

  if(!exists($opts->{recursion_detector})) {
    %been_there = ();
    $opts->{recursion_detector} = 0;
  }
  $opts->{recursion_detector}++;

  warn "Yaroo! deep recursion!\n" if($opts->{recursion_detector} == 99);
  
  if(
    (ref($x) && exists($been_there{"$x-$xpos-$xparent"}) && $been_there{"$x-$xpos-$xparent"} > 1) ||
    (ref($y) && exists($been_there{"$y-$ypos-$yparent"}) && $been_there{"$y-$ypos-$yparent"} > 1)
  ) {
    return 1; # we bail as soon as possible, so if we've *not* bailed and have got here, say we're OK and go to the next sub-structure
  } else {
    $been_there{"$x-$xpos-$xparent"}++ if(ref($x));
    $been_there{"$y-$ypos-$yparent"}++ if(ref($y));

    $opts->{ignore_hash_keys} = { map {
      ($_, 1)
    } @{$opts->{ignore_hash_keys}} } if(ref($opts->{ignore_hash_keys}) eq 'ARRAY');

    my $refx = ref $x;
    my $refy = ref $y;

    if(exists($handler{$refx}) && exists($handler{$refx}{$refy})) {
      $rval = &{$handler{$refx}{$refy}}($x, $y, $opts);
    } elsif(exists($handler{$refy}) && exists($handler{$refy}{$refx})) {
      $rval = &{$handler{$refy}{$refx}}($x, $y, $opts);
    }

    elsif(!$refx && !$refy) { # both are scalars
      if(defined $x && defined $y) { # both are defined
        $rval = $x eq $y;
      } else { $rval = !(defined $x || defined $y); }
    }
    elsif ($refx ne $refy) { # not the same type
      $rval = 0;
    }
    elsif ($x == $y) { # exactly the same reference
      $rval = 1;
    }
    elsif ($refx eq 'SCALAR' || $refx eq 'REF') {
      $rval = Compare(${$x}, ${$y}, $opts);
    }
    elsif ($refx eq 'ARRAY') {
      if ($#{$x} == $#{$y}) { # same length
        my $i = -1;
        $rval = 1;
        for (@$x) {
          $i++;
          $rval = 0 unless Compare($x->[$i], $y->[$i], { %{$opts}, xparent => $x, xpos => $i, yparent => $y, ypos => $i});
        }
      }
      else {
        $rval = 0;
      }
    }
    elsif ($refx eq 'HASH') {
      my @kx = grep { !$opts->{ignore_hash_keys}->{$_} } keys %$x;
      my @ky = grep { !$opts->{ignore_hash_keys}->{$_} } keys %$y; # heh, KY
      $rval = 1;
      $rval = 0 unless scalar @kx == scalar @ky;

      for (@kx) {
        next unless defined $x->{$_} || defined $y->{$_};
        $rval = 0 unless defined $y->{$_} && Compare($x->{$_}, $y->{$_}, { %{$opts}, xparent => $x, xpos => $_, yparent => $y, ypos => $_});
      }
    }
    elsif($refx eq 'Regexp') {
      $rval = Compare($x.'', $y.'', $opts);
    }
    elsif ($refx eq 'CODE') {
      $rval = 0;
    }
    elsif ($refx eq 'GLOB') {
      $rval = 0;
    }
    else { # a package name (object blessed)
      my ($type) = "$x" =~ m/^$refx=(\S+)\(/;
      if ($type eq 'HASH') {
        my %x = %$x;
        my %y = %$y;
        $rval = Compare(\%x, \%y, { %{$opts}, xparent => $xparent, xpos => $xpos, yparent => $yparent, ypos => $ypos});
        $been_there{\%x."-$xpos-$xparent"}--; # decrement count for temp structures
        $been_there{\%y."-$ypos-$yparent"}--;
      }
      elsif ($type eq 'ARRAY') {
        my @x = @$x;
        my @y = @$y;
        $rval = Compare(\@x, \@y, { %{$opts}, xparent => $xparent, xpos => $xpos, yparent => $yparent, ypos => $ypos});
        $been_there{\@x."-$xpos-$xparent"}--;
        $been_there{\@y."-$ypos-$yparent"}--;
      }
      elsif ($type eq 'SCALAR' || $type eq 'REF') {
        my $x = ${$x};
        my $y = ${$y};
        $rval = Compare($x, $y, $opts);
        # $been_there{\$x}--;
        # $been_there{\$y}--;
      }
      elsif ($type eq 'GLOB') {
        $rval = 0;
      }
      elsif ($type eq 'CODE') {
        $rval = 0;
      }
      else {
        croak "Can't handle $type type.";
        $rval = 0;
      }
    }
  }
  $opts->{recursion_detector}--;
  return $rval;
}

sub plugins {
  return { map { (($_ eq '') ? '[scalar]' : $_, [map { $_ eq '' ? '[scalar]' : $_ } keys %{$handler{$_}}]) } keys %handler };
}

sub plugins_printable {
  my $r = "The following comparisons are available through plugins\n\n";
  foreach my $key (sort keys %handler) {
    foreach(sort keys %{$handler{$key}}) {
      $r .= join(":\t", map { $_ eq '' ? '[scalar]' : $_ } ($key, $_))."\n";
    }
  }
  return $r;
}

1;

=head1 NAME

Data::Compare - compare perl data structures

=head1 SYNOPSIS

    use Data::Compare;

    my $h1 = { 'foo' => [ 'bar', 'baz' ],  'FOO' => [ 'one', 'two' ] };
    my $h2 = { 'foo' => [ 'bar', 'barf' ], 'FOO' => [ 'one', 'two' ] };
    my @a1 = ('one', 'two');
    my @a2 = ('bar', 'baz');
    my %v = ( 'FOO', \@a1, 'foo', \@a2 );

    # simple procedural interface
    print 'structures of $h1 and \%v are ',
      Compare($h1, \%v) ? "" : "not ", "identical.\n";

    print 'structures of $h1 and $h2 are ',
      Compare($h1, $h2, { ignore_hash_keys => [qw(foo)] }) ? '' : 'not ',
      "close enough to identical.\n";

    # OO usage
    my $c = new Data::Compare($h1, \%v);
    print 'structures of $h1 and \%v are ',
      $c->Cmp ? "" : "not ", "identical.\n";
    # or
    my $c = new Data::Compare;
    print 'structures of $h and \%v are ',
      $c->Cmp($h1, \%v) ? "" : "not ", "identical.\n";

=head1 DESCRIPTION

Compare two perl data structures recursively. Returns 0 if the
structures differ, else returns 1.

A few data types are treated as special cases:

=over 4

=item Scalar::Properties objects

This has been moved into a plugin, although functionality remains the
same as with the previous version.  Full documentation is in
L<Data::Compare::Plugins::Scalar::Properties>.

=item Compiled regular expressions, eg qr/foo/

These are stringified before comparison, so the following will match:

    $r = qr/abc/i;
    $s = qr/abc/i;
    Compare($r, $s);

and the following won't, despite them matching *exactly* the same text:

    $r = qr/abc/i;
    $s = qr/[aA][bB][cC]/;
    Compare($r, $s);

Sorry, that's the best we can do.

=item CODE and GLOB references

These are assumed not to match unless the references are identical - ie,
both are references to the same thing.

=back

You may also customise how we compare structures by supplying options in
a hashref as a third parameter to the C<Compare()> function.  This is not
yet available through the OO-ish interface.  These options will be in
force for the *whole* of your comparison, so will apply to structures
that are lurking deep down in your data as well as at the top level, so
beware!

=over 4

=item ignore_hash_keys

an arrayref of strings. When comparing two hashes, any keys mentioned in
this list will be ignored.

=back

=head1 CIRCULAR STRUCTURES

Comparing a circular structure to itself returns true:

    $x = \$y;
    $y = \$x;
    Compare([$x, $y], [$x, $y]);

And on a sort-of-related note, if you try to compare insanely deeply nested
structures, the module will spit a warning.  For this to affect you, you need to go
around a hundred levels deep though, and if you do that you have bigger
problems which I can't help you with ;-)

=head1 PLUGINS

The module takes plug-ins so you can provide specialised routines for
comparing your own objects and data-types.  For details see
L<Data::Compare::Plugins>.

Plugins are *not* available when running in "taint" mode.  You may
also make it not load plugins by providing an empty list as the
argument to import() - ie, by doing this:

    use Data::Compare ();

A couple of functions are provided to examine what goodies have been
made available through plugins:

=over 4

=item plugins

Returns a structure (a hash ref) describing all the comparisons made
available through plugins.
This function is *not* exported, so should be called as Data::Compare::plugins().
It takes no parameters.

=item plugins_printable

Returns formatted text

=back

=head1 EXPORTS

For historical reasons, the Compare() function is exported.  If you
don't want this, then pass an empty list to import() as explained
under PLUGINS.  If you want no export but do want plugins, then pass
the empty list, and then call the register_plugins class method:

    use Data::Compare ();
    Data::Compare->register_plugins;

or you could call it as a function if that floats your boat.

=head1 CODE REPOSITORY

L<http://www.cantrell.org.uk/cgit/cgit.cgi/perlmodules/>

=head1 BUGS

Plugin support is not quite finished (see the TODO file for details) but
is usable.  The missing bits are bells and whistles rather than core
functionality.

Plugins are unavailable if you can't change to the current directory.  This
might happen if you started your process as a priveleged user and then
dropped priveleges.  This is due to how we check for Taintedness.  If this
affects you, please supply a portable patch.

Please report any other bugs either by email to David Cantrell (see below
for address) or using rt.cpan.org:

L<https://rt.cpan.org/Ticket/Create.html?Queue=Data-Compare>

=head1 AUTHOR

Fabien Tassin E<lt>fta@sofaraway.orgE<gt>

Portions by David Cantrell E<lt>david@cantrell.org.ukE<gt>

=head1 COPYRIGHT and LICENCE

Copyright (c) 1999-2001 Fabien Tassin. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

Some parts copyright 2003 - 2010 David Cantrell.

Seeing that Fabien seems to have disappeared, David Cantrell has become
a co-maintainer so he can apply needed patches.  The licence, of course,
remains the same.  As the "perl licence" is "Artistic or GPL, your choice",
you can find them as the files ARTISTIC.txt and GPL2.txt in the
distribution.

=head1 SEE ALSO

perl(1), perlref(1)

=cut
