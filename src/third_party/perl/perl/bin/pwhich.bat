@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
"%~dp0perl.exe" -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl
:WinNT
"%~dp0perl.exe" -x -S %0 %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#!/usr/bin/perl
#line 15

use 5.004;
use strict;
use File::Which ();
use Getopt::Std ();

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.09';
}

# Handle options
my %opts = ();
Getopt::Std::getopts('av', \%opts);

if ( $opts{v} ) {
	print <<"END_TEXT";
This is pwhich running File::Which version $File::Which::VERSION

Copyright 2002 Per Einar Ellefsen.

Some parts copyright 2009 Adam Kennedy.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.
END_TEXT

	exit(0);
}

unless ( @ARGV ) {
	print <<"END_TEXT";
Usage: $0 [-a] [-v] programname [programname ...]
      -a        Print all matches in PATH, not just the first.
      -v        Prints version and exits

END_TEXT

	exit(0);
}

foreach my $file ( @ARGV ) {
	my @result = $opts{a}
		? File::Which::which($file)
		# Need to force scalar
		: scalar File::Which::which($file);

	# We might end up with @result = (undef) -> 1 elem
	@result = () unless defined $result[0];
	foreach my $result ( @result ) {
		print "$result\n" if $result;
	}
	unless ( @result ) {
		print STDERR "pwhich: no $file in PATH\n";
		exit(255);
	}
}

exit(0);

__END__

=pod

=head1 NAME

pwhich - Perl-only `which'

=head1 SYNOPSIS

  $ pwhich perl
  $ pwhich -a perl          # print all matches
  $ pwhich perl perldoc ... # look for multiple programs
  $ pwhich -a perl perldoc ...

=head1 DESCRIPTION

`pwhich' is a command-line utility program for finding paths to other
programs based on the user's C<PATH>. It is similar to the usualy Unix
tool `which', and tries to emulate its functionality, but is written
purely in Perl (uses the module C<File::Which>), so is portable.

=head2 Calling syntax

  $ pwhich [-a] [-v] programname [programname ...]

=head2 Options

=over

=item -a

The option I<-a> will make C<pwhich> print all matches found in the
C<PATH> variable instead of just the first one. Each match is printed
on a separate line.

=item -v

Prints version (of C<File::Which>) and copyright notice and exits.

=back

=head1 SUPPORT

Bugs should be reported via the CPAN bug tracker at

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=File-Which>

For other issues, contact the maintainer.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

Per Einar Ellefsen E<lt>pereinar@cpan.orgE<gt>

Originated in F<modperl-2.0/lib/Apache/Build.pm>. Changed for use in DocSet
(for the mod_perl site) and Win32-awareness by me, with slight modifications
by Stas Bekman, then extracted to create C<File::Which>.

Version 0.04 had some significant platform-related changes, taken from
the Perl Power Tools C<`which'> implementation by Abigail with
enhancements from Peter Prymmer. See
L<http://www.perl.com/language/ppt/src/which/index.html> for more
information.

=head1 COPYRIGHT

Copyright 2002 Per Einar Ellefsen.

Some parts copyright 2009 Adam Kennedy.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl>, L<File::Which>, L<which(1)>

=cut

__END__
:endofperl
