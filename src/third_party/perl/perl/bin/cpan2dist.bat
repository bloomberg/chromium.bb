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
#!/usr/bin/perl -w
#line 15
use strict;
use CPANPLUS::Backend;
use CPANPLUS::Dist;
use CPANPLUS::Internals::Constants;
use Data::Dumper;
use Getopt::Long;
use File::Spec;
use File::Temp                  qw|tempfile|;
use File::Basename;
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

local $Data::Dumper::Indent = 1;

use constant PREREQ_SKIP_CLASS  => 'CPANPLUS::To::Dist::PREREQ_SKIP';
use constant ALARM_CLASS        => 'CPANPLUS::To::Dist::ALARM';

### print when you can
$|++;

my $cb      = CPANPLUS::Backend->new
                or die loc("Could not create new CPANPLUS::Backend object");
my $conf    = $cb->configure_object;

my %formats = map { $_ => $_ } CPANPLUS::Dist->dist_types;

my $opts    = {};
GetOptions( $opts,
            'format=s',             'archive',
            'verbose!',             'force!',
            'skiptest!',            'keepsource!',
            'makefile!',            'buildprereq!',
            'help',                 'flushcache',
            'ban=s@',               'banlist=s@',
            'ignore=s@',            'ignorelist=s@',
            'defaults',             'modulelist=s@',
            'logfile=s',            'timeout=s',
            'dist-opts=s%',         'set-config=s%',
            'default-banlist!',     'set-program=s%',
            'default-ignorelist!',  'edit-metafile!',
            'install!'
        );

die usage() if exists $opts->{'help'};

### parse options
my $tarball     = $opts->{'archive'}    || 0;
my $keep        = $opts->{'keepsource'} ? 1 : 0;
my $prereqbuild = exists $opts->{'buildprereq'}
                    ? $opts->{'buildprereq'}
                    : 0;
my $timeout     = exists $opts->{'timeout'}
                    ? $opts->{'timeout'}
                    : 300;

### use default answers?
unless ( $ENV{'PERL_MM_USE_DEFAULT'} ) {
  $ENV{'PERL_MM_USE_DEFAULT'} = $opts->{'defaults'} ? 1 : 0;
}

my $format;
### if provided, we go with the command line option, fall back to conf setting
{   $format      = $opts->{'format'}         || $conf->get_conf('dist_type');
    $conf->set_conf( dist_type  => $format );

    ### is this a valid format??
    die loc("Invalid format: " . ($format || "[NONE]") ) . usage()
        unless $formats{$format};

    ### any options to fix config entries
    {   my $set_conf = $opts->{'set-config'} || {};
        while( my($key,$val) = each %$set_conf ) {
            $conf->set_conf( $key => $val );
        }
    }

    ### any options to fix program entries
    {   my $set_prog = $opts->{'set-program'} || {};
        while( my($key,$val) = each %$set_prog ) {
            $conf->set_program( $key => $val );
        }
    }

    ### any other options passed
    {   my %map = ( verbose     => 'verbose',
                    force       => 'force',
                    skiptest    => 'skiptest',
                    makefile    => 'prefer_makefile'
                );

        ### set config options from arguments
        while (my($key,$val) = each %map) {
            my $bool = exists $opts->{$key}
                            ? $opts->{$key}
                            : $conf->get_conf($val);
            $conf->set_conf( $val => $bool );
        }
    }
}

my @modules = @ARGV;
if( exists $opts->{'modulelist'} ) {
    push @modules, map { parse_file( $_ ) } @{ $opts->{'modulelist'} };
}

die usage() unless @modules;

### set up munge callback if requested
{   if( $opts->{'edit-metafile'} ) {
        my $editor = $conf->get_program('editor');

        if( $editor ) {

            ### register install callback ###
            $cb->_register_callback(
                name    => 'munge_dist_metafile',
                code    => sub {
                                my $self = shift;
                                my $text = shift or return;

                                my($fh,$file) = tempfile( UNLINK => 1 );

                                unless( print $fh $text ) {
                                    warn "Could not print metafile information: $!";
                                    return;
                                }

                                close $fh;

                                system( $editor => $file );

                                my $cont = $cb->_get_file_contents( file => $file );

                                return $cont;
                            },
            );

        } else {
            warn "No editor configured. Can not edit metafiles!\n";
        }
    }
}

my $fh;
LOGFILE: {
    if( my $file = $opts->{logfile} ) {
        open $fh, ">$file" or (
            warn loc("Could not open '%1' for writing: %2", $file,$!),
            last LOGFILE
        );

        warn "Logging to '$file'\n";

        *STDERR = $fh;
        *STDOUT = $fh;
    }
}

### reload indices if so desired
$cb->reload_indices() if $opts->{'flushcache'};

{   my @ban      = exists $opts->{'ban'}
                            ? map { qr/$_/ } @{ $opts->{'ban'} }
                            : ();


    if( exists $opts->{'banlist'} ) {
        push @ban, map { parse_file( $_, 1 ) } @{ $opts->{'banlist'} };
    }

    push @ban,  map  { s/\s+//; $_ }
                map  { [split /\s*#\s*/]->[0] }
                grep { /#/ }
                map  { split /\n/ } _default_ban_list()
        if $opts->{'default-banlist'};

    ### use our prereq install callback
    $conf->set_conf( prereqs => PREREQ_ASK );

    ### register install callback ###
    $cb->_register_callback(
            name    => 'install_prerequisite',
            code    => \&__ask_about_install,
    );


    ### check for ban patterns when handling prereqs
    sub __ask_about_install {

        my $mod     = shift or return;
        my $prereq  = shift or return;


        ### die with an error object, so we can verify that
        ### the die came from this location, and that it's an
        ### 'acceptable' death
        my $pat = ban_me( $prereq );
        die bless sub { loc("Module '%1' requires '%2' to be installed " .
                        "but found in your ban list (%3) -- skipping",
                        $mod->module, $prereq->module, $pat )
                  }, PREREQ_SKIP_CLASS if $pat;
        return 1;
    }

    ### should we skip this module?
    sub ban_me {
        my $mod = shift;

        for my $pat ( @ban ) {
            return $pat if $mod->module =~ /$pat/i;
        }
        return;
    }
}

### patterns to strip from prereq lists
{   my @ignore      = exists $opts->{'ignore'}
                        ? map { qr/$_/ } @{ $opts->{'ignore'} }
                        : ();

    if( exists $opts->{'ignorelist'} ) {
        push @ignore, map { parse_file( $_, 1 ) } @{ $opts->{'ignorelist'} };
    }

    push @ignore, map  { s/\s+//; $_ }
                  map  { [split /\s*#\s*/]->[0] }
                  grep { /#/ }
                  map  { split /\n/ } _default_ignore_list()
        if $opts->{'default-ignorelist'};


    ### register install callback ###
    $cb->_register_callback(
            name    => 'filter_prereqs',
            code    => \&__filter_prereqs,
    );

    sub __filter_prereqs {
        my $cb      = shift;
        my $href    = shift;

        for my $name ( keys %$href ) {
            my $obj = $cb->parse_module( module => $name ) or (
                warn "Cannot make a module object out of ".
                        "'$name' -- skipping\n",
                next );

            if( my $pat = ignore_me( $obj ) ) {
                warn loc("'%1' found in your ignore list (%2) ".
                         "-- filtering it out\n", $name, $pat);

                delete $href->{ $name };
            }
        }

        return $href;
    }

    ### should we skip this module?
    sub ignore_me {
        my $mod = shift;

        for my $pat ( @ignore ) {
            return $pat if $mod->module =~ /$pat/i;
            return $pat if $mod->package_name =~ /$pat/i;
        }
        return;
    }
}


my %done;
for my $name (@modules) {

    my $obj;

    ### is it a tarball? then we get it locally and transform it
    ### and its dependencies into .debs
    if( $tarball ) {
        ### make sure we use an absolute path, so chdirs() dont
        ### mess things up
        $name = File::Spec->rel2abs( $name );

        ### ENOTARBALL?
        unless( -e $name ) {
            warn loc("Archive '$name' does not exist");
            next;
        }

        $obj = CPANPLUS::Module::Fake->new(
                        module  => basename($name),
                        path    => dirname($name),
                        package => basename($name),
                    );

        ### if it's a traditional CPAN package, we can tidy
        ### up the module name some
        $obj->module( $obj->package_name ) if $obj->package_name;

        ### get the version from the package name
        $obj->version( $obj->package_version || 0 );

        ### set the location of the tarball
        $obj->status->fetch($name);

    ### plain old cpan module?
    } else {

        ### find the corresponding module object ###
        $obj = $cb->parse_module( module => $name ) or (
                warn "Cannot make a module object out of ".
                        "'$name' -- skipping\n",
                next );
    }

    ### you banned it?
    if( my $pat = ban_me( $obj ) ) {
        warn loc("'%1' found in your ban list (%2) -- skipping\n",
                    $obj->module, $pat );
        next;
    }

    ### or just ignored it?
    if( my $pat = ignore_me( $obj ) ) {
        warn loc("'%1' found in your ignore list (%2) -- skipping\n",
                    $obj->module, $pat );
        next;
    }


    my $target  = $opts->{'install'} ? 'install' : 'create';
    my $dist    = eval {
                    local $SIG{ALRM} = sub { die bless {}, ALARM_CLASS }
                        if $timeout;

                    alarm $timeout || 0;

                    my $dist_opts = $opts->{'dist-opts'} || {};

                    my $rv = $obj->install(
                            prereq_target   => $target,
                            target          => $target,
                            keep_source     => $keep,
                            prereq_build    => $prereqbuild,

                            ### any passed arbitrary options
                            %$dist_opts,
                    );

                    alarm 0;

                    $rv;
                };

    ### set here again, in case the install dies
    alarm 0;

    ### install failed due to a 'die' in our prereq skipper?
    if( $@ and ref $@ and $@->isa( PREREQ_SKIP_CLASS ) ) {
        warn loc("Dist creation of '%1' skipped: '%2'",
                    $obj->module, $@->() );
        next;

    } elsif ( $@ and ref $@ and $@->isa( ALARM_CLASS ) ) {
        warn loc("\nDist creation of '%1' skipped, build time exceeded: ".
                 "%2 seconds\n", $obj->module, $timeout );
        next;

    ### died for some other reason? just report and skip
    } elsif ( $@ ) {
        warn loc("Dist creation of '%1' failed: '%2'",
                    $obj->module, $@ );
        next;
    }

    ### we didn't get a dist object back?
    unless ($dist and $obj->status->dist) {
        warn loc("Unable to create '%1' dist of '%2'", $format, $obj->module);
        next
    }

    print "Created '$format' distribution for ", $obj->module,
                " to:\n\t", $obj->status->dist->status->dist, "\n";
}


sub parse_file {
    my $file    = shift or return;
    my $qr      = shift() ? 1 : 0;

    my $fh = OPEN_FILE->( $file ) or return;

    my @rv;
    while( <$fh> ) {
        chomp;
        next if /^#/;                   # skip comments
        next unless /\S/;               # skip empty lines
        s/^(\S+).*/$1/;                 # skip extra info
        push @rv, $qr ? qr/$_/ : $_;    # add pattern to the list
    }

    return @rv;
}

=head1 NAME

cpan2dist - The CPANPLUS distribution creator

=head1 DESCRIPTION

This script will create distributions of C<CPAN> modules of the format
you specify, including its prerequisites. These packages can then be
installed using the corresponding package manager for the format.

Note, you can also do this interactively from the default shell,
C<CPANPLUS::Shell::Default>. See the C<CPANPLUS::Dist> documentation,
as well as the documentation of your format of choice for any format
specific documentation.

=head1 USAGE

=cut

sub usage {
    my $me      = basename($0);
    my $formats = join "\n", map { "\t\t$_" } sort keys %formats;

    my $usage = << '=cut';
=pod

 Usage: cpan2dist [--format FMT] [OPTS] Mod::Name [Mod::Name, ...]
        cpan2dist [--format FMT] [OPTS] --modulelist /tmp/mods.list
        cpan2dist [--format FMT] [OPTS] --archive /tmp/dist [/tmp/dist2]

    Will create a distribution of type FMT of the modules
    specified on the command line, and all their prerequisites.

    Can also create a distribution of type FMT from a local
    archive and all of its prerequisites.

=cut

    $usage .= qq[
    Possible formats are:
$formats

    You can install more formats from CPAN!
    \n];

    $usage .= << '=cut';
=pod

Options:

    ### take no argument:
    --help          Show this help message
    --install       Install this package (and any prerequisites you built)
                    after building it.
    --skiptest      Skip tests. Can be negated using --noskiptest
    --force         Force operation. Can be negated using --noforce
    --verbose       Be verbose. Can be negated using --noverbose
    --keepsource    Keep sources after building distribution. Can be
                    negated by --nokeepsource. May not be supported
                    by all formats
    --makefile      Prefer Makefile.PL over Build.PL. Can be negated
                    using --nomakefile. Defaults to your config setting
    --buildprereq   Build packages of any prerequisites, even if they are
                    already uptodate on the local system. Can be negated
                    using --nobuildprereq. Defaults to false.
    --archive       Indicate that all modules listed are actually archives
    --flushcache    Update CPANPLUS' cache before commencing any operation
    --defaults      Instruct ExtUtils::MakeMaker and Module::Build to use
                    default answers during 'perl Makefile.PL' or 'perl
                    Build.PL' calls where possible
    --edit-metafile Edit the distributions metafile(s) before the distribution
                    is built. Requires a configured editor.

    ### take argument:
    --format      Installer format to use (defaults to config setting)
    --ban         Patterns of module names to skip during installation,
                  case-insensitive (affects prerequisites too)
                  May be given multiple times
    --banlist     File containing patterns that could be given to --ban
                  Are appended to the ban list built up by --ban
                  May be given multiple times.
    --ignore      Patterns of modules to exclude from prereq list. Useful
                  for when a prereq listed by a CPAN module is resolved
                  in another way than from its corresponding CPAN package
                  (Match is done on both module name, and package name of
                  the package the module is in, case-insensitive)
    --ignorelist  File containing patterns that may be given to --ignore.
                  Are appended to the ban list built up by --ignore.
                  May be given multiple times.
    --modulelist  File containing a list of modules that should be built.
                  Are appended to the list of command line modules.
                  May be given multiple times.
    --logfile     File to log all output to. By default, all output goes
                  to the console.
    --timeout     The allowed time for buliding a distribution before
                  aborting. This is useful to terminate any build that
                  hang or happen to be interactive despite being told not
                  to be. Defaults to 300 seconds. To turn off, you can
                  set it to 0.
    --set-config  Change any options as specified in your config for this
                  invocation only. See CPANPLUS::Config for a list of
                  supported options.
    --set-program Change any programs as specified in your config for this
                  invocation only. See CPANPLUS::Config for a list of
                  supported programs.
    --dist-opts   Arbitrary options passed along to the chosen installer
                  format's prepare()/create() routine. Please see the
                  documentation of the installer of your choice for
                  options it accepts.

    ### builtin lists
    --default-banlist    Use our builtin banlist. Works just like --ban
                         and --banlist, but with pre-set lists. See the
                         "Builtin Lists" section for details.
    --default-ignorelist Use our builtin ignorelist. Works just like
                         --ignore and --ignorelist but with pre-set lists.
                         See the "Builtin Lists" section for details.

Examples:

    ### build a debian package of DBI and its prerequisites,
    ### don't bother running tests
    cpan2dist --format CPANPLUS::Dist::Deb --buildprereq --skiptest DBI

    ### build a debian package of DBI and its prerequisites and install them
    cpan2dist --format CPANPLUS::Dist::Deb --buildprereq --install DBI

    ### Build a package, whose format is determined by your config, of
    ### the local tarball, reloading cpanplus' indices first and using
    ### the tarballs Makefile.PL if it has one.
    cpan2dist --makefile --flushcache --archive /path/to/Cwd-1.0.tgz

    ### build a package from Net::FTP, but dont build any packages or
    ### dependencies whose name match 'Foo', 'Bar' or any of the
    ### patterns mentioned in /tmp/ban
    cpan2dist --ban Foo --ban Bar --banlist /tmp/ban Net::FTP

    ### build a package from Net::FTP, but ignore its listed dependency
    ### on IO::Socket, as it's shipped per default with the OS we're on
    cpan2dist --ignore IO::Socket Net::FTP

    ### building all modules listed, plus their prerequisites
    cpan2dist --ignorelist /tmp/modules.ignore --banlist /tmp/modules.ban
      --modulelist /tmp/modules.list --buildprereq --flushcache
      --makefile --defaults

    ### pass arbitrary options to the format's prepare()/create() routine
    cpan2dist --dist-opts deb_version=3 --dist-opts prefix=corp

=cut

    $usage .= qq[
Builtin Lists:

    Ignore list:] . _default_ignore_list() . qq[
    Ban list:] .    _default_ban_list();

    ### strip the pod directives
    $usage =~ s/=pod\n//g;

    return $usage;
}

=pod

=head1 Built-In Filter Lists

Some modules you'd rather not package. Some because they
are part of core-perl and you dont want a new package.
Some because they won't build on your system. Some because
your package manager of choice already packages them for you.

There may be a myriad of reasons. You can use the C<--ignore>
and C<--ban> options for this, but we provide some built-in
lists that catch common cases. You can use these built-in lists
if you like, or supply your own if need be.

=head2 Built-In Ignore List

=pod

You can use this list of regexes to ignore modules matching
to be listed as prerequisites of a package. Particularly useful
if they are bundled with core-perl anyway and they have known
issues building.

Toggle it by supplying the C<--default-ignorelist> option.

=cut

sub _default_ignore_list {

    my $list = << '=cut';
=pod

    ^IO$                    # Provided with core anyway
    ^Cwd$                   # Provided with core anyway
    ^File::Spec             # Provided with core anyway
    ^Config$                # Perl's own config, not shipped separately
    ^ExtUtils::MakeMaker$   # Shipped with perl, recent versions
                            # have bug 14721 (see rt.cpan.org)
    ^ExtUtils::Install$     # Part of of EU::MM, same reason

=cut

    return $list;
}

=head2 Built-In Ban list

You can use this list of regexes to disable building of these
modules altogether.

Toggle it by supplying the C<--default-banlist> option.

=cut

sub _default_ban_list {

    my $list = << '=cut';
=pod

    ^GD$                # Needs c libaries
    ^Berk.*DB           # DB packages require specific options & linking
    ^DBD::              # DBD drivers require database files/headers
    ^XML::              # XML modules usually require expat libraries
    Apache              # These usually require apache libraries
    SSL                 # These usually require SSL certificates & libs
    Image::Magick       # Needs ImageMagick C libraries
    Mail::ClamAV        # Needs ClamAV C Libraries
    ^Verilog            # Needs Verilog C Libraries
    ^Authen::PAM$       # Needs PAM C libraries & Headers

=cut

    return $list;
}

__END__

=head1 SEE ALSO

L<CPANPLUS::Dist>, L<CPANPLUS::Module>, L<CPANPLUS::Shell::Default>,
C<cpanp>

=head1 BUG REPORTS

Please report bugs or other issues to E<lt>bug-cpanplus@rt.cpan.org<gt>.

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

The CPAN++ interface (of which this module is a part of) is copyright (c)
2001 - 2007, Jos Boumans E<lt>kane@cpan.orgE<gt>. All rights reserved.

This library is free software; you may redistribute and/or modify it
under the same terms as Perl itself.

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

__END__
:endofperl
