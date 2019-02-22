package pip;

use 5.006;
use strict;
use File::Spec         ();
use File::Temp         ();
use File::Which        ();
use Getopt::Long       ();
use URI::file          ();
use Module::Plan::Base ();

use vars qw{$VERSION};
BEGIN {
	$VERSION = '1.19';
}





#####################################################################
# Main Function

# Save a copy of @ARGV for error messages
my $install = 0;
Getopt::Long::GetOptions(
	install => \$install,
);

sub main {
	unless ( @ARGV ) {
		error("Did not provide a command");
	}

	# If the first argument is a file, install it
	if ( $ARGV[0] =~ /^(https?|ftp)\:\/\// ) {
		# resolving redirects to get the real URI, handy for handling
		# e.g. github URLs like http://github.com/john/repo-name/tarball/master
		require LWP::Simple;
		my $h = LWP::Simple::head($ARGV[0]);
		error("Probably non existing URI '$ARGV[0]'") unless defined($h);
		return fetch_any($h->request->uri || $ARGV[0]);
	}
	if ( -f $ARGV[0] ) {
		return install_any($ARGV[0]);
	}

	error("Unknown or unsupported command '$ARGV[0]'");
}

sub fetch_any {
	my $uri = $_[0];

	# Handle tarballs via a custom Module::Plan::Lite object.
	# Also handle PAR archives
	if ( $uri =~ /\.(?:par|zip|tar\.gz)$/  ) {
		require Module::Plan::Lite;
		my $plan = Module::Plan::Lite->new(
			p5i   => 'default.p5i',
			lines => [ '', $uri ],
		);
		$plan->run;
		return 1;
	}

	# P5I files can have a plan created for the remote URI
	if ( $uri =~ /\.p5i$/ ) {
		require Module::Plan::Lite;
		my $plan = Module::Plan::Lite->new(
			p5i => $uri,
		);
		$plan->run;
		return 1;
	}

	# We don't yet support remote p5z files
	if ( $uri =~ /\.p5z$/ ) {
		error("Remote p5z installation is not yet supported");
	}

	error("Unknown or unsupported uri '$uri'");
}

sub install_any {
	# Load the plan
	my $plan = read_any(@_);

	# Run it
	$plan->run;

	return 1;
}

sub read_any {
	my $param = $_[0];

	# If the first argument is a tar.gz file, hand off to install
	if ( $param =~ /\.(?:zip|tar\.gz|tgz)$/ ) {
		return read_archive(@_);
	}

	# If the first argument is a par file, hand off to install
	if ( $param =~ /\.par$/ ) {
		return read_archive(@_);
	}

	# If the first argument is a p5i file, hand off to read
	if ( $param =~ /\.p5i$/ ) {
		return read_p5i(@_);
	}

	# If the first argument is a p5z file, hand off to instal
	if ( $param =~ /\.p5z$/ ) {
		return read_p5z(@_);
	}

	error("Unknown or unsupported file '$param'");
}

# Create the plan object from a file
sub read_p5i {
	my $pip = @_ ? shift : File::Spec->curdir;
	if ( -d $pip ) {
		$pip = File::Spec->catfile( $pip, 'default.p5i' );
	}
	$pip = File::Spec->rel2abs( $pip );
	unless ( -f $pip ) {
		error( "The plan file $pip does not exist" );
	}

	# Create the plan object
	my $plan = eval {
		Module::Plan::Base->read( $pip );
	};
	if ( $@ ) {
		unless ( $@ =~ /The sources directory is not owned by the current user/ ) {
			# Rethrow the error
			die $@;
		}

		# Generate an appropriate error
		my @msg = (
			"The current user does not control the default CPAN client",
		);
		if ( File::Which::which('sudo') ) {
			my $cmd = join(' ', 'sudo', '-H', $0, @_);
			push @msg, "You may need to try again with the following command:";
			push @msg, "";
			push @msg, "  $cmd";
		}
		error( @msg );
	}

	return $plan;
}

sub read_archive {
	my $archive = File::Spec->rel2abs(shift);
	unless ( -f $archive ) {
		error("Filed does no exist: $archive");
	}
	require Module::Plan::Lite;
	Module::Plan::Lite->new(
		p5i   => 'default.p5i',
		lines => [ '', URI::file->new($archive)->as_string ],
	);
}

sub read_p5z {
	my $p5z = File::Spec->rel2abs(shift);
	unless ( -f $p5z ) {
		error("File does not exist: $p5z");
	}

	# Create the temp directory
	my $dir   = File::Temp::tempdir( CLEANUP => 1 );
	my $pushd = File::pushd::pushd( $dir );

	# Extract the tarball
	require Archive::Tar;
	my @files = Archive::Tar->extract_archive( $p5z, 1 );
	unless ( @files ) {
		error( "Failed to extract P5Z file: " . Archive::Tar->error );
	}

	# Find the plan
	my $path = File::Spec->catfile( $dir, 'default.p5i' );
	unless ( -f $path ) {
		error("P5Z file did not contain a default.p5i");
	}

	# Load the plan
	return read_p5i( $path );
}





#####################################################################
# Support Functions

sub error {
	print "\n";
	print map { $_ . "\n" } @_;
	exit(255);
}

1;

=pod

=head1 NAME

pip - The Perl Installation Program, for scripted and third-party
distribution installation.

=head1 SYNOPSIS

  pip script.p5i
  pip script.p5z
  pip Distribution-1.23.tgz
  pip Distribution-1.23.tar.gz
  pip Distribution-1.23-MSWin32-5.8.0.par
  pip http://server/Distribution-1.23.tar.gz
  pip http://github.com/gitpan/Distribution/tarball/1.23

=head1 DESCRIPTION

The B<pip> ("Perl Installation Program") console application is used to
install Perl distributions in a wide variety of formats, both from CPAN
and from external third-party locations, while supporting module
dependencies that go across the boundary from third-party to CPAN.

Using B<pip> you can install CPAN modules, arbitrary tarballs from both
the local file-system or across the internet from arbitrary URIs.

You can use B<pip> to ensure that specific versions of CPAN modules are
installed I<instead> of the most current version.

And beyond just single installations, you script script a series of
these installations by creating a "P5I" (Perl 5 Installation) file.

A Perl 5 Installation (P5I) file is a small script-like file that
describes a set of distributions to install, and integrates the
installation of these distributions with the CPAN installer.

The primary use of P5I files are for installing proprietary or
non-CPAN software that may still require the installation of a
number of CPAN dependencies in order to function.

P5I files are also extensible, with the first line of the file
specifying the name of the Perl class that implements the plan.

For the moment, the class described at the top of the P5I file
must be installed.

The simple L<Module::Plan::Lite> plan class is bundled with the main
distribution, and additional types can be installed if needed.

=head2 Future Additions

Also on the development schedule for B<pip> is the creation and
installation of distributions via "P5Z" files, which are tarballs
containing a P5I file, as well as all the distribution tarballs
referenced by the P5I file.

It is also anticipated that B<pip> will gain support for L<PAR>
binary packages and potentially also for ActivePerl L<PPM> files.

=head1 USAGE

The primary use of F<pip> is to install from a P5I script, with the
canonical use case as follows:

  pip directory/myplan.p5i

This command will load the plan file F<directory/myplan.p5i>, create
the plan, and then execute it.

If only a directory name is given, F<pip> will look for a F<default.p5i>
plan in the directory. Thus, all of the following are equivalent

  pip directory
  pip directory/
  pip directory/default.p5i

If no target is provided at all, then the current directory will be used.
Thus, the following are equivalent

  pip
  pip .
  pip default.p5i
  pip ./default.p5i

=head2 Syntax of a plan file

Initially, the only plan is available is the L<Module::Plan::Lite>
(MPL) plan.

A typical MPL plan will look like the following

  # myplan.p5i
  Module::Plan::Lite
  
  Process-0.17.tar.gz
  YAML-Tiny-0.10.tar.gz

=head2 Direct installation of a single tarball

With the functionality available in F<pip>, you can find that sometimes
you don't even want to make a file at all, you just want to install a
single tarball.

The C<-i> option lets you pass the name of a single file and it will treat
it as an installer for that single file. Further, if the extension of the
tarball is .tar.gz, the B<-i> option is implied.

For example, the following are equivalent.

  # Installing with the -i|--install option
  > pip Process-0.17.tar.gz
  > pip -i Process-0.17.tar.gz
  > pip --install Process-0.17.tar.gz
  
  # Installing from the file as normal
  > pip ./default.p5i
  
  # myplan.p5i
  Module::Plan::Lite
  
  Process-0.17.tar.gz

The C<-i> option can be used with any single value supported by
L<Module::Plan::Lite> (see above).

This means you can also use B<pip> to install a distribution from any
arbitrary URI, including installing direct from a subversion repository.

  > pip http://svn.ali.as/cpan/release/Process-0.17.tar.gz

=head1 SUPPORT

This module is stored in an Open Repository at the following address.

L<http://svn.ali.as/cpan/trunk/pip>

Write access to the repository is made available automatically to any
published CPAN author, and to most other volunteers on request.

If you are able to submit your bug report in the form of new (failing)
unit tests, or can apply your fix directly instead of submitting a patch,
you are B<strongly> encouraged to do so. The author currently maintains
over 100 modules and it may take some time to deal with non-Critical bug
reports or patches.

This will guarentee that your issue will be addressed in the next
release of the module.

If you cannot provide a direct test or fix, or don't have time to do so,
then regular bug reports are still accepted and appreciated via the CPAN
bug tracker.

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=pip>

For other issues, for commercial enhancement and support, or to have your
write access enabled for the repository, contact the author at the email
address above.

=head1 AUTHORS

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<Module::Plan::Base>, L<Module::Plan::Lite>, L<Module::Plan>

=head1 COPYRIGHT

Copyright 2006 - 2010 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
