#!perl

use Getopt::Long;
use File::Basename;
use Config;
use strict;

use PPM;

$PPM::VERSION = '11.11_03';

my %help;

# mapping of POD sections to command topics
my %topic = (
    'Error Recovery' => 'genconfig',
    'Installing'     => 'install',
    'Querying'       => 'query',
    'Removing'       => 'remove',
    'Searching'      => 'search',
    'Summarizing'    => 'summary',
    'Verifying'      => 'verify',
    'Synopsis'       => 'usage',
    'Options'        => 'set',
);

$help{'help'} = <<'EOT';
Commands:
    exit              - leave the program.
    help [command]    - prints this screen, or help on 'command'.
    install PACKAGES  - installs specified PACKAGES.
    quit              - leave the program.
    query [options]   - query information about installed packages.
    remove PACKAGES   - removes the specified PACKAGES from the system.
    search [options]  - search information about available packages.
    set [options]     - set/display current options.
    verify [options]  - verifies current install is up to date.
    version           - displays PPM version number

EOT

# Build the rest of the online help from the POD
$/ = "\n=";
while (<DATA>) {
    next unless my ($topic,$text) = /^(?:item|head[12]) ([^\n]+)\n\n(.*)=/s;
    next unless $topic{$topic};
    ($help{$topic{$topic}} = "\n$text"); # =~ s/\n *([^\n])/\n    $1/sg;
}
$/ = "\n";

# Need to do this here, because the user's config file is probably
# hosed.
if ($#ARGV == 0 && $ARGV[0] eq 'genconfig') {
    &genconfig;
    exit 0;
}

if ($#ARGV == 0 && $ARGV[0] eq 'getconfig') {
    print $PPM::PPMdat;
    exit 0;
}

my %options = PPM::GetPPMOptions();
my $location;

my $moremsg = "[Press return to continue or 'q' to quit] ";
my $interactive = 0;

my %repositories = PPM::ListOfRepositories();

my $prefix_pattern = $^O eq "MSWin32" ? '(--|-|\+|/)' : '(--|-|\+)';

$PPM::PPMShell = 1;

Getopt::Long::Configure("prefix_pattern=$prefix_pattern");

if ($#ARGV == -1 || ($#ARGV == 0 && $ARGV[0] =~ /^${prefix_pattern}location/)) {
    my $prompt = 'PPM> ';
    $interactive = 1;
    GetOptions("location=s" => \$location);

    print "PPM interactive shell ($PPM::VERSION) - type 'help' for available commands.\n";
    $| = 1;
    while () {
        print $prompt;
        last unless defined ($_ = <> );
        chomp;
        s/^\s+//;
        @ARGV = split(/\s+/, $_);
        next unless @ARGV;
        # exit/quit
        if (command($ARGV[0], "qu|it") or command($ARGV[0], "|exit")) {
            print "Quit!\n";
            last;
        }
        exec_command();
    }
    exit 0;
}

exit exec_command();

sub exec_command
{
    my $cmd = lc shift @ARGV;

    for (@ARGV) {
        s/::/-/g;
    }

    # help
    if (command($cmd, "|help")) {
        help(@ARGV);
    }
    # query
    elsif (command($cmd, "qu|ery")) {
        GetOptions("case!" => \my $case, "abstract" => \my $abstract, 
        "author" => \my $author );

        my %summary = InstalledPackageProperties();
        if (@ARGV) {
            my $searchtag;
            if ($abstract || $author) {
                $searchtag = ($abstract ? 'ABSTRACT' : 'AUTHOR');
            }
            my $RE = shift @ARGV;
            eval { $RE =~ /$RE/ };
            if ($@) {
                print "'$RE': invalid regular expression.\n";
                return 1;
            } 
            $case = !$options{'IGNORECASE'} unless defined $case;
            $RE = "(?i)$RE" if ($case == 0);
            foreach(keys %summary) {
                if ($searchtag) {
                    delete $summary{$_} unless $summary{$_}{$searchtag} =~ /$RE/;
                }
                else {
                    delete $summary{$_} unless /$RE/;
                }
            }
        }
        print_formatted(1, %summary);
    }
    # install
    elsif (command($cmd, "in|stall")) {
        my $location = $location;
        GetOptions("location=s" => \$location);
        unless (@ARGV) {
            if (!$interactive && -d "blib" && -f "Makefile") {
                return if InstallPackage(location => $location);
                print "Error installing blib: $PPM::PPMERR\n";
                return 1;
            }
            print "Package not specified.\n";
            return 1;
        }

        my %installed = InstalledPackageProperties();
        foreach my $package (@ARGV) {
            $package =~ s/::/-/g;
            if (my $pkg = (grep {/^$package$/i} keys %installed)[0]) {
                my $version = $installed{$pkg}{'VERSION'};
                $version =~ s/(,0)*$//;
                $version =~ tr/,/./;
                print "Version $version of '$pkg' is already installed.\n" .
                      "Remove it, or use 'verify --upgrade $pkg'.\n";
                next;
            }
            elsif ($interactive && $options{'CONFIRM'}) {
                print "Install package '$package?' (y/N): ";
                next unless <> =~ /^[yY]/;
            }
            print "Installing package '$package'...\n";
            if(!InstallPackage("package" => $package, "location" => $location)) {
                print "Error installing package '$package': $PPM::PPMERR\n";
            }
        }
    }
    # remove
    elsif (command($cmd, "|remove")) {
        unless (@ARGV) {
            print "Package not specified.\n";
            return 1;
        }
        foreach my $package (@ARGV) {
            $package =~ s/::/-/g;
            if ($interactive && $options{'CONFIRM'}) {
                print "Remove package '$package?' (y/N): ";
                next unless <> =~ /[yY]/;
            }
            unless (RemovePackage("package" => $package)) {
                print "Error removing $package: $PPM::PPMERR\n";
            }
        }
    }
    # search
    elsif (command($cmd, "se|arch")) {
        my (%summary, $searchtag);
        my $location = $location;
        GetOptions("case!" => \my $case, "location=s" => \$location, 
            "abstract" => \my $abstract, "author" => \my $author );
        my $searchRE = shift @ARGV;
        if (defined $searchRE) {
            eval { $searchRE =~ /$searchRE/ };
            if ($@) {
                print "'$searchRE': invalid regular expression.\n";
                return 1;
            }
        }
        $case = !$options{'IGNORECASE'} unless defined $case;
        if ($abstract || $author) {
            $searchtag = ($abstract ? 'ABSTRACT' : 'AUTHOR');
        }
        %summary = search_PPDs("location" => $location, "ignorecase" => !$case,
            "searchtag" => $searchtag, "searchRE" => $searchRE);
        foreach (keys %summary) {
            print "Packages available from $_:\n";
            print_formatted(2, %{$summary{$_}});
        }
    }
    # set
    elsif (command($cmd, "se|t")) {
        unless (set(@ARGV) || $interactive) {
            PPM::SetPPMOptions("options" => \%options, "save" => 1);
        }
    }
    # verify
    elsif (command($cmd, "ver|ify")) {
        my $location = $location;
        GetOptions("force" => \my $force, "location=s" => \$location, 
            "upgrade" => \my $upgrade);
        if ($interactive && $upgrade && $options{'CONFIRM'}) {
            printf "Upgrade package%s? (y/N): ", @ARGV == 1 ? " '$ARGV[0]'" : "s";
            return unless <> =~ /^[yY]/;
        }
        verify_packages("packages" => \@ARGV, "location" => $location, 
            "upgrade" => $upgrade, "force" => $force);
    }
    elsif (command($cmd, "ver|sion")) {
        print "$PPM::VERSION\n";
    }
    elsif ($cmd eq "purge") {
        my %summary = InstalledPackageProperties();
        foreach(keys %summary) {
            print "Purging $_\n";
            RemovePackage("package" => $_, "force" => 1);
        }
    }
    elsif ($cmd eq 'refresh') {
        my %summary = InstalledPackageProperties();
        foreach(keys %summary) {
            print "Refreshing $_\n";
            InstallPackage("package" => $_);
        }
    }
    else {
        print "Unknown or ambiguous command '$cmd'; type 'help' for commands.\n";
    }
}

sub help {
    my $topic = @_ && $help{lc $_[0]} ? lc $_[0] : 'help';
    my $help = $help{$topic};
    $help =~ s/^(\s*)ppm\s+/$1/mg if $interactive;
    print $help;
}

sub more
{
    my ($lines) = shift @_;
    if (++$$lines >= $options{'MORE'}) {
        print $moremsg;
        $_ = <>;
        $$lines = $_ eq "q\n" ? -1 : 1;
    }
}

# This nasty piece of business splits $pattern into a required prefix 
# and a "match any of this substring" suffix.  E.g. "in|stall" will
# match a $cmd of "ins", "inst", ..., "install".
sub command
{
    my ($cmd, $pattern) = @_;
    my @pattern = split(/\|/, $pattern);
    if ($pattern[1]) {
        my @optchars = split(//, $pattern[1]);
        # build up a "foo(b|ba|bar)" string
        $pattern = "$pattern[0](";
        $pattern[1] = shift @optchars;
        $pattern[1] .= "|$pattern[1]$_" foreach @optchars;
        $pattern .= "$pattern[1])";
    }
    return ($cmd =~ /^${pattern}$/i);
}

# This routine prints the output for query and search
# in a nicely formatted way, if $options{'VERBOSE'} is set.
sub print_formatted
{
    my ($lines, %summary) = @_;
    my $package;

    unless ($options{'VERBOSE'}) {
        foreach $package (sort keys %summary) {
            print "$package\n";
            &more(\$lines) if $options{'MORE'} && $interactive;
            last if $lines == -1;
        }
        return;
    }

    my ($maxname, $maxversion) = (0, 0);
    # find the longest package name and version strings, so we can
    # format them nicely
    $maxname < length($_) and $maxname = length($_) for keys %summary;
    foreach $package (keys %summary) {
        $summary{$package}{'VERSION'} =~ s/(,0)*$//;
        $summary{$package}{'VERSION'} =~ tr/,/./;
        $maxversion = length $summary{$package}{'VERSION'} > $maxversion ? 
            length $summary{$package}{'VERSION'} : $maxversion;
    }
    my $columns = $ENV{COLUMNS} ? $ENV{COLUMNS} : 80;
    my $namefield = "@" . "<" x ($maxname - 1);
    my $versionfield = "@" . "<" x ($maxversion - 1);
    my $abstractfield = "^" . "<" x ($columns - (6 + $maxname + $maxversion));
    my $abstractpad = " " x ($maxname + $maxversion + 3);

    foreach $package (sort keys %summary) {
        eval "format STDOUT = \n"
                   . "$namefield [$versionfield] $abstractfield\n"
                   . '$package, $summary{$package}{VERSION}, $summary{$package}{ABSTRACT}'
                   . "\n"
                   . "$abstractpad $abstractfield~~\n"
                   . '$summary{$package}{ABSTRACT}' 
                   . "\n"
                   . ".\n";

        my $diff = $-;
        write;
        $diff -= $-;
        $lines += ($diff - 1) if $diff > 1;
        &more(\$lines) if $options{'MORE'} && $interactive;
        last if $lines == -1;
    }
}

sub set
{
    my $option = lc shift @_; 

    unless ($option) {
        print "Commands will " . ($options{'CONFIRM'} ? "" : "not ") . 
            "be confirmed.\n";
        print "Temporary files will " . ($options{'CLEAN'} ? "" : "not ") .
            "be deleted.\n";
        print "Download status will " . (($options{'DOWNLOADSTATUS'} > 0) ?
            "be updated every $options{'DOWNLOADSTATUS'} bytes.\n" : 
            "not be updated.\n");
        print "Case-" . ($options{'IGNORECASE'} ? "in" : "") . 
            "sensitive searches will be performed.\n";
        print "Package installations will " . 
            ($options{'FORCE_INSTALL'} ? "" : "not ") .
               "continue if a dependency cannot be installed.\n";
        print "Tracing info will " . (($options{'TRACE'} > 0 ) ? 
            "be written to '$options{'TRACEFILE'}'.\n" : "not be written.\n");
        print "Screens will " . ($options{'MORE'} > 0 ? 
            "pause after $options{'MORE'} lines.\n" : "not pause.\n");
        print "Query/search results will " . 
            ($options{'VERBOSE'} ? "" : "not ") . "be verbose.\n";
        if (defined $location) { print "Current PPD repository: $location\n"; }
        else {
            print "Current PPD repository paths:\n";
            foreach $_ (keys %repositories) {
                print "\t$_: $repositories{$_}\n";
            }
        }
        print "Packages will be installed under: $options{'ROOT'}\n" 
            if ($options{'ROOT'});
        print "Packages will be built under: $options{'BUILDDIR'}\n" 
            if ($options{'BUILDDIR'});
        return;
    }

    my $value = shift @_;
    if (command($option, "r|epository")) {
        if ($value =~ /${prefix_pattern}remove/i) {
            $value = join(" ", @_);
            print "Location not specified.\n" and return 1 
                unless (defined $value);
            PPM::RemoveRepository("repository" => $value);
            %repositories = PPM::ListOfRepositories();
        }
        else {
            my $location = shift @_;
            print "Repository not specified.\n" and return 1
                unless (defined $value and defined $location);
            PPM::AddRepository("repository" => $value,
                "location" => $location);
            %repositories = PPM::ListOfRepositories();
        }
    }
    else {
        if (command($option, "c|onfirm")) {
            $options{'CONFIRM'} = defined $value ? 
                ($value != 0) : ($options{'CONFIRM'} ? 0 : 1);
            print "Commands will " . ($options{'CONFIRM'} ? "" : "not ") . 
                "be confirmed.\n";
        }
        elsif (command($option, "|save")) {
            PPM::SetPPMOptions("options" => \%options, "save" => 1);
            return 0;
        }
        elsif (command($option, "c|ase")) {
            $options{'IGNORECASE'} = defined $value ? 
                ($value == 0) : ($options{'IGNORECASE'} ? 0 : 1);
            print "Case-" . ($options{'IGNORECASE'} ? "in" : "") . 
                "sensitive searches will be performed.\n";
        }
        elsif (command($option, "r|oot")) {
            my $old_root;
            print "Directory not specified.\n" and return 1 unless ($value);
            print "$PPM::PPMERR" and return 1
                    unless ($old_root = PPM::chroot("location" => $value));
            $options{'ROOT'} = $value;
            print "Root is now $value [was $old_root].\n";
        }
        elsif (command($option, "|build")) {
            print "Directory not specified.\n" and return 1 unless ($value);
            print "Directory '$value' does not exist.\n" and return 1 
                unless (-d $value);
            $options{'BUILDDIR'} = $value;
            print "Build directory is now $value.\n";
        }
        elsif (command($option, "|force_install")) {
            $options{'FORCE_INSTALL'} = defined $value ? ($value != 0) : 
                ($options{'FORCE_INSTALL'} ? 0 : 1);
            print "Package installations will " .
                  ($options{'FORCE_INSTALL'} ? "" : "not ") .
                  "continue if a dependency cannot be installed.\n";
        }
        elsif (command($option, "c|lean")) {
            $options{'CLEAN'} = defined $value ? 
                ($value != 0) : ($options{'CLEAN'} ? 0 : 1);
            print "Temporary files will " . ($options{'CLEAN'} ? "" : "not ") . 
                "be deleted.\n";
        }
        elsif (command($option, "|downloadstatus")) {
            print "Numeric value must be given.\n" and return 1
                unless (defined $value && $value =~ /^\d+$/);
            $options{'DOWNLOADSTATUS'} = $value;
            print "Download status will " . (($options{'DOWNLOADSTATUS'} > 0) ?
                "be updated every $options{'DOWNLOADSTATUS'} bytes.\n" : 
                "not be updated.\n");
        }
        elsif (command($option, "|more")) {
            print "Numeric value must be given.\n" and return 1
                unless (defined $value && $value =~ /^\d+$/);
            $options{'MORE'} = $value;
            print "Screens will " . ($options{'MORE'} > 0 ? 
                "pause after $options{'MORE'} lines.\n" : "not pause.\n");
        }
        elsif (command($option, "trace|file")) {
            print "Filename not specified.\n" and return 1 unless ($value);
            $options{'TRACEFILE'} = $value;
            print "Tracing info will be written to $options{'TRACEFILE'}.\n";
        }
        elsif (command($option, "trace")) {
            print "Numeric value between 0 and 4 must be given.\n" and return 1
                unless (defined $value && 
                    $value =~ /^\d+$/ && $value >= 0 && $value <= 4);
            $options{'TRACE'} = $value;
            print "Tracing info will " . ($options{'TRACE'} > 0 ? 
                "be written to $options{'TRACEFILE'}.\n" : "not be written.\n");
        }
        elsif (command($option, "|verbose")) {
            $options{'VERBOSE'} = defined $value ? 
                ($value != 0) : ($options{'VERBOSE'} ? 0 : 1);
            print "Query/search results will " . 
                ($options{'VERBOSE'} ? "" : "not ") . "be verbose.\n";
        }
        else {
            print "Unknown or ambiguous option '$option'; see 'help set' for available options.\n";
            return 1;
        }
        PPM::SetPPMOptions("options" => \%options);
    }
    return;
}

sub search_PPDs
{
    my %argv = @_;
    my @locations = $argv{'location'} || $location;
    my $searchtag = $argv{'searchtag'};
    my $ignorecase = defined $argv{'ignorecase'} ? 
        $argv{'ignorecase'} : $options{'IGNORECASE'};
    my $searchRE = $argv{'searchRE'};
    if (defined $searchRE) {
        eval { $searchRE =~ /$searchRE/ };
        if ($@) {
            print "'$searchRE': invalid regular expression.\n";
            return;
        }
        $searchRE = "(?i)$searchRE" if $ignorecase;
    }

    my %packages;
    unless (defined $locations[0]) {
        my %reps = PPM::ListOfRepositories;
        @locations = values %reps;
    }
    foreach my $loc (@locations) {
        my %summary;

        # see if the repository has server-side searching
        if (defined $searchRE && (%summary = ServerSearch('location' => $loc, 
                'searchRE' => $searchRE, 'searchtag' => $searchtag))) {
            # XXX: clean this up
            foreach my $package (keys %{$summary{$loc}}) {
                $packages{$loc}{$package} = \%{$summary{$loc}{$package}};
            }
            next;
        }

        # see if a summary file is available
        %summary = RepositorySummary("location" => $loc);
        if (%summary) {
            foreach my $package (keys %{$summary{$loc}}) {
                next if (defined $searchtag && 
                    $summary{$loc}{$package}{$searchtag} !~ /$searchRE/);
                next if (!defined $searchtag && 
                    defined $searchRE && $package !~ /$searchRE/);
                $packages{$loc}{$package} = \%{$summary{$loc}{$package}};
            }
        }
        else {
            my %ppds = PPM::RepositoryPackages("location" => $loc);
            # No summary: oh my, nothing but 'Net
            foreach my $package (@{$ppds{$loc}}) {
                my %package_details = RepositoryPackageProperties(
                    "package" => $package, "location" => $loc);
                next unless %package_details;
                next if (defined $searchtag && 
                    $package_details{$searchtag} !~ /$searchRE/);
                next if (!defined $searchtag && 
                    defined $searchRE && $package !~ /$searchRE/);
                $packages{$loc}{$package} = \%package_details;
            }
        }
    }
    return %packages;
}

sub verify_packages
{
    my %argv = @_;
    my @packages = @{$argv{'packages'}};
    my $upgrade = $argv{'upgrade'};
    my $force = $argv{'force'};
    my $location = $argv{'location'} || $location;
    unless ($packages[0]) {
        my %info = QueryInstalledPackages();
        @packages = sort keys %info;
    }

    my $package = shift @packages;
    while ($package) {
        my $status = VerifyPackage("package" => $package, 
            "location" => $location, "upgrade" => $upgrade, "force" => $force);
        if (defined $status) {
            if ($status eq "0") {
                print "Package \'$package\' is up to date.\n";
            }
            elsif ($upgrade) {
                print "Package $package upgraded to version $status\n";
            }
            else {
                print "An upgrade to package \'$package\' is available.\n";
            }
        }
        else {
            # Couldn't find a PPD to compare it with.
            print "Package \'$package\' is up to date.\n";
        }
        $package = shift @packages;
    }
}

sub genconfig
{
  my $repositories =
    {'5.6' => {
	       'ActiveState' =>
	       {location => 'http://ppm.ActiveState.com/cgibin/PPM/ppmserver.pl?urn:/PPMServer',
		summaryfile => 'fetch_summary',
	       },
	       'Bribes' =>
	       {location => 'http://www.bribes.org/perl/ppm',
		summaryfile => 'searchsummary.ppm',
	       },
	       'UWinnipeg' =>
	       {location => 'http://theoryx5.uwinnipeg.ca/cgi-bin/ppmserver?urn:/PPMServer',
		summaryfile => 'fetch_summary',
	       },
	      },
     '5.8' => {
	       'ActiveState' =>
	       {location => 'http://ppm.activestate.com/PPMPackages/5.8-windows',
		summaryfile => 'searchsummary.ppm',
	       },
	       'Bribes' =>
	       {location => 'http://www.bribes.org/perl/ppm',
		summaryfile => 'searchsummary.ppm',
	       },
	       'UWinnipeg' =>
	       {location => 'http://theoryx5.uwinnipeg.ca/ppms',
		summaryfile => 'searchsummary.ppm',
	       },
	       'Trouchelle' =>
	       {location => 'http://trouchelle.com/ppm',
		summaryfile => 'searchsummary.ppm',
	       },
	      },
     '5.10' => {
		'ActiveState' =>
		{location => 'http://ppm.activestate.com/PPMPackages/5.10-windows',
		 summaryfile => 'searchsummary.ppm',
		},
		'Bribes' =>
		{location => 'http://www.bribes.org/perl/ppm',
		 summaryfile => 'searchsummary.ppm',
		},
		'UWinnipeg' =>
		{location => 'http://cpan.uwinnipeg.ca/PPMPackages/10xx',
		 summaryfile => 'searchsummary.ppm',
		},
		'Trouchelle' =>
		{location => 'http://trouchelle.com/ppm10',
		 summaryfile => 'package.xml',
		},
	       },
    };
   my ($perl_version);
 PERLV: {
     ($] < 5.008) and do {
       $perl_version = '5.6';
       last PERLV;
     };
     ($] < 5.01) and do {
       $perl_version = '5.8';
       last PERLV;
     };
     $perl_version = '5.10';
   }
   my $reps = $repositories->{$perl_version};
  my $PerlDir = $Config{'prefix'};

print <<"EOF";
<PPMCONFIG>
    <PPMVER>2,1,8,0</PPMVER>
    <PLATFORM CPU="x86" OSVALUE="$Config{'osname'}" OSVERSION="0,0,0,0" />
    <OPTIONS BUILDDIR="$ENV{'TEMP'}" CLEAN="1" CONFIRM="1" DOWNLOADSTATUS="16384" FORCEINSTALL="1" IGNORECASE="1" MORE="0" ROOT="$PerlDir" TRACE="0" TRACEFILE="PPM.LOG" VERBOSE="1" />
EOF
  foreach my $name(sort keys %$reps) {
    my $loc = $reps->{$name}->{location};
    my $sf = $reps->{$name}->{summaryfile};
    print << "EOF";
    <REPOSITORY LOCATION="$loc" NAME="$name" SUMMARYFILE="$sf" />
EOF
  }
  print <<"EOF";
    <PPMPRECIOUS>Compress-Zlib;Archive-Tar;Digest-MD5;File-CounterFile;Font-AFM;HTML-Parser;HTML-Tree;MIME-Base64;URI;XML-Element;libwww-perl;XML-Parser;SOAP-Lite;PPM;libnet;libwin32</PPMPRECIOUS>
</PPMCONFIG>
EOF
}

__DATA__

=head1 NAME

PPM - Perl Package Manager: locate, install, upgrade software packages.

=head1 SYNOPSIS

 ppm genconfig
 ppm help [command]
 ppm install [--location=location] package1 [... packageN]
 ppm query [--case|nocase] [--abstract|author] PATTERN
 ppm remove package1 [... packageN]
 ppm search [--case|nocase] [--location=location] [--abstract|author] PATTERN
 ppm set [option]
 ppm verify [--location=location] [--upgrade] [--force] [package1 ... packageN]
 ppm version
 ppm [--location=location]

=head1 DESCRIPTION

ppm is a utility intended to simplify the tasks of locating, installing,
upgrading and removing software packages.  It is a front-end to the
functionality provided in PPM.pm.  It can determine if the most recent
version of a software package is installed on a system, and can install
or upgrade that package from a local or remote host.

ppm runs in one of two modes: an interactive shell from which commands
may be entered; and command-line mode, in which one specific action is
performed per invocation of the program.

ppm uses files containing an extended form of the Open Software
Description (OSD) specification for information about software packages.
These description files, which are written in Extensible Markup
Language (XML) code, are referred to as 'PPD' files.  Information about
OSD can be found at the W3C web site (at the time of this writing,
http://www.w3.org/TR/NOTE-OSD.html).  The extensions to OSD used by ppm
are documented in PPM.ppd.

=head1 Using PPM

=over 4

=item Interactive mode

If ppm is invoked with no command specified, it is started in interactive
mode.  If the '--location' argument is specified, it is used as the
search location, otherwise the repositories specified in the PPM data file
are used. 

The syntax of PPM commands is the same in interactive mode as it is in
command-line mode.  The 'help' command lists the available commands.

ppm commands may be abbreviated to their shortest unique form.

=item Installing

 ppm install [--location=location] package1 [... packageN]

Installs the specified software packages. Attempts to install from the 
URL or directory 'location' if the '--location' option is specfied. 

The 'package' arguments may be either package names ('foo'), pathnames 
(p:/packages/foo.ppd) or URLs (http://www.ActiveState.com/packages/foo.ppd)
to specific PPD files.

In the case where a package name is specified, and the '--location'
option is not used, ppm will refer to the default repository locations.

See also: 'confirm' option.

=item Removing

 ppm remove package1 [... packageN]

Reads information from the PPD file for the named software package and
removes the package from the system.

See also: 'confirm' option.

=item Verifying

 ppm verify [--location=location] [--upgrade] [--force] [package1 ... packageN]

Verifies that the currently installed packages are up to date.  If no
packages are specified as arguments, all installed packages will be verified.

If the '--upgrade' option is specified, any package for which an upgrade 
is available will be upgraded.  

If the '--location' option is specified, upgrades will be looked for at 
the specified URL or directory.

If the '--force' option is specified, all currently installed packages will 
be reinstalled regardless of whether they are out of date or not.

See also: 'confirm' option.

=item Querying

 ppm query [--case|nocase] [--abstract|author] PATTERN

Searches for 'PATTERN' (a regular expression) in the name of any installed 
package.  If a search is successful, information about the matching 
package(s) is displayed.  If 'PATTERN' is omitted, information about
all installed packages will be displayed.

If either '--abstract' or '--author' is specified, PATTERN will be 
searched for in the <ABSTRACT> or <AUTHOR> tags of the installed packages.

The '--case' and '--nocase' options can be used to override the default
case-sensitivity search settings.

See also: 'case' option.

=item Searching

 ppm search [--case|nocase] [--location=location] [--abstract|author] PATTERN

Displays a list of any packages matching 'PATTERN' (a regular expression)
available from the specified location.  If 'PATTERN' is omitted, information 
about all available packages will be displayed.

If the '--location' option is specified, the specified URL or directory
will be searched.  If '--location' is not specified, the repository location 
as specified in the PPM data file will be searched.

If either '--abstract' or '--author' is specified, PATTERN will be 
searched for in the <ABSTRACT> or <AUTHOR> tags of the available packages.

The '--case' and '--nocase' options can be used to override the default
case-sensitivity search settings.

See also: 'case' option.

=item Error Recovery

 ppm genconfig
 ppm getconfig

The genconfig command will print a valid PPM config file (ppm.xml) to STDOUT.
This can be useful if the PPM config file ever gets damaged leaving PPM
unusable.

If required, this command should be run from a shell prompt:

    C:\Perl\site\lib> ppm genconfig > ppm.xml

The getconfig command prints the location of the PPM configuration file
used at PPM startup.

=item Options

 ppm set [option value]

Sets or displays current options.  With no arguments, current option
settings are displayed.

Available options:

    build DIRECTORY
        - Changes the package build directory.

    case [1|0]
        - Sets case-sensitive searches.  If one of '1' or '0' is
          not specified, the current setting is toggled.

    clean [1|0]
        - Sets removal of temporary files from package's build 
          area, on successful installation of a package.  If one of
          '1' or '0' is not specified, the current setting is
          toggled.

    confirm [1|0]
        - Sets confirmation of 'install', 'remove' and 'upgrade'.
          If one of '1' or '0' is not specified, the current
          setting is toggled.

    downloadstatus NUMBER
        - If non-zero, updates the download status after each NUMBER 
          of bytes transferred during an 'install'.  This can be
          reassuring when installing a large package (e.g. Tk) over
          a low-speed connection.

    force_install [1|0]
        - Continue installing a package even if a dependency cannot
          be installed.

    more NUMBER
        - Causes output to pause after NUMBER lines have been
          displayed.  Specifying '0' turns off this capability.

    set repository --remove NAME
        - Removes the repository 'NAME' from the list of repositories.

    set repository NAME LOCATION
        - Adds a repository to the list of PPD repositories for this
          session.  'NAME' is the name by which this repository will
          be referred; 'LOCATION' is a URL or directory name.

    root DIRECTORY
        - Changes the install root directory.  Packages will be
          installed under this new root.

    save
        - Saves the current options as default options for future
          sessions.

    trace
        - Tracing level--default is 1, maximum is 4, 0 indicates
          no tracing.

    tracefile
        - File to contain tracing information, default is 'PPM.LOG'.

    verbose [1|0]
        - Display additional package information for 'query' and
          'search' commands.

=back

=head1 EXAMPLES

=over 4

=item ppm

Starts ppm in interactive mode, using the repository locations specified
in the PPM data file.  A session might look like this:

    [show all available packages]
    PPM> search
    Packages available from P:\PACKAGES:
    bar [2.91 ] supplies bar methods for perl5.
    bax [0.072] module for manipulation of bax archives.
    baz [1.03 ] Interface to baz library
    foo [2.23 ] Foo parser class
    
    [list what has already been installed]
    PPM> query
    bax [0.071] module for manipulation of bax archives.
    baz [1.02 ] Interface to baz library
    
    [install a package]
    PPM> install foo
    Install package foo? (y/N): y
    [...]
    
    [toggle confirmations]
    PPM> set confirm
    Commands will not be confirmed.
    
    [see if 'baz' is up-to-date]
    PPM> verify baz
    An upgrade to package 'baz' is available.
    
    [upgrade 'baz']
    PPM> verify --upgrade baz
    [...]
    
    [forced upgrade of 'baz']
    PPM> verify --upgrade --force baz
    [...]
    
    [toggle case-sensitive searches]
    PPM> set case
    Case-sensitive searches will be performed.
    
    [display all available packages beginning with 'b']
    PPM> search ^b
    bar [2.91 ] supplies bar methods for perl5.
    bax [0.072] module for manipulation of bax archives.
    baz [1.03 ] Interface to baz library
    
    [search for installed packages containing 'baz' in the ABSTRACT tag]
    PPM> query --abstract baz
    Matching packages found at P:\PACKAGES:
    baz [1.03 ] Interface to baz library
    PPM> quit

=item ppm install http://www.ActiveState.com/packages/foo.ppd

Installs the software package 'foo' based on the information in the PPD
obtained from the specified URL.

=item ppm verify --upgrade foo

Compares the currently installed version of the software package 'foo'
to the one available according to the PPD obtained from the location
specified for this package in the PPM data file, and upgrades
to a newer version if available.

=item ppm verify --location=P:\PACKAGES --upgrade foo

Compares the currently installed version of the software package 'foo'
to the one available according to the PPD obtained from the specified
directory, and upgrades to a newer version if available.

=item ppm verify --upgrade --force

Forces verification and reinstalls every installed package on the system, 
using upgrade locations specified in the PPM data file.

=item ppm search --location=http://ppm.ActiveState.com/PPMpackages/5.6

Displays the packages with PPD files available at the specified location.

=item ppm search --location=P:\PACKAGES --author ActiveState

Searches the specified location for any package with an <AUTHOR> tag
containing the string 'ActiveState'.  On a successful search, the package
name and the matching string are displayed.

=back

=head1 ENVIRONMENT VARIABLES

=over 4

=item HTTP_proxy

If the environment variable 'HTTP_proxy' is set, then it will
be used as the address of a proxy server for accessing the Internet.

The value should be of the form: 'http://proxy:port'.

=back

=head1 FILES

The following files are fully described in the 'Files' section of PPM:ppm.

=over 4

=item package.ppd

A description of a software package, in extended Open Software Description
(OSD) format.  More information on this file format can be found in
PPM::ppd.

=item ppm.xml - PPM data file.

An XML format file containing information about the local system,
specifics regarding the locations from which PPM obtains PPD files, and
the installation details for any package installed by ppm.

This file usually resides in '[perl]/site/lib'.  If the environment 
variable 'PPM_DAT' is set, its value will be used as the full pathname
to a PPM data file.  If all else fails, ppm will look for a data file
in the current directory.

=back

=head1 AUTHOR

Murray Nesbitt, E<lt>F<murray@cpan.org>E<gt>

=head1 CREDITS

=over 4

=item *

The "geek-pit" at ActiveState.

=item *

Paul Kulchenko for his SOAP-Lite package, and for his enthusiastic
assistance in getting PPM to work with SOAP-Lite.

=back

=cut
