package CPAN::Checksums;

use strict;
use vars qw(
            $CAUTION
            $DIRNAME
            $IGNORE_MATCH
            $MIN_MTIME_CHECKSUMS
            $SIGNING_KEY
            $SIGNING_PROGRAM
            $TRY_SHORTNAME
            $VERSION
            @EXPORT_OK
            @ISA
           );

require Exporter;

@ISA = qw(Exporter);
@EXPORT_OK = qw(updatedir);
$VERSION = "2.08";
$VERSION =~ s/_//;
$CAUTION ||= 0;
$TRY_SHORTNAME ||= 0;
$SIGNING_PROGRAM ||= 'gpg --clearsign --default-key ';
$SIGNING_KEY ||= '';
$MIN_MTIME_CHECKSUMS ||= 0;
$IGNORE_MATCH = qr{(?i-xsm:readme$)};

use DirHandle ();
use IO::File ();
use Digest::MD5 ();
use Compress::Bzip2();
use Compress::Zlib ();
use File::Spec ();
use File::Temp;
use Data::Dumper ();
use Data::Compare ();
use Digest::SHA ();

sub _dir_to_dref {
  my($dirname,$old_dref) = @_;
  my($dref) = {};
  my($dh)= DirHandle->new;
  my($fh) = new IO::File;
  $dh->open($dirname) or die "Couldn't opendir $dirname\: $!";
  my(%shortnameseen);
 DIRENT: for my $de ($dh->read) {
    next if $de =~ /^\./;
    next if substr($de,0,9) eq "CHECKSUMS";
    next if $IGNORE_MATCH && $de =~ $IGNORE_MATCH;

    my $abs = File::Spec->catfile($dirname,$de);

    #
    # SHORTNAME offers an 8.3 name, probably not needed but it was
    # always there,,,
    #
    if ($TRY_SHORTNAME) {
      my $shortname = lc $de;
      $shortname =~ s/\.tar[._-]gz$/\.tgz/;
      my $suffix;
      ($suffix = $shortname) =~ s/.*\.//;
      substr($suffix,3) = "" if length($suffix) > 3;
      my @p;
      if ($shortname =~ /\-/) {
        @p = $shortname =~ /(.{1,16})-.*?([\d\.]{2,8})/;
      } else {
        @p = $shortname =~ /(.{1,8}).*?([\d\.]{2,8})/;
      }
      $p[0] ||= lc $de;
      $p[0] =~ s/[^a-z0-9]//g;
      $p[1] ||= 0;
      $p[1] =~ s/\D//g;
      my $counter = 7;
      while (length($p[0]) + length($p[1]) > 8) {
        substr($p[0], $counter) = "" if length($p[0]) > $counter;
        substr($p[1], $counter) = "" if length($p[1]) > $counter--;
      }
      my $dot = $suffix ? "." : "";
      $shortname = "$p[0]$p[1]$dot$suffix";
      while (exists $shortnameseen{$shortname}) {
        my($modi) = $shortname =~ /([a-z\d]+)/;
        $modi++;
        $shortname = "$modi$dot$suffix";
        if (++$counter > 1000){ # avoid endless loops and accept the buggy choice
          warn "Warning: long loop on shortname[$shortname]de[$de]";
          last;
        }
      }
      $dref->{$de}->{shortname} = $shortname;
      $shortnameseen{$shortname} = undef; # for exists check good enough
    }

    #
    # STAT facts
    #
    if (-l File::Spec->catdir($dirname,$de)){
      # Symlinks are a mess on a replicated, database driven system,
      # but as they are not forbidden, we cannot ignore them. We do
      # have a directory with nothing but a symlink in it. When we
      # ignored the symlink, we did not write a CHECKSUMS file and
      # CPAN.pm issued lots of warnings:-(
      $dref->{$de}{issymlink} = 1;
    }
    if (-d File::Spec->catdir($dirname,$de)){
      $dref->{$de}{isdir} = 1;
    } else {
      my @stat = stat $abs or next DIRENT;
      $dref->{$de}{size} = $stat[7];
      my(@gmtime) = gmtime $stat[9];
      $gmtime[4]++;
      $gmtime[5]+=1900;
      $dref->{$de}{mtime} = sprintf "%04d-%02d-%02d", @gmtime[5,4,3];
      _add_digests($de,$dref,"Digest::SHA",[256],"sha256",$abs,$old_dref);
      my $can_reuse_old_md5 = 1;
    COMPARE: for my $param (qw(size mtime sha256)) {
        if (!exists $old_dref->{$de}{$param} ||
            $dref->{$de}{$param} ne $old_dref->{$de}{$param}) {
          $can_reuse_old_md5 = 0;
          last COMPARE;
        }
      }
      if ( $can_reuse_old_md5 ) {
        for my $param (qw(md5 md5-ungz md5-unbz2)) {
          next unless exists $old_dref->{$de}{$param};
          $dref->{$de}{$param} = $old_dref->{$de}{$param};
        }
      } else {
        _add_digests($de,$dref,"Digest::MD5",[],"md5",$abs,$old_dref);
      }

    } # ! -d
  }
  $dh->close;
  $dref;
}

sub _read_old_ddump {
  my($ckfn) = @_;
  my $is_signed = 0;
  my($fh) = new IO::File;
  my $old_ddump = "";
  if ($fh->open($ckfn)) {
    local $/ = "\n";
    while (<$fh>) {
      next if /^\#/;
      $is_signed = 1 if /SIGNED MESSAGE/;
      $old_ddump .= $_;
    }
    close $fh;
  }
  return($old_ddump,$is_signed);
}

sub updatedir ($) {
  my($dirname) = @_;
  my $ckfn = File::Spec->catfile($dirname, "CHECKSUMS"); # checksum-file-name
  my($old_ddump,$is_signed) = _read_old_ddump($ckfn);
  my($old_dref) = makehashref($old_ddump);
  my $dref = _dir_to_dref($dirname,$old_dref);
  local $Data::Dumper::Indent = 1;
  local $Data::Dumper::Quotekeys = 1;
  local $Data::Dumper::Sortkeys = 1;
  my $ddump = Data::Dumper->new([$dref],["cksum"])->Dump;
  my @ckfnstat = stat $ckfn;
  if ($old_ddump) {
    local $DIRNAME = $dirname;
    if ( !!$SIGNING_KEY == !!$is_signed ) { # either both or neither
      if (!$MIN_MTIME_CHECKSUMS || $ckfnstat[9] > $MIN_MTIME_CHECKSUMS ) {
        # recent enough
        return 1 if $old_ddump eq $ddump;
        return 1 if ckcmp($old_dref,$dref);
      }
    }
    if ($CAUTION) {
      my $report = investigate($old_dref,$dref);
      warn $report if $report;
    }
  }
  my $ft = File::Temp->new(
                           DIR => $dirname,
                           TEMPLATE => "CHECKSUMS.XXXX",
                           CLEANUP => 0,
                          ) or die;
  my $tckfn = $ft->filename;
  close $ft;
  my($fh) = new IO::File;
  open $fh, ">$tckfn\0" or die "Couldn't open >$tckfn\: $!";

  local $\;
  if ($SIGNING_KEY) {
    print $fh "0&&<<''; # this PGP-signed message is also valid perl\n";
    close $fh;
    open $fh, "| $SIGNING_PROGRAM $SIGNING_KEY >> $tckfn"
        or die "Could not call gpg: $!";
    $ddump .= "__END__\n";
  }

  my $message = sprintf "# CHECKSUMS file written on %s GMT by CPAN::Checksums (v%s)\n%s",
      scalar gmtime, $VERSION, $ddump;
  print $fh $message;
  my $success = close $fh;
  if ($SIGNING_KEY && !$success) {
    warn "Couldn't run '$SIGNING_PROGRAM $SIGNING_KEY'!
Writing to $tckfn directly";
    open $fh, ">$tckfn\0" or die "Couldn't open >$tckfn\: $!";
    print $fh $message;
    close $fh or warn "Couldn't close $tckfn: $!";
  }
  chmod 0644, $ckfn or die "Couldn't chmod to 0644 for $ckfn\: $!" if -f $ckfn;
  rename $tckfn, $ckfn or die "Could not rename: $!";
  chmod 0444, $ckfn or die "Couldn't chmod to 0444 for $ckfn\: $!";
  return 2;
}

sub _add_digests ($$$$$$$) {
  my($de,$dref,$module,$constructor_args,$keyname,$abs,$old_dref) = @_;
  my($fh) = new IO::File;
  my $dig = $module->new(@$constructor_args);
  $fh->open("$abs\0") or die "Couldn't open $abs: $!";
  binmode($fh); # make sure it's called as a function, solaris with
                # perl 5.8.4 complained about missing method in
                # IO::File
  $dig->addfile($fh);
  $fh->close;
  my $digest = $dig->hexdigest;
  $dref->{$de}{$keyname} = $digest;
  $dig = $module->new(@$constructor_args);
  if ($de =~ /\.gz$/) {
    my($buffer, $zip);
    if (exists $old_dref->{$de}{$keyname} &&
        $dref->{$de}{$keyname} eq $old_dref->{$de}{$keyname} &&
        exists $old_dref->{$de}{"$keyname-ungz"}
       ) {
      $dref->{$de}{"$keyname-ungz"} = $old_dref->{$de}{"$keyname-ungz"};
      return;
    }
    if ($zip  = Compress::Zlib::gzopen($abs, "rb")) {
      $dig->add($buffer)
          while $zip->gzread($buffer) > 0;
      $dref->{$de}{"$keyname-ungz"} = $dig->hexdigest;
      $zip->gzclose;
    }
  } elsif ($de =~ /\.bz2$/) {
    my($buffer, $zip);
    if (exists $old_dref->{$de}{$keyname} &&
        $dref->{$de}{$keyname} eq $old_dref->{$de}{$keyname} &&
        exists $old_dref->{$de}{"$keyname-unbz2"}
       ) {
      $dref->{$de}{"$keyname-unbz2"} = $old_dref->{$de}{"$keyname-unbz2"};
      return;
    }
    if ($zip  = Compress::Bzip2::bzopen($abs, "rb")) {
      $dig->add($buffer)
          while $zip->bzread($buffer) > 0;
      $dref->{$de}{"$keyname-unbz2"} = $dig->hexdigest;
      $zip->bzclose;
    }
  }
}

sub ckcmp ($$) {
  my($old,$new) = @_;
  for ($old,$new) {
    $_ = makehashref($_);
  }
  Data::Compare::Compare($old,$new);
}

# see if a file changed but the name not
sub investigate ($$) {
  my($old,$new) = @_;
  for ($old,$new) {
    $_ = makehashref($_);
  }
  my $complain = "";
  for my $dist (sort keys %$new) {
    if (exists $old->{$dist}) {
      my $headersaid;
      for my $diff (qw/md5 sha256 size md5-ungz sha256-ungz mtime/) {
        next unless exists $old->{$dist}{$diff} &&
            exists $new->{$dist}{$diff};
        next if $old->{$dist}{$diff} eq $new->{$dist}{$diff};
        $complain .=
            scalar gmtime().
                " GMT:\ndiffering old/new version of same file $dist:\n"
                    unless $headersaid++;
        $complain .=
            qq{\t$diff "$old->{$dist}{$diff}" -> "$new->{$dist}{$diff}"\n}; #};
      }
    }
  }
  $complain;
}

sub makehashref ($) {
  local($_) = shift;
  unless (ref $_ eq "HASH") {
    require Safe;
    my($comp) = Safe->new("CPAN::Checksums::reval");
    my $cksum; # used by Data::Dumper
    $_ = $comp->reval($_) || {};
    die "CPAN::Checksums: Caught error[$@] while checking $DIRNAME" if $@;
  }
  $_;
}

1;

__END__

=head1 NAME

CPAN::Checksums - Write a C<CHECKSUMS> file for a directory as on CPAN

=head1 SYNOPSIS

  use CPAN::Checksums qw(updatedir);
  my $success = updatedir($directory);

=head1 INCOMPATIBILITY ALERT

Since version 1.0 the generation of the attribute C<shortname> is
turned off by default. It was too slow and was not used as far as I
know, and above all, it could fail on large directories. The shortname
feature can still be turned on by setting the global variable
$TRY_SHORTNAME to a true value.

=head1 DESCRIPTION

=over 2

=item $success = updatedir($dir)

C<updatedir()> takes a directory name as argument and writes a typical
C<CHECKSUMS> file in that directory as used on CPAN unless a previously
written C<CHECKSUMS> file is there that is still valid. Returns 2 if a
new C<CHECKSUMS> file has been written, 1 if a valid C<CHECKSUMS> file is
already there, otherwise dies.

Note: since version 2.0 updatedir on empty directories behaves just
the same. In older versions it silently did nothing.

=back

=head2 Global Variables in package CPAN::Checksums

=over

=item $IGNORE_MATCH

If the global variable $IGNORE_MATCH is set, then all files matching
this expression will be completely ignored and will not be included in
the CPAN C<CHECKSUMS> files. Per default this variable is set to

    qr{(?i-xsm:readme$)}

=item $CAUTION

Setting the global variable $CAUTION causes updatedir() to report
changes of files in the attributes C<size>, C<mtime>, C<md5>, or
C<md5-ungz> to STDERR.

=item $TRY_SHORTNAME

By setting the global variable $TRY_SHORTNAME to a true value, you can
tell updatedir() to include an attribute C<shortname> in the resulting
hash that is 8.3-compatible. Please note, that updatedir() in this
case may be slow and may even fail on large directories, because it
will always only try 1000 iterations to find a name that is not yet
taken and then give up.

=item $SIGNING_KEY

Setting the global variable $SIGNING_KEY makes the generated C<CHECKSUMS>
file to be clear-signed by the command specified in $SIGNING_PROGRAM
(defaults to C<gpg --clearsign --default-key >), passing the signing
key as an extra argument.  The resulting C<CHECKSUMS> file should look like:

    0&&<<''; # this PGP-signed message is also valid perl
    -----BEGIN PGP SIGNED MESSAGE-----
    Hash: SHA1

    # CHECKSUMS file written on ... by CPAN::Checksums (v...)
    $cksum = {
	...
    };

    __END__
    -----BEGIN PGP SIGNATURE-----
    ...
    -----END PGP SIGNATURE-----

note that the actual data remains intact, but two extra lines are
added to make it legal for both OpenPGP and perl syntax.

=item $MIN_MTIME_CHECKSUMS

If the global variable $MIN_MTIME_CHECKSUMS is set, then updatedir
will renew signatures on checksum files that have an older mtime than
the given value.

=back

=head1 PREREQUISITES

DirHandle, IO::File, Digest::MD5, Digest::SHA, Compress::Bzip2,
Compress::Zlib, File::Spec, Data::Dumper, Data::Compare, File::Temp

=head1 BUGS

If updatedir is interrupted, it may leave a temporary file lying
around. These files have the File::Temp template C<CHECKSUMS.XXXX> and
should be harvested by a cronjob.

=head1 AUTHOR

Andreas Koenig, andreas.koenig@anima.de; GnuPG support by Autrijus Tang

=head1 COPYRIGHT & LICENSE

Copyright (c) 2002-2008 Andreas Koenig, Audrey Tang, Steve Peters.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

perl(1).

=cut
