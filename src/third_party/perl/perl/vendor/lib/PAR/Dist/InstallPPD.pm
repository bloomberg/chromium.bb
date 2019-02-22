package PAR::Dist::InstallPPD;

use 5.006;
use strict;
use warnings;

use PAR::Dist::FromPPD ();
use PAR::Dist ();
use File::Temp ();
use File::Spec;
use File::Path;
use Cwd;

require Config;
require Exporter;

our @ISA = qw(Exporter);

our %EXPORT_TAGS = ( 'all' => [ qw(
    par_install_ppd
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
    par_install_ppd
);

our $VERSION = '0.02';

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

sub par_install_ppd {
    die "Uneven number of arguments to 'par_install_ppd'." if @_ % 2;
    my %args = @_;
 
    _verbose($args{'verbose'});

    _diag "Creating temporary directory for temporary .par";

    my $tdir = $args{out} = File::Temp::tempdir(
        CLEANUP => 1,
        DIR => File::Spec->tmpdir(),
    );

    _diag "Preparing meta data for temporary .par";

    # should be figured out by ::FromPPD
    delete $args{$_} for qw(
        distversion perlversion
    );
    # just need to be defined.
    $args{arch} = $Config::Config{archname};
    $args{perlversion} = sprintf('%vd', $^V);

    # Accept running perl version (5.8.8)
    # or main perl version (5.8)
    # or any other subversions (5.8.6)
    my $perlver = sprintf('%vd', $^V);
    my $mainperlver = $perlver;
    $mainperlver =~ s/^(\d+)\.(\d+)\..*$/$1.$2/;
    _diag "Setting perl version to ($perlver|$mainperlver|$mainperlver\\.\\d+)"
      if not defined $args{selectperl};
    $args{selectperl} ||= qr/^(?:$perlver|$mainperlver|$mainperlver\.\d+)$/;


    # Accept running arch
    my $arch = quotemeta( $Config::Config{archname} );
    _diag "Setting architecture to $Config::Config{archname}"
      if not defined $args{selectarch};
    my $perlver_nodots = $mainperlver;
    $perlver_nodots =~ s/\.//g;
    $args{selectarch} ||= qr/^(?:$arch-?(?:$perlver_nodots\d*|$mainperlver(?:\.\d+)?)|$arch)$/;

    _diag "Using temporary directory $tdir.";
    _diag "Invoking PAR::Dist::FromPPD to create the .par file.";

    PAR::Dist::FromPPD::ppd_to_par(%args);

    _diag "Searching for generated .par file.";

    _diag "chdir() to '$tdir'";
    my $cwd = Cwd::cwd();
    chdir($tdir);
    
    opendir my $dh, '.' or die $!;

    my @par_files = grep {-f $_ and /\.par$/i} readdir($dh);

    _diag "Found PAR files: @par_files.";

    _diag "Installing PAR files.";

    foreach my $file (@par_files) {
        _diag "Installing file '$file' with PAR::Dist::install_par().";
        PAR::Dist::install_par($file);
    }

    _diag "Done installing PAR files.";

    chdir($cwd);
    File::Path::rmtree([$tdir]);
    return(1);
}




1;
__END__

=head1 NAME

PAR::Dist::InstallPPD - Installs PPM packages the PAR way

=head1 SYNOPSIS

  use PAR::Dist::InstallPPD;
  
  # Creates a .par distribution of the Tk module in the
  # current directory based on the Tk.ppd file from the excellent
  # bribes.org PPM repository.
  par_install_ppd(uri => 'http://www.bribes.org/perl/ppm/Tk.ppd');

  # You could download the .ppd and .tar.gz files first and then do:
  par_install_ppd(uri => 'Tk.ppd', verbose => 1);
  
=head1 DESCRIPTION

This module creates PAR distributions from PPD XML documents which
are used by ActiveState's "Perl Package Manager", short PPM.
Then, it installs these newly created temporary F<.par> files in the
default location using L<PAR::Dist>'s C<install_par> routine.

Yes, that means you can install packages for the PPM without PPM.

The module uses L<PAR::Dist::FromPPD> to create the F<.par> files.

Please note that this code I<works for me> but hasn't been tested
to full extent.

=head2 EXPORT

By default, the C<par_install_ppd> subroutine is exported to the caller's
namespace.

=head1 SUBROUTINES

This is a list of all public subroutines in the module.

=head2 par_install_ppd

This routine takes the same arguments as C<ppd_to_par> from
L<PAR::Dist::FromPPD> except for the output directory and the
options that set the meta data for the produced F<.par> file.
The details are reproduced below.

The only mandatory parameter is an URI for the PPD file to parse.

Arguments:

  uri         => 'ftp://foo/bar' or 'file:///home/you/file.ppd', ...
  verbose     => 1/0 (verbose mode on/off)
  selectarch  => Regular Expression.
  selectperl  => Regular Expression.

If a regular expression is specified using C<selectarch>, that expression is
matched against the architecture settings of each implementation. The first
matching implementation is chosen. If none matches, the implementations
are tried in order of appearance.

C<selectperl> works the same as C<selectarch>, but operates on the (minimum)
perl version of an implementation. If both C<selectperl> and C<selectarch>
are present, C<selectperl> operates on the implementations matched by
C<selectarch>. That means C<selectarch> takes precedence.

=head1 SEE ALSO

The L<PAR::Dist> module is used to create L<.par> distributions and install
them. The L<PAR::Dist::FromPPD> module converts the PPD package descriptions.

PAR has a mailing list, <par@perl.org>, that you can write to; send an empty mail to <par-subscribe@perl.org> to join the list and participate in the discussion.

Please send bug reports to <bug-par-dist-fromcpan@rt.cpan.org>.

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
