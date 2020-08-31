#########################################################################################
# Package       Win32::Exe::Manifest::Parser
# Description:  XML Parser for Manifests
# Created       Wed Apr 21 07:54:51 2010
# SVN Id        $Id: Parser.pm 2 2010-11-30 16:40:31Z mark.dootson $
# Copyright:    Copyright (c) 2010 Mark Dootson
# Licence:      This program is free software; you can redistribute it 
#               and/or modify it under the same terms as Perl itself
#########################################################################################

package Win32::Exe::Manifest::Parser;

#########################################################################################

use strict;
use warnings;
use XML::Simple 2.18;
use base qw( XML::Simple );
use Carp;

our $VERSION = '0.15';

=head1 NAME

Win32::Exe::Manifest::Parser - MSWin Application and Assembly manifest handling

=head1 VERSION

This document describes version 0.15 of Win32::Exe::Manifest::Parser, released
November 30, 2010. 

=head1 DESCRIPTION

This is an internal module from the Win32::Exe distribution supporting
parsing of application and assembly manifests.

=cut


our $BASESCHEMA;

$XML::Simple::PREFERRED_PARSER = 'XML::Parser';

sub new {
    my $class = shift;
    my $self = $class->SUPER::new(@_);
    return $self;
}

sub get_current_schema { $_[0]->{_w32x_current_schema} || $BASESCHEMA; }
sub set_current_schema { $_[0]->{_w32x_current_schema} = $_[1]; }

#---------------------------------------------------------------
# override XML Simple methods
#---------------------------------------------------------------

sub sorted_keys {
    my($self, $name, $hashref) = @_;
    my @unsorted = (sort keys(%$hashref));
    my $schema = $self->get_current_schema();
    my @sorted = sort { _get_key_ordinal($name,$hashref,$a,$schema) <=> _get_key_ordinal($name,$hashref,$b,$schema) } @unsorted;
    
    # set indent
    my ($namenamespace,$lookupname);
    if($name =~ /:/) {
        ($namenamespace,$lookupname) = split(/:/, $name, 2);
    } else {
        $lookupname = $name;
    }
    if($lookupname) {
        $self->{opt}->{attrindent} = ( $lookupname =~ /^(assemblyIdentity)$/ )
            ? 1 : 0;
    } else {
        $self->{opt}->{attrindent} = 0;
    }
    return ( @sorted );
}

#--------------------------------------------------------------
# END XML::Simple overrides
#--------------------------------------------------------------

sub _get_key_ordinal {
    my($name,$hashref,$key, $schema) = @_;
    # some defaults
    return 1001 if $key eq 'xmlns';
    return 10000 if $key =~ /^xmlns:.+$/;
    my ($keynamespace,$lookupkey);
    if($key =~ /:/) {
        ($keynamespace,$lookupkey) = split(/:/, $key, 2);
    } else {
        $lookupkey = $key;
    }
    my ($namenamespace,$lookupname);
    if($name =~ /:/) {
        ($namenamespace,$lookupname) = split(/:/, $name, 2);
    } else {
        $lookupname = $name;
    }
    
    carp qq(UNKNOWN NAMEKEY NAME $name : $lookupname KEY $key : $lookupkey\n) if (!$lookupname || !$lookupkey);
    
    if(ref($hashref->{$key})) {
        return (exists($schema->{elementtypes}->{$lookupname}->{elements}->{$lookupkey}->{order}))
            ? $schema->{elementtypes}->{$lookupname}->{elements}->{$lookupkey}->{order}
            : 9999 ;
    } else {
        return (exists($schema->{elementtypes}->{$lookupname}->{attributes}->{$lookupkey}->{order}))
            ? $schema->{elementtypes}->{$lookupname}->{attributes}->{$lookupkey}->{order}
            : 9999 ;
    }
}

sub get_default_manifest {
    my $self_or_class = shift;
    my $defman =  q(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
    <assemblyIdentity type="win32" version="0.0.0.0" name="Perl.Win32.Application" />
    <description>Perl.Win32.Application</description>
    <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
        <security>
            <requestedPrivileges>
                <requestedExecutionLevel level="asInvoker" uiAccess="false" />
            </requestedPrivileges>
        </security>
    </trustInfo>
</assembly>
);
    return $defman;
}


sub validate_manifest_version {
    return ($_[1] eq '1.0') ? 1 : 0;
}

sub validate_string {
    my ($self, $val) = @_;
    return 1;
}

sub validate_class_name {
    my($this, $name) = @_;
    return ($name =~ /[^A-Za-z0-9\-_\.]/) ? 0 : 1;
}

sub validate_type {
    my($this, $type) = @_;
    return ( $type eq 'win32'); # specification demands lower case
}

sub validate_public_key {
    my ($this, $key) = @_;
    return 1;
}

sub validate_language {
    my ($self, $lang) = @_;
    return 1;
}

sub validate_architecture {
    my ($this, $arch) = @_;
    return 1 if $arch eq '*';
    # many files seem to uppercase X86 so we'll accept case insensitive data
    return 1 if $arch =~ /^(x86|msil|ia64|amd64)$/i;
    return 0;
}

sub validate_yesno {
    my ($this, $val) = @_;
    return ( $val =~ /^(yes|no)$/) ? 1 : 0;
}

sub validate_truefalse {
    my ($this, $val) = @_;
    return ( $val =~ /^(true|false)$/) ? 1 : 0;
}

sub validate_clsid {
    my ($this, $clsid) = @_;
    return ( $clsid =~ /^\{[A-Fa-f0-9]{8}-[A-Fa-f0-9]{4}-[A-Fa-f0-9]{4}-[A-Fa-f0-9]{4}-[A-Fa-f0-9]{12}\}$/ ) ? 1 : 0;
}

sub validate_int4 {
    my ($this, $int) = @_;
    return ( $int =~ /^\d{1,4}$/) ? 1 : 0;
}

sub validate_int8 {
    my ($this, $int) = @_;
    return ( $int =~ /^\d{1,8}$/) ? 1 : 0;
}

sub validate_flags {
    my ($this, $flags) = @_;
    return ( $flags =~ /^(control|hidden|restricted|hasdiskimage)$/i ) ? 1 : 0;
}

sub validate_level {
    my ($this, $level) = @_;
    return ( $level =~ /^(asInvoker|highestAvailable|requireAdministrator)$/ ) ? 1 : 0;
}

sub validate_hashalg {
    my ($this, $alg) = @_;
    return ( $alg =~ /^(SHA1|SHA|MD5|MD4|MD2)$/i ) ? 1 : 0;
}

sub validate_thread_model {
    my ($this, $model) = @_;
    return ( $model =~ /^(Apartment|Free|Both|Neutral)$/i ) ? 1 : 0;
}

sub validate_hex {
    my ($this, $hex) = @_;
    return 1;
}

sub validate_osid {
    my ($this, $osid) = @_;
    return 1 if $osid eq '{e2011457-1546-43c5-a5fe-008deee3d3f0}'; # Windows Vista Compatibility
    return 1 if $osid eq '{35138b9a-5d96-4fbd-8e2d-a2440225f93a}'; # Windows 7 Compatibility
    return 0;
}

sub validate_miscstatus {
    my ($this, $status) = @_;
    my @vals = split(/\s*,\s*/, $status);
    my @allowednames = qw(
        recomposeonresize onlyiconic insertnotreplace static cantlinkinside canlinkbyole1   
        islinkobject insideout activatewhenvisible renderingisdeviceindependent    
        invisibleatruntime alwaysrun actslikebutton actslikelabel nouiactivate    
        alignable simpleframe setclientsitefirst imemode ignoreativatewhenvisible    
        wantstomenumerge supportsmultilevelundo
        );
    my %allowed;
    @allowed{@allowednames} = (); 
    my $returnval = 1;
    for (@vals) {
        $returnval = (exists($allowed{$_})) ? 1 : 0;
        last if !$returnval;
    }
    return $returnval;
}

$BASESCHEMA = __PACKAGE__->get_default_schema();

sub get_default_schema {
    my $self_or_class = shift;    
    my $schema = {
        namespace => {
            'urn:schemas-microsoft-com:compatibility.v1' => 'cmpv1',
            'urn:schemas-microsoft-com:asm.v1' => 'asmv1',
            'urn:schemas-microsoft-com:asm.v2' => 'asmv2',
            'urn:schemas-microsoft-com:asm.v3' => 'asmv3',
        },
        nstranslation => {
            cmpv1 => 'cmpv1',
            asmv1 => 'asmv1',
            asmv2 => 'asmv2',
            asmv3 => 'asmv3',
        },
        namespacelookup => {
            cmpv1 => 'urn:schemas-microsoft-com:compatibility.v1',
            asmv1 => 'urn:schemas-microsoft-com:asm.v1',
            asmv2 => 'urn:schemas-microsoft-com:asm.v2',
            asmv3 => 'urn:schemas-microsoft-com:asm.v3',
        },
        attributes => {
            manifestVersion         => 'validate_manifest_version',
            name                    => 'validate_class_name',
            type                    => 'validate_type',
            publicKeyToken          => 'validate_public_key',
            language                => 'validate_language',
            processorArchitecture   => 'validate_architecture',
            version                 => 'validate_string',
            optional                => 'validate_yesno',
            clsid                   => 'validate_clsid',
            description             => 'validate_string',
            threadingModel          => 'validate_thread_model',
            tlbid                   => 'validate_clsid',
            progid                  => 'validate_string',
            helpdir                 => 'validate_string',
            iid                     => 'validate_clsid',
            numMethods              => 'validate_int4',
            resourceid              => 'validate_string',
            flags                   => 'validate_flags',
            hashalg                 => 'validate_hashalg',
            hash                    => 'validate_hex',
            proxyStubClsid32        => 'validate_string',
            baseInterface           => 'validate_clsid',
            versioned               => 'validate_yesno',
            oldVersion              => 'validate_string',
            newVersion              => 'validate_string',
            size                    => 'validate_int8',
            runtimeVersion          => 'validate_string',
            Id                      => 'validate_osid',
            xmlns                   => 'validate_string',
            level                   => 'validate_level',
            uiAccess                => 'validate_truefalse',
            miscStatus              => 'validate_miscstatus',
            miscStatusIcon          => 'validate_miscstatus',
            miscStatusContent       => 'validate_miscstatus',
            miscStatusDocprint      => 'validate_miscstatus',
            miscStatusThumbnail     => 'validate_miscstatus',
        },
        elementtypes => {
            assembly    =>  {
                exclusive   => 'none',
                content     => { elements => 1, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    xmlns           => {
                        required    => 1,
                        default     => 'urn:schemas-microsoft-com:asm.v1',
                    },
                    manifestVersion => {
                        required    => 1,
                        default     => '1.0',
                        order       => 1002,
                    },
                },
                elements    => {
                    # files, even from Microsoft, sometimes don't have an assemblyIdentity
                    assemblyIdentity                => {  min => 0, max => 1, order => 10,  },
                    compatibility                   => {  min => 0, max => 1, order => 64, },
                    application                     => {  min => 0, max => 1, order => 66, },
                    description                     => {  min => 0, max => 1, order => 20,  },
                    noInherit                       => {  min => 0, max => 1, order => 30, },
                    noInheritable                   => {  min => 0, max => 1, order => 40, },
                    comInterfaceExternalProxyStub   => {  min => 0, max => 0, order => 50, },
                    dependency                      => {  min => 0, max => 0, order => 60, },
                    file                            => {  min => 0, max => 0, order => 70, },
                    clrClass                        => {  min => 0, max => 0, order => 80, },
                    clrSurrogate                    => {  min => 0, max => 0, order => 90, },
                    trustInfo                       => {  min => 0, max => 1, order => 62, },
                    windowClass                     => {  min => 0, max => 1, order => 150, },
                },
            },
            trustInfo    =>  {
                exclusive   => 'none',
                content     => { elements => 1, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    xmlns           => {
                        required    => 1,
                        default     => 'urn:schemas-microsoft-com:asm.v3',
                    },
                },
                elements    => {
                    security                => {  min => 1, max => 1, order => 10  },
                },
            },
            security    =>  {
                exclusive   => 'none',
                content     => { elements => 1, attributes => 0, value => 0 },
                value_validator => undef,
                attributes  => {
                },
                elements    => {
                    requestedPrivileges     => {  min => 1, max => 1, order => 10,  },
                },
            },
            requestedPrivileges    =>  {
                exclusive   => 'none',
                content     => { elements => 1, attributes => 0, value => 0 },
                value_validator => undef,
                attributes  => {
                },
                elements    => {
                    requestedExecutionLevel     => {  min => 1, max => 1 , order => 10, },
                },
            },
            requestedExecutionLevel    =>  {
                exclusive   => 'none',
                content     => { elements => 0, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    level           => {
                        required    => 1,
                        default     => 'asInvoker',
                        order       => 1002,
                    },
                    uiAccess           => {
                        required    => 0,
                        default     => 'false',
                        order       => 1003,
                    },
                },
                elements    => {
                },
            },              
            compatibility    =>  {
                exclusive   => 'application',
                content     => { elements => 1, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    xmlns           => {
                        required    => 1,
                        default     => 'urn:schemas-microsoft-com:compatibility.v1',
                    },
                },
                elements    => {
                    application                     => {  min => 1, max => 1, order => 10,  },
                },
            },
            application     =>  {
                exclusive   => 'none',
                content     => { elements => 1, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    xmlns           => {
                        required    => 0,
                        default     => 'urn:schemas-microsoft-com:asm.v3',
                    },
                },
                elements    => {
                    supportedOS                     => {  min => 0, max => 0, order => 10  },
                    windowsSettings                 => {  min => 0, max => 0, order => 10  },
                },
            },
            supportedOS     =>  {
                exclusive   => 'none',
                content     => { elements => 0, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    Id => {
                        required    => 1,
                        # Windows Vista Compatibility Key
                        default     => '{e2011457-1546-43c5-a5fe-008deee3d3f0}',
                        order => 1002,
                    },
                },
                elements    => {
                },
            },
            clrClass        =>  {
                exclusive   => 'assembly',
                content     => { elements => 1, attributes => 0, value => 0 },
                value_validator => undef,
                attributes  => {
                    name => {
                        required    => 1,
                        default     => undef,
                        order       => 1002,
                    },
                    clsid => {
                        required    => 1,
                        default     => undef,
                        order       => 1003,
                    },
                    progid => {
                        required    => 0,
                        default     => undef,
                        order       => 1004,
                    },
                    tlbid => {
                        required    => 0,
                        default     => undef,
                        order       => 1005,
                    },
                    description => {
                        required    => 0,
                        default     => undef,
                        order       => 1006,
                    },
                    runtimeVersion => {
                        required    => 0,
                        default     => undef,
                        order       => 1007,
                    },
                    threadingModel => {
                        required    => 0,
                        default     => undef,
                        order       => 1008,
                    },                
                },
                elements    => {
                    progid                     => {  min => 0, max => 0, order => 10  },
                },
            },
            clrSurrogate        =>  {
                exclusive   => 'assembly',
                content     => { elements => 0, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    name => {
                        required    => 1,
                        default     => undef,
                        order       => 1002,
                    },
                    clsid => {
                        required    => 1,
                        default     => undef,
                        order       => 1003,
                    },
                    runtimeVersion => {
                        required    => 0,
                        default     => undef,
                        order       => 1004,
                    },
                },
                elements    => {
                },
            },
            assemblyIdentity        =>  {
                exclusive   => 'none',
                content     => { elements => 0, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    name => {
                        required    => 1,
                        default     => undef,
                        order       => 1003,
                    },
                    version => {
                        required    => 0,
                        default     => '0.0.0.0',
                        order       => 1004,
                    },
                    type => {
                        required    => 0,
                        default     => 'win32',
                        order       => 1002,
                    },
                    processorArchitecture => {
                        required    => 0,
                        default     => '*',
                        order       => 1006,
                    },
                    publicKeyToken => {
                        required    => 0,
                        default     => undef,
                        order       => 1007,
                    },
                    language => {
                        required    => 0,
                        default     => undef,
                        order       => 1005,
                    },
                },
                elements    => {
                },
            },
            comInterfaceProxyStub        =>  {
                exclusive   => 'assembly',
                content     => { elements => 0, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    name => {
                        required    => 1,
                        default     => undef,
                        order       => 1003,
                    },
                    iid => {
                        required    => 1,
                        default     => undef,
                        order       => 1002,
                    },
                    tblid => {
                        required    => 0,
                        default     => undef,
                        order       => 1004,
                    },
                    numMethods => {
                        required    => 0,
                        default     => undef,
                        order       => 1006,
                    },
                    proxyStubClsid32 => {
                        required    => 0,
                        default     => undef,
                        order       => 1007,
                    },
                    baseInterface => {
                        required    => 0,
                        default     => undef,
                        order       => 1005,
                    },
                    threadingModel => {
                        required    => 0,
                        default     => undef,
                        order       => 1008,
                    },
                },
                elements    => {
                },
            },
            description        =>  {
                exclusive   => 'none',
                content     => { elements => 0, attributes => 0, value => 1 },
                value_validator => 'validate_string',
                attributes  => {
                },
                elements    => {
                },
            },
            dependency        =>  {
                exclusive   => 'none',
                content     => { elements => 1, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    optional => {
                        required    => 0,
                        default     => 'no',
                        order       => 1002,
                    },
                },
                elements    => {
                    dependentAssembly       => {  min => 0, max => 1, order => 10  },                
                }
            },
            dependentAssembly        =>  {
                exclusive   => 'none',
                content     => { elements => 1, attributes => 0, value => 0 },
                value_validator => undef,
                attributes  => {
                },            
                elements    => {
                    assemblyIdentity       => {  min => 1, max => 1, order => 10  },
                    bindingRedirect        => {  min => 0, max => 0, order => 20  }, 
                },
            },
            bindingRedirect        =>  {
                exclusive   => 'application',
                content     => { elements => 0, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    oldVersion => {
                        required    => 1,
                        default     => undef,
                        order       => 1002,
                    },
                    newVersion => {
                        required    => 1,
                        default     => undef,
                        order       => 1003,
                    },                
                },
                elements    => {
                },
            },
            file        =>  {
                exclusive   => 'none',
                content     => { elements => 1, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    name => {
                        required    => 1,
                        default     => undef,
                        order       => 1002,
                    },
                    hash => {
                        required    => 0,
                        default     => undef,
                        order       => 1004,
                    },
                    hashalg => {
                        required    => 0,
                        default     => undef,
                        order       => 1003,
                    },
                    size => {
                        required    => 0,
                        default     => undef,
                        order       => 1005,
                    },
                },
                elements    => {
                    comClass               => {  min => 0, max => 0, order => 10,  },
                    comInterfaceProxyStub  => {  min => 0, max => 0, order => 20,  },
                    typelib                => {  min => 0, max => 0, order => 30,  },
                    windowClass            => {  min => 0, max => 0, order => 40,  },
                },
            },
            comClass        =>  {
                exclusive   => 'assembly',
                content     => { elements => 1, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    clsid => {
                        required    => 1,
                        default     => undef,
                        order       => 1003,
                    },
                    threadingModel => {
                        required    => 0,
                        default     => undef,
                        order       => 1004,
                    },
                    progid => {
                        required    => 0,
                        default     => undef,
                        order       => 1003,
                    },
                    tlbid => {
                        required    => 0,
                        default     => undef,
                        order       => 1005,
                    },
                    description => {
                        required    => 0,
                        default     => undef,
                        order       => 1002,
                    },
                    miscStatus => {
                        required    => 0,
                        default     => undef,
                        order       => 1006,
                    },
                    miscStatusIcon => {
                        required    => 0,
                        default     => undef,
                        order       => 1007,
                    },
                    miscStatusDocprint => {
                        required    => 0,
                        default     => undef,
                        order       => 1008,
                    },
                    miscStatusContent => {
                        required    => 0,
                        default     => undef,
                        order       => 1009,
                    },
                    musStatusThumbnail => {
                        required    => 0,
                        default     => undef,
                        order       => 1010,
                    },
                },
                elements    => {
                    progid                 => {  min => 0, max => 0, order => 10,  },
                },
            },
            comInterfaceExternalProxyStub        =>  {
                exclusive   => 'assembly',
                content     => { elements => 0, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    name => {
                        required    => 1,
                        default     => undef,
                        order       => 1005,
                    },
                    iid => {
                        required    => 1,
                        default     => undef,
                        order       => 1002,
                    },
                    tblid => {
                        required    => 0,
                        default     => undef,
                        order       => 1006,
                    },
                    numMethods => {
                        required    => 0,
                        default     => undef,
                        order       => 1004,
                    },
                    proxyStubClsid32 => {
                        required    => 0,
                        default     => undef,
                        order       => 1007,
                    },
                    baseInterface => {
                        required    => 0,
                        default     => undef,
                        order       => 1003,
                    },
                },
                elements    => {
                },
            },
            typelib        =>  {
                exclusive   => 'assembly',
                content     => { elements => 0, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    tblid => {
                        required    => 1,
                        default     => undef,
                        order       => 1002,
                    },
                    version => {
                        required    => 1,
                        default     => undef,
                        order       => 1003,
                    },
                    helpdir => {
                        required    => 1,
                        default     => '',
                        order       => 1004,
                    },
                    resourceid => {
                        required    => 0,
                        default     => undef,
                        order       => 1006,
                    },
                    flags => {
                        required    => 0,
                        default     => undef,
                        order       => 1005,
                    },
                },
                elements    => {
                },
            },
            windowClass        =>  {
                exclusive   => 'assembly',
                content     => { elements => 0, attributes => 1, value => 1 },
                value_validator => 'validate_class_name',
                attributes  => {
                    versioned => {
                        required    => 1,
                        default     => 'yes',
                        order       => 1002,
                    },
                },
                elements    => {
                },
            },
            noInherit        =>  {
                exclusive   => 'application',
                content     => { elements => 0, attributes => 0, value => 0 },
                value_validator => undef,
                attributes  => {
                },
                elements    => {
                },
            },
            noInheritable        =>  {
                exclusive   => 'assembly',
                content     => { elements => 0, attributes => 0, value => 0 },
                value_validator => undef,
                attributes  => {
                },
                elements    => {
                },
            },
            progid        =>  {
                exclusive   => 'assembly',
                content     => { elements => 0, attributes => 0, value => 1 },
                value_validator => 'validate_class_name',
                attributes  => {
                },
                elements    => {
                },
            },
            windowsSettings     =>  {
                exclusive   => 'none',
                content     => { elements => 1, attributes => 1, value => 0 },
                value_validator => undef,
                attributes  => {
                    xmlns           => {
                        required    => 1,
                        default     => 'http://schemas.microsoft.com/SMI/2005/WindowsSettings',
                    },
                },
                elements    => {
                    dpiAware                        => {  min => 0, max => 1, order => 10  },
                },
            },
            dpiAware       =>  {
                exclusive   => 'none',
                content     => { elements => 0, attributes => 0, value => 1 },
                value_validator => 'validate_truefalse',
                attributes  => {
                },
                elements    => {
                },
            },
        },
    };
    return $schema;
}

1;

__END__

=head1 SEE ALSO

Win32::Exe::Manifest

=head1 AUTHORS

Mark Dootson E<lt>mdootson@cpan.orgE<gt>

=head1 COPYRIGHT & LICENSE

Copyright 2010 by Mark Dootson E<lt>mdootson@cpan.orgE<gt>

This program is free software; you can redistribute it and/or 
modify it under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut


