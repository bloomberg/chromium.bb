package App::module::version;

use 5.008009;
use strict;
use warnings;

use Getopt::Long 2.13     qw(GetOptionsFromArray);
use Pod::Usage            qw(pod2usage);
use English               qw( -no_match_vars );
use Config;
use File::Spec::Functions qw(splitpath catfile);
use Carp                  qw(carp);

# following recommendation from http://www.dagolden.com/index.php/369/version-numbers-should-be-boring/
our $VERSION = "1.004";
$VERSION = eval $VERSION;

sub new {
	my $class = shift;
	return bless {
		prompt => 0,
		prompted => [],
		list => [],
	}, $class;
}

sub parse_options {
	my $self = shift;
	my @a = @_;

	GetOptionsFromArray(\@a,
		   'help|?'  => sub { pod2usage(-exitstatus => 0, -verbose => 0); }, 
		   'man'     => sub { pod2usage(-exitstatus => 0, -verbose => 2); },
		   'usage'   => sub { _usage(); },
		   'version' => sub { _version(); exit(1); },
		   'prompt'  => \$self->{prompt},
		  ) or pod2usage(-verbose => 2);

	if (0 == scalar @a) {
		$self->{prompt} = 1;
	}
	else {
		@{$self->{list}} = @a;
	}
}


sub do_job {
	my $self = shift;

	if ($self->{prompt}) {
		_version();
		print "\nPlease type in a space-separated list of modules you want to find\nthe installed versions for below.\n> ";
		my $cmd = <STDIN>;
		@{$self->{prompted}} = split m{\s+}, $cmd; 
	}

	print "\n";
	my $version_info;
	MODULE:
	for my $module (@{$self->{list}}, @{$self->{prompted}}) {
		if ('perl' eq lc($module)) {
			print "The version of perl is $PERL_VERSION on $OSNAME ($Config{archname})\n";
			next MODULE;
		}

		if ($module =~ m/\Astrawberry (?:perl)?\z/imsx) {
			if (('MSWin32' ne $OSNAME) or ($Config{libperl} !~ m{\.a\z}msx)) {
				print "This is not Strawberry Perl.\n";
			}
		
			if (($Config{libperl} =~ m{\.a\z}msx) and ($Config{myuname} !~ m/\AWin32 [ ] strawberryperl/msx )) {
				print "This is not a new enough version of Strawberry Perl to easily tell what version it is.\n";
				next MODULE;
			}
			
			my ($strawberryversion, $bits) = (q{}, q{});
			if ($Config{myuname} =~ 
				m{\AWin32 [ ] strawberryperl [ ] # Starting code.
				 (\S+)			   # Version
				 .* [ ]			  # The date Strawberry Perl was built.
				 (\S+)\z			 # The version
				 }msx) {
				($strawberryversion, $bits) = ($1, $2);
				$bits = ('i386' eq $bits) ? 32 : 64;
			}
			print "The version of Strawberry Perl is $strawberryversion ($bits-bit), using gcc $Config{gccversion}\n";
			next MODULE;
		}

		if ('activeperl' eq lc($module)) {
			my $buildnumber = eval { return Win32::BuildNumber() };
			if ($EVAL_ERROR) {
				print "This is not ActivePerl (at least, not on Windows.)\n";
				next MODULE;
			}
			print "The version of ActivePerl is $PERL_VERSION build number $buildnumber\n";
			next MODULE;
		}

	    my $version_info = {};
	    my $module_file = catfile(split(/::/, $module));

	    DIRECTORY: foreach my $dir (@INC) {
		my $filename = catfile($dir, "$module_file.pm");
		if (-e $filename ) {
		    $version_info->{dir} = $dir;
		    if (open IN, "$filename") {
			while (<IN>) {
			    # the following regexp comes from the Extutils::MakeMaker
			    # documentation.
			    if (/([\$*])(([\w\:\']*)\bVERSION)\b.*\=/) {
				local $VERSION;
				my $res = eval $_;
				$version_info->{version} = $VERSION || $res;
				last DIRECTORY;
			    }
			}
		    } else {
			carp "Can't open $filename: $!";
		    }
		}
	    }
		
		if (exists $version_info->{dir}) {
			if (exists $version_info->{version}) {
				print "The version of $module in " . $version_info->{dir} . ' is ' . $version_info->{version} . "\n";
			} else {
				print "$module is installed in " . $version_info->{dir} . ", but does not have a detectable version.\n";
			}
		} else {
			print "$module could not be found.\n";
		}
	}

	print "\n";

	if ($self->{prompt}) {
		require Term::ReadKey;
		my $char = undef;
		print "Press any key to exit.\n";
		$char = Term::ReadKey::ReadKey(-1) until $char;
	}

}

sub _version {

	my (undef, undef, $script) = splitpath( $PROGRAM_NAME );

	print <<"EOF";
This is $script, version $VERSION, which checks the
installed version of the modules named on the command line.

Copyright 2010 Curtis Jewell.

This script may be copied only under the terms of either the Artistic License
or the GNU General Public License, which may be found in the Perl 5 
distribution or the distribution containing this script.
EOF

	return;
}

sub _usage {
	my $error = shift;

	print "Error: $error\n\n" if (defined $error);
	my (undef, undef, $script) = splitpath( $PROGRAM_NAME );

	print <<"EOF";
This is $script, version $VERSION, which checks the
installed version of the modules named on the command line.

Usage: $script [ --help ] [ --usage ] [ --man ] [ --version ] [ -? ]
               [--prompt] Module::To::Check ...

For more assistance, run $script --help.
EOF

	exit(1);	
}

1;

__END__

=head1 NAME

App::module::version - Gets the version info about a module

=head1 DESCRIPTION

This is just a helper module for the main script L<module-version|module-version>.
