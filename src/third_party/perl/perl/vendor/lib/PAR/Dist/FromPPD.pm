package PAR::Dist::FromPPD;

use 5.006;
use strict;
use warnings;

our $VERSION = '0.03';

use PAR::Dist;
use LWP::Simple ();
use XML::Parser;
use Cwd qw/cwd abs_path/;
use File::Copy;
use File::Spec;
use File::Path;
use File::Temp ();
use Archive::Tar ();

require Exporter;

our @ISA = qw(Exporter);

our %EXPORT_TAGS = ( 'all' => [ qw(
    ppd_to_par get_ppd_content
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
    ppd_to_par
);


our $VERBOSE = 0;


sub _verbose {
    $VERBOSE = shift if (@_);
    return $VERBOSE
}

sub _diag {
    my $msg = shift;
    return unless _verbose();
    print $msg ."\n"; 
}

sub ppd_to_par {
    die "Uneven number of arguments to 'ppd_to_par'." if @_ % 2;
    my %args = @_;
    my @par_files;
 
    _verbose($args{'verbose'});

    if (not defined $args{uri}) {
        die "You need to specify an URI for the PPD file";
    }
    my $ppd_uri = $args{uri};

    my $outdir = abs_path(defined($args{out}) ? $args{out} : '.');
    die "Output path not a directory." if not -d $outdir;

    _diag "Looking for PPD.";

    my $ppd_text = get_ppd_content($ppd_uri);

    _diag "Parsing PPD XML.";
    my $parser = XML::Parser->new(Style => 'Tree');
    my $ppd_tree = $parser->parse($ppd_text);
    die "Parsing PPD XML failed" if not defined $ppd_tree;

    my $ppd_info = _ppd_to_info($ppd_tree);
    die "Malformed PPD" if not defined $ppd_info;

    _diag "Applying user overrides.";
    # override parsed data with user specified data
    my %arg_map = (
        distname => 'name',
        distversion => 'version',
    );
    _override_info($ppd_info, \%arg_map, \%args);

    if (not defined $ppd_info->{name}) {
        die "Missing distribution name";
    }
    if (not defined $ppd_info->{version}) {
        die "Missing distribution version";
    }
    if (not @{$ppd_info->{implementations}}) {
        die "No IMPLEMENTATION sections in the distribution";
    }

    # Select implementation
    _diag "Selecting implementation.";
    my $implem = [@{$ppd_info->{implementations}}];
    my $chosen;
    my $sperl = $args{selectperl};
    $sperl = qr/$sperl/ if defined $sperl;
    my $sarch = $args{selectarch};
    $sarch = qr/$sarch/ if defined $sarch;
    if (not $sarch) {
        if (not $sperl) {
            $chosen = $implem->[0];
        }
        else {
            # have $sperl not $sarch
            foreach my $impl (@$implem) {
                if ($impl->{perl} and $impl->{perl} =~ $sperl) {
                    $chosen = $impl;
                    last;
                }
            }
            $chosen = $implem->[0] if not $chosen;
        }
    }
    else {
        # have $sarch
        if (not $sperl) {
            foreach my $impl (@$implem) {
                if ($impl->{arch} and $impl->{arch} =~ $sarch) {
                    $chosen = $impl;
                    last;
                }
            }
            $chosen = $implem->[0] if not $chosen;
        }
        else {
            # both
            my @pre;
            foreach my $impl (@$implem) {
                if ($impl->{arch} and $impl->{arch} =~ $sarch) {
                    push @pre, $impl;
                }
            }
            if (not @pre) {
                $chosen = $implem->[0];
            }
            else {
                foreach my $impl (@pre) {
                    if ($impl->{perl} and $impl->{perl} =~ $sperl) {
                        $chosen = $impl;
                        last;
                    }
                }
                $chosen = $pre[0] if not $chosen;
            }
        }
    }
   
    # apply the rest of the overrides
    %arg_map = (
        arch => [qw(implementations arch)],
        perlversion => [qw(implementations perl)],
    );
    _override_info($ppd_info, \%arg_map, \%args);

    if (not defined $chosen->{arch}) {
        die "Architecture name of chosen implementation is undefined"
    }
    if (not defined $chosen->{perl}) {
        die "Minimum perl version of chosen implementation is undefined"
    }
    
    _diag "Creating temporary directory";
    my $tdir = File::Temp::tempdir( CLEANUP => 1 );
    
    _diag "Fetching (or finding) implementation file";
    my $impl_file;
    
    foreach my $uri (@{$chosen->{uri}}) {
        my $filename = $uri;
        $filename =~ s/^.*(?:\/|\\|:)([^\\\/:]+)$/$1/;
        my $localfile = File::Spec->catfile($tdir, $filename);
        if ($uri =~ /^(?:ftp|https?):\/\//) {
            my $code = LWP::Simple::getstore(
                $uri, $localfile
            );
            _diag("URI '$uri' via LWP '$localfile' failed. (LWP, code $code)"), next
              if not LWP::Simple::is_success($code);
            $impl_file = $localfile;
        }
        elsif ($uri =~ /^file:\/\// or $uri !~ /^\w+:\/\//) {
            # local file
            unless(-f $uri and File::Copy::copy($uri, $localfile)) {
                _diag "URI '$uri' failed. (local)";
                
                # try as relative URI
                my $base = $args{uri};
                if ($base =~ /^(?:https?|ftp):\/\//) {
                    $base =~ s!/[^/]+$!/$uri!;
                    my $code = LWP::Simple::getstore(
                        $base, $localfile
                    );
                    _diag("URI '$base' via LWP '$localfile' failed. (LWP, code $code)"), next
                      if not LWP::Simple::is_success($code);
                    $impl_file = $localfile;
                }
                else {
                    next;
                }
            }
            $impl_file = $localfile;
        }
        else {
            _diag "Invalid URI '$uri'.";
            next;
        }
    }
    

    if (not defined $impl_file) {
        _diag "All CODEBASEs failed.";
        File::Path::rmtree([$tdir]);
        return();
    }
    
    _diag "Local file: '$impl_file'";
    
    _diag "chdir() to '$tdir'";
    my $cwd = Cwd::cwd();
    chdir($tdir);
    
    _diag "Generating 'blib' stub'";
    PAR::Dist::generate_blib_stub(
        name => $ppd_info->{name},
        version => $ppd_info->{version},
        suffix => join('-', $chosen->{arch}, $chosen->{perl}),
    );
    
    _diag "Extracting local file.";
    my ($vol, $path, $file) = File::Spec->splitpath($impl_file);
    my $tar = Archive::Tar->new($file, 1)
      or chdir($cwd), die "Could not open .tar(.gz) file";
    
    $tar->extract();
    
    _diag "Building PAR ".$ppd_info->{name};

    my $par_file;
    eval {
        $par_file = PAR::Dist::blib_to_par(
            name => $ppd_info->{name},
            version => $ppd_info->{version},
            suffix => join('-', $chosen->{arch}, $chosen->{perl}).'.par',
        )
    } or chdir($cwd), die "Failed to build .par: $@";
  
    chdir($cwd), die "Could not find PAR distribution file '$par_file'."
      if not -f $par_file;
    
    _diag "Built PAR file '$par_file'.";

    _diag "Moving distribution file to output directory '$outdir'.";

    unless (File::Copy::move($par_file, $outdir)) {
        chdir($cwd);
        die "Could not move file '$par_file' to directory "
              . "'$outdir'. Reason: $!";
    }
   $par_file = File::Spec->catfile($outdir, $par_file);
   if (-f $par_file) {
       push @par_files, $par_file;
   }
   else {
       chdir($cwd);
       die "Lost PAR file along the way. (Ouch!) Expected it at '$par_file'";
   }

    # strip docs
    if ($args{strip_docs}) {
        _diag "Removing documentation from the PAR distribution(s).";
        PAR::Dist::remove_man($_) for @par_files;
    }

    chdir($cwd);
    File::Path::rmtree([$tdir]);
    return(1);
}



sub get_ppd_content {
    my $ppd_uri = shift;
    my $ppd_text;
        if ($ppd_uri =~ /^(?:https?|ftp):\/\//) {
        # fetch with LWP::Simple
        _diag "Fetching with LWP::Simple.";
        $ppd_text = LWP::Simple::get($ppd_uri);
        die "Could not fetch PPD content from '$ppd_uri' using LWP"
          if not defined $ppd_text;
    }
    elsif ($ppd_uri =~ /^file:\/\// or $ppd_uri !~ /^\w*:\/\//) {
        # It's a local file
        _diag "Reading PPD info from file.";
        $ppd_uri =~ s/^file:\/\///;
        open my $fh, '<', $ppd_uri
          or die "Could not read PPD content from file '$ppd_uri' ($!)";
        local $/ = undef;
        $ppd_text = <$fh>;
        close $fh;
        die "Could not read PPD content from file '$ppd_uri' ($!)"
          if not defined $ppd_text;
    }
    else {
        # Invalid URI (in our context)
        die "The PPD URI is invalid: '$ppd_uri'";
    }
    return $ppd_text;
}


sub _ppd_to_info {
    my $tree = shift;
    my $info = {
        name => undef,
        version => undef,
        title => undef,
        abstract => undef,
        author => undef,
        license => undef,
        deps => [],
        implementations => [],
    };

    return() if not defined $tree or not ref($tree) eq 'ARRAY';
    return() if not $tree->[0] =~ /^softpkg$/i;
    my $children = $tree->[1];
    my $dist_attr = shift @$children;
    $info->{name} = $dist_attr->{NAME};
    $info->{version} = $dist_attr->{VERSION};
    return() if not defined $info->{name} or not defined $info->{version};
    $info->{version} =~ s/,/./g;
    $info->{version} =~ s/(?:\.0)+$//;

    while (@$children) {
        my $tag = shift @$children;
        # Skip any direct content
        shift(@$children), next if $tag eq '0';
        if ($tag =~ /^implementation$/i) {
            my $impl = _parse_implementation(shift @$children);
            push @{$info->{implementations}}, $impl if defined $impl;
        }
        elsif ($tag =~ /^dependency$/i) {
            my $dep = _parse_dependency(shift @$children);
            push @{$info->{deps}}, $dep if defined $dep;
        }
        elsif ($tag =~ /^title$/i) {
            $info->{title} = shift(@$children)->[2];
        }
        elsif ($tag =~ /^abstract$/i) {
            $info->{abstract} = shift(@$children)->[2];
        }
        elsif ($tag =~ /^author$/i) {
            $info->{author} = shift(@$children)->[2];
        }
        elsif ($tag =~ /^license$/i) {
            $info->{license} = shift(@$children)->[0]{HREF};
        }
        else {
            shift @$children;
        }
    }
    return $info;
}


sub _parse_dependency {
    my $content_ary = shift;
    return(); # XXX currently unused and hence not implemented
}

sub _parse_implementation {
    my $impl_ary = shift;
    my $impl = {
        deps => [],
        os => [],
        arch => undef,
        uri => undef,
        processor => undef,
        language => undef,
        osversion => undef,
        perl => undef,
    };

    my $c = $impl_ary;
    shift @$c; # skip attributes

    while (@$c) {
        my $tag = shift @$c;
        if ($tag eq '0') {
            shift @$c;
        }
        elsif ($tag =~ /^language$/i) {
            $impl->{language} = shift(@$c)->[2];
        }
        elsif ($tag =~ /^os$/i) {
            my $attr = shift(@$c)->[0];
            push @{$impl->{os}}, $attr->{VALUE} || $attr->{NAME};
        }
        elsif ($tag =~ /^osversion$/i) {
            my $attr = shift(@$c)->[0];
            $impl->{osversion} = $attr->{VALUE} || $attr->{NAME};
        }
        elsif ($tag =~ /^perlcore$/i) {
            my $attr = shift(@$c)->[0];
            $impl->{perl} = $attr->{VERSION};
        }
        elsif ($tag =~ /^processor$/i) {
            my $attr = shift(@$c)->[0];
            $impl->{processor} = $attr->{VALUE} || $attr->{NAME};
        }
        elsif ($tag =~ /^architecture$/i) {
            my $attr = shift(@$c)->[0];
            $impl->{arch} = $attr->{VALUE} || $attr->{NAME};
        }
        elsif ($tag =~ /^codebase$/i) {
            my $attr = shift(@$c)->[0];
            push @{$impl->{uri}}, $attr->{HREF} || $attr->{FILENAME};
        }
        elsif ($tag =~ /^dependency$/i) {
            my $dep = _parse_dependency(shift @$c);
            push @{$impl->{deps}}, $dep if defined $dep;
        }
        else {
            shift @$c;
        }
    }

    return $impl;
}

sub _override_info {
    my $info = shift;
    my $arg_map = shift;
    my $args = shift;
    foreach my $arg (keys %$arg_map) {
        next if not defined $args->{$arg};
        my $to = $arg_map->{$arg};
        if (ref($to)) {
            my $ary = $info->{shift(@$to)};
            $ary->[$_]{$to->[0]} = $args->{$arg} for 0..$#$ary;
        }
        else {
            $info->{$to} = $args->{$arg};
        }
    }
}
    
1;
__END__

=head1 NAME

PAR::Dist::FromPPD - Create PAR distributions from PPD/PPM packages

=head1 SYNOPSIS

  use PAR::Dist::FromPPD;
  
  # Creates a .par distribution of the PAR module in the
  # current directory based on the PAR.ppd file from the excellent
  # bribes.org PPM repository.
  ppd_to_par(uri => 'http://www.bribes.org/perl/ppm/PAR.ppd');

  # You could download the .ppd and .tar.gz files first and then do:
  ppd_to_par(uri => 'PAR.ppd', verbose => 1);
  
=head1 DESCRIPTION

This module creates PAR distributions from PPD XML documents which
are used by ActiveState's "Perl Package Manager", short PPM.

It parses the PPD document to extract the required
information and then uses PAR::Dist to create a .par archive from it.

Please note that this code I<works for me> but hasn't been tested
to full extent.

=head2 EXPORT

By default, the C<ppd_to_par> subroutine is exported to the callers
namespace. C<get_ppd_content> will be exported on demand.

=head1 SUBROUTINES

This is a list of all public subroutines in the module.

=head2 ppd_to_par

The only mandatory parameter is an URI for the PPD file to parse.

Arguments:

  uri         => 'ftp://foo/bar' or 'file:///home/you/file.ppd', ...
  out         => 'directory'  (write distribution files to this directory)
  verbose     => 1/0 (verbose mode on/off)
  distname    => Override the distribution name
  distversion => Override the distribution version
  perlversion => Override the distribution's (minimum?) perl version
  arch        => Override the distribution's target architecture
  selectarch  => Regular Expression.
  selectperl  => Regular Expression.

C<arch> may also be set to C<any_arch> and C<perlversion> may be set to
C<any_version>.

If a regular expression is specified using C<selectarch>, that expression is
matched against the architecture settings of each implementation. The first
matching implementation is chosen. If none matches, the implementations
are tried in order of appearance. Of course, this heuristic is applied before
any architecture overriding via the C<arch> parameter is carried out.

C<selectperl> works the same as C<selectarch>, but operates on the (minimum)
perl version of an implementation. If both C<selectperl> and C<selectarch>
are present, C<selectperl> operates on the implementations matched by
C<selectarch>. That means C<selectarch> takes precedence.

=head2 get_ppd_content

First argument must be an URI string for the PPD.
(Supported are C<file://> URIs and whatever L<LWP>
supports.)

Fetches the PPD file and returns its contents as a string.

C<die()>s on error.

=head1 SEE ALSO

The L<PAR::Dist> module is used to create .par distributions from an
unpacked CPAN distribution. The L<CPAN> module is used to fetch the
distributions from the CPAN.

PAR has a mailing list, <par@perl.org>, that you can write to; send an empty mail to <par-subscribe@perl.org> to join the list and participate in the discussion.

Please send bug reports to <bug-par-dist-fromppd@rt.cpan.org>.

The official PAR website may be of help, too: http://par.perl.org

For details on the I<Perl Package Manager>, please refer to ActiveState's
website at L<http://activestate.com>.

=head1 AUTHOR

Steffen Mueller, E<lt>smueller at cpan dot orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006 by Steffen Mueller

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.6 or,
at your option, any later version of Perl 5 you may have available.

=cut
