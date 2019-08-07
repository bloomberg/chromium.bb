package CPANPLUS::Dist::Build;

use strict;
use warnings;
use vars    qw[@ISA $STATUS $VERSION];
@ISA =      qw[CPANPLUS::Dist];

use CPANPLUS::Internals::Constants;

### these constants were exported by CPANPLUS::Internals::Constants
### in previous versions.. they do the same though. If we want to have
### a normal 'use' here, up the dependency to CPANPLUS 0.056 or higher
BEGIN {
    require CPANPLUS::Dist::Build::Constants;
    CPANPLUS::Dist::Build::Constants->import()
        if not __PACKAGE__->can('BUILD') && __PACKAGE__->can('BUILD_DIR');
}

use CPANPLUS::Error;

use Config;
use FileHandle;
use Cwd;
use version;

use IPC::Cmd                    qw[run];
use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load check_install];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

local $Params::Check::VERBOSE = 1;

$VERSION = '0.62';

=pod

=head1 NAME

CPANPLUS::Dist::Build - CPANPLUS plugin to install packages that use Build.PL

=head1 SYNOPSIS

    my $build = CPANPLUS::Dist->new(
                                format  => 'CPANPLUS::Dist::Build',
                                module  => $modobj,
                            );

    $build->prepare;    # runs Build.PL
    $build->create;     # runs build && build test
    $build->install;    # runs build install


=head1 DESCRIPTION

C<CPANPLUS::Dist::Build> is a distribution class for C<Module::Build>
related modules.
Using this package, you can create, install and uninstall perl
modules. It inherits from C<CPANPLUS::Dist>.

Normal users won't have to worry about the interface to this module,
as it functions transparently as a plug-in to C<CPANPLUS> and will
just C<Do The Right Thing> when it's loaded.

=head1 ACCESSORS

=over 4

=item C<parent()>

Returns the C<CPANPLUS::Module> object that parented this object.

=item C<status()>

Returns the C<Object::Accessor> object that keeps the status for
this module.

=back

=head1 STATUS ACCESSORS

All accessors can be accessed as follows:
    $build->status->ACCESSOR

=over 4

=item C<build_pl ()>

Location of the Build file.
Set to 0 explicitly if something went wrong.

=item C<build ()>

BOOL indicating if the C<Build> command was successful.

=item C<test ()>

BOOL indicating if the C<Build test> command was successful.

=item C<prepared ()>

BOOL indicating if the C<prepare> call exited successfully
This gets set after C<perl Build.PL>

=item C<distdir ()>

Full path to the directory in which the C<prepare> call took place,
set after a call to C<prepare>.

=item C<created ()>

BOOL indicating if the C<create> call exited successfully. This gets
set after C<Build> and C<Build test>.

=item C<installed ()>

BOOL indicating if the module was installed. This gets set after
C<Build install> exits successfully.

=item uninstalled ()

BOOL indicating if the module was uninstalled properly.

=item C<_create_args ()>

Storage of the arguments passed to C<create> for this object. Used
for recursive calls when satisfying prerequisites.

=item C<_install_args ()>

Storage of the arguments passed to C<install> for this object. Used
for recursive calls when satisfying prerequisites.

=back

=cut

=head1 METHODS

=head2 $bool = CPANPLUS::Dist::Build->format_available();

Returns a boolean indicating whether or not you can use this package
to create and install modules in your environment.

=cut

### check if the format is available ###
sub format_available {
    my $mod = 'Module::Build';
    unless( can_load( modules => { $mod => '0.2611' }, nocache => 1 ) ) {
        error( loc( "You do not have '%1' -- '%2' not available",
                    $mod, __PACKAGE__ ) );
        return;
    }

    return 1;
}


=head2 $bool = $dist->init();

Sets up the C<CPANPLUS::Dist::Build> object for use.
Effectively creates all the needed status accessors.

Called automatically whenever you create a new C<CPANPLUS::Dist> object.

=cut

sub init {
    my $dist    = shift;
    my $status  = $dist->status;

    $status->mk_accessors(qw[build_pl build test created installed uninstalled
                             _create_args _install_args _prepare_args
                             _mb_object _buildflags
                            ]);

    ### just in case 'format_available' didn't get called
    require Module::Build;

    return 1;
}

=pod

=head2 $bool = $dist->prepare([perl => '/path/to/perl', buildflags => 'EXTRA=FLAGS', force => BOOL, verbose => BOOL])

C<prepare> prepares a distribution, running C<Build.PL>
and establishing any prerequisites this
distribution has.

The variable C<PERL5_CPANPLUS_IS_EXECUTING> will be set to the full path
of the C<Build.PL> that is being executed. This enables any code inside
the C<Build.PL> to know that it is being installed via CPANPLUS.

After a successful C<prepare> you may call C<create> to create the
distribution, followed by C<install> to actually install it.

Returns true on success and false on failure.

=cut

sub prepare {
    ### just in case you already did a create call for this module object
    ### just via a different dist object
    my $dist = shift;
    my $self = $dist->parent;

    ### we're also the cpan_dist, since we don't need to have anything
    ### prepared from another installer
    $dist    = $self->status->dist_cpan if      $self->status->dist_cpan;
    $self->status->dist_cpan( $dist )   unless  $self->status->dist_cpan;

    my $cb   = $self->parent;
    my $conf = $cb->configure_object;
    my %hash = @_;

    my $dir;
    unless( $dir = $self->status->extract ) {
        error( loc( "No dir found to operate on!" ) );
        return;
    }

    my $args;
    my( $force, $verbose, $buildflags, $perl, $prereq_target, $prereq_format,
        $prereq_build );
    {   local $Params::Check::ALLOW_UNKNOWN = 1;
        my $tmpl = {
            force           => {    default => $conf->get_conf('force'),
                                    store   => \$force },
            verbose         => {    default => $conf->get_conf('verbose'),
                                    store   => \$verbose },
            perl            => {    default => $^X, store => \$perl },
            buildflags      => {    default => $conf->get_conf('buildflags'),
                                    store   => \$buildflags },
            prereq_target   => {    default => '', store => \$prereq_target },
            prereq_format   => {    default => '',
                                    store   => \$prereq_format },
            prereq_build    => {    default => 0, store => \$prereq_build },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    return 1 if $dist->status->prepared && !$force;

    $dist->status->_prepare_args( $args );

    ### chdir to work directory ###
    my $orig = cwd();
    unless( $cb->_chdir( dir => $dir ) ) {
        error( loc( "Could not chdir to build directory '%1'", $dir ) );
        return;
    }

    ### by now we've loaded module::build, and we're using the API, so
    ### it's safe to remove CPANPLUS::inc from our inc path, especially
    ### because it can trip up tests run under taint (just like EU::MM).
    ### turn off our PERL5OPT so no modules from CPANPLUS::inc get
    ### included in make test -- it should build without.
    ### also, modules that run in taint mode break if we leave
    ### our code ref in perl5opt
    ### XXX we've removed the ENV settings from cp::inc, so only need
    ### to reset the @INC
    #local $ENV{PERL5OPT} = CPANPLUS::inc->original_perl5opt;
    #local $ENV{PERL5LIB} = CPANPLUS::inc->original_perl5lib;
    #local @INC           = CPANPLUS::inc->original_inc;

    ### this will generate warnings under anything lower than M::B 0.2606
    my @buildflags = $dist->_buildflags_as_list( $buildflags );
    $dist->status->_buildflags( $buildflags );

    my $fail; my $prereq_fail;
    my $status = { };
    RUN: {
        # 0.85_01
        ### we resolve 'configure requires' here, so we can run the 'perl
        ### Makefile.PL' command
        ### XXX for tests: mock f_c_r to something that *can* resolve and
        ### something that *doesn't* resolve. Check the error log for ok
        ### on this step or failure
        ### XXX make a separate tarball to test for this scenario: simply
        ### containing a makefile.pl/build.pl for test purposes?
        my $safe_ver = version->new('0.85_01');
        if ( version->new($CPANPLUS::Internals::VERSION) >= $safe_ver )
        {   my $configure_requires = $dist->find_configure_requires;
            my $ok = $dist->_resolve_prereqs(
                            format          => $prereq_format,
                            verbose         => $verbose,
                            prereqs         => $configure_requires,
                            target          => $prereq_target,
                            force           => $force,
                            prereq_build    => $prereq_build,
                    );

            unless( $ok ) {

                #### use $dist->flush to reset the cache ###
                error( loc( "Unable to satisfy '%1' for '%2' " .
                            "-- aborting install",
                            'configure_requires', $self->module ) );
                $dist->status->prepared(0);
                $prereq_fail++;
                $fail++;
                last RUN;
            }
            ### end of prereq resolving ###
        }

        # Wrap the exception that may be thrown here (should likely be
        # done at a much higher level).
        my $prep_output;

        my $env = ENV_CPANPLUS_IS_EXECUTING;
        local $ENV{$env} = BUILD_PL->( $dir );
        my @run_perl    = ( '-e', CPDB_PERL_WRAPPER );
        my $cmd = [$perl, @run_perl, BUILD_PL->($dir), @buildflags];

        unless ( scalar run(    command => $cmd,
                                buffer  => \$prep_output,
                                verbose => $verbose )
        ) {
            error( loc( "Build.PL failed: %1", $prep_output ) );
            if ( $conf->get_conf('cpantest') ) {
               $status->{stage} = 'prepare';
               $status->{capture} = $prep_output;
            }
            $fail++; last RUN;
        }

        msg( $prep_output, 0 );

        my $prereqs = $self->status->prereqs;

        $prereqs ||= $dist->_find_prereqs( verbose => $verbose,
                                           dir => $dir,
                                           perl => $perl,
                                           buildflags => $buildflags );

    }

    ### send out test report? ###
    if( $fail and $conf->get_conf('cpantest') and not $prereq_fail ) {
           $cb->_send_report(
            module  => $self,
            failed  => $fail,
            buffer  => CPANPLUS::Error->stack_as_string,
            status  => $status,
            verbose => $verbose,
            force   => $force,
        ) or error(loc("Failed to send test report for '%1'",
                    $self->module ) );
    }

    unless( $cb->_chdir( dir => $orig ) ) {
        error( loc( "Could not chdir back to start dir '%1'", $orig ) );
    }

    ### save where we wrote this stuff -- same as extract dir in normal
    ### installer circumstances
    $dist->status->distdir( $self->status->extract );

    return $dist->status->prepared( $fail ? 0 : 1 );
}

sub _find_prereqs {
    my $dist = shift;
    my $self = $dist->parent;
    my $cb   = $self->parent;
    my $conf = $cb->configure_object;
    my %hash = @_;

    my ($verbose, $dir, $buildflags, $perl);
    my $tmpl = {
        verbose => { default => $conf->get_conf('verbose'), store => \$verbose },
        dir     => { default => $self->status->extract, store => \$dir },
        perl    => { default => $^X, store => \$perl },
        buildflags => { default => $conf->get_conf('buildflags'),
                        store   => \$buildflags },
    };

    my $args = check( $tmpl, \%hash ) or return;

    my $prereqs = {};

    $prereqs = $dist->find_mymeta_requires()
       if $dist->can('find_mymeta_requires');

    if ( keys %$prereqs ) {
        # Ugly hack
    }
    else {
      my $safe_ver = version->new('0.31_03');
      my $content;
      PREREQS: {
        if ( version->new( $Module::Build::VERSION ) >= $safe_ver and IPC::Cmd->can_capture_buffer ) {
          my @buildflags = $dist->_buildflags_as_list( $buildflags );

          # Use the new Build action 'prereq_data'
          my @run_perl    = ( '-e', CPDB_PERL_WRAPPER );

          unless ( scalar run(    command => [$perl, @run_perl, BUILD->($dir), 'prereq_data', @buildflags],
                                buffer  => \$content,
                                verbose => 0 )
          ) {
            error( loc( "Build 'prereq_data' failed: %1 %2", $!, $content ) );
            #return;
          }
          else {
            last PREREQS;
          }

        }

        my $file = File::Spec->catfile( $dir, '_build', 'prereqs' );
        return unless -f $file;

        my $fh = FileHandle->new();

        unless( $fh->open( $file ) ) {
          error( loc( "Cannot open '%1': %2", $file, $! ) );
          return;
        }

        $content = do { local $/; <$fh> };

      }

      return unless $content;
      my $bphash = eval $content;
      return unless $bphash and ref $bphash eq 'HASH';
      foreach my $type ('requires', 'build_requires') {
        next unless $bphash->{$type} and ref $bphash->{$type} eq 'HASH';
        $prereqs->{$_} = $bphash->{$type}->{$_} for keys %{ $bphash->{$type} };
      }
    }

    {
      delete $prereqs->{'perl'}
         unless version->new($CPANPLUS::Internals::VERSION)
             >= version->new('0.9102');
    }

    ### allows for a user defined callback to filter the prerequisite
    ### list as they see fit, to remove (or add) any prereqs they see
    ### fit. The default installed callback will return the hashref in
    ### an unmodified form
    ### this callback got added after cpanplus 0.0562, so use a 'can'
    ### to find out if it's supported. For older versions, we'll just
    ### return the hashref as is ourselves.
    my $href    = $cb->_callbacks->can('filter_prereqs')
                    ? $cb->_callbacks->filter_prereqs->( $cb, $prereqs )
                    : $prereqs;

    $self->status->prereqs( $href );

    ### make sure it's not the same ref
    return { %$href };
}

=pod

=head2 $dist->create([perl => '/path/to/perl', buildflags => 'EXTRA=FLAGS', prereq_target => TARGET, force => BOOL, verbose => BOOL, skiptest => BOOL])

C<create> preps a distribution for installation. This means it will
run C<Build> and C<Build test>.
This will also satisfy any prerequisites the module may have.

If you set C<skiptest> to true, it will skip the C<Build test> stage.
If you set C<force> to true, it will go over all the stages of the
C<Build> process again, ignoring any previously cached results. It
will also ignore a bad return value from C<Build test> and still allow
the operation to return true.

Returns true on success and false on failure.

You may then call C<< $dist->install >> on the object to actually
install it.

=cut

sub create {
    ### just in case you already did a create call for this module object
    ### just via a different dist object
    my $dist = shift;
    my $self = $dist->parent;

    ### we're also the cpan_dist, since we don't need to have anything
    ### prepared from another installer
    $dist    = $self->status->dist_cpan if      $self->status->dist_cpan;
    $self->status->dist_cpan( $dist )   unless  $self->status->dist_cpan;

    my $cb   = $self->parent;
    my $conf = $cb->configure_object;
    my %hash = @_;

    my $dir;
    unless( $dir = $self->status->extract ) {
        error( loc( "No dir found to operate on!" ) );
        return;
    }

    my $args;
    my( $force, $verbose, $buildflags, $skiptest, $prereq_target,
        $perl, $prereq_format, $prereq_build);
    {   local $Params::Check::ALLOW_UNKNOWN = 1;
        my $tmpl = {
            force           => {    default => $conf->get_conf('force'),
                                    store   => \$force },
            verbose         => {    default => $conf->get_conf('verbose'),
                                    store   => \$verbose },
            perl            => {    default => $^X, store => \$perl },
            buildflags      => {    default => $conf->get_conf('buildflags'),
                                    store   => \$buildflags },
            skiptest        => {    default => $conf->get_conf('skiptest'),
                                    store   => \$skiptest },
            prereq_target   => {    default => '', store => \$prereq_target },
            ### don't set the default format to 'build' -- that is wrong!
            prereq_format   => {    #default => $self->status->installer_type,
                                    default => '',
                                    store   => \$prereq_format },
            prereq_build    => {    default => 0, store => \$prereq_build },
        };

        $args = check( $tmpl, \%hash ) or return;
    }

    # restore the state as we have created this already.
    if ( $dist->status->created && !$force ) {
        ### add this directory to your lib ###
        $self->add_to_includepath();
        return 1;
    }

    $dist->status->_create_args( $args );

    ### is this dist prepared?
    unless( $dist->status->prepared ) {
        error( loc( "You have not successfully prepared a '%2' distribution ".
                    "yet -- cannot create yet", __PACKAGE__ ) );
        return;
    }

    ### chdir to work directory ###
    my $orig = cwd();
    unless( $cb->_chdir( dir => $dir ) ) {
        error( loc( "Could not chdir to build directory '%1'", $dir ) );
        return;
    }

    ### by now we've loaded module::build, and we're using the API, so
    ### it's safe to remove CPANPLUS::inc from our inc path, especially
    ### because it can trip up tests run under taint (just like EU::MM).
    ### turn off our PERL5OPT so no modules from CPANPLUS::inc get
    ### included in make test -- it should build without.
    ### also, modules that run in taint mode break if we leave
    ### our code ref in perl5opt
    ### XXX we've removed the ENV settings from cp::inc, so only need
    ### to reset the @INC
    #local $ENV{PERL5OPT} = CPANPLUS::inc->original_perl5opt;
    #local $ENV{PERL5LIB} = CPANPLUS::inc->original_perl5lib;
    #local @INC           = CPANPLUS::inc->original_inc;

    ### but do it *before* the new_from_context, as M::B seems
    ### to be actually running the file...
    ### an unshift in the block seems to be ignored.. somehow...
    #{   my $lib = $self->best_path_to_module_build;
    #    unshift @INC, $lib if $lib;
    #}
    unshift @INC, $self->best_path_to_module_build
                if $self->best_path_to_module_build;

    ### this will generate warnings under anything lower than M::B 0.2606
    my @buildflags = $dist->_buildflags_as_list( $buildflags );
    $dist->status->_buildflags( $buildflags );

    my $fail; my $prereq_fail; my $test_fail;
    my $status = { };
    RUN: {

        my @run_perl    = ( '-e', CPDB_PERL_WRAPPER );

        ### this will set the directory back to the start
        ### dir, so we must chdir /again/
        my $ok = $dist->_resolve_prereqs(
                        force           => $force,
                        format          => $prereq_format,
                        verbose         => $verbose,
                        prereqs         => $self->status->prereqs,
                        target          => $prereq_target,
                        prereq_build    => $prereq_build,
                    );

        unless( $cb->_chdir( dir => $dir ) ) {
            error( loc( "Could not chdir to build directory '%1'", $dir ) );
            return;
        }

        unless( $ok ) {
            #### use $dist->flush to reset the cache ###
            error( loc( "Unable to satisfy prerequisites for '%1' " .
                        "-- aborting install", $self->module ) );
            $dist->status->build(0);
            $fail++; $prereq_fail++;
            last RUN;
        }

        my ($captured, $cmd);
        if ( ON_VMS ) {
            $cmd = [$perl, BUILD->($dir), @buildflags];
        }
        else {
            $cmd = [$perl, @run_perl, BUILD->($dir), @buildflags];
        }

        unless ( scalar run(    command => $cmd,
                                buffer  => \$captured,
                                verbose => $verbose )
        ) {
            error( loc( "MAKE failed:\n%1", $captured ) );
            $dist->status->build(0);
            if ( $conf->get_conf('cpantest') ) {
               $status->{stage} = 'build';
               $status->{capture} = $captured;
            }
            $fail++; last RUN;
        }

        msg( $captured, 0 );

        $dist->status->build(1);

        ### add this directory to your lib ###
        $self->add_to_includepath();

        ### this buffer will not include what tests failed due to a
        ### M::B/Test::Harness bug. Reported as #9793 with patch
        ### against 0.2607 on 26/1/2005
        unless( $skiptest ) {
            my $test_output;
            if ( ON_VMS ) {
                $cmd     = [$perl, BUILD->($dir), "test", @buildflags];
            }
            else {
                $cmd     = [$perl, @run_perl, BUILD->($dir), "test", @buildflags];
            }
            unless ( scalar run(    command => $cmd,
                                    buffer  => \$test_output,
                                    verbose => $verbose )
            ) {
                error( loc( "MAKE TEST failed:\n%1 ", $test_output ), ( $verbose ? 0 : 1 ) );

                ### mark specifically *test* failure.. so we dont
                ### send success on force...
                $test_fail++;

                if( !$force and !$cb->_callbacks->proceed_on_test_failure->(
                                      $self, $@ )
                ) {
                    $dist->status->test(0);
                    if ( $conf->get_conf('cpantest') ) {
                      $status->{stage} = 'test';
                      $status->{capture} = $test_output;
                    }
                    $fail++; last RUN;
                }

            }
            else {
                msg( loc( "MAKE TEST passed:\n%1", $test_output ), 0 );
                $dist->status->test(1);
                if ( $conf->get_conf('cpantest') ) {
                   $status->{stage} = 'test';
                   $status->{capture} = $test_output;
                }
            }
        }
        else {
            msg(loc("Tests skipped"), $verbose);
        }
    }

    unless( $cb->_chdir( dir => $orig ) ) {
        error( loc( "Could not chdir back to start dir '%1'", $orig ) );
    }

    ### send out test report? ###
    if( $conf->get_conf('cpantest') and not $prereq_fail ) {
        $cb->_send_report(
            module          => $self,
            failed          => $test_fail || $fail,
            buffer          => CPANPLUS::Error->stack_as_string,
            status          => $status,
            verbose         => $verbose,
            force           => $force,
            tests_skipped   => $skiptest,
        ) or error(loc("Failed to send test report for '%1'",
                    $self->module ) );
    }

    return $dist->status->created( $fail ? 0 : 1 );
}

=head2 $dist->install([verbose => BOOL, perl => /path/to/perl])

Actually installs the created dist.

Returns true on success and false on failure.

=cut

sub install {
    ### just in case you already did a create call for this module object
    ### just via a different dist object
    my $dist = shift;
    my $self = $dist->parent;

    ### we're also the cpan_dist, since we don't need to have anything
    ### prepared from another installer
    $dist    = $self->status->dist_cpan if $self->status->dist_cpan;

    my $cb   = $self->parent;
    my $conf = $cb->configure_object;
    my %hash = @_;


    my $verbose; my $perl; my $force; my $buildflags;
    {   local $Params::Check::ALLOW_UNKNOWN = 1;
        my $tmpl = {
            verbose => { default => $conf->get_conf('verbose'),
                         store   => \$verbose },
            force   => { default => $conf->get_conf('force'),
                         store   => \$force },
            buildflags => { default => $conf->get_conf('buildflags'),
                            store   => \$buildflags },
            perl    => { default => $^X, store   => \$perl },
        };

        my $args = check( $tmpl, \%hash ) or return;
        $dist->status->_install_args( $args );
    }

    my $dir;
    unless( $dir = $self->status->extract ) {
        error( loc( "No dir found to operate on!" ) );
        return;
    }

    my $orig = cwd();

    unless( $cb->_chdir( dir => $dir ) ) {
        error( loc( "Could not chdir to build directory '%1'", $dir ) );
        return;
    }

    ### value set and false -- means failure ###
    if( defined $self->status->installed &&
        !$self->status->installed && !$force
    ) {
        error( loc( "Module '%1' has failed to install before this session " .
                    "-- aborting install", $self->module ) );
        return;
    }

    my $fail;
    my @buildflags = $dist->_buildflags_as_list( $buildflags );
    my @run_perl    = ( '-e', CPDB_PERL_WRAPPER );

    ### hmm, how is this going to deal with sudo?
    ### for now, check effective uid, if it's not root,
    ### shell out, otherwise use the method
    if( $> ) {

        ### don't worry about loading the right version of M::B anymore
        ### the 'new_from_context' already added the 'right' path to
        ### M::B at the top of the build.pl
        my $cmd;
        if ( ON_VMS ) {
            $cmd     = [$perl, BUILD->($dir), "install", @buildflags];
        }
        else {
            $cmd     = [$perl, @run_perl, BUILD->($dir), "install", @buildflags];
        }

        ### Detect local::lib type behaviour. Do not use 'sudo' in these cases
        my $sudo    = $conf->get_program('sudo');
        SUDO: {
          ### Actual local::lib in use
          last SUDO if defined $ENV{PERL_MB_OPT} and $ENV{PERL_MB_OPT} =~ m!install_base!;
          ### 'buildflags' is configured with '--install_base'
          last SUDO if scalar grep { m!install_base! } @buildflags;
          ### oh well 'sudo make me a sandwich'
          unshift @$cmd, $sudo;
        }

        my $buffer;
        unless( scalar run( command => $cmd,
                            buffer  => \$buffer,
                            verbose => $verbose )
        ) {
            error(loc("Could not run '%1': %2", 'Build install', $buffer));
            $fail++;
        }
    } else {
        my ($install_output, $cmd);
        if ( ON_VMS ) {
            $cmd     = [$perl, BUILD->($dir), "install", @buildflags];
        }
        else {
            $cmd     = [$perl, @run_perl, BUILD->($dir), "install", @buildflags];
        }
        unless( scalar run( command => $cmd,
                            buffer  => \$install_output,
                            verbose => $verbose )
        ) {
            error(loc("Could not run '%1': %2", 'Build install', $install_output));
            $fail++;
        }
        else {
            msg( $install_output, 0 );
        }
    }


    unless( $cb->_chdir( dir => $orig ) ) {
        error( loc( "Could not chdir back to start dir '%1'", $orig ) );
    }

    return $dist->status->installed( $fail ? 0 : 1 );
}

### returns the string 'foo=bar --zot quux'
###        as the list 'foo=bar', '--zot', 'qux'
sub _buildflags_as_list {
    my $self    = shift;
    my $flags   = shift or return;

    return Module::Build->split_like_shell($flags);
}

=head1 AUTHOR

Originally by Jos Boumans E<lt>kane@cpan.orgE<gt>.  Brought to working
condition by Ken Williams E<lt>kwilliams@cpan.orgE<gt>.

Other hackery and currently maintained by Chris C<BinGOs> Williams ( no relation ). E<lt>bingos@cpan.orgE<gt>.

=head1 LICENSE

The CPAN++ interface (of which this module is a part of) is
copyright (c) 2001, 2002, 2003, 2004, 2005 Jos Boumans E<lt>kane@cpan.orgE<gt>.
All rights reserved.

This library is free software;
you may redistribute and/or modify it under the same
terms as Perl itself.

=cut

qq[Putting the Module::Build into CPANPLUS];


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

