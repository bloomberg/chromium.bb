# -*- perl -*-
#
#   DBI::DBD::SqlEngine - A base class for implementing DBI drivers that
#               have not an own SQL engine
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

use DBI ();
require DBI::SQL::Nano;

package DBI::DBD::SqlEngine;

use strict;

use Carp;
use vars qw( @ISA $VERSION $drh %methods_installed);

$VERSION = "0.03";

$drh = undef;    # holds driver handle(s) once initialized

DBI->setup_driver("DBI::DBD::SqlEngine");    # only needed once but harmless to repeat

my %accessors = ( versions => "get_driver_versions", );

sub driver ($;$)
{
    my ( $class, $attr ) = @_;

    # Drivers typically use a singleton object for the $drh
    # We use a hash here to have one singleton per subclass.
    # (Otherwise DBD::CSV and DBD::DBM, for example, would
    # share the same driver object which would cause problems.)
    # An alternative would be not not cache the $drh here at all
    # and require that subclasses do that. Subclasses should do
    # their own caching, so caching here just provides extra safety.
    $drh->{$class} and return $drh->{$class};

    $attr ||= {};
    {
        no strict "refs";
        unless ( $attr->{Attribution} )
        {
            $class eq "DBI::DBD::SqlEngine"
              and $attr->{Attribution} = "$class by Jens Rehsack";
            $attr->{Attribution} ||= ${ $class . "::ATTRIBUTION" }
              || "oops the author of $class forgot to define this";
        }
        $attr->{Version} ||= ${ $class . "::VERSION" };
        $attr->{Name} or ( $attr->{Name} = $class ) =~ s/^DBD\:\://;
    }

    $drh->{$class} = DBI::_new_drh( $class . "::dr", $attr );
    $drh->{$class}->STORE( ShowErrorStatement => 1 );

    my $prefix = DBI->driver_prefix($class);
    if ($prefix)
    {
        my $dbclass = $class . "::db";
        while ( my ( $accessor, $funcname ) = each %accessors )
        {
            my $method = $prefix . $accessor;
            $dbclass->can($method) and next;
            my $inject = sprintf <<'EOI', $dbclass, $method, $dbclass, $funcname;
sub %s::%s
{
    my $func = %s->can (q{%s});
    goto &$func;
    }
EOI
            eval $inject;
            $dbclass->install_method($method);
        }
    }

    # XXX inject DBD::XXX::Statement unless exists

    my $stclass = $class . "::st";
    $stclass->install_method("sql_get_colnames") unless ( $methods_installed{$class}++ );

    return $drh->{$class};
}    # driver

sub CLONE
{
    undef $drh;
}    # CLONE

# ====== DRIVER ================================================================

package DBI::DBD::SqlEngine::dr;

use strict;
use warnings;

use vars qw(@ISA $imp_data_size);

$imp_data_size = 0;

sub connect ($$;$$$)
{
    my ( $drh, $dbname, $user, $auth, $attr ) = @_;

    # create a 'blank' dbh
    my $dbh = DBI::_new_dbh(
                             $drh,
                             {
                                Name         => $dbname,
                                USER         => $user,
                                CURRENT_USER => $user,
                             }
                           );

    if ($dbh)
    {
        # must be done first, because setting flags implicitly calls $dbdname::db->STORE
        $dbh->func( 0, "init_default_attributes" );
        my $two_phased_init;
	defined $dbh->{sql_init_phase} and $two_phased_init = ++$dbh->{sql_init_phase};
        my %second_phase_attrs;

        my ( $var, $val );
        while ( length $dbname )
        {
            if ( $dbname =~ s/^((?:[^\\;]|\\.)*?);//s )
            {
                $var = $1;
            }
            else
            {
                $var    = $dbname;
                $dbname = "";
            }
            if ( $var =~ m/^(.+?)=(.*)/s )
            {
                $var = $1;
                ( $val = $2 ) =~ s/\\(.)/$1/g;
                if ($two_phased_init)
                {
                    eval { $dbh->STORE( $var, $val ); };
                    $@ and $second_phase_attrs{$var} = $val;
                }
                else
                {
                    $dbh->STORE( $var, $val );
                }
            }
            elsif ( $var =~ m/^(.+?)=>(.*)/s )
            {
                $var = $1;
                ( $val = $2 ) =~ s/\\(.)/$1/g;
                my $ref = eval $val;
                $dbh->$var($ref);
            }
        }

        if ($two_phased_init)
        {
            foreach $a (qw(Profile RaiseError PrintError AutoCommit))
            {    # do these first
                exists $attr->{$a} or next;
                eval {
                    $dbh->{$a} = $attr->{$a};
                    delete $attr->{$a};
                };
                $@ and $second_phase_attrs{$a} = delete $attr->{$a};
            }
            while ( my ( $a, $v ) = each %$attr )
            {
                eval { $dbh->{$a} = $v };
                $@ and $second_phase_attrs{$a} = $v;
            }

            $dbh->func( 1, "init_default_attributes" );
            %$attr = %second_phase_attrs;
        }

	$dbh->func("init_done");

        $dbh->STORE( Active => 1 );
    }

    return $dbh;
}    # connect

sub disconnect_all
{
}    # disconnect_all

sub DESTROY
{
    undef;
}    # DESTROY

# ====== DATABASE ==============================================================

package DBI::DBD::SqlEngine::db;

use strict;
use warnings;

use vars qw(@ISA $imp_data_size);

use Carp;

if ( eval { require Clone; } )
{
    Clone->import("clone");
}
else
{
    require Storable;    # in CORE since 5.7.3
    *clone = \&Storable::dclone;
}

$imp_data_size = 0;

sub ping
{
    ( $_[0]->FETCH("Active") ) ? 1 : 0;
}    # ping

sub prepare ($$;@)
{
    my ( $dbh, $statement, @attribs ) = @_;

    # create a 'blank' sth
    my $sth = DBI::_new_sth( $dbh, { Statement => $statement } );

    if ($sth)
    {
        my $class = $sth->FETCH("ImplementorClass");
        $class =~ s/::st$/::Statement/;
        my $stmt;

        # if using SQL::Statement version > 1
        # cache the parser object if the DBD supports parser caching
        # SQL::Nano and older SQL::Statements don't support this

        if ( $class->isa("SQL::Statement") )
        {
            my $parser = $dbh->{sql_parser_object};
            $parser ||= eval { $dbh->func("sql_parser_object") };
            if ($@)
            {
                $stmt = eval { $class->new($statement) };
            }
            else
            {
                $stmt = eval { $class->new( $statement, $parser ) };
            }
        }
        else
        {
            $stmt = eval { $class->new($statement) };
        }
        if ($@ || $stmt->{errstr})
        {
            $dbh->set_err( $DBI::stderr, $@ || $stmt->{errstr} );
            undef $sth;
        }
        else
        {
            $sth->STORE( "sql_stmt", $stmt );
            $sth->STORE( "sql_params", [] );
            $sth->STORE( "NUM_OF_PARAMS", scalar( $stmt->params() ) );
            my @colnames = $sth->sql_get_colnames();
            $sth->STORE( "NUM_OF_FIELDS", scalar @colnames );
        }
    }
    return $sth;
}    # prepare

sub set_versions
{
    my $dbh = $_[0];
    $dbh->{sql_engine_version} = $DBI::DBD::SqlEngine::VERSION;
    for (qw( nano_version statement_version ))
    {
        defined $DBI::SQL::Nano::versions->{$_} or next;
        $dbh->{"sql_$_"} = $DBI::SQL::Nano::versions->{$_};
    }
    $dbh->{sql_handler} =
      $dbh->{sql_statement_version}
      ? "SQL::Statement"
      : "DBI::SQL::Nano";

    return $dbh;
}    # set_versions

sub init_valid_attributes
{
    my $dbh = $_[0];

    $dbh->{sql_valid_attrs} = {
                               sql_engine_version         => 1,    # DBI::DBD::SqlEngine version
                               sql_handler                => 1,    # Nano or S:S
                               sql_nano_version           => 1,    # Nano version
                               sql_statement_version      => 1,    # S:S version
                               sql_flags                  => 1,    # flags for SQL::Parser
                               sql_dialect                => 1,    # dialect for SQL::Parser
                               sql_quoted_identifier_case => 1,    # case for quoted identifiers
                               sql_identifier_case        => 1,    # case for non-quoted identifiers
                               sql_parser_object          => 1,    # SQL::Parser instance
                               sql_sponge_driver          => 1,    # Sponge driver for table_info ()
                               sql_valid_attrs            => 1,    # SQL valid attributes
                               sql_readonly_attrs         => 1,    # SQL readonly attributes
			       sql_init_phase             => 1,    # Only during initialization
                              };
    $dbh->{sql_readonly_attrs} = {
                               sql_engine_version         => 1,    # DBI::DBD::SqlEngine version
                               sql_handler                => 1,    # Nano or S:S
                               sql_nano_version           => 1,    # Nano version
                               sql_statement_version      => 1,    # S:S version
                               sql_quoted_identifier_case => 1,    # case for quoted identifiers
                               sql_parser_object          => 1,    # SQL::Parser instance
                               sql_sponge_driver          => 1,    # Sponge driver for table_info ()
                               sql_valid_attrs            => 1,    # SQL valid attributes
                               sql_readonly_attrs         => 1,    # SQL readonly attributes
                                 };

    return $dbh;
}    # init_valid_attributes

sub init_default_attributes
{
    my ( $dbh, $phase ) = @_;
    my $given_phase = $phase;

    unless ( defined($phase) )
    {
        # we have an "old" driver here
        $phase = defined $dbh->{sql_init_phase};
	$phase and $phase = $dbh->{sql_init_phase};
    }

    if ( 0 == $phase )
    {
        # must be done first, because setting flags implicitly calls $dbdname::db->STORE
        $dbh->func("init_valid_attributes");

        $dbh->func("set_versions");

        $dbh->{sql_identifier_case}        = 2;    # SQL_IC_LOWER
        $dbh->{sql_quoted_identifier_case} = 3;    # SQL_IC_SENSITIVE

	$dbh->{sql_dialect} = "CSV";

        $dbh->{sql_init_phase} = $given_phase;

        # complete derived attributes, if required
        ( my $drv_class = $dbh->{ImplementorClass} ) =~ s/::db$//;
        my $drv_prefix  = DBI->driver_prefix($drv_class);
        my $valid_attrs = $drv_prefix . "valid_attrs";
        my $ro_attrs    = $drv_prefix . "readonly_attrs";

        my @comp_attrs = qw(valid_attrs version readonly_attrs);

        foreach my $comp_attr (@comp_attrs)
        {
            my $attr = $drv_prefix . $comp_attr;
            defined $dbh->{$valid_attrs}
              and !defined $dbh->{$valid_attrs}{$attr}
              and $dbh->{$valid_attrs}{$attr} = 1;
            defined $dbh->{$ro_attrs}
              and !defined $dbh->{$ro_attrs}{$attr}
              and $dbh->{$ro_attrs}{$attr} = 1;
        }
    }

    return $dbh;
}    # init_default_attributes

sub init_done
{
    defined $_[0]->{sql_init_phase} and delete $_[0]->{sql_init_phase};
    delete $_[0]->{sql_valid_attrs}->{sql_init_phase};
    return;
}

sub sql_parser_object
{
    my $dbh = $_[0];
    my $dialect = $dbh->{sql_dialect} || "CSV";
    my $parser = {
                   RaiseError => $dbh->FETCH("RaiseError"),
                   PrintError => $dbh->FETCH("PrintError"),
                 };
    my $sql_flags = $dbh->FETCH("sql_flags") || {};
    %$parser = ( %$parser, %$sql_flags );
    $parser = SQL::Parser->new( $dialect, $parser );
    $dbh->{sql_parser_object} = $parser;
    return $parser;
}    # sql_parser_object

sub sql_sponge_driver
{
    my $dbh  = $_[0];
    my $dbh2 = $dbh->{sql_sponge_driver};
    unless ($dbh2)
    {
        $dbh2 = $dbh->{sql_sponge_driver} = DBI->connect("DBI:Sponge:");
        unless ($dbh2)
        {
            $dbh->set_err( $DBI::stderr, $DBI::errstr );
            return;
        }
    }
}

sub disconnect ($)
{
    $_[0]->STORE( Active => 0 );
    return 1;
}    # disconnect

sub validate_FETCH_attr
{
    my ( $dbh, $attrib ) = @_;

    return $attrib;
}

sub FETCH ($$)
{
    my ( $dbh, $attrib ) = @_;
    $attrib eq "AutoCommit"
      and return 1;

    # Driver private attributes are lower cased
    if ( $attrib eq ( lc $attrib ) )
    {
	# first let the implementation deliver an alias for the attribute to fetch
	# after it validates the legitimation of the fetch request
        $attrib = $dbh->func( $attrib, "validate_FETCH_attr" ) or return;

        my $attr_prefix;
        $attrib =~ m/^([a-z]+_)/ and $attr_prefix = $1;
        unless ($attr_prefix)
        {
            ( my $drv_class = $dbh->{ImplementorClass} ) =~ s/::db$//;
            $attr_prefix = DBI->driver_prefix($drv_class);
            $attrib      = $attr_prefix . $attrib;
        }
        my $valid_attrs = $attr_prefix . "valid_attrs";
        my $ro_attrs    = $attr_prefix . "readonly_attrs";

        exists $dbh->{$valid_attrs}
          and ( $dbh->{$valid_attrs}{$attrib}
                or return $dbh->set_err( $DBI::stderr, "Invalid attribute '$attrib'" ) );
        exists $dbh->{$ro_attrs}
          and $dbh->{$ro_attrs}{$attrib}
          and defined $dbh->{$attrib}
          and refaddr( $dbh->{$attrib} )
          and return clone( $dbh->{$attrib} );

        return $dbh->{$attrib};
    }
    # else pass up to DBI to handle
    return $dbh->SUPER::FETCH($attrib);
}    # FETCH

sub validate_STORE_attr
{
    my ( $dbh, $attrib, $value ) = @_;

    if (     $attrib eq "sql_identifier_case" || $attrib eq "sql_quoted_identifier_case"
         and $value < 1 || $value > 4 )
    {
        croak "attribute '$attrib' must have a value from 1 .. 4 (SQL_IC_UPPER .. SQL_IC_MIXED)";
        # XXX correctly a remap of all entries in f_meta/f_meta_map is required here
    }

    return ( $attrib, $value );
}

# the ::db::STORE method is what gets called when you set
# a lower-cased database handle attribute such as $dbh->{somekey}=$someval;
#
# STORE should check to make sure that "somekey" is a valid attribute name
# but only if it is really one of our attributes (starts with dbm_ or foo_)
# You can also check for valid values for the attributes if needed
# and/or perform other operations
#
sub STORE ($$$)
{
    my ( $dbh, $attrib, $value ) = @_;

    if ( $attrib eq "AutoCommit" )
    {
        $value and return 1;    # is already set
        croak "Can't disable AutoCommit";
    }

    if ( $attrib eq lc $attrib )
    {
        # Driver private attributes are lower cased

        my $attr_prefix;
        $attrib =~ m/^([a-z]+_)/ and $attr_prefix = $1;
        unless ($attr_prefix)
        {
            ( my $drv_class = $dbh->{ImplementorClass} ) =~ s/::db$//;
            $attr_prefix = DBI->driver_prefix($drv_class);
            $attrib      = $attr_prefix . $attrib;
        }
        my $valid_attrs = $attr_prefix . "valid_attrs";
        my $ro_attrs    = $attr_prefix . "readonly_attrs";

        ( $attrib, $value ) = $dbh->func( $attrib, $value, "validate_STORE_attr" );
        $attrib or return;

        exists $dbh->{$valid_attrs}
          and ( $dbh->{$valid_attrs}{$attrib}
                or return $dbh->set_err( $DBI::stderr, "Invalid attribute '$attrib'" ) );
        exists $dbh->{$ro_attrs}
          and $dbh->{$ro_attrs}{$attrib}
          and defined $dbh->{$attrib}
          and return $dbh->set_err( $DBI::stderr,
                                    "attribute '$attrib' is readonly and must not be modified" );

        $dbh->{$attrib} = $value;
        return 1;
    }

    return $dbh->SUPER::STORE( $attrib, $value );
}    # STORE

sub get_driver_versions
{
    my ( $dbh, $table ) = @_;
    my %vsn = (
                OS   => "$^O ($Config::Config{osvers})",
                Perl => "$] ($Config::Config{archname})",
                DBI  => $DBI::VERSION,
              );
    my %vmp;

    my $sql_engine_verinfo =
      join " ",
      $dbh->{sql_engine_version}, "using", $dbh->{sql_handler},
      $dbh->{sql_handler} eq "SQL::Statement"
      ? $dbh->{sql_statement_version}
      : $dbh->{sql_nano_version};

    my $indent   = 0;
    my @deriveds = ( $dbh->{ImplementorClass} );
    while (@deriveds)
    {
        my $derived = shift @deriveds;
        $derived eq "DBI::DBD::SqlEngine::db" and last;
        $derived->isa("DBI::DBD::SqlEngine::db") or next;
        #no strict 'refs';
        eval "push \@deriveds, \@${derived}::ISA";
        #use strict;
        ( my $drv_class = $derived ) =~ s/::db$//;
        my $drv_prefix  = DBI->driver_prefix($drv_class);
        my $ddgv        = $dbh->{ImplementorClass}->can("get_${drv_prefix}versions");
        my $drv_version = $ddgv ? &$ddgv( $dbh, $table ) : $dbh->{ $drv_prefix . "version" };
        $drv_version ||= eval { $derived->VERSION() };    # XXX access $drv_class::VERSION via symbol table
        $vsn{$drv_class} = $drv_version;
        $indent and $vmp{$drv_class} = " " x $indent . $drv_class;
        $indent += 2;
    }

    $vsn{"DBI::DBD::SqlEngine"} = $sql_engine_verinfo;
    $indent and $vmp{"DBI::DBD::SqlEngine"} = " " x $indent . "DBI::DBD::SqlEngine";

    $DBI::PurePerl and $vsn{"DBI::PurePerl"} = $DBI::PurePerl::VERSION;

    $indent += 20;
    my @versions = map { sprintf "%-${indent}s %s", $vmp{$_} || $_, $vsn{$_} }
      sort {
        $a->isa($b)                    and return -1;
        $b->isa($a)                    and return 1;
        $a->isa("DBI::DBD::SqlEngine") and return -1;
        $b->isa("DBI::DBD::SqlEngine") and return 1;
        return $a cmp $b;
      } keys %vsn;

    return wantarray ? @versions : join "\n", @versions;
}    # get_versions

sub DESTROY ($)
{
    my $dbh = shift;
    $dbh->SUPER::FETCH("Active") and $dbh->disconnect;
    undef $dbh->{sql_parser_object};
}    # DESTROY

sub type_info_all ($)
{
    [
       {
          TYPE_NAME          => 0,
          DATA_TYPE          => 1,
          PRECISION          => 2,
          LITERAL_PREFIX     => 3,
          LITERAL_SUFFIX     => 4,
          CREATE_PARAMS      => 5,
          NULLABLE           => 6,
          CASE_SENSITIVE     => 7,
          SEARCHABLE         => 8,
          UNSIGNED_ATTRIBUTE => 9,
          MONEY              => 10,
          AUTO_INCREMENT     => 11,
          LOCAL_TYPE_NAME    => 12,
          MINIMUM_SCALE      => 13,
          MAXIMUM_SCALE      => 14,
       },
       [ "VARCHAR", DBI::SQL_VARCHAR(), undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999, ],
       [ "CHAR", DBI::SQL_CHAR(), undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999, ],
       [ "INTEGER", DBI::SQL_INTEGER(), undef, "", "", undef, 0, 0, 1, 0, 0, 0, undef, 0, 0, ],
       [ "REAL",    DBI::SQL_REAL(),    undef, "", "", undef, 0, 0, 1, 0, 0, 0, undef, 0, 0, ],
       [ "BLOB", DBI::SQL_LONGVARBINARY(), undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999, ],
       [ "BLOB", DBI::SQL_LONGVARBINARY(), undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999, ],
       [ "TEXT", DBI::SQL_LONGVARCHAR(), undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999, ],
    ];
}    # type_info_all

sub get_avail_tables
{
    my $dbh    = $_[0];
    my @tables = ();

    if ( $dbh->{sql_handler} eq "SQL::Statement" and $dbh->{sql_ram_tables} )
    {
        foreach my $table ( keys %{ $dbh->{sql_ram_tables} } )
        {
            push @tables, [ undef, undef, $table, "TABLE", "TEMP" ];
        }
    }

    return @tables;
}    # get_avail_tables

{
    my $names = [qw( TABLE_QUALIFIER TABLE_OWNER TABLE_NAME TABLE_TYPE REMARKS )];

    sub table_info ($)
    {
        my $dbh = shift;

        my @tables = $dbh->func("get_avail_tables");

        # Temporary kludge: DBD::Sponge dies if @tables is empty. :-(
        @tables or return;

        my $dbh2 = $dbh->func("sql_sponge_driver");
        my $sth = $dbh2->prepare(
                                  "TABLE_INFO",
                                  {
                                     rows  => \@tables,
                                     NAMES => $names,
                                  }
                                );
        $sth or $dbh->set_err( $DBI::stderr, $dbh2->errstr );
        return $sth;
    }    # table_info
}

sub list_tables ($)
{
    my $dbh = shift;
    my @table_list;

    my @tables = $dbh->func("get_avail_tables") or return;
    foreach my $ref (@tables)
    {
        push @tables, $ref->[2];
    }

    return @table_list;
}    # list_tables

sub quote ($$;$)
{
    my ( $self, $str, $type ) = @_;
    defined $str or return "NULL";
    defined $type && (    $type == DBI::SQL_NUMERIC()
                       || $type == DBI::SQL_DECIMAL()
                       || $type == DBI::SQL_INTEGER()
                       || $type == DBI::SQL_SMALLINT()
                       || $type == DBI::SQL_FLOAT()
                       || $type == DBI::SQL_REAL()
                       || $type == DBI::SQL_DOUBLE()
                       || $type == DBI::SQL_TINYINT() )
      and return $str;

    $str =~ s/\\/\\\\/sg;
    $str =~ s/\0/\\0/sg;
    $str =~ s/\'/\\\'/sg;
    $str =~ s/\n/\\n/sg;
    $str =~ s/\r/\\r/sg;
    return "'$str'";
}    # quote

sub commit ($)
{
    my $dbh = shift;
    $dbh->FETCH("Warn")
      and carp "Commit ineffective while AutoCommit is on", -1;
    return 1;
}    # commit

sub rollback ($)
{
    my $dbh = shift;
    $dbh->FETCH("Warn")
      and carp "Rollback ineffective while AutoCommit is on", -1;
    return 0;
}    # rollback

# ====== STATEMENT =============================================================

package DBI::DBD::SqlEngine::st;

use strict;
use warnings;

use vars qw(@ISA $imp_data_size);

$imp_data_size = 0;

sub bind_param ($$$;$)
{
    my ( $sth, $pNum, $val, $attr ) = @_;
    if ( $attr && defined $val )
    {
        my $type = ref $attr eq "HASH" ? $attr->{TYPE} : $attr;
        if (    $type == DBI::SQL_BIGINT()
             || $type == DBI::SQL_INTEGER()
             || $type == DBI::SQL_SMALLINT()
             || $type == DBI::SQL_TINYINT() )
        {
            $val += 0;
        }
        elsif (    $type == DBI::SQL_DECIMAL()
                || $type == DBI::SQL_DOUBLE()
                || $type == DBI::SQL_FLOAT()
                || $type == DBI::SQL_NUMERIC()
                || $type == DBI::SQL_REAL() )
        {
            $val += 0.;
        }
        else
        {
            $val = "$val";
        }
    }
    $sth->{sql_params}[ $pNum - 1 ] = $val;
    return 1;
}    # bind_param

sub execute
{
    my $sth = shift;
    my $params = @_ ? ( $sth->{sql_params} = [@_] ) : $sth->{sql_params};

    $sth->finish;
    my $stmt = $sth->{sql_stmt};
    unless ( $sth->{sql_params_checked}++ )
    {
        # bug in SQL::Statement 1.20 and below causes breakage
        # on all but the first call
        unless ( ( my $req_prm = $stmt->params() ) == ( my $nparm = @$params ) )
        {
            my $msg = "You passed $nparm parameters where $req_prm required";
            $sth->set_err( $DBI::stderr, $msg );
            return;
        }
    }
    my @err;
    my $result;
    eval {
        local $SIG{__WARN__} = sub { push @err, @_ };
        $result = $stmt->execute( $sth, $params );
    };
    unless ( defined $result )
    {
        $sth->set_err( $DBI::stderr, $@ || $stmt->{errstr} || $err[0] );
        return;
    }

    if ( $stmt->{NUM_OF_FIELDS} )
    {    # is a SELECT statement
        $sth->STORE( Active => 1 );
        $sth->FETCH("NUM_OF_FIELDS")
          or $sth->STORE( "NUM_OF_FIELDS", $stmt->{NUM_OF_FIELDS} );
    }
    return $result;
}    # execute

sub finish
{
    my $sth = $_[0];
    $sth->SUPER::STORE( Active => 0 );
    delete $sth->{sql_stmt}{data};
    return 1;
}    # finish

sub fetch ($)
{
    my $sth  = $_[0];
    my $data = $sth->{sql_stmt}{data};
    if ( !$data || ref $data ne "ARRAY" )
    {
        $sth->set_err(
            $DBI::stderr,
            "Attempt to fetch row without a preceeding execute () call or from a non-SELECT statement"
        );
        return;
    }
    my $dav = shift @$data;
    unless ($dav)
    {
        $sth->finish;
        return;
    }
    if ( $sth->FETCH("ChopBlanks") )    # XXX: (TODO) Only chop on CHAR fields,
    {                                   # not on VARCHAR or NUMERIC (see DBI docs)
        $_ && $_ =~ s/ +$// for @$dav;
    }
    return $sth->_set_fbav($dav);
}    # fetch

no warnings 'once';
*fetchrow_arrayref = \&fetch;

use warnings;

sub sql_get_colnames
{
    my $sth = $_[0];
    # Being a bit dirty here, as neither SQL::Statement::Structure nor
    # DBI::SQL::Nano::Statement_ does not offer an interface to the
    # required data
    my @colnames;
    if ( $sth->{sql_stmt}->{NAME} and "ARRAY" eq ref( $sth->{sql_stmt}->{NAME} ) )
    {
        @colnames = @{ $sth->{sql_stmt}->{NAME} };
    }
    elsif ( $sth->{sql_stmt}->isa('SQL::Statement') )
    {
        my $stmt = $sth->{sql_stmt} || {};
        my @coldefs = @{ $stmt->{column_defs} || [] };
        @colnames = map { $_->{name} || $_->{value} } @coldefs;
    }
    @colnames = $sth->{sql_stmt}->column_names() unless (@colnames);

    @colnames = () if ( grep { m/\*/ } @colnames );

    return @colnames;
}

sub FETCH ($$)
{
    my ( $sth, $attrib ) = @_;

    $attrib eq "NAME" and return [ $sth->sql_get_colnames() ];

    $attrib eq "TYPE"      and return [ (DBI::SQL_VARCHAR()) x scalar $sth->sql_get_colnames() ];
    $attrib eq "TYPE_NAME" and return [ ("VARCHAR") x scalar $sth->sql_get_colnames() ];
    $attrib eq "PRECISION" and return [ (0) x scalar $sth->sql_get_colnames() ];
    $attrib eq "NULLABLE"  and return [ (1) x scalar $sth->sql_get_colnames() ];

    if ( $attrib eq lc $attrib )
    {
        # Private driver attributes are lower cased
        return $sth->{$attrib};
    }

    # else pass up to DBI to handle
    return $sth->SUPER::FETCH($attrib);
}    # FETCH

sub STORE ($$$)
{
    my ( $sth, $attrib, $value ) = @_;
    if ( $attrib eq lc $attrib )    # Private driver attributes are lower cased
    {
        $sth->{$attrib} = $value;
        return 1;
    }
    return $sth->SUPER::STORE( $attrib, $value );
}    # STORE

sub DESTROY ($)
{
    my $sth = shift;
    $sth->SUPER::FETCH("Active") and $sth->finish;
    undef $sth->{sql_stmt};
    undef $sth->{sql_params};
}    # DESTROY

sub rows ($)
{
    return $_[0]->{sql_stmt}{NUM_OF_ROWS};
}    # rows

# ====== SQL::STATEMENT ========================================================

package DBI::DBD::SqlEngine::Statement;

use strict;
use warnings;

use Carp;

@DBI::DBD::SqlEngine::Statement::ISA = qw(DBI::SQL::Nano::Statement);

# ====== SQL::TABLE ============================================================

package DBI::DBD::SqlEngine::Table;

use strict;
use warnings;

@DBI::DBD::SqlEngine::Table::ISA = qw(DBI::SQL::Nano::Table);

=pod

=head1 NAME

DBI::DBD::SqlEngine - Base class for DBI drivers without their own SQL engine

=head1 SYNOPSIS

    package DBD::myDriver;

    use base qw(DBI::DBD::SqlEngine);

    sub driver
    {
	...
	my $drh = $proto->SUPER::driver($attr);
	...
	return $drh->{class};
	}

    package DBD::myDriver::dr;

    @ISA = qw(DBI::DBD::SqlEngine::dr);

    sub data_sources { ... }
    ...

    package DBD::myDriver::db;

    @ISA = qw(DBI::DBD::SqlEngine::db);

    sub init_valid_attributes { ... }
    sub init_default_attributes { ... }
    sub set_versions { ... }
    sub validate_STORE_attr { my ($dbh, $attrib, $value) = @_; ... }
    sub validate_FETCH_attr { my ($dbh, $attrib) = @_; ... }
    sub get_myd_versions { ... }
    sub get_avail_tables { ... }

    package DBD::myDriver::st;

    @ISA = qw(DBI::DBD::SqlEngine::st);

    sub FETCH { ... }
    sub STORE { ... }

    package DBD::myDriver::Statement;

    @ISA = qw(DBI::DBD::SqlEngine::Statement);

    sub open_table { ... }

    package DBD::myDriver::Table;

    @ISA = qw(DBI::DBD::SqlEngine::Table);

    sub new { ... }

=head1 DESCRIPTION

DBI::DBD::SqlEngine abstracts the usage of SQL engines from the
DBD. DBD authors can concentrate on the data retrieval they want to
provide.

It is strongly recommended that you read L<DBD::File::Developers> and
L<DBD::File::Roadmap>, because many of the DBD::File API is provided
by DBI::DBD::SqlEngine.

Currently the API of DBI::DBD::SqlEngine is experimental and will
likely change in the near future to provide the table meta data basics
like DBD::File.

=head2 Metadata

The following attributes are handled by DBI itself and not by
DBI::DBD::SqlEngine, thus they all work as expected:

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

=head3 The following DBI attributes are handled by DBI::DBD::SqlEngine:

=head4 AutoCommit

Always on.

=head4 ChopBlanks

Works.

=head4 NUM_OF_FIELDS

Valid after C<< $sth->execute >>.

=head4 NUM_OF_PARAMS

Valid after C<< $sth->prepare >>.

=head4 NAME

Valid after C<< $sth->execute >>; probably undef for Non-Select statements.

=head4 NULLABLE

Not really working, always returns an array ref of ones, as DBD::CSV
does not verify input data. Valid after C<< $sth->execute >>; undef for
non-select statements.

=head3 The following DBI attributes and methods are not supported:

=over 4

=item bind_param_inout

=item CursorName

=item LongReadLen

=item LongTruncOk

=back

=head3 DBI::DBD::SqlEngine specific attributes

In addition to the DBI attributes, you can use the following dbh
attributes:

=head4 sql_engine_version

Contains the module version of this driver (B<readonly>)

=head4 sql_nano_version

Contains the module version of DBI::SQL::Nano (B<readonly>)

=head4 sql_statement_version

Contains the module version of SQL::Statement, if available (B<readonly>)

=head4 sql_handler

Contains the SQL Statement engine, either DBI::SQL::Nano or SQL::Statement
(B<readonly>).

=head4 sql_parser_object

Contains an instantiated instance of SQL::Parser (B<readonly>).
This is filled when used first time (only when used with SQL::Statement).

=head4 sql_sponge_driver

Contains an internally used DBD::Sponge handle (B<readonly>).

=head4 sql_valid_attrs

Contains the list of valid attributes for each DBI::DBD::SqlEngine based
driver (B<readonly>).

=head4 sql_readonly_attrs

Contains the list of those attributes which are readonly (B<readonly>).

=head4 sql_identifier_case

Contains how DBI::DBD::SqlEngine deals with non-quoted SQL identifiers:

  * SQL_IC_UPPER (1) means all identifiers are internally converted
    into upper-cased pendants
  * SQL_IC_LOWER (2) means all identifiers are internally converted
    into lower-cased pendants
  * SQL_IC_MIXED (4) means all identifiers are taken as they are

These conversions happen if (and only if) no existing identifier matches.
Once existing identifier is used as known.

The SQL statement execution classes doesn't have to care, so don't expect
C<sql_identifier_case> affects column names in statements like

  SELECT * FROM foo

=head4 sql_quoted_identifier_case

Contains how DBI::DBD::SqlEngine deals with quoted SQL identifiers
(B<readonly>). It's fixated to SQL_IC_SENSITIVE (3), which is interpreted
as SQL_IC_MIXED.

=head4 sql_flags

Contains additional flags to instantiate an SQL::Parser. Because an
SQL::Parser is instantiated only once, it's recommended to set this flag
before any statement is executed.

=head4 sql_dialect

Controls the dialect understood by SQL::Parser. Possible values (delivery
state of SQL::Statement):

  * ANSI
  * CSV
  * AnyData

Defaults to "CSV".  Because an SQL::Parser is instantiated only once and
SQL::Parser doesn't allow to modify the dialect once instantiated,
it's strongly recommended to set this flag before any statement is
executed (best place is connect attribute hash).

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc DBI::DBD::SqlEngine

You can also look for information at:

=over 4

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=DBI>
L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=SQL-Statement>

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/DBI>
L<http://annocpan.org/dist/SQL-Statement>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/DBI>

=item * Search CPAN

L<http://search.cpan.org/dist/DBI/>

=back

=head2 Where can I go for more help?

For questions about installation or usage, please ask on the
dbi-dev@perl.org mailing list.

If you have a bug report, patch or suggestion, please open
a new report ticket on CPAN, if there is not already one for
the issue you want to report. Of course, you can mail any of the
module maintainers, but it is less likely to be missed if
it is reported on RT.

Report tickets should contain a detailed description of the bug or
enhancement request you want to report and at least an easy way to
verify/reproduce the issue and any supplied fix. Patches are always
welcome, too.

=head1 ACKNOWLEDGEMENTS

Thanks to Tim Bunce, Martin Evans and H.Merijn Brand for their continued
support while developing DBD::File, DBD::DBM and DBD::AnyData.
Their support, hints and feedback helped to design and implement this
module.

=head1 AUTHOR

This module is currently maintained by

H.Merijn Brand < h.m.brand at xs4all.nl > and
Jens Rehsack  < rehsack at googlemail.com >

The original authors are Jochen Wiedmann and Jeff Zucker.

=head1 COPYRIGHT AND LICENSE

 Copyright (C) 2009-2010 by H.Merijn Brand & Jens Rehsack
 Copyright (C) 2004-2009 by Jeff Zucker
 Copyright (C) 1998-2004 by Jochen Wiedmann

All rights reserved.

You may freely distribute and/or modify this module under the terms of
either the GNU General Public License (GPL) or the Artistic License, as
specified in the Perl README file.

=head1 SEE ALSO

L<DBI>, L<DBD::File>, L<DBD::AnyData> and L<DBD::Sys>.

=cut
