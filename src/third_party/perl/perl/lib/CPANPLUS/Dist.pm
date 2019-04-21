package CPANPLUS::Dist;

use strict;

use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;

use Cwd ();
use Object::Accessor;
use Parse::CPAN::Meta;

use IPC::Cmd                    qw[run];
use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load check_install];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

use base 'Object::Accessor';

local $Params::Check::VERBOSE = 1;

=pod

=head1 NAME

CPANPLUS::Dist - base class for plugins

=head1 SYNOPSIS

    my $dist = CPANPLUS::Dist::YOUR_DIST_TYPE_HERE->new(
                                module  => $modobj,
                            );

=head1 DESCRIPTION

C<CPANPLUS::Dist> is a base class for C<CPANPLUS::Dist::MM>
and C<CPANPLUS::Dist::Build>. Developers of other C<CPANPLUS::Dist::*>
plugins should look at C<CPANPLUS::Dist::Base>.

=head1 ACCESSORS

=over 4

=item parent()

Returns the C<CPANPLUS::Module> object that parented this object.

=item status()

Returns the C<Object::Accessor> object that keeps the status for
this module.

=back

=head1 STATUS ACCESSORS

All accessors can be accessed as follows:
    $deb->status->ACCESSOR

=over 4

=item created()

Boolean indicating whether the dist was created successfully.
Explicitly set to C<0> when failed, so a value of C<undef> may be
interpreted as C<not yet attempted>.

=item installed()

Boolean indicating whether the dist was installed successfully.
Explicitly set to C<0> when failed, so a value of C<undef> may be
interpreted as C<not yet attempted>.

=item uninstalled()

Boolean indicating whether the dist was uninstalled successfully.
Explicitly set to C<0> when failed, so a value of C<undef> may be
interpreted as C<not yet attempted>.

=item dist()

The location of the final distribution. This may be a file or
directory, depending on how your distribution plug in of choice
works. This will be set upon a successful create.

=cut

=back

=head2 $dist = CPANPLUS::Dist::YOUR_DIST_TYPE_HERE->new( module => MODOBJ );

Create a new C<CPANPLUS::Dist::YOUR_DIST_TYPE_HERE> object based on the
provided C<MODOBJ>.

*** DEPRECATED ***
The optional argument C<format> is used to indicate what type of dist
you would like to create (like C<CPANPLUS::Dist::MM> or
C<CPANPLUS::Dist::Build> and so on ).

C<< CPANPLUS::Dist->new >> is exclusively meant as a method to be
inherited by C<CPANPLUS::Dist::MM|Build>.

Returns a C<CPANPLUS::Dist::YOUR_DIST_TYPE_HERE> object on success
and false on failure.

=cut

sub new {
    my $self    = shift;
    my $class   = ref $self || $self;
    my %hash    = @_;

    ### first verify we got a module object ###
    my( $mod, $format );
    my $tmpl = {
        module  => { required => 1, allow => IS_MODOBJ, store => \$mod },
        ### for backwards compatibility
        format  => { default  => $class, store => \$format,
                     allow    => [ __PACKAGE__->dist_types ],
        },
    };
    check( $tmpl, \%hash ) or return;

    unless( can_load( modules => { $format => '0.0' }, verbose => 1 ) ) {
        error(loc("'%1' not found -- you need '%2' version '%3' or higher ".
                    "to detect plugins", $format, 'Module::Pluggable','2.4'));
        return;
    }

    ### get an empty o::a object for this class
    my $obj = $format->SUPER::new;

    $obj->mk_accessors( qw[parent status] );

    ### set the parent
    $obj->parent( $mod );

    ### create a status object ###
    {   my $acc = Object::Accessor->new;
        $obj->status($acc);

        ### add minimum supported accessors
        $acc->mk_accessors( qw[prepared created installed uninstalled
                               distdir dist] );
    }

    ### get the conf object ###
    my $conf = $mod->parent->configure_object();

    ### check if the format is available in this environment ###
    if( $conf->_get_build('sanity_check') and not $obj->format_available ) {
        error( loc( "Format '%1' is not available", $format) );
        return;
    }

    ### now initialize it or admit failure
    unless( $obj->init ) {
        error(loc("Dist initialization of '%1' failed for '%2'",
                    $format, $mod->module));
        return;
    }

    ### return the object
    return $obj;
}

=head2 @dists = CPANPLUS::Dist->dist_types;

Returns a list of the CPANPLUS::Dist::* classes available

=cut

### returns a list of dist_types we support
### will get overridden by Module::Pluggable if loaded
### XXX add support for 'plugin' dir in config as well
{   my $Loaded;
    my @Dists   = (INSTALLER_MM);
    my @Ignore  = ();

    ### backdoor method to add more dist types
    sub _add_dist_types     { my $self = shift; push @Dists,  @_ };

    ### backdoor method to exclude dist types
    sub _ignore_dist_types  { my $self = shift; push @Ignore, @_ };
    sub _reset_dist_ignore  { @Ignore = () };

    ### locally add the plugins dir to @INC, so we can find extra plugins
    #local @INC = @INC, File::Spec->catdir(
    #                        $conf->get_conf('base'),
    #                        $conf->_get_build('plugins') );

    ### load any possible plugins
    sub dist_types {

        if ( !$Loaded++ and check_install(  module  => 'Module::Pluggable',
                                            version => '2.4')
        ) {
            require Module::Pluggable;

            my $only_re = __PACKAGE__ . '::\w+$';
            my %except  = map { $_ => 1 }
                              INSTALLER_SAMPLE,
                              INSTALLER_BASE;

            Module::Pluggable->import(
                            sub_name    => '_dist_types',
                            search_path => __PACKAGE__,
                            only        => qr/$only_re/,
                            require     => 1,
                            except      => [ keys %except ]
                        );
            my %ignore = map { $_ => $_ } @Ignore;

            push @Dists, grep { not $ignore{$_} and not $except{$_} }
                __PACKAGE__->_dist_types;
        }

        return @Dists;
    }

=head2 $bool = CPANPLUS::Dist->rescan_dist_types;

Rescans C<@INC> for available dist types. Useful if you've installed new
C<CPANPLUS::Dist::*> classes and want to make them available to the
current process.

=cut

    sub rescan_dist_types {
        my $dist    = shift;
        $Loaded     = 0;    # reset the flag;
        return $dist->dist_types;
    }
}

=head2 $bool = CPANPLUS::Dist->has_dist_type( $type )

Returns true if distribution type C<$type> is loaded/supported.

=cut

sub has_dist_type {
    my $dist = shift;
    my $type = shift or return;

    return scalar grep { $_ eq $type } CPANPLUS::Dist->dist_types;
}

=head2 $bool = $dist->prereq_satisfied( modobj => $modobj, version => $version_spec )

Returns true if this prereq is satisfied.  Returns false if it's not.
Also issues an error if it seems "unsatisfiable," i.e. if it can't be
found on CPAN or the latest CPAN version doesn't satisfy it.

=cut

sub prereq_satisfied {
    my $dist = shift;
    my $cb   = $dist->parent->parent;
    my %hash = @_;

    my($mod,$ver);
    my $tmpl = {
        version => { required => 1, store => \$ver },
        modobj  => { required => 1, store => \$mod, allow => IS_MODOBJ },
    };

    check( $tmpl, \%hash ) or return;

    return 1 if $mod->is_uptodate( version => $ver );

    if ( $cb->_vcmp( $ver, $mod->version ) > 0 ) {

        error(loc(
                "This distribution depends on %1, but the latest version".
                " of %2 on CPAN (%3) doesn't satisfy the specific version".
                " dependency (%4). You may have to resolve this dependency ".
                "manually.",
                $mod->module, $mod->module, $mod->version, $ver ));

    }

    return;
}

=head2 $configure_requires = $dist->find_configure_requires( [file => /path/to/META.yml] )

Reads the configure_requires for this distribution from the META.yml or META.json
file in the root directory and returns a hashref with module names
and versions required.

=cut

sub find_configure_requires {
    my $self = shift;
    my $mod  = $self->parent;
    my %hash = @_;

    my ($meta);
    my $href = {};

    my $tmpl = {
        file => { store => \$meta },
    };

    check( $tmpl, \%hash ) or return;

    my $meth = 'configure_requires';

    {

      ### the prereqs as we have them now
      my @args = (
        defaults => $mod->status->$meth || {},
      );

      my @possibles = do { defined $mod->status->extract
                           ? ( META_JSON->( $mod->status->extract ),
                               META_YML->( $mod->status->extract ) )
                           : ()
                      };

      unshift @possibles, $meta if $meta;

      META: foreach my $mfile ( grep { -e } @possibles ) {
          push @args, ( file => $mfile );
          if ( $mfile =~ /\.json/ ) {
            $href = $self->_prereqs_from_meta_json( @args, keys => [ 'configure' ] );
          }
          else {
            $href = $self->_prereqs_from_meta_file( @args, keys => [ $meth ] );
          }
          last META;
      }

    }

    ### and store it in the module
    $mod->status->$meth( $href );

    return { %$href };
}

sub find_mymeta_requires {
    my $self = shift;
    my $mod  = $self->parent;
    my %hash = @_;

    my ($meta);
    my $href = {};

    my $tmpl = {
        file => { store => \$meta },
    };

    check( $tmpl, \%hash ) or return;

    my $meth = 'prereqs';

    {

      ### the prereqs as we have them now
      my @args = (
        defaults => $mod->status->$meth || {},
      );

      my @possibles = do { defined $mod->status->extract
                           ? ( MYMETA_JSON->( $mod->status->extract ),
                               MYMETA_YML->( $mod->status->extract ) )
                           : ()
                      };

      unshift @possibles, $meta if $meta;

      META: foreach my $mfile ( grep { -e } @possibles ) {
          push @args, ( file => $mfile );
          if ( $mfile =~ /\.json/ ) {
            $href = $self->_prereqs_from_meta_json( @args,
                keys => [ qw|build test runtime| ] );
          }
          else {
            $href = $self->_prereqs_from_meta_file( @args,
                keys => [ qw|build_requires requires| ] );
          }
          last META;
      }

    }

    ### and store it in the module
    $mod->status->$meth( $href );

    return { %$href };
}

sub _prereqs_from_meta_file {
    my $self = shift;
    my $mod  = $self->parent;
    my %hash = @_;

    my( $meta, $defaults, $keys );
    my $tmpl = {                ### check if we have an extract path. if not, we
                                ### get 'undef value' warnings from file::spec
        file        => { default => do { defined $mod->status->extract
                                        ? META_YML->( $mod->status->extract )
                                        : '' },
                        store   => \$meta,
                    },
        defaults    => { required => 1, default => {}, strict_type => 1,
                         store => \$defaults },
        keys        => { required => 1, default => [], strict_type => 1,
                         store => \$keys },
    };

    check( $tmpl, \%hash ) or return;

    ### if there's a meta file, we read it;
    if( -e $meta ) {

        ### Parse::CPAN::Meta uses exceptions for errors
        ### hash returned in list context!!!

        local $ENV{PERL_JSON_BACKEND};

        my ($doc) = eval { Parse::CPAN::Meta::LoadFile( $meta ) };

        unless( $doc ) {
            error(loc( "Could not read %1: '%2'", $meta, $@ ));
            return $defaults;
        }

        ### read the keys now, make sure not to throw
        ### away anything that was already added
        for my $key ( @$keys ) {
            $defaults = {
                %$defaults,
                %{ $doc->{$key} },
            } if $doc->{ $key };
        }
    }

    ### and return a copy
    return \%{ $defaults };
}

sub _prereqs_from_meta_json {
    my $self = shift;
    my $mod  = $self->parent;
    my %hash = @_;

    my( $meta, $defaults, $keys );
    my $tmpl = {                ### check if we have an extract path. if not, we
                                ### get 'undef value' warnings from file::spec
        file        => { default => do { defined $mod->status->extract
                                        ? META_JSON->( $mod->status->extract )
                                        : '' },
                        store   => \$meta,
                    },
        defaults    => { required => 1, default => {}, strict_type => 1,
                         store => \$defaults },
        keys        => { required => 1, default => [], strict_type => 1,
                         store => \$keys },
    };

    check( $tmpl, \%hash ) or return;

    ### if there's a meta file, we read it;
    if( -e $meta ) {

        ### Parse::CPAN::Meta uses exceptions for errors
        ### hash returned in list context!!!

        local $ENV{PERL_JSON_BACKEND};

        my ($doc) = eval { Parse::CPAN::Meta->load_file( $meta ) };

        unless( $doc ) {
            error(loc( "Could not read %1: '%2'", $meta, $@ ));
            return $defaults;
        }

        ### read the keys now, make sure not to throw
        ### away anything that was already added
        #for my $key ( @$keys ) {
        #    $defaults = {
        #        %$defaults,
        #        %{ $doc->{$key} },
        #    } if $doc->{ $key };
        #}
        my $prereqs = $doc->{prereqs} || {};
        for my $key ( @$keys ) {
            $defaults = {
                %$defaults,
                %{ $prereqs->{$key}->{requires} },
            } if $prereqs->{ $key }->{requires};
        }
    }

    ### and return a copy
    return \%{ $defaults };
}

=head2 $bool = $dist->_resolve_prereqs( ... )

Makes sure prerequisites are resolved

    format          The dist class to use to make the prereqs
                    (ie. CPANPLUS::Dist::MM)

    prereqs         Hash of the prerequisite modules and their versions

    target          What to do with the prereqs.
                        create  => Just build them
                        install => Install them
                        ignore  => Ignore them

    prereq_build    If true, always build the prereqs even if already
                    resolved

    verbose         Be verbose

    force           Force the prereq to be built, even if already resolved

=cut

sub _resolve_prereqs {
    my $dist = shift;
    my $self = $dist->parent;
    my $cb   = $self->parent;
    my $conf = $cb->configure_object;
    my %hash = @_;

    my ($prereqs, $format, $verbose, $target, $force, $prereq_build,$tolerant);
    my $tmpl = {
        ### XXX perhaps this should not be required, since it may not be
        ### packaged, just installed...
        ### Let it be empty as well -- that means the $modobj->install
        ### routine will figure it out, which is fine if we didn't have any
        ### very specific wishes (it will even detect the favourite
        ### dist_type).
        format          => { required => 1, store => \$format,
                                allow => ['',__PACKAGE__->dist_types], },
        prereqs         => { required => 1, default => { },
                                strict_type => 1, store => \$prereqs },
        verbose         => { default => $conf->get_conf('verbose'),
                                store => \$verbose },
        force           => { default => $conf->get_conf('force'),
                                store => \$force },
                        ### make sure allow matches with $mod->install's list
        target          => { default => '', store => \$target,
                                allow => ['',qw[create ignore install]] },
        prereq_build    => { default => 0, store => \$prereq_build },
        tolerant        => { default => $conf->get_conf('allow_unknown_prereqs'),
                                store => \$tolerant },
    };

    check( $tmpl, \%hash ) or return;

    ### so there are no prereqs? then don't even bother
    return 1 unless keys %$prereqs;

    ### Make sure we wound up where we started.
    my $original_wd = Cwd::cwd;

    ### so you didn't provide an explicit target.
    ### maybe your config can tell us what to do.
    $target ||= {
        PREREQ_ASK,     TARGET_INSTALL, # we'll bail out if the user says no
        PREREQ_BUILD,   TARGET_CREATE,
        PREREQ_IGNORE,  TARGET_IGNORE,
        PREREQ_INSTALL, TARGET_INSTALL,
    }->{ $conf->get_conf('prereqs') } || '';

    ### XXX BIG NASTY HACK XXX FIXME at some point.
    ### when installing Bundle::CPANPLUS::Dependencies, we want to
    ### install all packages matching 'cpanplus' to be installed last,
    ### as all CPANPLUS' prereqs are being installed as well, but are
    ### being loaded for bootstrapping purposes. This means CPANPLUS
    ### can find them, but for example cpanplus::dist::build won't,
    ### which gets messy FAST. So, here we sort our prereqs only IF
    ### the parent module is Bundle::CPANPLUS::Dependencies.
    ### Really, we would wnat some sort of sorted prereq mechanism,
    ### but Bundle:: doesn't support it, and we flatten everything
    ### to a hash internally. A sorted hash *might* do the trick if
    ### we got a transparent implementation.. that would mean we would
    ### just have to remove the 'sort' here, and all will be well
    my @sorted_prereqs;

    ### use regex, could either be a module name, or a package name
    if( $self->module =~ /^Bundle(::|-)CPANPLUS(::|-)Dependencies/ ) {
        my (@first, @last);
        for my $mod ( sort keys %$prereqs ) {
            $mod =~ /CPANPLUS/
                ? push @last,  $mod
                : push @first, $mod;
        }
        @sorted_prereqs = (@first, @last);
    } else {
        @sorted_prereqs = sort keys %$prereqs;
    }

    ### first, transfer this key/value pairing into a
    ### list of module objects + desired versions
    my @install_me;

    my $flag;

    for my $mod ( @sorted_prereqs ) {
        ( my $version = $prereqs->{$mod} ) =~ s#[^0-9\._]+##g;

        ### 'perl' is a special case, there's no mod object for it
        if( $mod eq PERL_CORE ) {

            unless( $cb->_vcmp( sprintf('v%vd',$^V), $version ) >= 0 ) {
                error(loc(  "Module '%1' needs perl version '%2', but you ".
                            "only have version '%3' -- can not proceed",
                            $self->module, $version,
                            $cb->_perl_version( perl => $^X ) ) );
                return;
            }

            next;
        }

        my $modobj  = $cb->module_tree($mod);

        #### XXX we ignore the version, and just assume that the latest
        #### version from cpan will meet your requirements... dodgy =/
        unless( $modobj ) {
            # Check if it is a core module
            my $sub = CPANPLUS::Module->can(
                        'module_is_supplied_with_perl_core' );
            my $core = $sub->( $mod );
            unless ( defined $core ) {
               error( loc( "No such module '%1' found on CPAN", $mod ) );
               $flag++ unless $tolerant;
               next;
            }
            if ( $cb->_vcmp( $version, $core ) > 0 ) {
               error(loc( "Version of core module '%1' ('%2') is too low for ".
                          "'%3' (needs '%4') -- carrying on but this may be a problem",
                          $mod, $core,
                          $self->module, $version ));
            }
            next;
        }

        ### it's not uptodate, we need to install it
        if( !$dist->prereq_satisfied(modobj => $modobj, version => $version)) {
            msg(loc("Module '%1' requires '%2' version '%3' to be installed ",
                    $self->module, $modobj->module, $version), $verbose );

            push @install_me, [$modobj, $version];

        ### it's not an MM or Build format, that means it's a package
        ### manager... we'll need to install it as well, via the PM
        } elsif ( INSTALL_VIA_PACKAGE_MANAGER->($format) and
                    !$modobj->package_is_perl_core and
                    ($target ne TARGET_IGNORE)
        ) {
            msg(loc("Module '%1' depends on '%2', may need to build a '%3' ".
                    "package for it as well", $self->module, $modobj->module,
                    $format));
            push @install_me, [$modobj, $version];
        }
    }



    ### so you just want to ignore prereqs? ###
    if( $target eq TARGET_IGNORE ) {

        ### but you have modules you need to install
        if( @install_me ) {
            msg(loc("Ignoring prereqs, this may mean your install will fail"),
                $verbose);
            msg(loc("'%1' listed the following dependencies:", $self->module),
                $verbose);

            for my $aref (@install_me) {
                my ($mod,$version) = @$aref;

                my $str = sprintf "\t%-35s %8s\n", $mod->module, $version;
                msg($str,$verbose);
            }

            return;

        ### ok, no problem, you have all needed prereqs anyway
        } else {
            return 1;
        }
    }

    for my $aref (@install_me) {
        my($modobj,$version) = @$aref;

        ### another prereq may have already installed this one...
        ### so dont ask again if the module turns out to be uptodate
        ### see bug [#11840]
        ### if either force or prereq_build are given, the prereq
        ### should be built anyway
        next if (!$force and !$prereq_build) &&
                $dist->prereq_satisfied(modobj => $modobj, version => $version);

        ### either we're told to ignore the prereq,
        ### or the user wants us to ask him
        if( ( $conf->get_conf('prereqs') == PREREQ_ASK and not
              $cb->_callbacks->install_prerequisite->($self, $modobj)
            )
        ) {
            msg(loc("Will not install prerequisite '%1' -- Note " .
                    "that the overall install may fail due to this",
                    $modobj->module), $verbose);
            next;
        }

        ### value set and false -- means failure ###
        if( defined $modobj->status->installed
            && !$modobj->status->installed
        ) {
            error( loc( "Prerequisite '%1' failed to install before in " .
                        "this session", $modobj->module ) );
            $flag++;
            last;
        }

        ### part of core?
        if( $modobj->package_is_perl_core ) {
            error(loc("Prerequisite '%1' is perl-core (%2) -- not ".
                      "installing that. -- Note that the overall ".
                      "install may fail due to this.",
                      $modobj->module, $modobj->package ) );
            next;
        }

        ### circular dependency code ###
        my $pending = $cb->_status->pending_prereqs || {};

        ### recursive dependency ###
        if ( $pending->{ $modobj->module } ) {
            error( loc( "Recursive dependency detected (%1) -- skipping",
                        $modobj->module ) );
            next;
        }

        ### register this dependency as pending ###
        $pending->{ $modobj->module } = $modobj;
        $cb->_status->pending_prereqs( $pending );

        ### call $modobj->install rather than doing
        ### CPANPLUS::Dist->new and the like ourselves,
        ### since ->install will take care of fetch &&
        ### extract as well
        my $pa = $dist->status->_prepare_args   || {};
        my $ca = $dist->status->_create_args    || {};
        my $ia = $dist->status->_install_args   || {};

        unless( $modobj->install(   %$pa, %$ca, %$ia,
                                    force   => $force,
                                    verbose => $verbose,
                                    format  => $format,
                                    target  => $target )
        ) {
            error(loc("Failed to install '%1' as prerequisite " .
                      "for '%2'", $modobj->module, $self->module ) );
            $flag++;
        }

        ### unregister the pending dependency ###
        $pending->{ $modobj->module } = 0;
        $cb->_status->pending_prereqs( $pending );

        last if $flag;

        ### don't want us to install? ###
        if( $target ne TARGET_INSTALL ) {
            my $dir = $modobj->status->extract
                        or error(loc("No extraction dir for '%1' found ".
                                     "-- weird", $modobj->module));

            $modobj->add_to_includepath();

            next;
        }
    }

    ### reset the $prereqs iterator, in case we bailed out early ###
    keys %$prereqs;

    ### chdir back to where we started
    $cb->_chdir( dir => $original_wd );

    return 1 unless $flag;
    return;
}

1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
