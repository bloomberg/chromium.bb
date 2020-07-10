#############################################################################
# Package       Win32::Exe::Manifest
# Description:  Handle Win32 PE Manifests
# Created       Mon Apr 19 17:15:24 2010
# SVN Id        $Id: Manifest.pm 2 2010-11-30 16:40:31Z mark.dootson $
# Copyright:    Copyright (c) 2010 Mark Dootson
# Licence:      This program is free software; you can redistribute it 
#               and/or modify it under the same terms as Perl itself
#############################################################################

package Win32::Exe::Manifest;

#############################################################################
# References
# http://msdn.microsoft.com/en-us/library/dd371711(VS.85).aspx
# http://msdn.microsoft.com/en-us/library/aa374191(VS.85).aspx
# http://msdn.microsoft.com/en-us/library/aa374219(v=VS.85).aspx
# http://msdn.microsoft.com/en-us/library/aa376607(v=VS.85).aspx
#----------------------------------------------------------------------------

use strict;
use warnings;
use Win32::Exe::Manifest::Parser;
use Exporter;
use base qw( Exporter );
use Carp;

our $VERSION = '0.15';

=head1 NAME

Win32::Exe::Manifest - MSWin Application and Assembly manifest handling

=head1 VERSION

This document describes version 0.15 of Win32::Exe::Manifest, released
November 30, 2010.

=head1 SYNOPSIS

    use Win32::Exe
    my $exe = Win32:::Exe->new('somefilepath');
    
    my $manifest = $exe->get_manifest if $exe->has_manifest;
    
    # Get themed controls
    $manifest->add_common_controls;
    
    # Change name
    $manifest->set_assembly_name('My.App.Name');
    
    # Require Admin permissions
    $manifest->set_execution_level('requireAdministrator');
    
    # write it out
    $exe->set_manifest($manifest);
    $exe->write;
    
    #roll your own
    use Win32::Exe::Manifest
    my $xml = $handmademanifest;
    ......
    my $manifest = Win32::Exe::Manifest->new($xml, 'application');
    $exe->set_manifest($manifest);
    $exe->write;
    
    #get formated $xml
    my $xml = $manifest->output;
    
    #try merge (experimental)
    
    my $mfest1 = $exe->get_manifest;
    my $mfest2 = Win32::Exe::Manifest->new($xml, 'application');
    
    $mfest1->merge_manifest($mfest2);
    
    $exe->set_manifest($mfest1);
    $exe->write;
    
    #add a dependency
    my $info =
    { type     => 'win32',
      name     => 'Dependency.Prog.Id',
      version  => 1.0.0.0,
      language => '*',
      processorArchitecture => '*',
      publicKeyToken => 'hjdajhahdsa7sadhaskl',
    };
    
    $manifest->add_dependency($info);
    $exe->set_manifest($manifest);
    $exe->write;



=head1 DESCRIPTION

This module parses and manipulates application and assembly
manifests used in MS Windows executables and DLLs. It is
part of the Win32::Exe distribution.

=head1 METHODS

=head2 Constructors

=head3 new

    Win32::Exe::Manifest->new($xml, $type);

Create a new manifest instance from the manifest xml in $xml. The
$type param is optional and may contain 'application' (the default),
or 'assembly'.

Manifest objects can also be created using Win32::Exe

    my $exe = Win32:::Exe->new('somefilepath');
    my $manifest = $exe->get_manifest if $exe->has_manifest;

=cut

sub new {
    my ($class, $xml, $type) = @_;
    my $self = bless {}, $class;
    $xml = $self->get_default_manifest() if !$xml;
    $type = 'application' if !$type;
    $type =~ /^(application|assembly)$/ or croak(qq(Invalid manifest type : $type : valid types are 'application', 'assembly'));
    $self->{_w32_exe_schema} = $self->get_default_schema;
    $self->{_w32_exe_datatype} = $type;
    $self->{_w32_exe_manifestid} = 1;
    my $handler = Win32::Exe::Manifest::Parser->new( $self->get_parser_config );
    while( $xml =~ s/""/"/g ) { carp qq(Manifest has badly formed value quotes); }
    $self->{_w32_exe_dataref} = $handler->xml_in( $xml );
    $self->_compress_schema();
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors: ' . $errors) if $errors;
    return $self;
}

=head2 Output

=head3 output

    my $xml = $manifest->output;

Returns a formated $xml string containing all edits and changes made
using Win32::Exe::Manifest


=cut

sub output {
    my $self = shift;
    my $handler = Win32::Exe::Manifest::Parser->new( $self->get_parser_config );
    my $ref = $self->refhash;
    $self->_expand_schema();
    my $xml = $handler->xml_out( $ref );
    $self->_compress_schema();
    return $xml;
}

=head2 Application Identity

=head3 set_assembly_name

    $manifest->set_assembly_name('My.Application.Name');

Set the application or assembly name. The name should take the form of a progid 
and should not include any spaces.


=head3 get_assembly_name

    my $appname = $manifest->get_assembly_name;

Return the assembly or application name from the manifest.


=cut

sub set_assembly_name {
    my($self, $name) = @_;
    $self->refhash()->{assembly}->[0]->{assemblyIdentity}->[0]->{name} = $name;
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following set name: ' . $errors) if $errors;
}

sub get_assembly_name {
    my($self) = @_;
    my $ref = $self->refhash();
    return (exists($ref->{assembly}->[0]->{assemblyIdentity}->[0]->{name})) ? $ref->{assembly}->[0]->{assemblyIdentity}->[0]->{name} : undef;
}

=head3 set_assembly_description

    $manifest->set_assembly_description('My Application Description');

Set the application description. The description is an informative string.


=head3 get_assembly_decription

    my $desc = $manifest->get_assembly_description;

Return the assembly description from the manifest.


=cut

sub set_assembly_description {
    my($self, $desc) = @_;
    my $valuename = $self->default_content;
    $self->refhash()->{assembly}->[0]->{description}->[0]->{$valuename} = $desc;
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following set description: ' . $errors) if $errors;
}

sub get_assembly_description {
    my($self) = @_;
    my $valuename = $self->default_content;
    my $ref = $self->refhash();
    return (exists($ref->{assembly}->[0]->{description}->[0]->{$valuename})) ? $ref->{assembly}->[0]->{description}->[0]->{$valuename} : undef;
}

=head3 set_assembly_version

    $manifest->set_assembly_version('1.7.8.34456');

Set the application or assembly version. The version should take the form of
'n.n.n.n' where each n is a number between 0-65535 inclusive.


=head3 get_assembly_version

    my $version = $manifest->get_assembly_version;

Return the assembly or application version from the manifest.


=cut

sub set_assembly_version {
    my($self, $version) = @_;
    $self->refhash()->{assembly}->[0]->{assemblyIdentity}->[0]->{version} = $version;
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following set version: ' . $errors) if $errors;
}

sub get_assembly_version {
    my($self) = @_;
    my $ref = $self->refhash();
    return (exists($ref->{assembly}->[0]->{assemblyIdentity}->[0]->{version})) ? $ref->{assembly}->[0]->{assemblyIdentity}->[0]->{version} : undef;
}

=head3 set_assembly_language

    $manifest->set_assembly_language($langid);

Set the application or assembly language. The language id is the 
DHTML language code. If you want to set 'language neutral' then pass
'*' for the value.

see : L<http://msdn.microsoft.com/en-us/library/ms533052(VS.85).aspx>


=head3 get_assembly_language

    my $langid = $manifest->get_assembly_language;

Return the assembly or application language from the manifest. If there
is no language id in the manifest, the method will return '*' 


=cut

sub set_assembly_language {
    my($self, $newval) = @_;
    $self->refhash()->{assembly}->[0]->{assemblyIdentity}->[0]->{language} = $newval;
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following set language: ' . $errors) if $errors;
}

sub get_assembly_language {
    my($self) = @_;
    my $ref = $self->refhash();
    return (exists($ref->{assembly}->[0]->{assemblyIdentity}->[0]->{language})) ? $ref->{assembly}->[0]->{assemblyIdentity}->[0]->{language} : '*';
}

=head3 set_assembly_architecture

    $manifest->set_assembly_architecture($arch);

Set the application or assembly architecture. Accepted values are :
x86 msil ia64 amd64 *. Note the lowercase format. If you want your manifest
to be architecture neutral, set architecture to '*'.


=head3 get_assembly_architecture

    my $arch = $manifest->get_assembly_architecture;

Return the assembly or application architecture from the manifest.


=cut

sub set_assembly_architecture {
    my($self, $newval) = @_;
    $self->refhash()->{assembly}->[0]->{assemblyIdentity}->[0]->{processorArchitecture} = $newval;
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following set processorArchitecture: ' . $errors) if $errors;
}

sub get_assembly_architecture {
    my($self) = @_;
    my $ref = $self->refhash();
    return (exists($ref->{assembly}->[0]->{assemblyIdentity}->[0]->{processorArchitecture})) ? $ref->{assembly}->[0]->{assemblyIdentity}->[0]->{processorArchitecture} : '*';
}

=head2 Trust and Security

=head3 set_execution_level

    $manifest->set_execution_level($level);

Set the application execution level. Accepted values are : asInvoker,
highestAvailable, requireAdministrator, none. If you pass the value
'none', any trustInfo section will be removed from the manifest.

See L<http://msdn.microsoft.com/en-us/library/bb756929.aspx>


=head3 get_execution_level

    my $level = $manifest->get_execution_level;

Return the application execution level.


=cut

sub set_execution_level {
    my($self, $level) = @_;
    $level =~ /^(asInvoker|requireAdministrator|highestAvailable|none)$/
        or croak(qq(Invalid exec level '$level'. Valid levels are 'asInvoker', 'requireAdministrator', 'highestAvailable', 'none'));
    
    my $ref = $self->refhash()->{assembly}->[0];
    my $schema = $self->get_current_schema;
    my $xmlns = $schema->{elementtypes}->{trustInfo}->{attributes}->{xmlns}->{default};
    
    if($level eq 'none') {
        # delete element and collapse tree if empty
        $self->_delete_collapse_tree($ref, [ qw(trustInfo security requestedPrivileges requestedExecutionLevel) ] );
    } else {
        # create value if it does not exist
        my $writeref = $self->_get_first_tree_node($ref, [ qw(trustInfo security requestedPrivileges requestedExecutionLevel)  ], undef );
        $writeref->{level} = $level;
        $writeref->{uiAccess} = 'false' if not exists($writeref->{uiAccess});
    }
    
    # in all cases, dump any namespace definitions - which cause crashes on some OS / namespace combos
    # and fix trustInfo namespace decl
    
    $self->_fixup_namespace( $ref, 'trustInfo', $xmlns );
    
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following set execution level: ' . $errors) if $errors;
}

sub get_execution_level {
    my $self = shift;
    my $ref = $self->refhash()->{assembly}->[0];
    return 'none' if not exists($ref->{trustInfo}->[0]->{security}->[0]->{requestedPrivileges}->[0]->{requestedExecutionLevel}->[0]->{level});
    return $ref->{trustInfo}->[0]->{security}->[0]->{requestedPrivileges}->[0]->{requestedExecutionLevel}->[0]->{level};
}

=head3 set_uiaccess

    $manifest->set_uiaccess($needed);

Set the application uiAccess requirement in the trustInfo manifest section.
Accepted values are 'true', 'false'. If no trustInfo section exists, one is
created with the execution level set to 'asInvoker'.

See L<http://msdn.microsoft.com/en-us/library/bb756929.aspx>


=head3 get_uiaccess

    my $accessneeded = $manifest->get_uiaccess;

Return the uiAccess setting from the trustInfo structure. If no trustInfo
structure exists, method returns undef.


=cut

sub set_uiaccess {
    my ($self, $access) = @_;
    $access =~ /^(true|false)$/
        or croak(qq(Invalid uiAccess setting '$access'. Valid settings are 'true', 'false'));
    
    my $ref = $self->refhash()->{assembly}->[0];
    if(exists($ref->{trustInfo}->[0]->{security}->[0]->{requestedPrivileges}->[0]->{requestedExecutionLevel}->[0]->{uiAccess})) {
        $ref->{trustInfo}->[0]->{security}->[0]->{requestedPrivileges}->[0]->{requestedExecutionLevel}->[0]->{uiAccess} = $access;
    } else {
        my $schema = $self->get_current_schema;
        my $xmlns = $schema->{elementtypes}->{trustInfo}->{attributes}->{xmlns}->{default};
        $ref->{trustInfo} = [ { xmlns => $xmlns, security => [ { requestedPrivileges => [ { requestedExecutionLevel => [ { level => 'asInvoker', uiAccess => $access  } ] } ] } ] } ];
    }
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following set uiAccess: ' . $errors) if $errors;
}

sub get_uiaccess {
    my $self = shift;
    my $ref = $self->refhash()->{assembly}->[0];
    return undef if not exists($ref->{trustInfo}->[0]->{security}->[0]->{requestedPrivileges}->[0]->{requestedExecutionLevel});
    return $ref->{trustInfo}->[0]->{security}->[0]->{requestedPrivileges}->[0]->{requestedExecutionLevel}->[0]->{uiAccess};
}

=head2 Application Dependencies

=head3 set_resource_id

    $manifest->set_resource_id($id);

Set the resource Id for the manifest. Valid id's are 1, 2 and 3. The default
is 1. Don't set this unless you are fully aware of the effects.

See L<http://msdn.microsoft.com/en-us/library/aa376607(VS.85).aspx>


=head3 get_resource_id

    my $id = $manifest->get_resource_id();

Return the resource Id for the manifest.

=cut

sub get_resource_id { $_[0]->{_w32_exe_manifestid} }

sub set_resource_id {
    my ($self, $id) = @_;
    $id =~ /^(1|2|3)$/ or croak(qq(invalid manifest resource id $id. Valid values are 1,2,3));
    $self->{_w32_exe_manifestid} = $id;
}

=head3 add_common_controls

    $manifest->add_common_controls();

Add a dependency on minimum version 6.0.0.0 of the Microsoft.Windows.Common-Controls shared
library. This is normally done with GUI applications to use themed controls on Windows XP
and above.


=cut

sub add_common_controls { $_[0]->add_template_dependency('Microsoft.Windows.Common-Controls'); }

sub add_template_dependency {
    my($self, $assemblyname) = @_;
    my $tmpl = $self->get_dependency_template($assemblyname);
    croak(qq(No template found for dependency $assemblyname)) if !$tmpl;
    $self->add_dependency($tmpl);
}

=head3 add_dependency

    $manifest->add_dependency($info);

Add a dependency on the assembly detailed in the $info hash reference. The contents of
$info should be of the form:

    my $info = { type     => 'win32',
                 name     => 'Dependency.Prog.Id',
                 version  => 1.0.0.0,
                 language => '*',
                 processorArchitecture => '*',
                 publicKeyToken => 'hjdajhahdsa7sadhaskl',
    };

Note that the version should be the least specific that your application requires. For
example, a version of '2.0.0.0' would mean the system loads the first matching
assembly it finds with a version of at least '2.0.0.0'.

See: L<http://msdn.microsoft.com/en-us/library/aa374219(v=VS.85).aspx>


=cut

sub add_dependency {
    my($self, $proto) = @_;
    my $name = $proto->{name};
    $proto->{type} = 'win32';
    $proto->{version} ||= '0.0.0.0';
    $proto->{language} ||= '*';
    $proto->{processorArchitecture} ||= '*';
    my %vals = %$proto;
    if( defined(my $depindex = $self->_get_dependendency_index($name)) ) {
        $self->refhash()->{assembly}->[0]->{dependency}->[$depindex]->{dependentAssembly}->[0]->{assemblyIdentity}->[0] =  \%vals;
    } else {
        $self->refhash()->{assembly}->[0]->{dependency} = [] if not exists $self->refhash()->{assembly}->[0]->{dependency};
        my $newdep = { dependentAssembly => [ { assemblyIdentity => [ \%vals ] } ] };
        push @{ $self->refhash()->{assembly}->[0]->{dependency} } , $newdep;
    }
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following dependency addition: ' . $errors) if $errors;
}

=head3 remove_dependency

    $manifest->remove_dependency($progid);

Remove a dependency with the $progid. For example, passing a $progid of
'Microsoft.Windows.Common-Controls' will remove the dependency added via
'add_common_controls' from the manifest.


=cut

sub remove_dependency {
    my($self, $depname) = @_;
    if( defined(my $depindex = $self->_get_dependendency_index($depname)) ) {
        my $ref = $self->refhash()->{assembly}->[0]->{dependency};
        my @depends = @$ref;
        splice(@depends, $depindex, 1);
        $self->refhash()->{assembly}->[0]->{dependency} = \@depends;
        return 1;
    } else {
        return 0;
    }
}

=head3 get_dependency

    my $info = $manifest->get_dependency($progid);

Return a dependency info hash for a dependency in the manifest with the 'name'
$progid. The info hash is a reference to a hash with the format:

    { type     => 'win32',
      name     => 'Dependency.Prog.Id',
      version  => 1.0.0.0,
      language => '*',
      processorArchitecture => '*',
      publicKeyToken => 'hjdajhahdsa7sadhaskl',
    };

If there is no dependency with the name $progid, returns undef.


=cut

sub get_dependency {
    my($self, $depname) = @_;
    if( defined(my $depindex = $self->_get_dependendency_index($depname)) ) {
        my $ref = $self->refhash()->{assembly}->[0]->{dependency}->[$depindex]->{dependentAssembly}->[0]->{assemblyIdentity}->[0];
        my %vals = %$ref;
        return \%vals;
    } else {
        return undef;
    }
}


=head3 get_dependencies

    my @deps = $manifest->get_dependencies($progid);

Return an array of hash references, one for each dependency in the manifest.
Each member is a reference to a hash with the format:

    { type     => 'win32',
      name     => 'Dependency.Prog.Id',
      version  => 1.0.0.0,
      language => '*',
      processorArchitecture => '*',
      publicKeyToken => 'hjdajhahdsa7sadhaskl',
    };

If there are no dependencies, returns an empty array.


=cut

sub get_dependencies {
    my $self = shift;
    my @depends = ();
    return (@depends) if not exists $self->refhash()->{assembly}->[0]->{dependency};
    for my $dependency ( @{ $self->refhash()->{assembly}->[0]->{dependency} } ) {
        my $dep = $dependency->{dependentAssembly}->[0]->{assemblyIdentity}->[0];
        my %vals = %$dep;
        push(@depends, \%vals);
    }
    return ( @depends );
}


=head2 Compatibility Settings


=head3 set_compatibility

    $manifest->set_compatibility( ('Windows Vista') );

Set the operating system feature compatibility flags. Parameter is a list of operating
systems that the application targets. In addition to the opertating system identifier
keys, this method also accepts the shorthand strings 'Windows Vista' and 'Windows 7'.

See : L<http://msdn.microsoft.com/en-us/library/dd371711(VS.85).aspx>


=head3 get_compatibility

    my @osids = $manifest->get_compatibility();

Returns a list of operating system identifier keys that the manifest notes as targetted
operating systems. You can convert these os ids to the shorthand strings 'Windows Vista'
and 'Windows 7' using the method

    my $shortstring = $manifest->get_osname_from_osid($osid);

There is a reverse method

    my $osid = $manifest->get_osid_from_osname($shortstring);

NOTE: Don't set this unless you fully understand the effects.

See : L<http://msdn.microsoft.com/en-us/library/dd371711(VS.85).aspx>


=cut

sub set_compatibility {
    my($self, @vals) = @_;
    return if ! scalar(@vals);
    my @osblock = ();
    for my $val ( @vals ) {
        my $compat = $val;
        $compat = '{e2011457-1546-43c5-a5fe-008deee3d3f0}' if(lc($compat) eq 'windows vista');
        $compat = '{35138b9a-5d96-4fbd-8e2d-a2440225f93a}' if(lc($compat) eq 'windows 7');
        croak(qq(Invalid OS key '$val' for compatibility)) if !Win32::Exe::Manifest::Parser->validate_osid($compat);
        push(@osblock, { Id => $compat });
    }
    my $ref = $self->refhash()->{assembly}->[0];
    my $schema = $self->get_current_schema;
    my $xmlns = $schema->{elementtypes}->{compatibility}->{attributes}->{xmlns}->{default};
    $ref->{compatibility} = [ { xmlns => $xmlns, application => [ { supportedOS => \@osblock, }, ], }, ];
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following set compatibility: ' . $errors) if $errors;
}

sub get_compatibility {
    my $self = shift;
    my @compats = ();
    my $ref = $self->refhash()->{assembly}->[0];
    if(exists($ref->{compatibility})) {
        my @oids = @{ $ref->{compatibility}->[0]->{application}->[0]->{supportedOS} };
        for my $oidref ( @oids ) {
            my $oskey = $oidref->{Id};
            push(@compats, $oskey);
        }
    }
    return (@compats);
}

=head3 set_dpiaware

    $manifest->set_dpdaware( 'true' );

Set section in the manifest if the application is dpi aware. Accepts values
true, false, and none. If the value 'none' is passed, the application\windowsSettings
section is removed from the manifest entirely.

See : L<http://msdn.microsoft.com/en-us/library/ee308410(VS.85).aspx>


=head3 get_dpiaware

    $manifest->set_dpdaware( 'true' );

Return the dpiAware setting from the manifest, if any. If there is no setting,
the method returns undef.

See : L<http://msdn.microsoft.com/en-us/library/ee308410(VS.85).aspx>


=cut

sub set_dpiaware {
    my ($self, $setval) = @_;
    
    $setval =~ /^(true|false|none)$/i or croak(qq(Invalid value for set_dpiaware '$setval'. Valid values are true:false:none));
    $setval = lc($setval);
    my $ref = $self->refhash()->{assembly}->[0];
    my $elementvaluename = $self->default_content;
    my $schema = $self->get_current_schema;
    my $namealias = $schema->{namespace}->{'urn:schemas-microsoft-com:asm.v3'};
    
    # handle remove value
    if($setval eq 'none') {
        # delete element and collapse tree if empty
        $self->_delete_collapse_tree($ref, [ qw(application windowSettings dpiAware) ] );
    } else {
        # create value if it does not exist
        my $writeref = $self->_get_first_tree_node($ref, [ qw(application windowsSettings)  ], $namealias );
        $writeref->{dpiAware} = [ { $elementvaluename => $setval } ];
        # add xmlns for windowsSettings
        my $xmlns = $schema->{elementtypes}->{windowsSettings}->{attributes}->{xmlns}->{default};
        $ref->{application}->[0]->{windowsSettings}->[0]->{xmlns} = $xmlns;
    }
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors after dpiAware setting: ' . $errors) if $errors;
}

sub get_dpiaware {
    my $self = shift;
    my $ref = $self->refhash()->{assembly}->[0];
    my $elementvaluename = $self->default_content;
    return 'none' if(not exists($ref->{application}->[0]->{windowsSettings}->[0]->{dpiAware}->[0]->{$elementvaluename}));
    return $ref->{application}->[0]->{windowsSettings}->[0]->{dpiAware}->[0]->{$elementvaluename};
}

=head2 Manifest Information

=head3 get_manifest_type

    my $type = $manifest->get_manifest_type;

Returns the manifest type ( 'application' or 'assembly' );


=cut

sub get_manifest_type { $_[0]->{_w32_exe_datatype}; }


sub get_current_schema { $_[0]->{_w32_exe_schema}; }


sub set_current_schema {
    my($self, $schema) = @_;
    $self->{_w32_exe_schema} = $schema;
    my $errors = $self->validate_errors;
    croak('Manifest XML had errors following set schema: ' . $errors) if $errors;
}

sub default_xmldecl { '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' }

sub default_content { 'elementValue' }

sub get_parser_config {
    my $self = shift;
    my %config = (
        KeepRoot   => 1,
        ForceArray => 1,
        KeyAttr => {},
        XMLDecl => $self->default_xmldecl,
        ForceContent => 1,
        ContentKey => $self->default_content,
        # AttrIndent => 1,
    );
    return %config;
}

sub refhash { $_[0]->{_w32_exe_dataref}; }


sub merge_manifest {
    my($self, $xml) = @_;
    my $mergefest;
    my $class = ref($self);
    eval { $mergefest = $class->new($xml, $self->get_manifest_type); };
    croak(qq(Merging $@)) if $@;
    my $newref = $mergefest->refhash()->{assembly}->[0];
    my $oldref = $self->refhash()->{assembly}->[0];
    $self->_merge_element_merge('assembly', $oldref, $newref);
    my $errors = $self->validate_errors;
    croak('Manifest had errors after merging: ' . $errors) if $errors;
}


sub get_osname_from_osid {
    my($self, $osid) = @_;
    my $name = 'Unknown Windows Version';
    $name = 'Windows Vista' if $osid =~ /e2011457-1546-43c5-a5fe-008deee3d3f0/;
    $name = 'Windows 7' if $osid =~ /35138b9a-5d96-4fbd-8e2d-a2440225f93a/;
    return $name;
}

sub get_osid_from_osname {
    my($self, $name) = @_;
    my $osid = undef;
    $osid = '{e2011457-1546-43c5-a5fe-008deee3d3f0}' if(lc($name) eq 'windows vista');
    $osid = '{35138b9a-5d96-4fbd-8e2d-a2440225f93a}' if(lc($name) eq 'windows 7');
    return $osid;
}

sub _get_dependendency_index {
    my($self, $name) = @_;
    my $exref = $self->refhash()->{assembly}->[0];
    return undef if not exists $exref->{dependency};
    my $rval = undef;
    my @existing = @{ $exref->{dependency} };
    for(my $i = 0; $i < @existing; $i++) {
        my $depname = $existing[$i]->{dependentAssembly}->[0]->{assemblyIdentity}->[0]->{name};
        if($depname eq $name) {
            $rval = $i;
            last;
        }
    }
    return $rval;
}

sub _get_first_tree_node {
    my ($self, $ref, $paths, $namealias ) = @_;
    my $newref = $ref;
    while(my $path = shift(@$paths) ) {
        $newref->{$path} = [{}]  if(not exists($newref->{$path}));
        $newref = $newref->{$path}->[0];
        $newref->{elementnamespace} = $namealias if $namealias;
    }
    return $newref;
}

sub _delete_collapse_tree {
    my ($self, $ref, $paths ) = @_;
    my $rootkey = $paths->[0];
    my $deletekey = pop(@$paths);
    my $delref = $ref;
    my $dodelete = 1;
    while(my $checkpath = shift @$paths) {
        if(exists($delref->{$checkpath}->[0])) {
            $delref = $delref->{$checkpath}->[0];
        } else {
            $dodelete = 0;
            last;
        }
    }
    delete($delref->{$deletekey}) if($dodelete && exists($delref->{$deletekey}));
    delete($delref->{elementnamespace}) if($dodelete && exists($delref->{elementnamespace}));
    $self->_delete_if_empty($ref, $rootkey);
}

sub _merge_element_merge {
    my($self, $elementname, $exref, $mergeref) = @_;
    my $schema = $self->get_current_schema;
    my $elementvaluename = $self->default_content;
    my $elementdef = $schema->{elementtypes}->{$elementname};
    
    my @keynames = (sort keys(%$mergeref));
    my %elementnames = ();
    my %attributenames = ();
    my $valuepresent = 0;
    
    for my $kname(@keynames) {
        if( $kname eq $elementvaluename ) {
            $valuepresent = 1;
        } elsif(ref($mergeref->{$kname})) {
            $elementnames{$kname} = 1;
        } else {
            $attributenames{$kname} = 1;
        }
    }
    
    # merge value
    if($valuepresent) {
        $exref->{$elementvaluename} = $mergeref->{$elementvaluename};
    }
    
    # merge attributes
    for my $aname (sort keys( %attributenames )) {
        $exref->{$aname} = $mergeref->{$aname};
    }
    
    # merge elements
    for my $ename (sort keys( %elementnames )) {
        if(not exists($exref->{$ename})) {
            # simple addition
            $exref->{$ename} = $mergeref->{$ename};
            next;
        }
        if($ename eq 'dependency') {
            # handle dependency merge
            $self->_merge_dependencies( $exref, $mergeref );
            next;
        }
        my $maxallowed = $elementdef->{elements}->{$ename}->{max};
        if($maxallowed == 1) {
            $self->_merge_element_merge($ename, $exref->{$ename}->[0], $mergeref->{$ename}->[0]);
        } else {
            push(@{ $exref->{$ename} }, @{ $mergeref->{$ename} });
        }
    }
}

sub _merge_dependencies {
    my($self, $exref, $mergeref) = @_;
    my %existingdepends = ();
    my %mergingdepends = ();
    
    my @existing = @{ $exref->{dependency} };
    my @merging = @{ $mergeref->{dependency} };
    my @merged = ();
    
    for(my $i = 0; $i < @existing; $i++) {
        my $depname = $existing[$i]->{dependentAssembly}->[0]->{assemblyIdentity}->[0]->{name};
        $existingdepends{$depname} = $i;
    }
    
    for(my $i = 0; $i < @merging; $i++) {
        my $depname = $merging[$i]->{dependentAssembly}->[0]->{assemblyIdentity}->[0]->{name};
        $mergingdepends{$depname} = $i;
    }
    
    foreach my $classname (sort keys(%existingdepends)) {
        push(@merged, $existing[$existingdepends{$classname}]) if not exists $mergingdepends{$classname}
    }
    
    push(@merged, @merging);
    $exref->{dependency} = \@merged;
}

sub validate_errors {
    my $self = shift;
    eval { $self->validate_data; };
    return ($@) ? $@ : undef;
}

sub _fixup_namespace {
    my( $self, $ref, $keyname, $xmlns ) = @_;
    for my $element( @{ $ref->{$keyname} } ) {
        $element->{xmlns} = $xmlns if $xmlns;
        delete($element->{elementnamespace}) if(exists($element->{elementnamespace}));
        my @keynames = (sort keys(%$element));
        for my $subkeyname( @keynames ) {
            if(ref($element->{$subkeyname})) {
                # a sub-element
                $self->_fixup_namespace($element, $subkeyname, undef );
            }
        }
    }
}

sub _delete_if_empty {
    my($self, $ref, $refname) = @_;
    
    # this element array is empty if it only contains namespace attributes
    # and all it's elements are similarly empty;
    
    my $candelete = 1;
    for my $element ( @{ $ref->{$refname} }) {
        my @keynames = (sort keys(%$element));
        for my $keyname( @keynames ) {
            next if $keyname eq 'elementnamespace';
            next if $keyname =~ /^xmlns/;
            my $value = $element->{$keyname};
            if(ref($value)) {
                # this is an element array
                $candelete = 0 if !$self->_delete_if_empty($element, $keyname);
            } else {
                # we have a none-namespace attribute
                $candelete = 0;
            }
        }
    }

    delete($ref->{$refname}) if $candelete;
    return $candelete;
}

sub _compress_schema {
    my $self = shift;
    my $ref = $self->refhash;
    $self->_compress_element_reference($ref);
}

sub _compress_element_reference {
    my($self, $hashref) = @_;
    my $schema = $self->get_current_schema;
       
    my @attributes;
    my @elements;
    
    for my $keyname ( sort keys(%$hashref) ) {
        my $value = $hashref->{$keyname};
        if(ref($value)) {
            push(@elements, $keyname);
        } else {
            push(@attributes, $keyname);
        }
    }
    
    for my $keyname ( @attributes ) {
        #next if $keyname eq 'elementnamespace'; # - should never appear
        my $value = $hashref->{$keyname};
        if($keyname =~ /^xmlns:(.+)$/) {
            # this is a namespace declaration
            # record it in the current schema
            # and delete it from the record
            my $replacerequired = 0;
            my $namespacesname = $1;
            if(exists($schema->{namespace}->{$value})) {
                # namespace exists
                my $alias = $schema->{namespace}->{$value};
                if($namespacesname ne $alias) {
                    $schema->{nstranslation}->{$namespacesname} = $alias;
                    $namespacesname = $alias;
                    $replacerequired = 1;
                }
            } else {
                # make sure namespace name is unique before adding values
                while(exists($schema->{nstranslation}->{$namespacesname})) {
                    $namespacesname .= 'n';
                }
                $schema->{namespace}->{$value} = $namespacesname;
                $schema->{nstranslation}->{$namespacesname} = $namespacesname;
                $replacerequired = 1;
            }
            # delete the declaration
            delete($hashref->{$keyname});
        }
    }
    
    for my $keyname ( @elements ) {
        my $value = $hashref->{$keyname};
        if($keyname =~ /^(.+):(.+)$/) {
            # element has a namespace
            my $namespace = $1;
            my $shortname = $2;
            my $nsalias = $schema->{nstranslation}->{$namespace} || $namespace;
            $hashref->{$shortname} = $value;
            delete($hashref->{$keyname});
            
            $_->{elementnamespace} = $nsalias for( @$value );
            
        }
        $self->_compress_element_reference($_) for ( @$value );
    }
}

sub _expand_schema {
    my $self = shift;
    $self->{parser}->{namespaces} = {};
    my $ref = $self->refhash;
    $self->_expand_element_reference($ref);
    # add namespace keys to
    my $assembly = $ref->{assembly}->[0];
    if($assembly) {
        foreach my $key (sort keys(%{ $self->{parser}->{namespaces} })) {
            my $xmlns = $self->{parser}->{namespaces}->{$key};
            my $xmlnsname = qq(xmlns:$key);
            $assembly->{$xmlnsname} = $xmlns;
        }
    }
    $self->{parser}->{namespaces} = {};
}

sub _expand_element_reference {
    # prepend saved namespace prefixes to elements
    my($self, $hashref) = @_;
    my $schema = $self->get_current_schema;
    my @keynames = ( sort keys(%$hashref) );
    for my $keyname ( @keynames  ) {
        my $value = $hashref->{$keyname};
        if(ref($value)) { # an array
            # get namespace if any
            my $namespace = undef;
            for my $element(@$value) {
                if(exists($element->{elementnamespace})) {
                    $namespace = $element->{elementnamespace};
                    delete($element->{elementnamespace});
                }
            }
            if($namespace) {
                my $newname = qq($namespace:$keyname);
                $hashref->{$newname} = $value;
                delete($hashref->{$keyname});
                my $xmlns = $schema->{namespacelookup}->{$namespace};
                $self->{parser}->{namespaces}->{$namespace} = $xmlns;
            }
            $self->_expand_element_reference($_) for ( @$value );
        }
    }
}

sub validate_data {
    my($self) = @_;
    # top level element must be assembly
    # grab that and validate onwards
    
    my $schema = $self->get_current_schema;
    my $ref = $self->refhash;
    my @levels = sort keys(%$ref);
    croak('Too many top level elements : ' . join(', ', @levels)) if (scalar(@levels) != 1);
    croak(qq(Unexpected top level element $levels[0])) if $levels[0] ne 'assembly';
    
    my $elementname = 'assembly';
    my $elementdef = exists($schema->{elementtypes}->{$elementname}) ? $schema->{elementtypes}->{$elementname} : undef;
    croak(qq(no definition found for element type $elementname)) if !defined($elementdef);
    my $min = 1;
    my $max = 1;
    my $raw = $ref->{assembly};
    $self->validate_element_array('assembly', $min, $max, $raw);
}

sub validate_element_array {
    my($self, $elementname, $min, $max, $ref) = @_;
    
    my $schema = $self->get_current_schema;
    my $elementvaluename = $self->default_content;
    
    # get element definition
    my $elementdef = exists($schema->{elementtypes}->{$elementname}) ? $schema->{elementtypes}->{$elementname} : undef;
    croak(qq(no definition found for element type $elementname)) if !defined($elementdef);
    
    # check if this manifest type accepts element type
    my $currenttype = $self->get_manifest_type;
    my $exclusive = $elementdef->{exclusive};
    if(($exclusive ne 'none') && ($currenttype ne $exclusive)) {
        croak(qq(element type $elementname cannot appear in $currenttype manifests));
    }
    
    # check that numbers are OK
    my $numelements = scalar @$ref;
    croak (qq(not enough $elementname elements - count = $numelements - minimum = $min)) if(($min > 0) && ($numelements < $min));
    croak (qq(too many $elementname elements - count = $numelements - maximum = $max)) if(($max > 0) && ($numelements > $max));
    
    # validate each element item
    $self->validate_element($elementname, $elementdef, $_) for (@$ref);
}

sub validate_element {
    my($self, $ename, $elementdef, $ref) = @_;
    
    # ref contains elements, and attributes
    # any value is present as attribute 'elementvalue'
    # elements are arrayrefs
    # attributes are values
    
    my $schema = $self->get_current_schema;
    my $elementvaluename = $self->default_content;
    
    my @keynames = (sort keys(%$ref));
    my %elementnames = ();
    my %attributenames = ();
    my $valuepresent = 0;
    
    for my $kname(@keynames) {
        next if $kname eq 'elementnamespace';
        if( $kname eq $elementvaluename ) {
            $valuepresent = 1;
        } elsif(ref($ref->{$kname})) {
            if(not exists($elementdef->{elements}->{$kname})) {
                croak(qq(Unexpected element $kname in $ename));
            }
            $elementnames{$kname} = 1;
        } else {
            # allow no value namespace attributes to pass
            next if $kname =~ /^xmlns:.+$/;
            next if $kname eq 'xmlns';
            if(not exists($elementdef->{attributes}->{$kname})) {
                croak(qq(Unexpected attribute $kname in $ename));
            }
            $attributenames{$kname} = 1;
        }
    }
    
    # check we have all required elements
    foreach my $ekey (sort keys(%{ $elementdef->{elements} })) {
        if( $elementdef->{elements}->{$ekey}->{min} > 0 ) {
            croak qq(required element $ekey not found) if !exists($elementnames{$ekey});
        }
    }
    # check we have all required attributes
    foreach my $akey (sort keys(%{ $elementdef->{attributes} })) {
        next if $akey eq 'xmlns';
        if( $elementdef->{attributes}->{$akey}->{required} == 1 ) {
            croak qq(required attribute $akey not found) if !exists($attributenames{$akey});
        }
    }
    # check we have correct value / novalue
    my $valuerequired = $elementdef->{content}->{value};
    if($valuerequired != $valuepresent) {
        croak qq( unexpected value in $ename  - value required = $valuerequired : value present = $valuepresent);
    }
    
    # validate any value
    if($valuerequired) {
        my $validator = $elementdef->{value_validator};
        Win32::Exe::Manifest::Parser->$validator($ref->{$elementvaluename})
            or croak(qq(Invalid Value $ref->{$elementvaluename} for element $ename));
    }
    
    # validate attributes
    foreach my $aname (sort keys( %attributenames ) ) {
        next if $aname eq 'xmlns';
        my $avalue = $ref->{$aname};
        my $validator = $schema->{attributes}->{$aname};
        Win32::Exe::Manifest::Parser->$validator($avalue) or croak(qq(Invalid value $avalue for attribute $aname in $ename));
    }
    
    # validate elements
    foreach my $sub_ename (sort keys( %elementnames ) ) {
        my $elearryref = $ref->{$sub_ename};
        my $min = $elementdef->{elements}->{$sub_ename}->{min};
        my $max = $elementdef->{elements}->{$sub_ename}->{max};
        $self->validate_element_array($sub_ename, $min, $max, $elearryref);
    }
}

sub get_dependency_template {
    my ($self, $name) = @_;
    my $dependencytemplates = $self->get_dependency_template_hash;
    if(exists($dependencytemplates->{$name})) {
        return $dependencytemplates->{$name};
    } else {
        return undef;
    }
}

sub get_dependency_template_hash {
    my $self = shift;
    my $dependencytemplates = {
        'Microsoft.Windows.Common-Controls' => {
            name => 'Microsoft.Windows.Common-Controls',
            type => 'win32',
            version => '6.0.0.0',
            publicKeyToken => '6595b64144ccf1df',
            language => '*',
            processorArchitecture => '*',
        },
    };
    return $dependencytemplates;
}

sub get_default_manifest { Win32::Exe::Manifest::Parser->get_default_manifest(); }

sub get_default_schema { Win32::Exe::Manifest::Parser->get_default_schema(); }


1;

__END__

=head1 SEE ALSO

Modules that use Win32::Exe::Manifest

Win32::Exe

=head1 AUTHORS

Mark Dootson E<lt>mdootson@cpan.orgE<gt>

=head1 COPYRIGHT & LICENSE

Copyright 2010 by Mark Dootson E<lt>mdootson@cpan.orgE<gt>

This program is free software; you can redistribute it and/or 
modify it under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut

