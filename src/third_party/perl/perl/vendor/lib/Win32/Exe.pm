package Win32::Exe;
$Win32::Exe::VERSION = '0.17';

=head1 NAME

Win32::Exe - Manipulate Win32 executable files

=head1 VERSION

This document describes version 0.17 of Win32::Exe, released
July 19, 2011.

=head1 SYNOPSIS

    use Win32::Exe;
    my $exe = Win32::Exe->new('c:/windows/notepad.exe');
    
    # add a default resource structure if none exists
    # create_resource_section only works on MSWin and
    # does not work on Windows XP - requires Vista or
    # above
    
    $exe = $exe->create_resource_section if $exe->can_create_resource_section;

    # Get version information
    my $info = $exe->get_version_info;
    print qq($_ = $info->{$_}\n) for (sort keys(%$info));

    # Extract icons from an executable
    my @iconnames = $exe->get_group_icon_names;
    for ( my $i = 0; $i < @iconnames; $i++ ) {
        my $filename = 'icon' . $i . '.ico';
        my $iconname = $iconnames[$i];
        $exe->extract_group_icon($iconname,$filename);
    }

    # Import icons from a .exe or .ico file and write back the file
    $exe->update( icon => '/c/windows/taskman.exe' );
    $exe->update( icon => 'myicon.ico' );

    # Change it to a console application, then save to another .exe
    $exe->set_subsystem_console;
    $exe->write('c:/windows/another.exe');
    
    # Add a manifest section
    $exe->update( manifest => $mymanifestxml );
    # or a default
    $exe->update( defaultmanifest => 1 );
    
    # or specify manifest args
    $exe->update( manifestargs => { ExecLevel => 'requireAdministrator' } );
    
    # Get manifest object
    $manifest = $exe->get_manifest if $exe->has_manifest;
    
    # change execution level
    $manifest->set_execution_level('requireAdministrator');
    $exe->set_manifest($manifest);
    $exe->write;
    

=head1 DESCRIPTION

This module parses and manipulating Win32 PE/COFF executable headers,
including version information, icons, manifest and other resources.
The module Win32::Exe::Manifest can be used for manifest handling.

A script exe_update.pl is provided for simple file updates. 

Also, please see the test files in the source distributions
F<t/> directory for examples of using this module.

=head1 METHODS

=head2 new

    my $exe = Win32::Exe->new($filename);

Create a Win32::Exe object from $filename. Filename can be an executable
or a DLL.

=head2 update

    $exe->update( icon         => 'c:/my/icon.ico',
                  gui          => 1,
                  info         => [ 'FileDescription=My File', 'FileVersion=1.4.3.3567' ],
                  manifest     => 'c:/my/manifest.xml',
                  manifestargs => [ 'ExecLevel=asInvoker', 'CommonControls=1' ],
                  );

The B<update> method provides a convenience method for the most common actions. It
writes the information you provide to the file opened by Win32::Exe->new($filename).
You do not have to call $exe->write - the method automatically updates the opened
file.

Param detail:

B<icon> Pass the name of an executable, dll or ico file to extract the icon and
make it the main icon for the Win32 executable.

B<info> Pass a reference to an array of strings containing key - value pairs
separated by '='.

e.g. info => [ 'FileDescription=My File', 'FileVersion=1.4.3.3567' ]

Recognised keys are

    Comments        CompanyName     FileDescription FileVersion
    InternalName    LegalCopyright  LegalTrademarks OriginalFilename
    ProductName     ProductVersion

B<gui  or  console> Use parameter 'gui' or 'console' to set the executable
subsystem to Windows or Console. You can, of course, only use one or the other
of gui / console, not both.

B<manifest> Specify a manifest file to add to the executable resources.

B<manifestargs> As an alternative to specifying a manifest file, pass a
reference to an array of strings containing key - value pairs separated
by '='.

e.g. manifestargs => [ 'ExecLevel=asInvoker', 'CommonControls=1' ]

Recognised keys are

    ExecutionLevel  UIAccess     ExecName   Description
    CommonControls  Version

=head2 create_resource_section

    $exe = $exe->create_resource_section if $exe->can_create_resource_section;

If an executable file does not have an existing resource section, you must create
one before attempting to add version, icon or manifest resources. The method
create_resource_section is only available on MSWin platforms. Also, the method
will fail if your windows version is Windows XP or below. You can check if it is
possible to call create_resource_section by calling $exe->can_create_resource_section.
After calling create_resource_section, the original Win32::Exe object does not
reference the updated data. The method therefore returns a reference to a new
Win32::Exe object that references the updated data.

Always call as :

$exe = $exe->create_resource_section if $exe->can_create_resource_section;

if the $exe already has a resource section, this call will safely return a reference
to the original object without updating the original exe.

=head2 can_create_resource_section

    $exe = $exe->create_resource_section if $exe->can_create_resource_section;

Check if the operating system and version allow addition of a resource section
when none exists in the target executable.

=head2 get_manifest

    my $manifest = $exe->get_manifest;

Retrieves a Win32::Exe::Manifest object. (See docs for Win32::Exe::Manifest)

=head2 set_manifest

    $exe->set_manifest($manifest);
    $exe->write;
    
Takes a Win32::Exe::Manifest object. You must explicitly call 'write' to commit
changes to file. Also takes a filepath to an xml file containing raw manifest
information.

=head2 set_manifest_args

    $exe->set_manifest_args($argref);
    $exe->write;

Accepts a reference to a hash with one or more of the the keys

    ExecutionLevel  UIAccess     ExecName   Description
    CommonControls  Version

Also accepts a reference to an array of strings of the format:
    [ 'key1=value1', 'key2=value2' ]

Example Values:

    ExecutionLevel=asInvoker
    UIAccess=false
    CommonControls=1
    Version=6.8.67.334534
    ExecName=My.Application
    Description=My Application

The CommonControls argument can be specified to add a dependency on
Common Controls Library version 6.0.0.0

=head2 get_version_info

    my $inforef = $exe->get_version_info;

Returns a reference to a hash with the keys:

    Comments        CompanyName     FileDescription FileVersion
    InternalName    LegalCopyright  LegalTrademarks OriginalFilename
    ProductName     ProductVersion

=head2 set_version_info

    $exe->set_version_info($inforef);
    $exe->write;

Accepts a reference to a hash with one or more of the the keys

    Comments        CompanyName     FileDescription FileVersion
    InternalName    LegalCopyright  LegalTrademarks OriginalFilename
    ProductName     ProductVersion

Also accepts a reference to an array of strings of the format:
    [ 'key1=value1', 'key2=value2' ]

=head2 set_subsystem_windows

    $exe->set_subsystem_windows;
    $exe->write;

Sets the executable system as 'windows'. (GUI).
You may also call $exe->SetSubsystem('windows);
This is the equivalent of $exe->update( gui => 1);

=head2 set_subsystem_console

    $exe->set_subsystem_console;
    $exe->write;

Sets the executable system as 'console'.
You may also call $exe->SetSubsystem('console);
This is the equivalent of $exe->update( console => 1);

=head2 get_subsystem

    my $subsys = $exe->get_subsystem;

Returns a descriptive string for the subsystem.
Possible values:  windows | console | posix | windowsce | native
You can usefully update executables with the windows or console
subsystem

=head2 set_single_group_icon

    $exe->set_single_group_icon($iconfile);
    $exe->write;

Accepts the path to an icon file. Replaces all the icons in the
exec with the icons from the file.

=head2 get_group_icon_names

    my @iconnames = $exe->get_group_icon_names;

Returns a list of the names of all the group icons in the
executable or dll. If there are no group icons, returns an empty
list. The names returned can be used as parameters to other icon
handling methods.

=head2 get_group_icon

    $exe->get_group_icon($groupiconname);

Accepts a group icon name as returned by get_group_icon_names.
Returns the Wx::Exe::Resource::GroupIcon object for the named
GroupIcon

=head2 add_group_icon

    $exe->add_group_icon($groupiconname, $iconfilepath);
    $exe->write;

Accepts a group icon name and a path to an icon file. Adds the
icon to the exec without affecting existing icons. The group
icon name must not already exist.

=head2 replace_group_icon

    $exe->replace_group_icon($groupiconname, $iconfilepath);
    $exe->write;

Accepts a group icon name and a path to an icon file.
Replaces the groupicon named with the contents of the icon file.

=head2 remove_group_icon

    $exe->remove_group_icon($groupiconname);
    $exe->write;

Accepts a group icon name. Removes the group icon with that name
from the exec or dll.

=head2 export_group_icon

    $exe->export_group_icon($groupiconname, $iconfilepath);

Accepts a group icon name and a .ico filepath. Writes the named
icon group to the file in filepath.


=cut

use strict;
use base 'Win32::Exe::Base';
use constant FORMAT => (
    Magic       => 'a2',    # "MZ"
    _           => 'a58',
    PosPE       => 'V',
    _           => 'a{($PosPE > 64) ? $PosPE - 64 : "*"}',
    PESig       => 'a4',
    Data        => 'a*',
);
use constant DELEGATE_SUBS => (
    'IconFile'  => [ 'dump_iconfile', 'write_iconfile' ],
);
use constant DISPATCH_FIELD => 'PESig';
use constant DISPATCH_TABLE => (
    "PE\0\0"    => "PE",
    '*'     => sub { die "Incorrect PE header -- not a valid .exe file" },
);
use constant DEBUG_INDEX         => 6;
use constant DEBUG_ENTRY_SIZE    => 28;

use File::Basename ();
use Win32::Exe::IconFile;
use Win32::Exe::DebugTable;
use Win32::Exe::Manifest;

sub is_application { ( $_[0]->is_assembly ) ? 0 : 1; }

sub is_assembly { $_[0]->Characteristics & 0x2000; }

sub has_resource_section {
    my ($self) = @_;
    my $section = $self->first_member('Resources');
    return( $section ) ? 1 : 0;
}

sub can_create_resource_section {
    my $self = shift;
    return 0 if ($^O !~ /^mswin/i );
    require Win32;
    my ($winstring, $winmajor, $winminor, $winbuild, $winid) = Win32::GetOSVersion();
    return ( $winmajor > 5 ) ? 1 : 0;
}

sub create_resource_section {
    my $self = shift;
    return $self if($self->has_resource_section || ( $^O !~ /^mswin/i ) );
    if(!$self->can_create_resource_section) {
        die('Cannot create resource section on this version of Windows');
    }
    require Win32::Exe::InsertResourceSection;
    my $filename = (exists($self->{filename}) && (-f $self->{filename} )) ? $self->{filename} : undef;
    return $self if !$filename;
    if(my $newref = Win32::Exe::InsertResourceSection::insert_pe_resource_section($filename)) {
        return $newref;
    } else {
        return $self;
    }
}

sub resource_section {
    my ($self) = @_;
    my $section = $self->first_member('Resources');
    return $section if $section;
    my $wmsg = 'No resource section found in file ';
    $wmsg .= $self->{filename} if(exists($self->{filename}) && $self->{filename}); 
    warn $wmsg;
    return undef;
}

sub sections {
    my ($self) = @_;
    my $method = (wantarray ? 'members' : 'first_member');
    return $self->members('Section');
}

sub data_directories {
    my ($self) = @_;
    return $self->members('DataDirectory');
}

sub update_debug_directory {
    my ($self, $boundary, $extra) = @_;

    $self->SetSymbolTable( $self->SymbolTable + $extra )
    if ($boundary <= $self->SymbolTable);

    my @dirs = $self->data_directories;
    return if DEBUG_INDEX > $#dirs;

    my $dir = $dirs[DEBUG_INDEX] or return;
    my $size = $dir->Size;
    my $addr = $dir->VirtualAddress;

    return unless $size or $addr;

    my $count = $size / DEBUG_ENTRY_SIZE or return;

    (($size % DEBUG_ENTRY_SIZE) == 0) or return;

    foreach my $section ($self->sections) {
    my $offset = $section->FileOffset;
    my $f_size = $section->FileSize;
    my $v_addr = $section->VirtualAddress;

    next unless $v_addr <= $addr;
    next unless $addr < ($v_addr + $f_size);
    next unless ($addr + $size) < ($v_addr + $f_size);

    $offset += $addr - $v_addr;
    my $data = $self->substr($offset, $size);

    my $table = Win32::Exe::DebugTable->new(\$data);

    foreach my $dir ($table->members) {
        next unless $boundary <= $dir->Offset;

        $dir->SetOffset($dir->Offset + $extra);
        $dir->SetVirtualAddress($dir->VirtualAddress + $extra)
        if $dir->VirtualAddress > 0;
    }

    $self->substr($offset, $size, $table->dump);
    last;
    }
}

sub default_info {
    my $self = shift;

    my $filename = File::Basename::basename($self->filename);

    return join(';',
    "CompanyName= ",
    "FileDescription= ",
    "FileVersion=0.0.0.0",
    "InternalName=$filename",
    "LegalCopyright= ",
    "LegalTrademarks= ",
    "OriginalFilename=$filename",
    "ProductName= ",
    "ProductVersion=0.0.0.0",
    );
}

sub update {
    my ($self, %args) = @_;
    
    if ($args{defaultmanifest}) {
            $self->add_default_manifest();
    }
    
    if (my $manifest = $args{manifest}) {
        $self->set_manifest($manifest);
    }
    
    if (my $manifestargs = $args{manifestargs}) {
        $self->set_manifest_args($manifestargs);
    }

    if (my $icon = $args{icon}) {
        my @icons = Win32::Exe::IconFile->new($icon)->icons;
        $self->set_icons(\@icons) if @icons;
    }

    if (my $info = $args{info}) {
        $self->set_version_info( $info);
    }

    die "'gui' and 'console' cannot both be true"
    if $args{gui} and $args{console};

    $self->SetSubsystem("windows") if $args{gui};
    $self->SetSubsystem("console") if $args{console};
    $self->write;
}

sub icons {
    my ($self) = @_;
    my $rsrc = $self->resource_section or return;
    my @icons = map $_->members, $rsrc->objects('GroupIcon');
    wantarray ? @icons : \@icons;
}

sub set_icons {
    my ($self, $icons) = @_;

    my $rsrc = $self->resource_section;
    my $name = eval { $rsrc->first_object('GroupIcon')->PathName }
        || '/#RT_GROUP_ICON/#1/#0';

    $rsrc->remove('/#RT_GROUP_ICON');
    $rsrc->remove('/#RT_ICON');

    my $group = $self->require_class('Resource::GroupIcon')->new;
    $group->SetPathName($name);
    $group->set_parent($rsrc);
    $rsrc->insert($group->PathName, $group);

    $group->set_icons($icons);
    $group->refresh;
}

sub version_info {
    my ($self) = @_;
    my $rsrc = $self->resource_section or return;

    # XXX - return a hash in list context?

    return $rsrc->first_object('Version');
}

sub get_version_info {
    my $self = shift;
    my $vinfo = $self->version_info or return;
    my @keys = qw(
        Comments        CompanyName     FileDescription FileVersion
        InternalName    LegalCopyright  LegalTrademarks OriginalFilename
        ProductName     ProductVersion
    );
    my $rval = {};
    for my $key (@keys) {
        my $val = $vinfo->get($key);
        $val =~ s/,/\./g if(defined($val) && ( $key =~ /version/i ) );
        $rval->{$key} = $val if defined($val);
    }
    return $rval
}

sub set_version_info {
    my ($self, $inputpairs) = @_;
    my $inputref;
    if(ref($inputpairs) eq 'HASH') {
        my @newinput = ();
        push(@newinput, qq($_=$inputpairs->{$_})) for (sort keys(%$inputpairs));
        $inputref = \@newinput;
    } else {
        $inputref = $inputpairs;
    }
    my @info = ($self->default_info, @$inputref);
    my @pairs;
    foreach my $pairs (map split(/\s*;\s*(?=[\w\\\/]+\s*=)/, $_), @info) {
        my ($key, $val) = split(/\s*=\s*/, $pairs, 2);
        next if $key =~ /language/i;

        if ($key =~ /^(product|file)version$/i) {
            $key = "\u$1Version";
            $val =~ /^(?:\d+\.)+\d+$/ or die "$key cannot be '$val'";
            $val .= '.0' while $val =~ y/.,// < 3;

            push(@pairs,
                [ $key => $val ],
                [ "/StringFileInfo/#1/$key", $val ]);
        } else {
            push(@pairs, [ $key => $val ]);
        }
    }
    my $rsrc = $self->resource_section or return;
    my $version = $rsrc->first_object('Version') or return;
    $version->set(@$_) for @pairs;
    $version->refresh;
}

sub manifest {
    my ($self) = @_;
    my $rsrc = $self->resource_section or return;
    if( my $obj = $rsrc->first_object('Manifest') ) {
        return $obj;
    } else {
        return $self->require_class('Resource::Manifest')->new;
    }
}    

sub has_manifest {
    my ($self) = @_;
    my $rsrc = $self->resource_section or return 0;
    if( my $obj = $rsrc->first_object('Manifest') ) {
        return 1;
    } else {
        return 0;
    }
}

sub set_manifest {
    my ($self, $input) = @_;
    # support code that passes xml, filepaths and objects
    my $resid = 0;
    my $xml;
    if(ref($input) && $input->isa('Win32::Exe::Manifest')) {
        $resid = $input->get_resource_id;
        $xml = $input->output ;
    } else {
        my $filecontent;
        eval {
            my $paramisfile = 0;
            {
                no warnings qw( io );
                $paramisfile = (-f $input);
            }
            if($paramisfile) {
                open my $fh, '<', $input;
                $filecontent = do { local $/; <$fh> };
                my $errors = $@;
                close($fh);
                die $errors if $errors;
            } else {
                $filecontent = $input;
            }
        };
        $xml = ( $@ ) ? $input : $filecontent;
    }
    $resid ||= 1;
    my $rsrc = $self->resource_section;
    my $name = '/#RT_MANIFEST/#' . $resid . '/#0';
    $rsrc->remove("/#RT_MANIFEST");
    my $manifest = $self->require_class('Resource::Manifest')->new;
    $manifest->SetPathName( $name ); 
    $manifest->set_parent( $rsrc );
    $manifest->update_manifest( $xml );
    $rsrc->insert($manifest->PathName, $manifest);
    $rsrc->refresh;
}

sub set_manifest_args {
    my ($self, $inputpairs) = @_;
    my $inputref;
    if(ref($inputpairs eq 'HASH')) {
        my @newinput = ();
        push(@newinput, qq($_ = $inputpairs->{$_})) for (sort keys(%$inputpairs));
        $inputref = \@newinput;
    } else {
        $inputref = $inputpairs;
    }
    my @manifestargs = @$inputpairs;
    my %arghash;
    foreach my $pairs (map split(/\s*;\s*(?=[\w\\\/]+\s*=)/, $_), @manifestargs) {
        my ($key, $val) = split(/\s*=\s*/, $pairs, 2);
        my $addkey = lc($key);
        $arghash{$addkey} = $val;
    }
    my $manifest = $self->get_manifest;
    $manifest->set_execution_level($arghash{executionlevel}) if exists($arghash{executionlevel});
    $manifest->set_uiaccess($arghash{uiaccess}) if exists($arghash{uiaccess});
    $manifest->set_assembly_name($arghash{execname}) if exists($arghash{execname});
    $manifest->set_assembly_description($arghash{description}) if exists($arghash{description});
    $manifest->set_assembly_version($arghash{version}) if exists($arghash{version});
    $manifest->add_common_controls() if $arghash{commoncontrols};
    
    $self->set_manifest($manifest);
}

sub get_manifest {
    my ($self) = @_;
    my $mtype = ($self->is_assembly) ? 'assembly' : 'application';
    my $mfestxml = $self->manifest->get_manifest;
    my $mfest = Win32::Exe::Manifest->new($mfestxml, $mtype);
    $mfest->set_resource_id( $self->manifest->get_manifest_id );
    return $mfest;
}

sub add_default_manifest {
    my ($self) = @_;
    my $rsrc = $self->resource_section;
    my $name = '/#RT_MANIFEST/#1/#0';
    $rsrc->remove("/#RT_MANIFEST");
    my $manifest = $self->require_class('Resource::Manifest')->new;
    my $xml = $manifest->default_manifest;
    $manifest->SetPathName( $name ); 
    $manifest->set_parent( $rsrc );
    $manifest->update_manifest( $xml );
    $rsrc->insert($manifest->PathName, $manifest);
    $rsrc->refresh;
}

sub merge_manifest {
    my ($self, $mnf) = @_;
    return if !(ref($mnf) && $mnf->isa('Win32::Exe::Manifest'));
    my $main = $self->get_manifest;
    $main->merge_manifest($mnf);
    $self->set_manifest($main);
}

sub set_subsystem_windows {
    my $self = shift;
    return if !$self->is_application;
    $self->SetSubsystem("windows")
}

sub set_subsystem_console {
    my $self = shift;
    return if !$self->is_application;
    $self->SetSubsystem("console")
}

sub get_subsystem { $_[0]->Subsystem; }

sub get_group_icon_names {
    my $self = shift;
    my @names = ();
    my $section = $self->resource_section or return @names;
    for my $resource ( $section->objects('GroupIcon')) {
        my $path = $resource->PathName;
        my($_null, $_rtgi, $name, $_langid) = split(/\//, $path);
        push(@names, $name);# if $resource->isa('Win32::Exe::Resource::GroupIcon');
    }
    return @names;
}

sub set_single_group_icon {
    my($self, $iconfile) = @_;
    my @icons = Win32::Exe::IconFile->new($iconfile)->icons;
    $self->set_icons(\@icons) if @icons;
}

sub get_group_icon {
    my ($self, $getname) = @_;
    return undef if !$getname;
    my $res = undef;
    my $section = $self->resource_section or return $res;
    $res = $self->_exists_group_icon($getname);
    return $res;
}

sub add_group_icon {
    my ($self, $newname, $filename) = @_;
    my $exists = 0;
    my $section = $self->resource_section or return;
    my ($res, $langid) = $self->_exists_group_icon($newname);
    return undef if $res; # it already exists
    
    my @icons = Win32::Exe::IconFile->new($filename)->icons;
    return if !(scalar @icons);
    
    my $group = $self->require_class('Resource::GroupIcon')->new;
    my $pathname = '/#RT_GROUP_ICON/' . $newname . '/' . $langid;
    $group->SetPathName($pathname);
    $group->set_parent($section);
    $section->insert($group->PathName, $group);
    $group->set_icons(\@icons);
    $group->refresh;
}

sub replace_group_icon {
    my ($self, $getname, $filename) = @_;
    my $section = $self->resource_section or return;
    my @icons = Win32::Exe::IconFile->new($filename)->icons;
    return if !(scalar @icons);
    my $group = $self->get_group_icon($getname) or return;
    my $pathname = $group->PathName;
    $section->remove($pathname);
    my $newgroup = $self->require_class('Resource::GroupIcon')->new;
    $newgroup->SetPathName($pathname);
    $newgroup->set_parent($section);
    $section->insert($newgroup->PathName, $newgroup);
    $newgroup->set_icons(\@icons);
    $newgroup->refresh;
}

sub remove_group_icon {
    my ($self, $matchname) = @_;
    my $section = $self->resource_section or return;
    my $existing = $self->get_group_icon($matchname) or return;
    $section->remove($existing->PathName);
    $section->refresh;
}

sub _exists_group_icon {
    my ($self, $matchname) = @_;
    my $section = $self->resource_section or return;
    my $langid = '#0';
    my $res = undef;
    for my $resource ( $section->objects('GroupIcon')) {
        my $path = $resource->PathName;
        my($_null, $_rtgi, $name, $_langid) = split(/\//, $path);
        $langid = $_langid;
        if($name eq $matchname) {
            $res = $resource;
            last;
        }
    }
    return ( wantarray ) ? ( $res, $langid ) : $res;
}

sub export_group_icon {
    my ($self, $matchname, $filename) = @_;
    my $existing = $self->get_group_icon($matchname) or return;
    my $iconobject = Win32::Exe::IconFile->new();
    my @icons = $existing->icons;
    $iconobject->set_icons(\@icons);
    $iconobject->write_file($filename, $iconobject->dump);
}


1;

__END__

=head1 AUTHORS

Audrey Tang E<lt>cpan@audreyt.orgE<gt>

Mark Dootson E<lt>mdootson@cpan.orgE<gt>

Steffen Mueller E<lt>smueller@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2004-2007, 2010 by Audrey Tang E<lt>cpan@audreyt.orgE<gt>.

This program is free software; you can redistribute it and/or 
modify it under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
