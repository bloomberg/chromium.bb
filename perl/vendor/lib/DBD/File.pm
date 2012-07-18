# -*- perl -*-
#
#   DBD::File - A base class for implementing DBI drivers that
#               act on plain files
#
#  This module is currently maintained by
#
#      H.Merijn Brand & Jens Rehsack
#
#  The original author is Jochen Wiedmann.
#
#  Copyright (C) 2009,2010 by H.Merijn Brand & Jens Rehsack
#  Copyright (C) 2004 by Jeff Zucker
#  Copyright (C) 1998 by Jochen Wiedmann
#
#  All rights reserved.
#
#  You may distribute this module under the terms of either the GNU
#  General Public License or the Artistic License, as specified in
#  the Perl README file.

require 5.008;

use strict;
use warnings;

use DBI ();

package DBD::File;

use strict;
use warnings;

use base qw(DBI::DBD::SqlEngine);
use Carp;
use vars qw(@ISA $VERSION $drh);

$VERSION = "0.40";

$drh = undef;		# holds driver handle(s) once initialized

my %accessors = (
    get_meta   => "get_file_meta",
    set_meta   => "set_file_meta",
    clear_meta => "clear_file_meta",
    );

sub driver ($;$)
{
    my ($class, $attr) = @_;

    # Drivers typically use a singleton object for the $drh
    # We use a hash here to have one singleton per subclass.
    # (Otherwise DBD::CSV and DBD::DBM, for example, would
    # share the same driver object which would cause problems.)
    # An alternative would be not not cache the $drh here at all
    # and require that subclasses do that. Subclasses should do
    # their own caching, so caching here just provides extra safety.
    $drh->{$class} and return $drh->{$class};

    $attr ||= {};
    {	no strict "refs";
	unless ($attr->{Attribution}) {
	    $class eq "DBD::File" and
		$attr->{Attribution} = "$class by Jeff Zucker";
	    $attr->{Attribution} ||= ${$class . "::ATTRIBUTION"} ||
		"oops the author of $class forgot to define this";
	    }
	$attr->{Version} ||= ${$class . "::VERSION"};
	$attr->{Name} or ($attr->{Name} = $class) =~ s/^DBD\:\://;
	}

    $drh->{$class} = $class->SUPER::driver ($attr);

    my $prefix = DBI->driver_prefix ($class);
    if ($prefix) {
	my $dbclass = $class . "::db";
	while (my ($accessor, $funcname) = each %accessors) {
	    my $method = $prefix . $accessor;
	    $dbclass->can ($method) and next;
	    my $inject = sprintf <<'EOI', $dbclass, $method, $dbclass, $funcname;
sub %s::%s
{
    my $func = %s->can (q{%s});
    goto &$func;
    }
EOI
	    eval $inject;
	    $dbclass->install_method ($method);
	    }
	}

    # XXX inject DBD::XXX::Statement unless exists

    return $drh->{$class};
    } # driver

sub CLONE
{
    undef $drh;
    } # CLONE

# ====== DRIVER ================================================================

package DBD::File::dr;

use strict;
use warnings;

use vars qw(@ISA $imp_data_size);

@DBD::File::dr::ISA           = qw(DBI::DBD::SqlEngine::dr);
$DBD::File::dr::imp_data_size = 0;

sub dsn_quote
{
    my $str = shift;
    ref     $str and return "";
    defined $str or  return "";
    $str =~ s/([;:\\])/\\$1/g;
    return $str;
    } # dsn_quote

sub data_sources ($;$)
{
    my ($drh, $attr) = @_;
    my $dir = $attr && exists $attr->{f_dir}
	? $attr->{f_dir}
	: File::Spec->curdir ();
    my %attrs;
    $attr and %attrs = %$attr;
    delete $attrs{f_dir};
    my $dsnextra = join ";", map { $_ . "=" . dsn_quote ($attrs{$_}) } keys %attrs;
    my ($dirh) = Symbol::gensym ();
    unless (opendir $dirh, $dir) {
	$drh->set_err ($DBI::stderr, "Cannot open directory $dir: $!");
	return;
	}

    my ($file, @dsns, %names, $driver);
    $driver = $drh->{ImplementorClass} =~ m/^dbd\:\:([^\:]+)\:\:/i ? $1 : "File";

    while (defined ($file = readdir ($dirh))) {
	my $d = File::Spec->catdir ($dir, $file);
	# allow current dir ... it can be a data_source too
	$file ne File::Spec->updir () && -d $d and
	    push @dsns, "DBI:$driver:f_dir=" . dsn_quote ($d) . ($dsnextra ? ";$dsnextra" : "");
	}
    return @dsns;
    } # data_sources

sub disconnect_all
{
    } # disconnect_all

sub DESTROY
{
    undef;
    } # DESTROY

# ====== DATABASE ==============================================================

package DBD::File::db;

use strict;
use warnings;

use vars qw(@ISA $imp_data_size);

use Carp;
require File::Spec;
require Cwd;
use Scalar::Util qw(refaddr); # in CORE since 5.7.3

@DBD::File::db::ISA           = qw(DBI::DBD::SqlEngine::db);
$DBD::File::db::imp_data_size = 0;

sub set_versions
{
    my $dbh = shift;
    $dbh->{f_version} = $DBD::File::VERSION;

    return $dbh->SUPER::set_versions ();
    } # set_versions

sub init_valid_attributes
{
    my $dbh = shift;

    $dbh->{f_valid_attrs} = {
	f_version        => 1, # DBD::File version
	f_dir            => 1, # base directory
	f_ext            => 1, # file extension
	f_schema         => 1, # schema name
	f_meta           => 1, # meta data for tables
	f_meta_map       => 1, # mapping table for identifier case
	f_lock           => 1, # Table locking mode
	f_lockfile       => 1, # Table lockfile extension
	f_encoding       => 1, # Encoding of the file
	f_valid_attrs    => 1, # File valid attributes
	f_readonly_attrs => 1, # File readonly attributes
	};
    $dbh->{f_readonly_attrs} = {
	f_version        => 1, # DBD::File version
	f_valid_attrs    => 1, # File valid attributes
	f_readonly_attrs => 1, # File readonly attributes
	};

    return $dbh->SUPER::init_valid_attributes ();
    } # init_valid_attributes

sub init_default_attributes
{
    my ($dbh, $phase) = @_;

    # must be done first, because setting flags implicitly calls $dbdname::db->STORE
    $dbh->SUPER::init_default_attributes ($phase);

    # DBI::BD::SqlEngine::dr::connect will detect old-style drivers and
    # don't call twice
    unless (defined $phase) {
        # we have an "old" driver here
        $phase = defined $dbh->{sql_init_phase};
	$phase and $phase = $dbh->{sql_init_phase};
	}

    if (0 == $phase) {
	# check whether we're running in a Gofer server or not (see
	# validate_FETCH_attr for details)
	$dbh->{f_in_gofer} = (defined $INC{"DBD/Gofer.pm"} && (caller(5))[0] eq "DBI::Gofer::Execute");
	# f_ext should not be initialized
	# f_map is deprecated (but might return)
	$dbh->{f_dir}      = Cwd::abs_path (File::Spec->curdir ());
	$dbh->{f_meta}     = {};
	$dbh->{f_meta_map} = {}; # choose new name because it contains other keys

	# complete derived attributes, if required
	(my $drv_class = $dbh->{ImplementorClass}) =~ s/::db$//;
	my $drv_prefix = DBI->driver_prefix ($drv_class);
	my $valid_attrs = $drv_prefix . "valid_attrs";
	my $ro_attrs = $drv_prefix . "readonly_attrs";

	my @comp_attrs = ();
	if (exists $dbh->{$drv_prefix . "meta"} and !$dbh->{f_in_gofer}) {
	    my $attr = $dbh->{$drv_prefix . "meta"};
	    defined $attr and defined $dbh->{$valid_attrs} and
		!defined $dbh->{$valid_attrs}{$attr} and
		$dbh->{$valid_attrs}{$attr} = 1;

	    my %h;
	    tie %h, "DBD::File::TieTables", $dbh;
	    $dbh->{$attr} = \%h;

	    push @comp_attrs, "meta";
	    }

	foreach my $comp_attr (@comp_attrs) {
	    my $attr = $drv_prefix . $comp_attr;
	    defined $dbh->{$valid_attrs} and !defined $dbh->{$valid_attrs}{$attr} and
		$dbh->{$valid_attrs}{$attr} = 1;
	    defined $dbh->{$ro_attrs} and !defined $dbh->{$ro_attrs}{$attr} and
		$dbh->{$ro_attrs}{$attr} = 1;
	    }
	}

    return $dbh;
    } # init_default_attributes

sub disconnect ($)
{
    %{$_[0]->{f_meta}} = ();
    return $_[0]->SUPER::disconnect ();
    } # disconnect

sub validate_FETCH_attr
{
    my ($dbh, $attrib) = @_;

    # If running in a Gofer server, access to our tied compatibility hash
    # would force Gofer to serialize the tieing object including it's
    # private $dbh reference used to do the driver function calls.
    # This will result in nasty exceptions. So return a copy of the
    # f_meta structure instead, which is the source of for the compatibility
    # tie-hash. It's not as good as liked, but the best we can do in this
    # situation.
    if ($dbh->{f_in_gofer}) {
	(my $drv_class = $dbh->{ImplementorClass}) =~ s/::db$//;
	my $drv_prefix = DBI->driver_prefix ($drv_class);
	exists $dbh->{$drv_prefix . "meta"} && $attrib eq $dbh->{$drv_prefix . "meta"} and
	    $attrib = "f_meta";
	}

    return $attrib;
    } # validate_FETCH_attr

sub validate_STORE_attr
{
    my ($dbh, $attrib, $value) = @_;

    if ($attrib eq "f_dir") {
	-d $value or
	    return $dbh->set_err ($DBI::stderr, "No such directory '$value'");
	File::Spec->file_name_is_absolute ($value) or
	    $value = Cwd::abs_path ($value);
	}

    if ($attrib eq "f_ext") {
	$value eq "" || $value =~ m{^\.\w+(?:/[rR]*)?$} or
	    carp "'$value' doesn't look like a valid file extension attribute\n";
	}

    (my $drv_class = $dbh->{ImplementorClass}) =~ s/::db$//;
    my $drv_prefix = DBI->driver_prefix ($drv_class);

    if (exists $dbh->{$drv_prefix . "meta"}) {
	my $attr = $dbh->{$drv_prefix . "meta"};
	if ($attrib eq $attr) {
	    while (my ($k, $v) = each %$value) {
		$dbh->{$attrib}{$k} = $v;
		}
	    }
	}

    return $dbh->SUPER::validate_STORE_attr ($attrib, $value);
    } # validate_STORE_attr

sub get_f_versions
{
    my ($dbh, $table) = @_;

    my $class = $dbh->{ImplementorClass};
    $class =~ s/::db$/::Table/;
    my (undef, $meta);
    $table and (undef, $meta) = $class->get_table_meta ($dbh, $table, 1);
    unless ($meta) {
	$meta = {};
	$class->bootstrap_table_meta ($dbh, $meta, $table);
	}

    my $dver;
    my $dtype = "IO::File";
    eval {
	$dver = IO::File->VERSION ();

	# when we're still alive here, everthing went ok - no need to check for $@
	$dtype .= " ($dver)";
	};

    $meta->{f_encoding} and $dtype .= " + " . $meta->{f_encoding} . " encoding";

    return sprintf "%s using %s", $dbh->{f_version}, $dtype;
    } # get_f_versions

sub get_single_table_meta
{
    my ($dbh, $table, $attr) = @_;
    my $meta;

    $table eq "." and
	return $dbh->FETCH ($attr);

    (my $class = $dbh->{ImplementorClass}) =~ s/::db$/::Table/;
    (undef, $meta) = $class->get_table_meta ($dbh, $table, 1);
    $meta or croak "No such table '$table'";

    # prevent creation of undef attributes
    return $class->get_table_meta_attr ($meta, $attr);
    } # get_single_table_meta

sub get_file_meta
{
    my ($dbh, $table, $attr) = @_;

    my $gstm = $dbh->{ImplementorClass}->can ("get_single_table_meta");

    $table eq "*" and
	$table = [ ".", keys %{$dbh->{f_meta}} ];
    $table eq "+" and
	$table = [ grep { m/^[_A-Za-z0-9]+$/ } keys %{$dbh->{f_meta}} ];
    ref $table eq "Regexp" and
	$table = [ grep { $_ =~ $table } keys %{$dbh->{f_meta}} ];

    ref $table || ref $attr or
	return &$gstm ($dbh, $table, $attr);

    ref $table or $table = [ $table ];
    ref $attr  or $attr  = [ $attr  ];
    "ARRAY" eq ref $table or
	croak "Invalid argument for \$table - SCALAR, Regexp or ARRAY expected but got " . ref $table;
    "ARRAY" eq ref $attr or
	croak "Invalid argument for \$attr - SCALAR or ARRAY expected but got " . ref $attr;

    my %results;
    foreach my $tname (@{$table}) {
	my %tattrs;
	foreach my $aname (@{$attr}) {
	    $tattrs{$aname} = &$gstm ($dbh, $tname, $aname);
	    }
	$results{$tname} = \%tattrs;
	}

    return \%results;
    } # get_file_meta

sub set_single_table_meta
{
    my ($dbh, $table, $attr, $value) = @_;
    my $meta;

    $table eq "." and
	return $dbh->STORE ($attr, $value);

    (my $class = $dbh->{ImplementorClass}) =~ s/::db$/::Table/;
    (undef, $meta) = $class->get_table_meta ($dbh, $table, 1);
    $meta or croak "No such table '$table'";
    $class->set_table_meta_attr ($meta, $attr, $value);

    return $dbh;
    } # set_single_table_meta

sub set_file_meta
{
    my ($dbh, $table, $attr, $value) = @_;

    my $sstm = $dbh->{ImplementorClass}->can ("set_single_table_meta");

    $table eq "*" and
	$table = [ ".", keys %{$dbh->{f_meta}} ];
    $table eq "+" and
	$table = [ grep { m/^[_A-Za-z0-9]+$/ } keys %{$dbh->{f_meta}} ];
    ref ($table) eq "Regexp" and
	$table = [ grep { $_ =~ $table } keys %{$dbh->{f_meta}} ];

    ref $table || ref $attr or
	return &$sstm ($dbh, $table, $attr, $value);

    ref $table or $table = [ $table ];
    ref $attr  or $attr  = { $attr => $value };
    "ARRAY" eq ref $table or
	croak "Invalid argument for \$table - SCALAR, Regexp or ARRAY expected but got " . ref $table;
    "HASH" eq ref $attr or
	croak "Invalid argument for \$attr - SCALAR or HASH expected but got " . ref $attr;

    foreach my $tname (@{$table}) {
	my %tattrs;
	while (my ($aname, $aval) = each %$attr) {
	    &$sstm ($dbh, $tname, $aname, $aval);
	    }
	}

    return $dbh;
    } # set_file_meta

sub clear_file_meta
{
    my ($dbh, $table) = @_;

    (my $class = $dbh->{ImplementorClass}) =~ s/::db$/::Table/;
    my (undef, $meta) = $class->get_table_meta ($dbh, $table, 1);
    $meta and %{$meta} = ();

    return;
    } # clear_file_meta

sub get_avail_tables
{
    my $dbh = shift;

    my @tables = $dbh->SUPER::get_avail_tables ();
    my $dir    = $dbh->{f_dir};
    my $dirh   = Symbol::gensym ();

    unless (opendir $dirh, $dir) {
	$dbh->set_err ($DBI::stderr, "Cannot open directory $dir: $!");
	return @tables;
	}

    my $class = $dbh->FETCH ("ImplementorClass");
    $class =~ s/::db$/::Table/;
    my ($file, %names);
    my $schema = exists $dbh->{f_schema}
	? defined $dbh->{f_schema} && $dbh->{f_schema} ne ""
	    ? $dbh->{f_schema} : undef
	: eval { getpwuid ((stat $dir)[4]) }; # XXX Win32::pwent
    my %seen;
    while (defined ($file = readdir ($dirh))) {
	my ($tbl, $meta) = $class->get_table_meta ($dbh, $file, 0, 0) or next; # XXX
	# $tbl && $meta && -f $meta->{f_fqfn} or next;
	$seen{defined $schema ? $schema : "\0"}{$tbl}++ or
	    push @tables, [ undef, $schema, $tbl, "TABLE", "FILE" ];
	}
    closedir $dirh or
	$dbh->set_err ($DBI::stderr, "Cannot close directory $dir: $!");

    return @tables;
    } # get_avail_tables

# ====== Tie-Meta ==============================================================

package DBD::File::TieMeta;

use Carp qw(croak);
require Tie::Hash;
@DBD::File::TieMeta::ISA = qw(Tie::Hash);

sub TIEHASH
{
    my ($class, $tblClass, $tblMeta) = @_;

    my $self = bless ({ tblClass => $tblClass, tblMeta => $tblMeta, }, $class);
    return $self;
    } # new

sub STORE
{
    my ($self, $meta_attr, $meta_val) = @_;

    $self->{tblClass}->set_table_meta_attr ($self->{tblMeta}, $meta_attr, $meta_val);

    return;
    } # STORE

sub FETCH
{
    my ($self, $meta_attr) = @_;

    return $self->{tblClass}->get_table_meta_attr ($self->{tblMeta}, $meta_attr);
    } # FETCH

sub FIRSTKEY
{
    my $a = scalar keys %{$_[0]->{tblMeta}};
    each %{$_[0]->{tblMeta}};
    } # FIRSTKEY

sub NEXTKEY
{
    each %{$_[0]->{tblMeta}};
    } # NEXTKEY

sub EXISTS
{
    exists $_[0]->{tblMeta}{$_[1]};
    } # EXISTS

sub DELETE
{
    croak "Can't delete single attributes from table meta structure";
    } # DELETE

sub CLEAR
{
    %{$_[0]->{tblMeta}} = ()
    } # CLEAR

sub SCALAR
{
    scalar %{$_[0]->{tblMeta}}
    } # SCALAR

# ====== Tie-Tables ============================================================

package DBD::File::TieTables;

use Carp qw(croak);
require Tie::Hash;
@DBD::File::TieTables::ISA = qw(Tie::Hash);

sub TIEHASH
{
    my ($class, $dbh) = @_;

    (my $tbl_class = $dbh->{ImplementorClass}) =~ s/::db$/::Table/;
    my $self = bless ({ dbh => $dbh, tblClass => $tbl_class, }, $class);
    return $self;
    } # new

sub STORE
{
    my ($self, $table, $tbl_meta) = @_;

    "HASH" eq ref $tbl_meta or
        croak "Invalid data for storing as table meta data (must be hash)";

    (undef, my $meta) = $self->{tblClass}->get_table_meta ($self->{dbh}, $table, 1);
    $meta or croak "Invalid table name '$table'";

    while (my ($meta_attr, $meta_val) = each %$tbl_meta) {
	$self->{tblClass}->set_table_meta_attr ($meta, $meta_attr, $meta_val);
	}

    return;
    } # STORE

sub FETCH
{
    my ($self, $table) = @_;

    (undef, my $meta) = $self->{tblClass}->get_table_meta ($self->{dbh}, $table, 1);
    $meta or croak "Invalid table name '$table'";

    my %h;
    tie %h, "DBD::File::TieMeta", $self->{tblClass}, $meta;

    return \%h;
    } # FETCH

sub FIRSTKEY
{
    my $a = scalar keys %{$_[0]->{dbh}->{f_meta}};
    each %{$_[0]->{dbh}->{f_meta}};
    } # FIRSTKEY

sub NEXTKEY
{
    each %{$_[0]->{dbh}->{f_meta}};
    } # NEXTKEY

sub EXISTS
{
    exists $_[0]->{dbh}->{f_meta}->{$_[1]} or
	exists $_[0]->{dbh}->{f_meta_map}->{$_[1]};
    } # EXISTS

sub DELETE
{
    my ($self, $table) = @_;

    (undef, my $meta) = $self->{tblClass}->get_table_meta ($self->{dbh}, $table, 1);
    $meta or croak "Invalid table name '$table'";

    delete $_[0]->{dbh}->{f_meta}->{$meta->{table_name}};
    } # DELETE

sub CLEAR
{
    %{$_[0]->{dbh}->{f_meta}} = ();
    %{$_[0]->{dbh}->{f_meta_map}} = ();
    } # CLEAR

sub SCALAR
{
    scalar %{$_[0]->{dbh}->{f_meta}}
    } # SCALAR

# ====== STATEMENT =============================================================

package DBD::File::st;

use strict;
use warnings;

use vars qw(@ISA $imp_data_size);

@DBD::File::st::ISA           = qw(DBI::DBD::SqlEngine::st);
$DBD::File::st::imp_data_size = 0;

my %supported_attrs = (
    TYPE      => 1,
    PRECISION => 1,
    NULLABLE  => 1,
    );

sub FETCH
{
    my ($sth, $attr) = @_;

    if ($supported_attrs{$attr}) {
	my $stmt = $sth->{sql_stmt};

	if (exists $sth->{ImplementorClass} &&
	    exists $sth->{sql_stmt} &&
	    $sth->{sql_stmt}->isa ("SQL::Statement")) {

	    # fill overall_defs unless we know
	    unless (exists $sth->{f_overall_defs} && ref $sth->{f_overall_defs}) {
		my $all_meta =
		    $sth->{Database}->func ("*", "table_defs", "get_file_meta");
		while (my ($tbl, $meta) = each %$all_meta) {
		    exists $meta->{table_defs} && ref $meta->{table_defs} or next;
		    foreach (keys %{$meta->{table_defs}{columns}}) {
			$sth->{f_overall_defs}{$_} = $meta->{table_defs}{columns}{$_};
			}
		    }
		}

	    my @colnames = $sth->sql_get_colnames ();

	    $attr eq "TYPE"      and
		return [ map { $sth->{f_overall_defs}{$_}{data_type}   || "CHAR" }
			    @colnames ];

	    $attr eq "PRECISION" and
		return [ map { $sth->{f_overall_defs}{$_}{data_length} || 0 }
			    @colnames ];

	    $attr eq "NULLABLE"  and
		return [ map { ( grep m/^NOT NULL$/ =>
			    @{ $sth->{f_overall_defs}{$_}{constraints} || [] })
			       ? 0 : 1 }
			    @colnames ];
	    }
	}

    return $sth->SUPER::FETCH ($attr);
    } # FETCH

# ====== SQL::STATEMENT ========================================================

package DBD::File::Statement;

use strict;
use warnings;

@DBD::File::Statement::ISA = qw( DBI::DBD::SqlEngine::Statement );

sub open_table ($$$$$)
{
    my ($self, $data, $table, $createMode, $lockMode) = @_;

    my $class = ref $self;
    $class =~ s/::Statement/::Table/;

    my $flags = {
	createMode	=> $createMode,
	lockMode	=> $lockMode,
	};
    $self->{command} eq "DROP" and $flags->{dropMode} = 1;

    return $class->new ($data, { table => $table }, $flags);
    } # open_table

# ====== SQL::TABLE ============================================================

package DBD::File::Table;

use strict;
use warnings;

use Carp;
require IO::File;
require File::Basename;
require File::Spec;
require Cwd;

# We may have a working flock () built-in but that doesn't mean that locking
# will work on NFS (flock () may hang hard)
my $locking = eval { flock STDOUT, 0; 1 };

@DBD::File::Table::ISA = qw( DBI::DBD::SqlEngine::Table );

# ====== FLYWEIGHT SUPPORT =====================================================

my $fn_any_ext_regex = qr/\.[^.]*/;

# Flyweight support for table_info
# The functions file2table, init_table_meta, default_table_meta and
# get_table_meta are using $self arguments for polymorphism only. The
# must not rely on an instantiated DBD::File::Table
sub file2table
{
    my ($self, $meta, $file, $file_is_table, $respect_case) = @_;

    $file eq "." || $file eq ".."	and return; # XXX would break a possible DBD::Dir

    my ($ext, $req) = ("", 0);
    if ($meta->{f_ext}) {
	($ext, my $opt) = split m/\//, $meta->{f_ext};
	if ($ext && $opt) {
	    $opt =~ m/r/i and $req = 1;
	    }
	}

    # (my $tbl = $file) =~ s/$ext$//i;
    my ($tbl, $basename, $dir, $fn_ext, $user_spec_file);
    if ($file_is_table and defined $meta->{f_file}) {
	$tbl = $file;
	($basename, $dir, $fn_ext) = File::Basename::fileparse ($meta->{f_file}, $fn_any_ext_regex);
	$file = $basename . $fn_ext;
	$user_spec_file = 1;
	}
    else {
	($basename, $dir, undef) = File::Basename::fileparse ($file, $ext);
	$file = $tbl = $basename;
	$user_spec_file = 0;
	}

    if (!$respect_case and $meta->{sql_identifier_case} == 1) { # XXX SQL_IC_UPPER
        $basename = uc $basename;
        $tbl = uc $tbl;
	}
    if( !$respect_case and $meta->{sql_identifier_case} == 2) { # XXX SQL_IC_LOWER
        $basename = lc $basename;
        $tbl = lc $tbl;
	}

    my $searchdir = File::Spec->file_name_is_absolute ($dir)
	? ($dir =~ s|/$||, $dir)
	: Cwd::abs_path (File::Spec->catdir ($meta->{f_dir}, $dir));
    -d $searchdir or
	croak "-d $searchdir: $!";

    $searchdir eq $meta->{f_dir} and
	$dir = "";

    unless ($user_spec_file) {
	$file_is_table and $file = "$basename$ext";

	# Fully Qualified File Name
	my $cmpsub;
	if ($respect_case) {
	    $cmpsub = sub {
		my ($fn, undef, $sfx) = File::Basename::fileparse ($_, $fn_any_ext_regex);
		$sfx = '' if $^O eq 'VMS' and $sfx eq '.';  # no extension turns up as a dot
		$fn eq $basename and
		    return (lc $sfx eq lc $ext or !$req && !$sfx);
		return 0;
		}
	    }
	else {
	    $cmpsub = sub {
		my ($fn, undef, $sfx) = File::Basename::fileparse ($_, $fn_any_ext_regex);
		$sfx = '' if $^O eq 'VMS' and $sfx eq '.';  # no extension turns up as a dot
		lc $fn eq lc $basename and
		    return (lc $sfx eq lc $ext or !$req && !$sfx);
		return 0;
		}
	    }

	opendir my $dh, $searchdir or croak "Can't open '$searchdir': $!";
	my @f = sort { length $b <=> length $a } grep { &$cmpsub ($_) } readdir $dh;
	@f > 0 && @f <= 2 and $file = $f[0];
	!$respect_case && $meta->{sql_identifier_case} == 4 and # XXX SQL_IC_MIXED
	    ($tbl = $file) =~ s/$ext$//i;
	closedir $dh or croak "Can't close '$searchdir': $!";

	my $tmpfn = $file;
	if ($ext && $req) {
            # File extension required
            $tmpfn =~ s/$ext$//i or return;
            }
	}

    my $fqfn = File::Spec->catfile ($searchdir, $file);
    my $fqbn = File::Spec->catfile ($searchdir, $basename);

    $meta->{f_fqfn} = $fqfn;
    $meta->{f_fqbn} = $fqbn;
    defined $meta->{f_lockfile} && $meta->{f_lockfile} and
	$meta->{f_fqln} = $meta->{f_fqbn} . $meta->{f_lockfile};

    $dir && !$user_spec_file  and $tbl = File::Spec->catfile ($dir, $tbl);
    $meta->{table_name} = $tbl;

    return $tbl;
    } # file2table

sub bootstrap_table_meta
{
    my ($self, $dbh, $meta, $table) = @_;

    exists  $meta->{f_dir}	or $meta->{f_dir}	= $dbh->{f_dir};
    defined $meta->{f_ext}	or $meta->{f_ext}	= $dbh->{f_ext};
    defined $meta->{f_encoding}	or $meta->{f_encoding}	= $dbh->{f_encoding};
    exists  $meta->{f_lock}	or $meta->{f_lock}	= $dbh->{f_lock};
    exists  $meta->{f_lockfile}	or $meta->{f_lockfile}	= $dbh->{f_lockfile};
    defined $meta->{f_schema}	or $meta->{f_schema}	= $dbh->{f_schema};
    defined $meta->{sql_identifier_case} or
        $meta->{sql_identifier_case} = $dbh->{sql_identifier_case};
    } # bootstrap_table_meta

sub init_table_meta
{
    my ($self, $dbh, $meta, $table) = @_;

    return;
    } # init_table_meta

sub get_table_meta ($$$$;$)
{
    my ($self, $dbh, $table, $file_is_table, $respect_case) = @_;
    unless (defined $respect_case) {
	$respect_case = 0;
	$table =~ s/^\"// and $respect_case = 1;    # handle quoted identifiers
	$table =~ s/\"$//;
	}

    unless ($respect_case) {
	defined $dbh->{f_meta_map}{$table} and $table = $dbh->{f_meta_map}{$table};
	}

    my $meta = {};
    defined $dbh->{f_meta}{$table} and $meta = $dbh->{f_meta}{$table};

    unless ($meta->{initialized}) {
	$self->bootstrap_table_meta ($dbh, $meta, $table);

	unless (defined $meta->{f_fqfn}) {
	    $self->file2table ($meta, $table, $file_is_table, $respect_case) or return;
	    }

	if (defined $meta->{table_name} and $table ne $meta->{table_name}) {
	    $dbh->{f_meta_map}{$table} = $meta->{table_name};
	    $table = $meta->{table_name};
	    }

	# now we know a bit more - let's check if user can't use consequent spelling
	# XXX add know issue about reset sql_identifier_case here ...
	if (defined $dbh->{f_meta}{$table} && defined $dbh->{f_meta}{$table}{initialized}) {
	    $meta = $dbh->{f_meta}{$table};
	    $self->file2table ($meta, $table, $file_is_table, $respect_case) or
		return unless $dbh->{f_meta}{$table}{initialized};
	    }
	unless ($dbh->{f_meta}{$table}{initialized}) {
	    $self->init_table_meta ($dbh, $meta, $table);
	    $meta->{initialized} = 1;
	    $dbh->{f_meta}{$table} = $meta;
	    }
	}

    return ($table, $meta);
    } # get_table_meta

my %reset_on_modify = (
    f_file     => "f_fqfn",
    f_dir      => "f_fqfn",
    f_ext      => "f_fqfn",
    f_lockfile => "f_fqfn", # forces new file2table call
    );

my %compat_map = map { $_ => "f_$_" } qw( file ext lock lockfile );

sub register_reset_on_modify
{
    my ($proto, $extra_resets) = @_;
    %reset_on_modify = (%reset_on_modify, %$extra_resets);
    return;
    } # register_reset_on_modify

sub register_compat_map
{
    my ($proto, $extra_compat_map) = @_;
    %compat_map = (%compat_map, %$extra_compat_map);
    return;
    } # register_compat_map

sub get_table_meta_attr
{
    my ($class, $meta, $attrib) = @_;
    exists $compat_map{$attrib} and
	$attrib = $compat_map{$attrib};
    exists $meta->{$attrib} and
	return $meta->{$attrib};
    return;
    } # get_table_meta_attr

sub set_table_meta_attr
{
    my ($class, $meta, $attrib, $value) = @_;
    exists $compat_map{$attrib} and
	$attrib = $compat_map{$attrib};
    $class->table_meta_attr_changed ($meta, $attrib, $value);
    $meta->{$attrib} = $value;
    } # set_table_meta_attr

sub table_meta_attr_changed
{
    my ($class, $meta, $attrib, $value) = @_;
    defined $reset_on_modify{$attrib} and
	delete $meta->{$reset_on_modify{$attrib}} and
	$meta->{initialized} = 0;
    } # table_meta_attr_changed

# ====== FILE OPEN =============================================================

sub open_file ($$$)
{
    my ($self, $meta, $attrs, $flags) = @_;

    defined $meta->{f_fqfn} && $meta->{f_fqfn} ne "" or croak "No filename given";

    my ($fh, $fn);
    unless ($meta->{f_dontopen}) {
	$fn = $meta->{f_fqfn};
	if ($flags->{createMode}) {
	    -f $meta->{f_fqfn} and
		croak "Cannot create table $attrs->{table}: Already exists";
	    $fh = IO::File->new ($fn, "a+") or
		croak "Cannot open $fn for writing: $! (" . ($!+0) . ")";
	    }
	else {
	    unless ($fh = IO::File->new ($fn, ($flags->{lockMode} ? "r+" : "r"))) {
		croak "Cannot open $fn: $! (" . ($!+0) . ")";
		}
	    }

	if ($fh) {
	    $fh->seek (0, 0) or
		croak "Error while seeking back: $!";
	    if (my $enc = $meta->{f_encoding}) {
		binmode $fh, ":encoding($enc)" or
		    croak "Failed to set encoding layer '$enc' on $fn: $!";
		}
	    else {
		binmode $fh or croak "Failed to set binary mode on $fn: $!";
		}
	    }

	$meta->{fh} = $fh;
	}
    if ($meta->{f_fqln}) {
	$fn = $meta->{f_fqln};
	if ($flags->{createMode}) {
	    -f $fn and
		croak "Cannot create table lock for $attrs->{table}: Already exists";
	    $fh = IO::File->new ($fn, "a+") or
		croak "Cannot open $fn for writing: $! (" . ($!+0) . ")";
	    }
	else {
	    unless ($fh = IO::File->new ($fn, ($flags->{lockMode} ? "r+" : "r"))) {
		croak "Cannot open $fn: $! (" . ($!+0) . ")";
		}
	    }

	$meta->{lockfh} = $fh;
	}

    if ($locking && $fh) {
	my $lm = defined $flags->{f_lock}
		      && $flags->{f_lock} =~ m/^[012]$/
		       ? $flags->{f_lock}
		       : $flags->{lockMode} ? 2 : 1;
	if ($lm == 2) {
	    flock $fh, 2 or croak "Cannot obtain exclusive lock on $fn: $!";
	    }
	elsif ($lm == 1) {
	    flock $fh, 1 or croak "Cannot obtain shared lock on $fn: $!";
	    }
	# $lm = 0 is forced no locking at all
	}
    } # open_file

# ====== SQL::Eval API =========================================================

sub new
{
    my ($className, $data, $attrs, $flags) = @_;
    my $dbh = $data->{Database};

    my ($tblnm, $meta) = $className->get_table_meta ($dbh, $attrs->{table}, 1) or
        croak "Cannot find appropriate file for table '$attrs->{table}'";
    $attrs->{table} = $tblnm;

    # Being a bit dirty here, as SQL::Statement::Structure does not offer
    # me an interface to the data I want
    $flags->{createMode} && $data->{sql_stmt}{table_defs} and
	$meta->{table_defs} = $data->{sql_stmt}{table_defs};

    $className->open_file ($meta, $attrs, $flags);

    my $columns = {};
    my $array   = [];
    my $tbl     = {
	%{$attrs},
	meta          => $meta,
	col_names     => $meta->{col_names} || [],
	};
    return $className->SUPER::new ($tbl);
    } # new

sub drop ($)
{
    my ($self, $data) = @_;
    my $meta = $self->{meta};
    # We have to close the file before unlinking it: Some OS'es will
    # refuse the unlink otherwise.
    $meta->{fh} and $meta->{fh}->close ();
    $meta->{lockfh} and $meta->{lockfh}->close ();
    undef $meta->{fh};
    undef $meta->{lockfh};
    $meta->{f_fqfn} and unlink $meta->{f_fqfn};
    $meta->{f_fqln} and unlink $meta->{f_fqln};
    delete $data->{Database}{f_meta}{$self->{table}};
    return 1;
    } # drop

sub seek ($$$$)
{
    my ($self, $data, $pos, $whence) = @_;
    my $meta = $self->{meta};
    if ($whence == 0 && $pos == 0) {
	$pos = defined $meta->{first_row_pos} ? $meta->{first_row_pos} : 0;
	}
    elsif ($whence != 2 || $pos != 0) {
	croak "Illegal seek position: pos = $pos, whence = $whence";
	}

    $meta->{fh}->seek ($pos, $whence) or
	croak "Error while seeking in " . $meta->{f_fqfn} . ": $!";
    } # seek

sub truncate ($$)
{
    my ($self, $data) = @_;
    my $meta = $self->{meta};
    $meta->{fh}->truncate ($meta->{fh}->tell ()) or
	croak "Error while truncating " . $meta->{f_fqfn} . ": $!";
    return 1;
    } # truncate

sub DESTROY
{
    my $self = shift;
    my $meta = $self->{meta};
    $meta->{fh} and $meta->{fh}->close ();
    $meta->{lockfh} and $meta->{lockfh}->close ();
    undef $meta->{fh};
    undef $meta->{lockfh};
    } # DESTROY

1;

__END__

=head1 NAME

DBD::File - Base class for writing file based DBI drivers

=head1 SYNOPSIS

This module is a base class for writing other L<DBD|DBI::DBD>s.
It is not intended to function as a DBD itself (though it is possible).
If you want to access flat files, use L<DBD::AnyData|DBD::AnyData>, or
L<DBD::CSV|DBD::CSV> (both of which are subclasses of DBD::File).

=head1 DESCRIPTION

The DBD::File module is not a true L<DBI|DBI> driver, but an abstract
base class for deriving concrete DBI drivers from it. The implication
is, that these drivers work with plain files, for example CSV files or
INI files. The module is based on the L<SQL::Statement|SQL::Statement>
module, a simple SQL engine.

See L<DBI|DBI> for details on DBI, L<SQL::Statement|SQL::Statement> for
details on SQL::Statement and L<DBD::CSV|DBD::CSV>, L<DBD::DBM|DBD::DBM>
or L<DBD::AnyData|DBD::AnyData> for example drivers.

=head2 Metadata

The following attributes are handled by DBI itself and not by DBD::File,
thus they all work as expected:

    Active
    ActiveKids
    CachedKids
    CompatMode             (Not used)
    InactiveDestroy
    AutoInactiveDestroy
    Kids
    PrintError
    RaiseError
    Warn                   (Not used)

=head3 The following DBI attributes are handled by DBD::File:

=head4 AutoCommit

Always on.

=head4 ChopBlanks

Works.

=head4 NUM_OF_FIELDS

Valid after C<< $sth->execute >>.

=head4 NUM_OF_PARAMS

Valid after C<< $sth->prepare >>.

=head4 NAME

Valid after C<< $sth->execute >>; undef for Non-Select statements.

=head4 NULLABLE

Not really working, always returns an array ref of ones, except the
affected table has been created in this session.  Valid after
C<< $sth->execute >>; undef for non-select statements.

=head3 The following DBI attributes and methods are not supported:

=over 4

=item bind_param_inout

=item CursorName

=item LongReadLen

=item LongTruncOk

=back

=head3 DBD::File specific attributes

In addition to the DBI attributes, you can use the following dbh
attributes:

=head4 f_dir

This attribute is used for setting the directory where the files are
opened and it defaults to the current directory (F<.>). Usually you set
it on the dbh but it may be overridden per table (see L<f_meta>).

When the value for C<f_dir> is a relative path, it is converted into
the appropriate absolute path name (based on the current working
directory) when the dbh attribute is set.

See L<KNOWN BUGS AND LIMITATIONS>.

=head4 f_ext

This attribute is used for setting the file extension. The format is:

  extension{/flag}

where the /flag is optional and the extension is case-insensitive.
C<f_ext> allows you to specify an extension which:

=over

=item *

makes DBD::File prefer F<table.extension> over F<table>.

=item *

makes the table name the filename minus the extension.

=back

    DBI:CSV:f_dir=data;f_ext=.csv

In the above example and when C<f_dir> contains both F<table.csv> and
F<table>, DBD::File will open F<table.csv> and the table will be
named "table". If F<table.csv> does not exist but F<table> does
that file is opened and the table is also called "table".

If C<f_ext> is not specified and F<table.csv> exists it will be opened
and the table will be called "table.csv" which is probably not what
you want.

NOTE: even though extensions are case-insensitive, table names are
not.

    DBI:CSV:f_dir=data;f_ext=.csv/r

The C<r> flag means the file extension is required and any filename
that does not match the extension is ignored.

Usually you set it on the dbh but it may be overridden per table
(see L<f_meta>).

=head4 f_schema

This will set the schema name and defaults to the owner of the
directory in which the table file resides. You can set C<f_schema> to
C<undef>.

    my $dbh = DBI->connect ("dbi:CSV:", "", "", {
        f_schema => undef,
        f_dir    => "data",
        f_ext    => ".csv/r",
        }) or die $DBI::errstr;

By setting the schema you affect the results from the tables call:

    my @tables = $dbh->tables ();

    # no f_schema
    "merijn".foo
    "merijn".bar

    # f_schema => "dbi"
    "dbi".foo
    "dbi".bar

    # f_schema => undef
    foo
    bar

Defining C<f_schema> to the empty string is equal to setting it to C<undef>
so the DSN can be C<"dbi:CSV:f_schema=;f_dir=.">.

=head4 f_lock

The C<f_lock> attribute is used to set the locking mode on the opened
table files. Note that not all platforms support locking.  By default,
tables are opened with a shared lock for reading, and with an
exclusive lock for writing. The supported modes are:

  0: No locking at all.

  1: Shared locks will be used.

  2: Exclusive locks will be used.

But see L<KNOWN BUGS|/"KNOWN BUGS AND LIMITATIONS"> below.

=head4 f_lockfile

If you wish to use a lockfile extension other than C<.lck>, simply specify
the C<f_lockfile> attribute:

  $dbh = DBI->connect ("dbi:DBM:f_lockfile=.foo");
  $dbh->{f_lockfile} = ".foo";
  $dbh->{f_meta}{qux}{f_lockfile} = ".foo";

If you wish to disable locking, set the C<f_lockfile> to C<0>.

  $dbh = DBI->connect ("dbi:DBM:f_lockfile=0");
  $dbh->{f_lockfile} = 0;
  $dbh->{f_meta}{qux}{f_lockfile} = 0;

=head4 f_encoding

With this attribute, you can set the encoding in which the file is opened.
This is implemented using C<< binmode $fh, ":encoding(<f_encoding>)" >>.

=head4 f_meta

Private data area which contains information about the tables this
module handles. Table meta data might not be available until the
table has been accessed for the first time e.g., by issuing a select
on it however it is possible to pre-initialize attributes for each table
you use.

DBD::File recognizes the (public) attributes C<f_ext>, C<f_dir>,
C<f_file>, C<f_encoding>, C<f_lock>, C<f_lockfile>, C<f_schema>,
C<col_names>, C<table_name> and C<sql_identifier_case>. Be very careful
when modifying attributes you do not know, the consequence might be a
destroyed or corrupted table.

C<f_file> is an attribute applicable to table meta data only and you
will not find a corresponding attribute in the dbh. Whilst it may be
reasonable to have several tables with the same column names, it is
not for the same file name. If you need access to the same file using
different table names, use C<SQL::Statement> as the SQL engine and the
C<AS> keyword:

    SELECT * FROM tbl AS t1, tbl AS t2 WHERE t1.id = t2.id

C<f_file> can be an absolute path name or a relative path name but if
it is relative, it is interpreted as being relative to the C<f_dir>
attribute of the table meta data. When C<f_file> is set DBD::File will
use C<f_file> as specified and will not attempt to work out an
alternative for C<f_file> using the C<table name> and C<f_ext>
attribute.

While C<f_meta> is a private and readonly attribute (which means, you
cannot modify it's values), derived drivers might provide restricted
write access through another attribute. Well known accessors are
C<csv_tables> for L<DBD::CSV>, C<ad_tables> for L<DBD::AnyData> and
C<dbm_tables> for L<DBD::DBM>.

=head3 Internally private attributes to deal with SQL backends:

Do not modify any of these private attributes unless you understand
the implications of doing so. The behavior of DBD::File and derived
DBDs might be unpredictable when one or more of those attributes are
modified.

=head4 sql_nano_version

Contains the version of loaded DBI::SQL::Nano.

=head4 sql_statement_version

Contains the version of loaded SQL::Statement.

=head4 sql_handler

Contains either the text 'SQL::Statement' or 'DBI::SQL::Nano'.

=head4 sql_ram_tables

Contains optionally temporary tables.

=head4 sql_flags

Contains optional flags to instantiate the SQL::Parser parsing engine
when SQL::Statement is used as SQL engine. See L<SQL::Parser> for valid
flags.

=head2 Driver private methods

=head3 Default DBI methods

=head4 data_sources

The C<data_sources> method returns a list of subdirectories of the current
directory in the form "dbi:CSV:f_dir=$dirname".

If you want to read the subdirectories of another directory, use

    my ($drh)  = DBI->install_driver ("CSV");
    my (@list) = $drh->data_sources (f_dir => "/usr/local/csv_data");

=head4 list_tables

This method returns a list of file names inside $dbh->{f_dir}.
Example:

    my ($dbh)  = DBI->connect ("dbi:CSV:f_dir=/usr/local/csv_data");
    my (@list) = $dbh->func ("list_tables");

Note that the list includes all files contained in the directory, even
those that have non-valid table names, from the view of SQL.

=head3 Additional methods

The following methods are only available via their documented name when
DBD::File is used directly. Because this is only reasonable for testing
purposes, the real names must be used instead. Those names can be computed
by replacing the C<f_> in the method name with the driver prefix.

=head4 f_versions

Signature:

    sub f_versions (;$)
    {
	my ($table_name) = @_;
	$table_name ||= ".";
	...
    }

Returns the versions of the driver, including the DBI version, the Perl
version, DBI::PurePerl version (if DBI::PurePerl is active) and the version
of the SQL engine in use.

    my $dbh = DBI->connect ("dbi:File:");
    my $f_versions = $dbh->f_versions ();
    print "$f_versions\n";
    __END__
    # DBD::File        0.39 using SQL::Statement 1.28
    # DBI              1.612
    # OS               netbsd (5.99.24)
    # Perl             5.010001 (x86_64-netbsd-thread-multi)

Called in list context, f_versions will return an array containing each
line as single entry.

Some drivers might use the optional (table name) argument and modify
version information related to the table (e.g. DBD::DBM provides storage
backend information for the requested table, when it has a table name).

=head4 f_get_meta

Signature:

    sub f_get_meta ($$)
    {
	my ($table_name, $attrib) = @_;
	...
    }

Returns the value of a meta attribute set for a specific table, if any.
See L<f_meta> for the possible attributes.

A table name of C<"."> (single dot) is interpreted as the default table.
This will retrieve the appropriate attribute globally from the dbh.
This has the same restrictions as C<< $dbh->{$attrib} >>.

=head4 f_set_meta

Signature:

    sub f_set_meta ($$$)
    {
	my ($table_name, $attrib, $value) = @_;
	...
    }

Sets the value of a meta attribute set for a specific table.
See L<f_meta> for the possible attributes.

A table name of C<"."> (single dot) is interpreted as the default table
which will set the specified attribute globally for the dbh.
This has the same restrictions as C<< $dbh->{$attrib} = $value >>.

=head4 f_clear_meta

Signature:

    sub f_clear_meta ($)
    {
	my ($table_name) = @_;
	...
    }

Clears the table specific meta information in the private storage of the
dbh.

=head1 SQL ENGINES

DBD::File currently supports two SQL engines: L<SQL::Statement|SQL::Statement>
and L<DBI::SQL::Nano::Statement_|DBI::SQL::Nano>. DBI::SQL::Nano supports a
I<very> limited subset of SQL statements, but it might be faster for some
very simple tasks. SQL::Statement in contrast supports a much larger subset
of ANSI SQL.

To use SQL::Statement, you need at least version 1.28 of
SQL::Statement and the environment variable C<DBI_SQL_NANO> must not
be set to a true value.

=head1 KNOWN BUGS AND LIMITATIONS

=over 4

=item *

This module uses flock () internally but flock is not available on all
platforms. On MacOS and Windows 95 there is no locking at all (perhaps
not so important on MacOS and Windows 95, as there is only a single
user).

=item *

The module stores details about the handled tables in a private area
of the driver handle (C<$drh>). This data area is not shared between
different driver instances, so several C<< DBI->connect () >> calls will
cause different table instances and private data areas.

This data area is filled for the first time when a table is accessed,
either via an SQL statement or via C<table_info> and is not
destroyed until the table is dropped or the driver handle is released.
Manual destruction is possible via L<f_clear_meta>.

The following attributes are preserved in the data area and will
evaluated instead of driver globals:

=over 8

=item f_ext

=item f_dir

=item f_lock

=item f_lockfile

=item f_encoding

=item f_schema

=item col_names

=item sql_identifier_case

=back

The following attributes are preserved in the data area only and
cannot be set globally.

=over 8

=item f_file

=back

The following attributes are preserved in the data area only and are
computed when initializing the data area:

=over 8

=item f_fqfn

=item f_fqbn

=item f_fqln

=item table_name

=back

For DBD::CSV tables this means, once opened "foo.csv" as table named "foo",
another table named "foo" accessing the file "foo.txt" cannot be opened.
Accessing "foo" will always access the file "foo.csv" in memorized
C<f_dir>, locking C<f_lockfile> via memorized C<f_lock>.

You can use L<f_clear_meta> or the C<f_file> attribute for a specific table
to work around this.

=item *

When used with SQL::Statement and temporary tables e.g.,

  CREATE TEMP TABLE ...

the table data processing bypasses DBD::File::Table. No file system
calls will be made and there are no clashes with existing (file based)
tables with the same name. Temporary tables are chosen over file
tables, but they will not covered by C<table_info>.

=back

=head1 AUTHOR

This module is currently maintained by

H.Merijn Brand < h.m.brand at xs4all.nl > and
Jens Rehsack  < rehsack at googlemail.com >

The original author is Jochen Wiedmann.

=head1 COPYRIGHT AND LICENSE

 Copyright (C) 2009-2010 by H.Merijn Brand & Jens Rehsack
 Copyright (C) 2004-2009 by Jeff Zucker
 Copyright (C) 1998-2004 by Jochen Wiedmann

All rights reserved.

You may freely distribute and/or modify this module under the terms of
either the GNU General Public License (GPL) or the Artistic License, as
specified in the Perl README file.

=head1 SEE ALSO

L<DBI|DBI>, L<DBD::DBM|DBD::DBM>, L<DBD::CSV|DBD::CSV>, L<Text::CSV|Text::CSV>,
L<Text::CSV_XS|Text::CSV_XS>, L<SQL::Statement|SQL::Statement>, and
L<DBI::SQL::Nano|DBI::SQL::Nano>

=cut
