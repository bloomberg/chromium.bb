@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
IF EXIST "%~dp0perl.exe" (
"%~dp0perl.exe" -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
) ELSE IF EXIST "%~dp0..\..\bin\perl.exe" (
"%~dp0..\..\bin\perl.exe" -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
) ELSE (
perl -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
)

goto endofperl
:WinNT
IF EXIST "%~dp0perl.exe" (
"%~dp0perl.exe" -x -S %0 %*
) ELSE IF EXIST "%~dp0..\..\bin\perl.exe" (
"%~dp0..\..\bin\perl.exe" -x -S %0 %*
) ELSE (
perl -x -S %0 %*
)

if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#!/usr/bin/perl
#line 29
use strict;
use warnings;

use Getopt::Long qw/GetOptions/;
use PAR::Dist::InstallPPD;

our $VERSION = '0.02';

=pod

=head1 NAME

parinstallppd - Install PPD/PPM packages using PAR

=head1 SYNOPSIS

parinstallppd --help

parinstallppd [-v ...] -u PPD-URI-OR-FILE

=head1 DESCRIPTION

This script converts packages for the I<Perl Package Manager>
which is specific to ActiveState's perl distributions to F<.par>
files and installs those using L<PAR::Dist>.

It works much the same way as the L<ppd2par> tool but with an additional
installation step after the conversion.

=head2 Parameters

  -u --uri
    Set the place to fetch the .ppd file from. Can be an URL
    (http://..., https://..., ftp://...) or a local file.
  -v --verbose
    Sets the verbose mode.

  --sa --selectarch
    Regexp for selecting the implementation based on architecture.
    Defaults to the currently running architecture.
  --sp --selectperl
    Regexp for selecting the implementation based on perl version.
    Defaults to the currently running perl version (e.g. 5.8.8) and falls
    back to the main version (e.g. 5.8) and then other sub-versions
    (e.g. 5.8.7)

=head1 SEE ALSO

This tool is implemented using the L<PAR::Dist::InstallPPD> module. Please
refer to that module's documentation for details on how this all works.

PAR has a mailing list, <par@perl.org>, that you can write to; send
an empty mail to <par-subscribe@perl.org> to join the list and
participate in the discussion.

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

my $usage = <<"HERE";
$0 --help          # for help

$0 [-v --no-docs] -u PPD-URI-OR-FILE

This script converts packages for the I<Perl Package Manager>
which is specific to ActiveState's perl distributions to F<.par>
files and installs those using L<PAR::Dist>.

It works much the same way as the L<ppd2par> tool but with an additional
installation step after the conversion.

-u --uri
  Set the place to fetch the .ppd file from. Can be an URL
  (http://..., https://..., ftp://...) or a local file.
-v --verbose
  Sets the verbose mode.

--sa --selectarch
  Regexp for selecting the implementation based on architecture.
  Defaults to the currently running architecture.
--sp --selectperl
  Regexp for selecting the implementation based on perl version.
  Defaults to the currently running perl version (e.g. 5.8.8) and
  falls   back to the main version (e.g. 5.8) and then other
  sub-versions (e.g. 5.8.7)
HERE

my $uri;
my $v = 0;
my $sperl;
my $sarch;
GetOptions(
    'sa|selectarch=s' => \$sarch,
    'sp|selectperl=s' => \$sperl,
	'h|help' => sub { print $usage; exit(1) },
	'u|uri=s' => \$uri,
	'v|verbose' => \$v,
);

par_install_ppd(
	uri => $uri,
	($v               ? (verbose      => 1            ) : ()),
    ($sarch           ? (selectarch   => $sarch       ) : ()),
    ($sperl           ? (selectperl   => $sperl       ) : ()),
);


__END__
:endofperl
