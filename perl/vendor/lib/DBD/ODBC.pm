# $Id: ODBC.pm 15265 2012-04-07 13:53:28Z mjevans $
#
# Copyright (c) 1994,1995,1996,1998  Tim Bunce
# portions Copyright (c) 1997-2004  Jeff Urlwin
# portions Copyright (c) 1997  Thomas K. Wenrich
# portions Copyright (c) 2007-2012 Martin J. Evans
#
# You may distribute under the terms of either the GNU General Public
# License or the Artistic License, as specified in the Perl README file.

## no critic (ProhibitManyArgs ProhibitMultiplePackages)

require 5.008;

# NOTE: Don't forget to update the version reference in the POD below too.
# NOTE: If you create a developer release x.y_z ensure y is greater than
# the preceding y in the non developer release e.g., 1.24 should be followed
# by 1.25_1 and then released as 1.26.
# see discussion on dbi-users at
# http://www.nntp.perl.org/group/perl.dbi.dev/2010/07/msg6096.html and
# http://www.dagolden.com/index.php/369/version-numbers-should-be-boring/
$DBD::ODBC::VERSION = '1.37';

{
    ## no critic (ProhibitMagicNumbers ProhibitExplicitISA)
    ## no critic (ProhibitPackageVars)
    package DBD::ODBC;

    use DBI ();
    use DynaLoader ();
    use Exporter ();

    @ISA = qw(Exporter DynaLoader);

    # my $Revision = substr(q$Id: ODBC.pm 15265 2012-04-07 13:53:28Z mjevans $, 13,2);

    require_version DBI 1.609;

    bootstrap DBD::ODBC $VERSION;

    $err = 0;           # holds error code   for DBI::err
    $errstr = q{};              # holds error string for DBI::errstr
    $sqlstate = "00000";
    $drh = undef;       # holds driver handle once initialised

    use constant {
        # header fields in SQLGetDiagField:
        SQL_DIAG_CURSOR_ROW_COUNT => -1249,
        SQL_DIAG_DYNAMIC_FUNCTION => 7,
        SQL_DIAG_DYNAMIC_FUNCTION_CODE => 12,
        SQL_DIAG_NUMBER => 2,
        SQL_DIAG_RETURNCODE => 1,
        SQL_DIAG_ROW_COUNT => 3,
        # record fields in SQLGetDiagField:
        SQL_DIAG_CLASS_ORIGIN => 8,
        SQL_DIAG_COLUMN_NUMBER => -1247,
        SQL_DIAG_CONNECTION_NAME => 10,
        SQL_DIAG_MESSAGE_TEXT => 6,
        SQL_DIAG_NATIVE => 5,
        SQL_DIAG_ROW_NUMBER => -1248,
        SQL_DIAG_SERVER_NAME => 11,
        SQL_DIAG_SQLSTATE => 4,
        SQL_DIAG_SUBCLASS_ORIGIN => 9
    };
    our @EXPORT_DIAGS = qw(SQL_DIAG_CURSOR_ROW_COUNT SQL_DIAG_DYNAMIC_FUNCTION SQL_DIAG_DYNAMIC_FUNCTION_CODE SQL_DIAG_NUMBER SQL_DIAG_RETURNCODE SQL_DIAG_ROW_COUNT SQL_DIAG_CLASS_ORIGIN SQL_DIAG_COLUMN_NUMBER SQL_DIAG_CONNECTION_NAME SQL_DIAG_MESSAGE_TEXT SQL_DIAG_NATIVE SQL_DIAG_ROW_NUMBER SQL_DIAG_SERVER_NAME SQL_DIAG_SQLSTATE SQL_DIAG_SUBCLASS_ORIGIN);
    our @EXPORT_OK = (@EXPORT_DIAGS);
    our %EXPORT_TAGS = (
        diags => \@EXPORT_DIAGS);

    sub parse_trace_flag {
        my ($class, $name) = @_;
        return 0x02_00_00_00 if $name eq 'odbcunicode';
        return 0x04_00_00_00 if $name eq 'odbcconnection';
        return DBI::parse_trace_flag($class, $name);
    }

    sub parse_trace_flags {
        my ($class, $flags) = @_;
        return DBI::parse_trace_flags($class, $flags);
    }

    sub driver{
        return $drh if $drh;
        my($class, $attr) = @_;

        $class .= "::dr";

        # not a 'my' since we use it above to prevent multiple drivers

        $drh = DBI::_new_drh($class, {
            'Name' => 'ODBC',
            'Version' => $VERSION,
            'Err'    => \$DBD::ODBC::err,
            'Errstr' => \$DBD::ODBC::errstr,
            'State' => \$DBD::ODBC::sqlstate,
            'Attribution' => 'DBD::ODBC by Jeff Urlwin, Tim Bunce and Martin J. Evans',
	    });
        DBD::ODBC::st->install_method("odbc_lob_read");
        # don't clear errors - IMA_KEEP_ERR = 0x00000004
        DBD::ODBC::st->install_method("odbc_getdiagrec", { O=>0x00000004 });
        DBD::ODBC::db->install_method("odbc_getdiagrec", { O=>0x00000004 });
        DBD::ODBC::db->install_method("odbc_getdiagfield", { O=>0x00000004 });
        DBD::ODBC::st->install_method("odbc_getdiagfield", { O=>0x00000004 });
        return $drh;
    }

    sub CLONE { undef $drh }
    1;
}


{   package DBD::ODBC::dr; # ====== DRIVER ======
    use strict;
    use warnings;

    ## no critic (ProhibitBuiltinHomonyms)
    sub connect {
	my($drh, $dbname, $user, $auth, $attr)= @_;
	#$user = q{} unless defined $user;
	#$auth = q{} unless defined $auth;

	# create a 'blank' dbh
	my $this = DBI::_new_dbh($drh, {
	    'Name' => $dbname,
	    'USER' => $user,
	    'CURRENT_USER' => $user,
	    });

	# Call ODBC _login func in Driver.xst file => dbd_db_login6
	# and populate internal handle data.
	# There are 3 versions (currently) if you have a recent DBI:
	# dbd_db_login (oldest)
	# dbd_db_login6 (with attribs hash & char * args) and
	# dbd_db_login6_sv (as dbd_db_login6 with perl scalar args

	DBD::ODBC::db::_login($this, $dbname, $user, $auth, $attr) or return;

	return $this;
    }
    ## use critic

}


{   package DBD::ODBC::db; # ====== DATABASE ======
    use strict;
    use warnings;

    use constant SQL_DRIVER_HSTMT => 5;
    use constant SQL_DRIVER_HLIB => 76;
    use constant SQL_DRIVER_HDESC => 135;


    sub parse_trace_flag {
        my ($h, $name) = @_;
        return DBD::ODBC->parse_trace_flag($name);
    }

    sub private_attribute_info {
        return {
            odbc_ignore_named_placeholders => undef, # sth and dbh
            odbc_default_bind_type         => undef, # sth and dbh
            odbc_force_bind_type           => undef, # sth and dbh
            odbc_force_rebind              => undef, # sth and dbh
            odbc_async_exec                => undef, # sth and dbh
            odbc_exec_direct               => undef,
            odbc_old_unicode               => undef,
            odbc_describe_parameters       => undef,
            odbc_SQL_ROWSET_SIZE           => undef,
            odbc_SQL_DRIVER_ODBC_VER       => undef,
            odbc_cursortype                => undef,
            odbc_query_timeout             => undef, # sth and dbh
            odbc_has_unicode               => undef,
            odbc_out_connect_string        => undef,
            odbc_version                   => undef,
            odbc_err_handler               => undef,
            odbc_putdata_start             => undef, # sth and dbh
            odbc_column_display_size       => undef, # sth and dbh
            odbc_utf8_on                   => undef, # sth and dbh
            odbc_driver_complete           => undef,
            odbc_batch_size                => undef,
            odbc_array_operations          => undef, # sth and dbh
        };
    }

    sub prepare {
        my($dbh, $statement, @attribs)= @_;

        # create a 'blank' sth
        my $sth = DBI::_new_sth($dbh, {
            'Statement' => $statement,
	    });

        # Call ODBC func in ODBC.xs file.
        # (This will actually also call SQLPrepare for you.)
        # and populate internal handle data.

        DBD::ODBC::st::_prepare($sth, $statement, @attribs)
              or return;

        return $sth;
    }

    sub column_info {
        my ($dbh, $catalog, $schema, $table, $column) = @_;

        $catalog = q{} if (!$catalog);
        $schema = q{} if (!$schema);
        $table = q{} if (!$table);
        $column = q{} if (!$column);
        # create a "blank" statement handle
        my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLColumns" });

        _columns($dbh,$sth, $catalog, $schema, $table, $column)
            or return;

        return $sth;
    }

    sub columns {
	my ($dbh, $catalog, $schema, $table, $column) = @_;

	$catalog = q{} if (!$catalog);
	$schema = q{} if (!$schema);
	$table = q{} if (!$table);
	$column = q{} if (!$column);
	# create a "blank" statement handle
	my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLColumns" });

	_columns($dbh,$sth, $catalog, $schema, $table, $column)
	    or return;

	return $sth;
    }


    sub table_info {
 	my ($dbh, $catalog, $schema, $table, $type) = @_;

	if ($#_ == 1) {
	   my $attrs = $_[1];
	   $catalog = $attrs->{TABLE_CAT};
	   $schema = $attrs->{TABLE_SCHEM};
	   $table = $attrs->{TABLE_NAME};
	   $type = $attrs->{TABLE_TYPE};
 	}

	$catalog = q{} if (!$catalog);
	$schema = q{} if (!$schema);
	$table = q{} if (!$table);
	$type = q{} if (!$type);

	# create a "blank" statement handle
	my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLTables" });

	DBD::ODBC::st::_tables($dbh,$sth, $catalog, $schema, $table, $type)
	      or return;
	return $sth;
    }

    sub primary_key_info {
       my ($dbh, $catalog, $schema, $table ) = @_;

       # create a "blank" statement handle
       my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLPrimaryKeys" });

       $catalog = q{} if (!$catalog);
       $schema = q{} if (!$schema);
       $table = q{} if (!$table);
       DBD::ODBC::st::_primary_keys($dbh,$sth, $catalog, $schema, $table )
	     or return;
       return $sth;
    }
    sub statistics_info {
       my ($dbh, $catalog, $schema, $table, $unique, $quick ) = @_;

       # create a "blank" statement handle
       my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLStatistics" });

       $catalog = q{} if (!$catalog);
       $schema = q{} if (!$schema);
       $table = q{} if (!$table);
       $unique = 1 if (!$unique);
       $quick = 1 if (!$quick);

       DBD::ODBC::st::_statistics($dbh, $sth, $catalog, $schema, $table,
                                 $unique, $quick)
	     or return;
       return $sth;
    }

    sub foreign_key_info {
       my ($dbh, $pkcatalog, $pkschema, $pktable, $fkcatalog, $fkschema, $fktable ) = @_;

       # create a "blank" statement handle
       my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLForeignKeys" });

       $pkcatalog = q{} if (!$pkcatalog);
       $pkschema = q{} if (!$pkschema);
       $pktable = q{} if (!$pktable);
       $fkcatalog = q{} if (!$fkcatalog);
       $fkschema = q{} if (!$fkschema);
       $fktable = q{} if (!$fktable);
       _GetForeignKeys($dbh, $sth, $pkcatalog, $pkschema, $pktable, $fkcatalog, $fkschema, $fktable) or return;
       return $sth;
    }

    sub ping {
	my $dbh = shift;

        # DBD::Gofer does the following (with a 0 instead of "0") but it I
        # cannot make it set a warning.
        #return $dbh->SUPER::set_err("0", "can't ping while not connected") # warning
        #    unless $dbh->SUPER::FETCH('Active');

        #my $pe = $dbh->FETCH('PrintError');
        #$dbh->STORE('PrintError', 0);
        my $evalret = eval {
           # create a "blank" statement handle
            my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLTables_PING" })
                or return 1;

            my ($catalog, $schema, $table, $type);

            $catalog = q{};
            $schema = q{};
            $table = 'NOXXTABLE';
            $type = q{};

            DBD::ODBC::st::_tables($dbh,$sth, $catalog, $schema, $table, $type)
                  or return 1;
            $sth->finish;
            return 0;
        };
        #$dbh->STORE('PrintError', $pe);
        $dbh->set_err(undef,'',''); # clear any stored error from eval above
        if ($evalret == 0) {
            return 1;
        } else {
            return 0;
        }
    }

#####    # saved, just for posterity.
#####    sub oldping  {
#####	my $dbh = shift;
#####	my $state = undef;
#####
#####	# should never 'work' but if it does, that's okay!
#####	# JLU incorporated patches from Jon Smirl 5/4/99
#####	{
#####	    local $dbh->{RaiseError} = 0 if $dbh->{RaiseError};
#####	    # JLU added local PrintError handling for completeness.
#####	    # it shouldn't print, I think.
#####	    local $dbh->{PrintError} = 0 if $dbh->{PrintError};
#####	    my $sql = "select sysdate from dual1__NOT_FOUND__CANNOT";
#####	    my $sth = $dbh->prepare($sql);
#####	    # fixed "my" $state = below.  Was causing problem with
#####	    # ping!  Also, fetching fields as some drivers (Oracle 8)
#####	    # may not actually check the database for activity until
#####	    # the query is "described".
#####	    # Right now, Oracle8 is the only known version which
#####	    # does not actually check the server during prepare.
#####	    my $ok = $sth && $sth->execute();
#####
#####	    $state = $dbh->state;
#####	    $DBD::ODBC::err = 0;
#####	    $DBD::ODBC::errstr = "";
#####	    $DBD::ODBC::sqlstate = "00000";
#####	    return 1 if $ok;
#####	}
#####        return 1 if $state eq 'S0002';  # Base table not found
##### 	return 1 if $state eq '42S02';  # Base table not found.Solid EE v3.51
#####        return 1 if $state eq 'S0022';  # Column not found
#####	return 1 if $state eq '37000';  # statement could not be prepared (19991011, JLU)
#####	# return 1 if $state eq 'S1000';  # General Error? ? 5/30/02, JLU.  This is what Openlink is returning
#####	# We assume that any other error means the database
#####	# is no longer connected.
#####	# Some special cases may need to be added to the code above.
#####	return 0;
#####    }

    # New support for DBI which has the get_info command.
    # leaving support for ->func(xxx, GetInfo) (below) for a period of time
    # to support older applications which used this.
    sub get_info {
	my ($dbh, $item) = @_;
	# Ignore some we cannot do
	if ($item == SQL_DRIVER_HSTMT ||
                $item == SQL_DRIVER_HLIB ||
                    $item == SQL_DRIVER_HDESC) {
	   return;
	}
	return _GetInfo($dbh, $item);
    }

    # new override of do method provided by Merijn Broeren
    # this optimizes "do" to use SQLExecDirect for simple
    # do statements without parameters.
    ## no critic (ProhibitBuiltinHomonyms)
    sub do {
        my($dbh, $statement, $attr, @params) = @_;
        my $rows = 0;
        ## no critic (ProhibitMagicNumbers)
        if( -1 == $#params )
        {
            $dbh->STORE(Statement => $statement);
            # No parameters, use execute immediate
            $rows = ExecDirect( $dbh, $statement );
            if( 0 == $rows ) {
                $rows = "0E0";    # 0 but true
            } elsif( $rows < -1 ) {
                undef $rows;
            }
        }
        else
        {
          $rows = $dbh->SUPER::do( $statement, $attr, @params );
        }
        return $rows
    }
    ## use critic
    #
    # can also be called as $dbh->func($sql, ExecDirect);
    # if, for some reason, there are compatibility issues
    # later with DBI's do.
    #
    sub ExecDirect {
       my ($dbh, $sql) = @_;
       return _ExecDirect($dbh, $sql);
    }

    # Call the ODBC function SQLGetInfo
    # Args are:
    #	$dbh - the database handle
    #	$item: the requested item.  For example, pass 6 for SQL_DRIVER_NAME
    # See the ODBC documentation for more information about this call.
    #
    sub GetInfo {
	my ($dbh, $item) = @_;
	return get_info($dbh, $item);
    }

    # Call the ODBC function SQLStatistics
    # Args are:
    # See the ODBC documentation for more information about this call.
    #
    sub GetStatistics {
        my ($dbh, $catalog, $schema, $table, $unique) = @_;
        # create a "blank" statement handle
        my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLStatistics" });
        _GetStatistics($dbh, $sth, $catalog, $schema,
		       $table, $unique) or return;
        return $sth;
    }

    # Call the ODBC function SQLForeignKeys
    # Args are:
    # See the ODBC documentation for more information about this call.
    #
    sub GetForeignKeys {
        my ($dbh, $pk_catalog, $pk_schema, $pk_table,
            $fk_catalog, $fk_schema, $fk_table) = @_;
        # create a "blank" statement handle
        my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLForeignKeys" });
        _GetForeignKeys($dbh, $sth, $pk_catalog, $pk_schema, $pk_table,
			$fk_catalog, $fk_schema, $fk_table) or return;
        return $sth;
    }

    # Call the ODBC function SQLPrimaryKeys
    # Args are:
    # See the ODBC documentation for more information about this call.
    #
    sub GetPrimaryKeys {
        my ($dbh, $catalog, $schema, $table) = @_;
        # create a "blank" statement handle
        my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLPrimaryKeys" });
        _GetPrimaryKeys($dbh, $sth, $catalog, $schema, $table) or return;
        return $sth;
    }

    # Call the ODBC function SQLSpecialColumns
    # Args are:
    # See the ODBC documentation for more information about this call.
    #
    sub GetSpecialColumns {
	my ($dbh, $identifier, $catalog, $schema, $table, $scope, $nullable) = @_;
	# create a "blank" statement handle
	my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLSpecialColumns" });
	_GetSpecialColumns($dbh, $sth, $identifier, $catalog, $schema,
			   $table, $scope, $nullable) or return;
	return $sth;
    }

    sub GetTypeInfo {
	my ($dbh, $sqltype) = @_;
	# create a "blank" statement handle
	my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLGetTypeInfo" });
	# print "SQL Type is $sqltype\n";
	_GetTypeInfo($dbh, $sth, $sqltype) or return;
	return $sth;
    }

    sub type_info_all {
	my ($dbh, $sqltype) = @_;
	$sqltype = DBI::SQL_ALL_TYPES unless defined $sqltype;
	my $sth = DBI::_new_sth($dbh, { 'Statement' => "SQLGetTypeInfo" });
	_GetTypeInfo($dbh, $sth, $sqltype) or return;
	my $info = $sth->fetchall_arrayref;
	unshift @{$info}, {
	    map { ($sth->{NAME}->[$_] => $_) } 0..$sth->{NUM_OF_FIELDS}-1
	};
	return $info;
    }
}


{   package DBD::ODBC::st; # ====== STATEMENT ======
    use strict;
    use warnings;

    *parse_trace_flag = \&DBD::ODBC::db::parse_trace_flag;

    sub private_attribute_info {
        return {
            odbc_ignore_named_placeholders => undef, # sth and dbh
            odbc_default_bind_type         => undef, # sth and dbh
            odbc_force_bind_type           => undef, # sth and dbh
            odbc_force_rebind              => undef, # sth and dbh
            odbc_async_exec                => undef, # sth and dbh
            odbc_query_timeout             => undef, # sth and dbh
            odbc_putdata_start             => undef, # sth and dbh
            odbc_column_display_size       => undef, # sth and dbh
            odbc_utf8_on                   => undef, # sth and dbh
            odbc_exec_direct               => undef, # sth and dbh
            odbc_old_unicode               => undef, # sth and dbh
            odbc_describe_parameters       => undef, # sth and dbh
            odbc_batch_size                => undef, # sth and dbh
            odbc_array_operations          => undef, # sth and dbh
        };
    }

    sub ColAttributes { # maps to SQLColAttributes
	my ($sth, $colno, $desctype) = @_;
	my $tmp = _ColAttributes($sth, $colno, $desctype);
	return $tmp;
    }

    sub cancel {
	my $sth = shift;
	my $tmp = _Cancel($sth);
	return $tmp;
    }

    sub execute_for_fetch {
        my ($sth, $fetch_tuple_sub, $tuple_status) = @_;
        #print "execute_for_fetch\n";
        my $row_count = 0;
        my $tuple_count="0E0";
        my $tuple_batch_status;
        my $batch_size = $sth->FETCH('odbc_batch_size');

        $sth->trace_msg("execute_for_fetch($fetch_tuple_sub, " .
                            ($tuple_status ? $tuple_status : 'undef') .
                                ") batch_size = $batch_size\n", 4);
        # Use DBI's execute_for_fetch if ours is disabled
        my $override = (defined($ENV{ODBC_DISABLE_ARRAY_OPERATIONS}) ?
                            $ENV{ODBC_DISABLE_ARRAY_OPERATIONS} : -1);
        if ((($sth->FETCH('odbc_array_operations') == 0) && ($override != 0)) ||
                $override == 1) {
            $sth->trace_msg("array operations disabled\n", 4);
            my $sth = shift;
            return $sth->SUPER::execute_for_fetch(@_);
        }

	$tuple_batch_status = [ ]; # we always want this here
        if (defined($tuple_status)) {
            @$tuple_status = ();
        }
        my $finished;
        while (1) {
            my @tuple_batch;
            for (my $i = 0; $i < $batch_size; $i++) {
                $finished = $fetch_tuple_sub->();
                push @tuple_batch, [ @{$finished || last} ];
            }
            $sth->trace_msg("Found " . scalar(@tuple_batch) . " rows\n", 4);
            last unless @tuple_batch;
            my $res = odbc_execute_for_fetch($sth,
					     \@tuple_batch,
					     scalar(@tuple_batch),
					     $tuple_batch_status);
            $sth->trace_msg("odbc_execute_array returns " .
                                ($res ? $res : 'undef') . "\n", 4);

            #print "odbc_execute_array XS returned $res\n";
            # count how many tuples were used
            # basically they are all used unless marked UNUSED
            if ($tuple_batch_status) {
                foreach (@$tuple_batch_status) {
                    $tuple_count++ unless $_ == 7; # SQL_PARAM_UNUSED
                    next if ref($_);
                    $_ = -1;	# we don't know individual row counts
                }
                if ($tuple_status) {
                    push @$tuple_status, @$tuple_batch_status
                        if defined($tuple_status);
                }
            }
            if (!defined($res)) {	# error
                $row_count = undef;
                last;
            } else {
                $row_count += $res;
            }
            last if !$finished;
        }
        if (!wantarray) {
            return undef if !defined $row_count;
            return $tuple_count;
        }
        return (defined $row_count ? $tuple_count : undef, $row_count);
    }
}

1;
__END__

=head1 NAME

DBD::ODBC - ODBC Driver for DBI

=head1 VERSION

This documentation refers to DBD::ODBC version 1.37.

=head1 SYNOPSIS

  use DBI;

  $dbh = DBI->connect('dbi:ODBC:DSN', 'user', 'password');

See L<DBI> for more information.

=head1 DESCRIPTION

=head2 Change log and FAQs

Please note that the change log has been moved to
DBD::ODBC::Changes. To access this documentation, use
C<perldoc DBD::ODBC::Changes>.

The FAQs have also moved to DBD::ODBC::FAQ.pm. To access the FAQs use
C<perldoc DBD::ODBC::FAQ>.

=head2 Important note about the tests

Please note that some tests may fail or report they are unsupported on
this platform.  Notably Oracle's ODBC driver will fail the "advanced"
binding tests in t/08bind2.t.  These tests run perfectly under SQL
Server 2000. This is normal and expected.  Until Oracle fixes their
drivers to do the right thing from an ODBC perspective, it's going to
be tough to fix the issue.  The workaround for Oracle is to bind date
types with SQL_TIMESTAMP.  Also note that some tests may be skipped,
such as t/09multi.t, if your driver doesn't seem to support returning
multiple result sets.  This is normal.

=head2 DBI attribute handling

If a DBI defined attribute is not mentioned here it behaves as per the
DBI specification.

=head3 ReadOnly (boolean)

DBI documents the C<ReadOnly> attribute as being settleable and
retrievable on connection and statement handles. In ODBC setting
ReadOnly to true causes the connection attribute C<SQL_ATTR_ACCESS_MODE>
to be set to C<SQL_MODE_READ_ONLY> and setting it to false will set the
access mode to C<SQL_MODE_READ_WRITE> (which is the default in ODBC).

B<Note:> There is no equivalent of setting ReadOnly on a statement
handle in ODBC.

B<Note:> See ODBC documentation on C<SQL_ATTR_ACCESS_MODE> as setting it
to C<SQL_MODE_READ_ONLY> does B<not> prevent your script from running
updates or deletes; it is simply a hint to the driver/database that
you won't being doing updates.

This attribute requires DBI version 1.55 or better.

=head2 Private attributes common to connection and statement handles

=head3 odbc_ignore_named_placeholders

Use this if you have special needs (such as Oracle triggers, etc)
where :new or :name mean something special and are not just place
holder names. You I<must> then use ? for binding parameters.  Example:

 $dbh->{odbc_ignore_named_placeholders} = 1;
 $dbh->do("create trigger foo as if :new.x <> :old.x then ... etc");

Without this, DBD::ODBC will think :new and :old are placeholders for
binding and get confused.

=head3 odbc_default_bind_type

This value defaults to 0.

Older versions of DBD::ODBC assumed that the parameter binding type
was 12 (C<SQL_VARCHAR>).  Newer versions always attempt to call
C<SQLDescribeParam> to find the parameter types but if
C<SQLDescribeParam> is unavailable DBD::ODBC falls back to a default
bind type. The internal default bind type is C<SQL_VARCHAR> (for
non-unicode build) and C<SQL_WVARCHAR> (for a unicode build). If you
set C<odbc_default_bind_type> to a value other than 0 you override the
internal default.

B<N.B> If you call the C<bind_param> method with a SQL type this
overrides everything else above.

=head3 odbc_force_bind_type

This value defaults to 0.

If set to anything other than 0 this will force bound parameters to be
bound as this type and C<SQLDescribeParam> will not be used.

Older versions of DBD::ODBC assumed the parameter binding type was 12
(C<SQL_VARCHAR>) and newer versions always attempt to call
C<SQLDescribeParam> to find the parameter types. If your driver
supports C<SQLDescribeParam> and it succeeds it may still fail to
describe the parameters accurately (MS SQL Server sometimes does this
with some SQL like I<select myfunc(?)  where 1 = 1>). Setting
C<odbc_force_bind_type> to C<SQL_VARCHAR> will force DBD::ODBC to bind
all the parameters as C<SQL_VARCHAR> and ignore SQLDescribeParam.

Bare in mind that if you are inserting unicode data you probably want
to use C<SQL_WVARCHAR> and not C<SQL_VARCHAR>.

As this attribute was created to work around buggy ODBC Drivers which
support SQLDescribeParam but describe the parameters incorrectly you
are probably better specifying the bind type on the C<bind_param> call
on a per statement level rather than blindly setting
C<odbc_force_bind_type> across a whole connection.

B<N.B> If you call the C<bind_param> method with a SQL type this
overrides everything else above.

=head3 odbc_force_rebind

This is to handle special cases, especially when using multiple result sets.
Set this before execute to "force" DBD::ODBC to re-obtain the result set's
number of columns and column types for each execute.  Especially useful for
calling stored procedures which may return different result sets each
execute.  The only performance penalty is during execute(), but I didn't
want to incur that penalty for all circumstances.  It is probably fairly
rare that this occurs.  This attribute will be automatically set when
multiple result sets are triggered.  Most people shouldn't have to worry
about this.

=head3 odbc_async_exec

Allow asynchronous execution of queries.  This causes a spin-loop
(with a small "sleep") until the ODBC API being called is complete
(i.e., while the ODBC API returns C<SQL_STILL_EXECUTING>).  This is
useful, however, if you want the error handling and asynchronous
messages (see the L</odbc_err_handler> and F<t/20SQLServer.t> for an
example of this).

=head3 odbc_query_timeout

This allows you to change the ODBC query timeout (the ODBC statement
attribute C<SQL_ATTR_QUERY_TIMEOUT>). ODBC defines the query time out as
the number of seconds to wait for a SQL statement to execute before
returning to the application. A value of 0 (the default) means there
is no time out. Do not confuse this with the ODBC attributes
C<SQL_ATTR_LOGIN_TIMEOUT> and C<SQL_ATTR_CONNECTION_TIMEOUT>. Add

  { odbc_query_timeout => 30 }

to your connect, set on the C<dbh> before creating a statement or
explicitly set it on your statement handle. The odbc_query_timeout on
a statement is inherited from the parent connection.

Note that internally DBD::ODBC only sets the query timeout if you set it
explicitly and the default of 0 (no time out) is implemented by the
ODBC driver and not DBD::ODBC.

Note that some ODBC drivers implement a maximum query timeout value
and will limit timeouts set above their maximum. You may see a
warning if your time out is capped by the driver but there is
currently no way to retrieve the capped value back from the driver.

Note that some drivers may not support this attribute.

See F<t/20SqlServer.t> for an example.

=head3 odbc_putdata_start

C<odbc_putdata_start> defines the size at which DBD::ODBC uses
C<SQLPutData> and C<SQLParamData> to send larger objects to the
database instead of simply binding them as normal with
C<SQLBindParameter>. It is mostly a placeholder for future changes
allowing chunks of data to be sent to the database and there is little
reason for anyone to change it currently.

The default for odbc_putdata_start is 32768 because this value was
hard-coded in DBD::ODBC until 1.16_1.

=head3 odbc_column_display_size

If you ODBC driver does not support the SQL_COLUMN_DISPLAY_SIZE and
SQL_COLUMN_LENGTH attributes to SQLColAtrributes then DBD::ODBC does
not know how big the column might be. odbc_column_display_size sets
the default value for the column size when retrieving column data
where the size cannot be determined.

The default for odbc_column_display_size is 2001 because this value was
hard-coded in DBD::ODBC until 1.17_3.

=head3 odbc_utf8_on

Set this flag to treat all strings returned from the ODBC driver
(except columns described as SQL_BINARY or SQL_TIMESTAMP and its
variations) as UTF-8 encoded.  Some ODBC drivers (like Aster and maybe
PostgreSQL) return UTF-8 encoded data but do not support the SQLxxxW
unicode API. Enabling this flag will cause DBD::ODBC to treat driver
returned data as UTF-8 encoded and it will be marked as such in Perl.

Do not confuse this with DBD::ODBC's unicode support. The
C<odbc_utf8_on> attribute only applies to non-unicode enabled builds
of DBD::ODBC.

=head3 odbc_old_unicode

Defaults to off. If set to true returns DBD::ODBC to the old unicode
behavior in 1.29 and earlier. You can also set this on the prepare
method.

By default DBD::ODBC now binds all char columns as SQL_WCHARs meaning
the driver is asked to return the bound data as wide (Unicode)
characters encoded in UCS2. So long as the driver supports the ODBC
Unicode API properly this should mean you get your data back correctly
in Perl even if it is in a character set (codepage) different from the
one you are working in.

However, if you wrote code using DBD::ODBC 1.29 or earlier and knew
DBD::ODBC bound varchar/longvarchar columns as SQL_CHARs and decoded
them yourself the new behaviour will adversely affect you (sorry). To
revert to the old behaviour set odbc_old_unicode to true.

You can also set this attribute in the attributes passed to the
prepare method.

See the stackoverflow question at
L<http://stackoverflow.com/questions/5912082>, the RT at
L<http://rt.cpan.org/Public/Bug/Display.html?id=67994> and lastly a
small discussion on dbi-dev at
L<http://www.nntp.perl.org/group/perl.dbi.dev/2011/05/msg6559.html>.

=head3 odbc_describe_parameters

Defaults to on. When set this allows DBD::ODBC to call SQLDescribeParam
(if the driver supports it) to retrieve information about any
parameters.

When off/false DBD::ODBC will not call SQLDescribeParam and defaults
to binding parameters as SQL_CHAR/SQL_WCHAR depending on the build
type.

You do not have to disable odbc_describe_parameters just because your
driver does not support SQLDescribeParam as DBD::ODBC will work this
out at the start via SQLGetFunctions.

Note: disabling odbc_describe_parameters when your driver does support
SQLDescribeParam may prevent DBD::ODBC binding parameters for some
column types properly.

You can also set this attribute in the attributes passed to the
prepare method.

This attribute was added so someone moving from freeTDS (a driver
which does not support SQLDescribeParam) to a driver which does
support SQLDescribeParam could do so without changing any Perl. The
situation was very specific since dates were being bound as dates when
SQLDescribeParam was called and chars without and the data format was
not a supported date format.

=head2 Private methods common to connection and statement handles

=head3 odbc_getdiagrec

  @diags = $handle->odbc_getdiagrec($record_number);

NOTE: This is an experimental method and may change.

Introduced in 1.34_3.

This is just a wrapper around the ODBC API SQLGetDiagRec. When a
method on a connection or statement handle fails if there are any ODBC
diagnostics you can use this method to retrieve them. Records start at
1 and there may be more than 1. It returns an array containing the
state, native and error message text or an empty array if the requested
diagnostic record does not exist. To get all diagnostics available
keep incrementing $record_number until odbc_getdiagrec returns an
empty array.

All of the state, native and message text are already passed to DBI
via its set_err method so this method does not really tell you
anything you cannot already get from DBI except when there is more
than one diagnostic.

You may find this useful in an error handler as you can get the ODBC
diagnostics as they are and not how DBD::ODBC was forced to fit them
into the DBI's system.

NOTE: calling this method does not clear DBI's error values as usually
happens.

=head3 odbc_getdiagfield

  $diag = $handle->odbc_getdiagfield($record, $identifier);

NOTE: This is an experimental method and may change.

This is just a wrapper around the ODBC API SQLGetDiagField. When a
method on a connection or statement handle fails if there are any
ODBC diagnostics you can use this method to retrieve the individual
diagnostic fields. As with L</odbc_getdiagrec> records start at 1. The
identifier is one of:

  SQL_DIAG_CURSOR_ROW_COUNT
  SQL_DIAG_DYNAMIC_FUNCTION
  SQL_DIAG_DYNAMIC_FUNCTION_CODE
  SQL_DIAG_NUMBER
  SQL_DIAG_RETURNCODE
  SQL_DIAG_ROW_COUNT
  SQL_DIAG_CLASS_ORIGIN
  SQL_DIAG_COLUMN_NUMBER
  SQL_DIAG_CONNECTION_NAME
  SQL_DIAG_MESSAGE_TEXT
  SQL_DIAG_NATIVE
  SQL_DIAG_ROW_NUMBER
  SQL_DIAG_SERVER_NAME
  SQL_DIAG_SQLSTATE
  SQL_DIAG_SUBCLASS_ORIGIN

DBD::ODBC exports these constants as 'diags' e.g.,

  use DBD::ODBC qw(:diags);

Of particular interest is SQL_DIAG_COLUMN_NUMBER as it will tell you
which bound column or parameter is in error (assuming your driver
supports it). See params_in_error in the examples dir.

NOTE: calling this method does not clear DBI's error values as usually
happens.

=head2 Private connection attributes

=head3 odbc_err_handler

B<NOTE:> You might want to look at DBI's error handler before using
the one in DBD::ODBC however, there are subtle
differences. DBD::ODBC's odbc_err_handler is called for error B<and>
informational diagnostics i.e., it is called when an ODBC call fails
the SQL_SUCCEEDED macro which means the ODBC call returned SQL_ERROR
(-1) or SQL_SUCCESS_WITH_INFO (1).

Allow error and informational diagnostics to be handled by the
application.  A call-back function supplied by the application to
handle or ignore messages.

The callback function receives four parameters: state (string),
error (string), native error code (number) and the status returned
from the last ODBC API. The fourth argument was added in 1.30_7.

If the error handler returns 0, the error is ignored, otherwise the
error is passed through the normal DBI error handling. Note, if the
status is SQL_SUCCESS_WITH_INFO this will B<not> reach the DBI error
handler as it is not an error.

This can also be used for procedures under MS SQL Server (Sybase too,
probably) to obtain messages from system procedures such as DBCC.
Check F<t/20SQLServer.t> and F<t/10handler.t>.

  $dbh->{RaiseError} = 1;
  sub err_handler {
     ($state, $msg, $native, $rc, $status) = @_;
     if ($state = '12345')
         return 0; # ignore this error
     else
         return 1; # propagate error
  }
  $dbh->{odbc_err_handler} = \&err_handler;
  # do something to cause an error
  $dbh->{odbc_err_handler} = undef; # cancel the handler

=head3 odbc_SQL_ROWSET_SIZE

Setting odbc_SQL_ROWSET_SIZE results in a call to SQLSetConnectAttr
to set the ODBC SQL_ROWSET_SIZE (9) attribute to whatever value you
set odbc_SQL_ROWSET_SIZE to.

The ODBC default for SQL_ROWSET_SIZE is 1.

Usually MS SQL Server does not support multiple active statements
(MAS) i.e., you cannot have 2 or more outstanding selects.  You can
set odbc_SQL_ROWSET_SIZE to 2 to persuade MS SQL Server to support
multiple active statements.

Setting SQL_ROWSET_SIZE usually only affects calls to SQLExtendedFetch
but does allow MAS and as DBD::ODBC does not use SQLExtendedFetch there
should be no ill effects to DBD::ODBC.

Be careful with this attribute as once set to anything larger than 1
(the default) you must retrieve all result-sets before the statement
handle goes out of scope or you can upset the TDS protocol and this
can result in a hang. With DBI this is unlikely as DBI warns when a
statement goes out of scope with outstanding results.

NOTE: if you get an error saying "[Microsoft][ODBC SQL Server
Driver]Invalid attribute/option identifier (SQL-HY092)" when you set
odbc_SQL_ROWSET_SIZE in the connect method you need to either a)
upgrade to DBI 1.616 or above b) set odbc_SQL_ROWSET_SIZE after
connect.

In versions of SQL Server 2005 and later see "Multiple Active
Statements (MAS)" in the DBD::ODBC::FAQ instead of using this
attribute.

Thanks to Andrew Brown for the original patch.

DBD developer note: Here lies a bag of worms. Firstly, SQL_ROWSET_SIZE
is an ODBC 2 attribute and is usally a statement attribute not a
connection attribute. However, in ODBC 2.0 you could set statement
attributes on a connection handle and it acted as a default for all
subsequent statement handles created under that connection handle. If
you are using ODBC 3 the driver manager continues to map this call but
the ODBC Driver needs to act on it (the MS SQL Server driver still
appears to but some other ODBC drivers for MS SQL Server do not).
Secondly, somewhere a long the line MS decided it was no longer valid
to retrieve the SQL_ROWSET_SIZE attribute from a connection handle in
an ODBC 3 application (which DBD::ODBC now is). In itself, this would
not be a problem except for a minor bug in DBI which until release
1.616 mistakenly issued a FETCH on any attribute mentioned in the
connect method call. As a result, it you use a DBI prior to 1.616 and
attempt to set odbc_SQL_ROWSET_SIZE in the connect method call, DBI
issues a FETCH on odbc_SQL_ROWSET_SIZE and the driver manager throws
it out as an invalid attribute thus resulting in an error. The only
way around this (other than upgrading DBI) is to set
odbc_SQL_ROWSET_SIZE AFTER the call to connect. Thirdly, MS withdrew
the SQLROWSETSIZE macro from the sql header files in MDAC 2.7 for 64
bit platforms i.e., SQLROWSETSIZE is not defined on 64 bit platforms
from MDAC 2.7 as it is in a "#ifdef win32" (see
http://msdn.microsoft.com/en-us/library/ms716287%28v=vs.85%29.aspx).
Setting SQL_ROWSET_SIZE still seems to take effect on 64 bit platforms
but you can no longer retrieve its value from a connection handle
(hence the issue above with DBI redundant FETCH).

=head3 odbc_exec_direct

Force DBD::ODBC to use C<SQLExecDirect> instead of
C<SQLPrepare>/C<SQLExecute>.

There are drivers that only support C<SQLExecDirect> and the DBD::ODBC
do() override does not allow returning result sets.  Therefore, the
way to do this now is to set the attribute odbc_exec_direct.

NOTE: You may also want to use this option if you are creating
temporary objects (e.g., tables) in MS SQL Server and for some
reason cannot use the C<do> method. see
L<http://technet.microsoft.com/en-US/library/ms131667.aspx> which says
I<Prepared statements cannot be used to create temporary objects on
SQL Server 2000 or later...>. Without odbc_exec_direct, the temporary
object will disappear before you can use it.

There are currently two ways to get this:

    $dbh->prepare($sql, { odbc_exec_direct => 1});

and

    $dbh->{odbc_exec_direct} = 1;

B<NOTE:> Even if you build DBD::ODBC with unicode support you can
still not pass unicode strings to the prepare method if you also set
odbc_exec_direct. This is a restriction in this attribute which is
unavoidable.

=head3 odbc_SQL_DRIVER_ODBC_VER

This, while available via get_info() is captured here.  I may get rid
of this as I only used it for debugging purposes.

=head3 odbc_cursortype

This allows multiple concurrent statements on SQL*Server.  In your
connect, add

  { odbc_cursortype => 2 }.

If you are using DBI > 1.41, you should also be able to use

 { odbc_cursortype => DBI::SQL_CURSOR_DYNAMIC }

instead.  For example:

    my $dbh = DBI->connect("dbi:ODBC:$DSN", $user, $pass,
                  { RaiseError => 1, odbc_cursortype => 2});
    my $sth = $dbh->prepare("one statement");
    my $sth2 = $dbh->prepare("two statement");
    $sth->execute;
    my @row;
    while (@row = $sth->fetchrow_array) {
       $sth2->execute($row[0]);
    }

See F<t/20SqlServer.t> for an example.

In versions of SQL Server 2005 and later see "Multiple Active Statements (MAS)" in the DBD::ODBC::FAQ instead of using this attribute.

=head3 odbc_has_unicode

A read-only attribute signifying whether DBD::ODBC was built with the
C macro WITH_UNICODE or not. A value of 1 indicates DBD::ODBC was built
with WITH_UNICODE else the value returned is 0.

Building WITH_UNICODE affects columns and parameters which are
SQL_C_WCHAR, SQL_WCHAR, SQL_WVARCHAR, and SQL_WLONGVARCHAR, SQL,
the connect method and a lot more. See L</Unicode>.

When odbc_has_unicode is 1, DBD::ODBC will:

=over

=item bind columns the database declares as wide characters as SQL_Wxxx

This means that UNICODE data stored in these columns will be returned
to Perl in UTF-8 and with the UTF-8 flag set.

=item bind parameters the database declares as wide characters as SQL_Wxxx

Parameters bound where the database declares the parameter as being a
wide character (or where the parameter type is explicitly set to a
wide type - SQL_Wxxx) can be UTF-8 in Perl and will be mapped to
UTF-16 before passing to the driver.

=item SQL

SQL passed to the C<prepare> or C<do> methods which has the UTF-8 flag
set will be converted to UTF-16 before being passed to the ODBC APIs
C<SQLPrepare> or C<SQLExecDirect>.

=item connection strings

Connection strings passed to the C<connect> method will be converted
to UTF-16 before being passed to the ODBC API
C<SQLDriverConnectW>. This happens irrespective of whether the UTF-8
flag is set on the perl connect strings because unixODBC requires an
application to call SQLDriverConnectW to indicate it will be calling
the wide ODBC APIs.

=back

NOTE: You will need at least Perl 5.8.1 to use UNICODE with DBD::ODBC.

NOTE: Binding of unicode output parameters is coded but untested.

NOTE: When building DBD::ODBC on Windows ($^O eq 'MSWin32') the
WITH_UNICODE macro is automatically added. To disable specify -nou as
an argument to Makefile.PL (e.g. C<perl Makefile.PL -nou>). On non-Windows
platforms the WITH_UNICODE macro is B<not> enabled by default and to enable
you need to specify the -u argument to Makefile.PL. Please bare in mind
that some ODBC drivers do not support SQL_Wxxx columns or parameters.

UNICODE support in ODBC Drivers differs considerably. Please read the
README.unicode file for further details.

=head3 odbc_out_connect_string

After calling the connect method this will be the ODBC driver's
out connection string - see documentation on SQLDriverConnect.

=head3 odbc_version

This was added prior to the move to ODBC 3.x to allow the caller to
"force" ODBC 3.0 compatibility.  It's probably not as useful now, but
it allowed get_info and get_type_info to return correct/updated
information that ODBC 2.x didn't permit/provide.  Since DBD::ODBC is
now 3.x, this can be used to force 2.x behavior via something like: my

  $dbh = DBI->connect("dbi:ODBC:$DSN", $user, $pass,
                      { odbc_version =>2});

=head3 odbc_driver_complete

This attribute was added to DBD::ODBC in 1.32_2.

odbc_driver_complete is only relevant to the Windows operating system
and will be ignored on other platforms. It is off by default.

When set to a true value DBD::ODBC attempts to obtain a window handle
and calls SQLDriverConnect with the SQL_DRIVER_COMPLETE attribute
instead of the normal SQL_DRIVER_NOPROMPT option. What this means is
that if the connection string does not describe sufficient attributes
to enable the ODBC driver manager to connect to a data source it will
throw a dialogue allowing you to input the remaining attributes. Once
you ok that dialogue the ODBC Driver Manager will continue as if you
specified those attributes in the connection string. Once the
connection is complete you may want to look at the odbc_out_connect_string
attribute to obtain a connection string you can use in the future to
pass into the connect method without prompting.

As a window handle is passed to SQLDriverConnect it also means the
ODBC driver may throw a dialogue e.g., if your password has expired
the MS SQL Server driver will often prompt for a new one.

An example is:

  my $h = DBI->connect('dbi:ODBC:DRIVER={SQL Server}', "username", "password",
                       {odbc_driver_complete => 1});

As this only provides the driver and further attributes are required a
dialogue will be thrown allowing you to specify the SQL Server to
connect to and possibly other attributes.

=head3 odbc_batch_size

Sets the batch size for execute_for_fetch which defaults to 10.
Bare in mind the bigger you set this the more memory DBD::ODBC will need
to allocate when running execute_for_fetch and the memory required is
max_length_of_pn * odbc_batch_size * n_parameters.

=head3 odbc_array_operations

NOTE: this was briefly odbc_disable_array_operations in 1.35 and 1.36_1.
I did warn it was experimental and it turned out the default was too
ambitious and it was a poor name anyway. Also the default was to use
array operations and now the default is the opposite.

If set to true DBD::ODBC uses its own internal execute_for_fetch
instead of DBI's default execute_for_fetch. The default is false.
Using the internal execute_for_fetch should be quite a bit faster when
using arrays of parameters for insert/update/delete operations as
batches of parameters are sent to the database in one go. However,
the required support in some ODBC drivers is a little sketchy and there
is no way for DBD::ODBC to ascertain this until it is too late.

Please read the documentation on L<execute_array> and L<execute_for_fetch>
which details subtle differences in DBD::ODBC's implementation compared
with using DBI's default implementation. If these difference cause you
a problem you can set odbc_array_operations to false and DBD::ODBC
will revert to DBI's implementations of the array methods.

You can use the environment variable ODBC_DISABLE_ARRAY_OPERATIONS to
switch array operations on/off too. When set to 1 array operations are
disabled. When not set the default is used (which currently is off).
When set to 0 array operations are used no matter what. I know this is
slightly counter intuitive but I've found it difficult to change the
name (it got picked up and used in a few places very quickly).

=head2 Private statement attributes

=head3 odbc_more_results

Use this attribute to determine if there are more result sets
available.  SQL Server supports this feature.  Use this as follows:

  do {
     my @row;
     while (@row = $sth->fetchrow_array()) {
        # do stuff here
     }
  } while ($sth->{odbc_more_results});

Note that with multiple result sets and output parameters (i.e,. using
bind_param_inout), don't expect output parameters to written to until ALL
result sets have been retrieved.

=head2 Private statement methods

=head3 odbc_lob_read

  $chrs_or_bytes_read = $sth->lob_read($column_no, \$lob, $length, \%attr);

Reads C<$length> bytes from the lob at column C<$column_no> returning
the lob into C<$lob> and the number of bytes or characters read into
C<$chrs_or_bytes_read>. If an error occurs undef will be returned.
When there is no more data to be read 0 is returned.

NOTE: This is currently an experimental method and may change in the
future e.g., it may support automatic concatenation of the lob
parts onto the end of the C<$lob> with the addition of an extra flag
or destination offset as in DBI's undocumented blob_read.

The type the lob is retrieved as may be overridden in C<%attr> using
C<TYPE =E<gt> sql_type>. C<%attr> is optional and if omitted defaults
to SQL_C_BINARY for binary columns and SQL_C_CHAR/SQL_C_WCHAR for
other column types depending on whether DBD::ODBC is built with
unicode support. C<$chrs_or_bytes_read> will by the bytes read when
the column types SQL_C_CHAR or SQL_C_BINARY are used and characters
read if the column type is SQL_C_WCHAR.

When built with unicode support C<$length> specifes the amount of
buffer space to be used when retrieving the lob data but as it is
returned as SQLWCHAR characters this means you at most retrieve
C<$length/2> characters. When those retrieved characters are encoded
in UTF-8 for Perl, the C<$lob> scalar may need to be larger than
C<$length> so DBD::ODBC grows it appropriately.

You can retrieve a lob in chunks like this:

  $sth->bind_col($column, undef, {TreatAsLOB=>1});
  while(my $retrieved = $sth->odbc_lob_read($column, \my $data, $length)) {
      print "retrieved=$retrieved lob_data=$data\n";
  }

NOTE: to retrieve a lob like this you B<must> first bind the lob
column specifying BindAsLOB or DBD::ODBC will 1) bind the column as
normal and it will be subject to LongReadLen and b) fail
odbc_lob_read.

NOTE: Some database engines and ODBC drivers do not allow you to
retrieve columns out of order (e.g., MS SQL Server unless you are
using cursors).  In those cases you must ensure the lob retrieved is
the last (or only) column in your select list.

NOTE: You can retrieve only part of a lob but you will probably have
to call finish on the statement handle before you do anything else
with that statement. When only retrieving part of a large lob you
could see a small delay when you call finish as some protocols used
by ODBC drivers send the lob down the socket synchonously and there is
no way to stop it (this means the ODBC driver needs to read all the
lob from the socket even though you never retrieved it all yourself).

NOTE: If your select contains multiple lobs you cannot read part of
the first lob, the second lob then return to the first lob. You must
read all lobs in order and completely or read part of a lob and then
do no further calls to odbc_lob_read.

=head2 Private DBD::ODBC Functions

You use DBD::ODBC private functions like this:

  $dbh->func(arg, private_function_name, @args);

=head3 GetInfo

B<This private function is now superceded by DBI's get_info method.>

This function maps to the ODBC SQLGetInfo call and the argument
should be a valid ODBC information type (see ODBC specification).
e.g.

  $value = $dbh->func(6, 'GetInfo');

which returns the C<SQL_DRIVER_NAME>.

This function returns a scalar value, which can be a numeric or string
value depending on the information value requested.

=head3 SQLGetTypeInfo

B<This private function is now superceded by DBI's type_info and
type_info_all methods.>

This function maps to the ODBC SQLGetTypeInfo API and the argument
should be a SQL type number (e.g. SQL_VARCHAR) or
SQL_ALL_TYPES. SQLGetTypeInfo returns information about a data type
supported by the data source.

e.g.

  use DBI qw(:sql_types);

  $sth = $dbh->func(SQL_ALL_TYPES, GetTypeInfo);
  DBI::dump_results($sth);

This function returns a DBI statement handle for the SQLGetTypeInfo
result-set containing many columns of type attributes (see ODBC
specification).

NOTE: It is VERY important that the C<use DBI> includes the
C<qw(:sql_types)> so that values like SQL_VARCHAR are correctly
interpreted.  This "imports" the sql type names into the program's
name space.  A very common mistake is to forget the C<qw(:sql_types)>
and obtain strange results.

=head3 GetFunctions

This function maps to the ODBC SQLGetFunctions API which returns
information on whether a function is supported by the ODBC driver.

The argument should be C<SQL_API_ALL_FUNCTIONS> (0) for all functions
or a valid ODBC function number (e.g. C<SQL_API_SQLDESCRIBEPARAM>
which is 58). See ODBC specification or examine your sqlext.h and
sql.h header files for all the SQL_API_XXX macros.

If called with C<SQL_API_ALL_FUNCTIONS> (0), then a 100 element array is
returned where each element will contain a '1' if the ODBC function with
that SQL_API_XXX index is supported or '' if it is not.

If called with a specific SQL_API_XXX value for a single function it will
return true if the ODBC driver supports that function, otherwise false.

e.g.


    my @x = $dbh->func(0,"GetFunctions");
    print "SQLDescribeParam is supported\n" if ($x[58]);

or

    print "SQLDescribeParam is supported\n"
        if $dbh->func(58, "GetFunctions");

=head3 GetStatistics

B<This private function is now superceded by DBI's statistics_info
method.>

See the ODBC specification for the SQLStatistics API.
You call SQLStatistics like this:

  $dbh->func($catalog, $schema, $table, $unique, 'GetStatistics');

Prior to DBD::ODBC 1.16 $unique was not defined as being true/false or
SQL_INDEX_UNIQUE/SQL_INDEX_ALL. In fact, whatever value you provided
for $unique was passed through to the ODBC API SQLStatistics call
unchanged. This changed in 1.16, where $unique became a true/false
value which is interpreted into SQL_INDEX_UNIQUE for true and
SQL_INDEX_ALL for false.

=head3 GetForeignKeys

B<This private function is now superceded by DBI's foreign_key_info
method.>

See the ODBC specification for the SQLForeignKeys API.
You call SQLForeignKeys like this:

  $dbh->func($pcatalog, $pschema, $ptable,
             $fcatalog, $fschema, $ftable,
             "GetForeignKeys");

=head3 GetPrimaryKeys

B<This private function is now superceded by DBI's primary_key_info
method.>

See the ODBC specification for the SQLPrimaryKeys API.
You call SQLPrimaryKeys like this:

  $dbh->func($catalog, $schema, $table, "GetPrimaryKeys");

=head3 data_sources

B<This private function is now superceded by DBI's data_sources
method.>

You call data_sources like this:

  @dsns = $dbh->func("data_sources);

Handled since 0.21.

=head3 GetSpecialColumns

See the ODBC specification for the SQLSpecialColumns API.
You call SQLSpecialColumns like this:

  $dbh->func($identifier, $catalog, $schema, $table, $scope,
             $nullable, 'GetSpecialColumns');

Handled as of version 0.28

=head3 ColAttributes

B<This private function is now superceded by DBI's statement attributes
NAME, TYPE, PRECISION, SCALE, NULLABLE etc).>

See the ODBC specification for the SQLColAttributes API.
You call SQLColAttributes like this:

  $sth->func($column, $ftype, "ColAttributes");

  SQL_COLUMN_COUNT = 0
  SQL_COLUMN_NAME = 1
  SQL_COLUMN_TYPE = 2
  SQL_COLUMN_LENGTH = 3
  SQL_COLUMN_PRECISION = 4
  SQL_COLUMN_SCALE = 5
  SQL_COLUMN_DISPLAY_SIZE = 6
  SQL_COLUMN_NULLABLE = 7
  SQL_COLUMN_UNSIGNED = 8
  SQL_COLUMN_MONEY = 9
  SQL_COLUMN_UPDATABLE = 10
  SQL_COLUMN_AUTO_INCREMENT = 11
  SQL_COLUMN_CASE_SENSITIVE = 12
  SQL_COLUMN_SEARCHABLE = 13
  SQL_COLUMN_TYPE_NAME = 14
  SQL_COLUMN_TABLE_NAME = 15
  SQL_COLUMN_OWNER_NAME = 16
  SQL_COLUMN_QUALIFIER_NAME = 17
  SQL_COLUMN_LABEL = 18

B<Note:>Oracle's ODBC driver for linux in instant client 11r1 often
returns strange values for column name e.g., '20291'. It is wiser to
use DBI's NAME and NAME_xx attributes for portability.


=head3 DescribeCol

B<This private function is now superceded by DBI's statement attributes
NAME, TYPE, PRECISION, SCALE, NULLABLE etc).>

See the ODBC specification for the SQLDescribeCol API.
You call SQLDescribeCol like this:

  @info = $sth->func($column, "DescribeCol");

The returned array contains the column attributes in the order described
in the ODBC specification for SQLDescribeCol.

=head2 Additional bind_col attributes

DBD::ODBC supports a few additional attributes which may be passed to
the bind_col method in the attributes.

=head3 DiscardString

See DBI's sql_type_cast utility function.

If you bind a column as a specific type (SQL_INTEGER, SQL_DOUBLE and
SQL_NUMERIC are the only ones supported currently) and you add
DiscardString to the prepare attributes then if the returned bound
data is capable of being converted to that type the scalar's pv (the
string portion of a scalar) is cleared.

This is especially useful if you are using a module which uses a
scalars flags and/or pv to decide if a scalar is a number. JSON::XS
does this and without this flag you have to add 0 to all bound column
data returning numbers to get JSON::XS to encode it is N instead of
"N".

NOTE: For DiscardString you need at least DBI 1.611.

=head3 StrictlyTyped

See DBI's sql_type_cast utility function.

See L</DiscardString> above.

Specifies that when DBI's sql_type_cast function is called on returned
data where a bind type is specified that if the conversion cannot be
performed an error will be raised.

This is probably not a lot of use with DBD::ODBC as if you ask for say
an SQL_INTEGER and the data is not able to be converted to an integer
the ODBC driver will problably return "Invalid character value for
cast specification (SQL-22018)".

NOTE: For DiscardString you need at least DBI 1.611.

=head3 TreatAsLOB

See L</odbc_lob_read>.

=head2 Tracing

DBD::ODBC now supports the parse_trace_flag and parse_trace_flags
methods introduced in DBI 1.42 (see DBI for a full description).  As
of DBI 1.604, the only trace flag defined which is relevant to
DBD::ODBC is 'SQL' which DBD::ODBC supports by outputting the SQL
strings (after modification) passed to the prepare and do methods.

From DBI 1.617 DBI also defines ENC (encoding), CON (connection) TXN
(transaction) and DBD (DBD only) trace flags. DBI's ENC and CON trace
flags are synonomous with DBD::ODBC's odbcunicode and odbcconnection
trace flags though I may remove the DBD::ODBC ones in the
future. DBI's DBD trace flag allows output of only DBD::ODBC trace
messages without DBI's trace messages.

Currently DBD::ODBC supports two private trace flags. The
'odbcunicode' flag traces some unicode operations and the
odbcconnection traces the connect process.

To enable tracing of particular flags you use:

  $h->trace($h->parse_trace_flags('SQL|odbcconnection'));
  $h->trace($h->parse_trace_flags('1|odbcunicode'));

In the first case 'SQL' and 'odbcconnection' tracing is enabled on
$h. In the second case trace level 1 is set and 'odbcunicode' tracing
is enabled.

If you want to enable a DBD::ODBC private trace flag before connecting
you need to do something like:

  use DBD::ODBC;
  DBI->trace(DBD::ODBC->parse_trace_flag('odbcconnection'));

or

  use DBD::ODBC;
  DBI->trace(DBD::ODBC->parse_trace_flags('odbcconnection|odbcunicode'));

or

  DBI_TRACE=odbcconnection|odbcunicode perl myscript.pl

From DBI 1.617 you can output only DBD::ODBC trace messages using

  DBI_TRACE=DBD perl myscript.pl

DBD::ODBC outputs tracing at levels 3 and above (as levels 1 and 2 are
reserved for DBI).

For comprehensive tracing of DBI method calls without all the DBI
internals see L<DBIx::Log4perl>.

=head2 Deviations from the DBI specification

=head3 last_insert_id

DBD::ODBC does not support DBI's last_insert_id. There is no ODBC
defined way of obtaining this information. Generally the mechanism
(and it differs vastly between databases and ODBC drivers) it to issue
a select of some form (e.g., select @@identity or select
sequence.currval from dual, etc).

There are literally dozens of databases and ODBC drivers supported by
DBD::ODBC and I cannot have them all. If you know how to retrieve the
information for last_insert_id and you mail me the ODBC Driver
name/version and database name/version with a small working example I
will collect examples and document them here.

B<Microsoft Access>. Recent versions of MS Access support I<select
@@identity> to retrieve the last insert ID.  See
http://support.microsoft.com/kb/815629. Information provided by Robert
Freimuth.

=head3 Comments in SQL

DBI does not say anything in particular about comments in SQL.
DBD::ODBC looks for placeholders in the SQL string and until 1.24_2 it
did not recognise comments in SQL strings so could find what it
believes to be a placeholder in a comment e.g.,

  select '1' /* placeholder ? in comment */
  select -- named placeholder :named in comment
    '1'

I cannot be exact about support for ignoring placeholders in literals
but it has existed for a long time in DBD::ODBC. Support for ignoring
placeholders in comments was added in 1.24_2. If you find a case where
a named placeholder is not ignored and should be, see
L</odbc_ignore_named_placeholders> for a workaround and mail me an
example along with your ODBC driver name.

=head3 do

This is not really a deviation from the DBI specification since DBI
allows a driver to avoid the overhead of creating an DBI statement
handle for do().

DBD::ODBC implements C<do> by calling SQLExecDirect in ODBC and not
SQLPrepare followed by SQLExecute so C<do> is not the same as:

  $dbh->prepare($sql)->execute()

It does this to avoid a round-trip to the server so it is faster.
Normally this is good but some people fall foul of this with MS SQL
Server if they call a procedure which outputs print statements (e.g.,
backup) as the procedure may not complete. See the DBD::ODBC FAQ and
in general you are better to use prepare/execute when calling
procedures.

In addition, you should realise that since DBD::ODBC does not create a
DBI statement for do calls, if you set up an error handler the handle
passed in when a do fails will be the database handle and not
a statement handle.

=head3 Mixed placeholder types

There are 3 conventions for place holders in DBI. These are '?', ':N'
and ':name' (where 'N' is a number and 'name' is an alpha numeric
string not beginning with a number). DBD::ODBC supports all these methods
for naming placeholders but you must only use one method throughout
a particular SQL string. If you mix placeholder methods you will get
an error like:

  Can't mix placeholder styles (1/2)

=head3 Using the same placeholder more than once

DBD::ODBC does not support (currently) the use of one named placeholder
more than once in the a single SQL string. i.e.,

  insert into foo values (:bar, :p1, :p2, :bar);

is not supported because 'bar' is used more than once but:

  insert into foo values(:bar, :p1, :p2)

is ok. If you do the former you will get an error like:

  DBD::ODBC does not yet support binding a named parameter more than once

=head3 Binding named placeholders

Although the DBI documentation (as of 1.604) does not say how named
parameters are bound Tim Bunce has said that in Oracle they are bound
with the leading ':' as part of the name and that has always been the
case. i.e.,

  prepare("insert into mytable values (:fred)");
  bind_param(":foo", 1);

DBD::ODBC does not support binding named parameters with the ':' introducer.
In the above example you must use:

  bind_param("foo", 1);

In discussion on the dbi-dev list is was suggested that the ':' could
be made optional and there were no basic objections but it has not
made it's way into the pod yet.

=head3 Sticky Parameter Types

The DBI specification post 1.608 says in bind_param:

  The data type is 'sticky' in that bind values passed to execute()
  are bound with the data type specified by earlier bind_param()
  calls, if any.  Portable applications should not rely on being able
  to change the data type after the first C<bind_param> call.

DBD::ODBC does allow a parameter to be rebound with another data type as
ODBC inherently allows this. Therefore you can do:

  # parameter 1 set as a SQL_LONGVARCHAR
  $sth->bind_param(1, $data, DBI::SQL_LONGVARCHAR);
  # without the bind above the $data parameter would be either a DBD::ODBC
  # internal default or whatever the ODBC driver said it was but because
  # parameter types are sticky, the type is still SQL_LONGVARCHAR.
  $sth->execute($data);
  # change the bound type to SQL_VARCHAR
  # some DBDs will ignore the type in the following, DBD::ODBC does not
  $sth->bind_param(1, $data, DBI::SQL_VARCHAR);

=head3 disconnect and transactions

DBI does not define whether a driver commits or rolls back any
outstanding transaction when disconnect is called. As such DBD::ODBC
cannot deviate from the specification but you should know it rolls
back an uncommitted transaction when disconnect is called if
SQLDisconnect returns state 25000 (transaction in progress).

=head3 execute_for_fetch and execute_array

From version 1.34_1 DBD::ODBC implements its own execute_for_fetch
which binds arrays of parameters and can send multiple rows
(L</odbc_batch_size>) of parameters through the ODBC driver in one go
(this overrides DBI's default execute_for_fetch). This is much faster
when inserting, updating or deleting many rows in one go. Note,
execute_array uses execute_for_fetch when the parameters are passed
for column-wise binding.

However, there are a small number of differences between using
DBD::ODBC's execute_for_fetch compared with using DBI's default
implementation (which simply calls execute repeatedly once per row).
The differences you may see are:

o as DBI's execute_for_fetch does one row at a time the result from
execute is for one row and just about all ODBC drivers can report the
number of affected rows when SQLRowCount is called per execute. When
batches of parameters are sent the driver can still return the number
of affected rows but it is usually per batch rather than per row. As a
result, the tuple_status array you may pass to execute_for_fetch (or
execute_array) usually shows -1 (unknown) for each row although the total
affected returned in array context is a correct total affected.

o not all ODBC drivers have sufficient ODBC support (arguably a bug)
for correct diagnostics support when using arrays. DBI dictates that
if a row in the batch is in error the tuple_status will contain the
state, native and error message text. However the batch may generate
multiple errors per row (which DBI says nothing about) and more than
one row may error. In ODBC we get a list of errors but to associate
each one with a particular row we need to call SQLGetDiagField for
SQL_DIAG_ROW_NUMBER and it should say which row in the batch the
diagnostic is associated with. Some ODBC drivers do not support
SQL_DIAG_ROW_NUMBER properly and then DBD::ODBC cannot know which row
in the batch an error refers to. In this case DBD::ODBC will report an
error saying "failed to retrieve diags", state of HY000 and a native
of 1 so you'll still see an error but not necessarily the exact
one. Also, when more than one diagnostic is found for a row DBD::ODBC
picks the first one (which is usually most relevant) as there is no
way to report more than one diagnostic per row in the tuple_status. If
the first problem of SQL_DIAG_ROW_NUMBER proves to be a problem for
you the DBD::ODBC tracing will show all errors and you can also use
L</odbc_getdiagrec> yourself.

o Binding parameters with execute_array and execute_for_fetch does not
allow the parameter types to be set. However, as parameter types are sticky
you can call bind_param(param_num, undef, {TYPE => sql_type}) before
calling execute_for_fetch/execute_array and the TYPE should be sticky
when the batch of parameters is bound.

o Although you can insert very large columns execute_for_fetch will
need L</odbc_batch_size> * max length of parameter per parameter so
you may hit memory limits. If you use DBI's execute_for_fetch
DBD::ODBC uses the ODBC API SQLPutData (see L</odbc_putdata_start>)
which does not require large amounts of memory as large columns are
sent in pieces.

o A lot of drivers have bugs with arrays of parameters (see the ODBC
FAQ). e.g., as of 18-MAR-2012 I've seen the latest SQLite ODBC driver
seg fault and freeTDS 8/0.91 returns the wrong row count for batches.

o B<DO NOT> attempt to do an insert/update/delete and a select in the
same SQL with execute_array e.g.,

  SET IDENTITY_INSERT mytable ON
  insert into mytable (id, name) values (?,?)
  SET IDENTITY_INSERT mytable OFF
  SELECT SCOPE_IDENTITY()

It just won't/can't work although you may not have noticed when using
DBI's inbuilt execute_* methods. See rt 75687.

=head3 type_info_all

Many ODBC drivers now return 20 columns in type_info_all rather than
the 19 DBI documents DBI documents. The 20th column is usually called
"USERTYPE".  Recent MS SQL Server ODBC drivers do this. Fortunately
this should not adversely affect you so long as you are using the keys
provided at the start of type_info_all.

=head2 Unicode

The ODBC specification supports wide character versions (a postfix of
'W') of some of the normal ODBC APIs e.g., SQLDriverConnectW is a wide
character version of SQLDriverConnect.

In ODBC on Windows the wide characters are defined as SQLWCHARs (2
bytes) and are UCS-2. On non-Windows, the main driver managers I know
of have implemented the wide character APIs differently:

=over

=item unixODBC

unixODBC mimics the Windows ODBC API precisely meaning the wide
character versions expect and return 2-byte characters in
UCS-2.

unixODBC will happily recognise ODBC drivers which only have the ANSI
versions of the ODBC API and those that have the wide versions
too.

unixODBC will allow an ANSI application to work with a unicode
ODBC driver and vice versa (although in the latter case you obviously
cannot actually use unicode).

unixODBC does not prevent you sending UTF-8 in the ANSI versions of
the ODBC APIs but whether that is understood by your ODBC driver is
another matter.

unixODBC differs in only one way from the Microsoft ODBC driver in
terms of unicode support in that it avoids unnecessary translations
between single byte and double byte characters when an ANSI
application is using a unicode-aware ODBC driver by requiring unicode
applications to signal their intent by calling SQLDriverConnectW
first. On Windows, the ODBC driver manager always uses the wide
versions of the ODBC API in ODBC drivers which provide the wide
versions regardless of what the application really needs and this
results in a lot of unnecessary character translations when you have
an ANSI application and a unicode ODBC driver.

=item iODBC

The wide character versions expect and return wchar_t types.

=back

DBD::ODBC has gone with unixODBC so you cannot use iODBC with a
unicode build of DBD::ODBC. However, some ODBC drivers support UTF-8
(although how they do this with SQLGetData reliably I don't know)
and so you should be able to use those with DBD::ODBC not built for
unicode.

=head3 Enabling and Disabling Unicode support

On Windows Unicode support is enabled by default and to disable it
you will need to specify C<-nou> to F<Makefile.PL> to get back to the
original behavior of DBD::ODBC before any Unicode support was added.

e.g.,

  perl Makfile.PL -nou

On non-Windows platforms Unicode support is disabled by default. To
enable it specify C<-u> to F<Makefile.PL> when you configure DBD::ODBC.

e.g.,

  perl Makefile.PL -u

=head3 Unicode - What is supported?

As of version 1.17 DBD::ODBC has the following unicode support:

=over

=item SQL (introduced in 1.16_2)

Unicode strings in calls to the C<prepare> and C<do> methods are
supported so long as the C<odbc_execdirect> attribute is not used.

=item unicode connection strings (introduced in 1.16_2)

Unicode connection strings are supported but you will need a DBI
post 1.607 for that.

=item column names

Unicode column names are returned.

=item bound columns (introduced in 1.15)

If the DBMS reports the column as being a wide character (SQL_Wxxx) it
will be bound as a wide character and any returned data will be
converted from UTF-16 to UTF-8 and the UTF-8 flag will then be set on
the data.

=item bound parameters

If the perl scalars you bind to parameters are marked UTF-8 and the
DBMS reports the type as being a wide type or you bind the parameter
as a wide type they will be converted to wide characters and bound as
such.

=item metadata calls like table_info, column_info

As of DBD::ODBC 1.32_3 meta data calls accept Unicode strings.

=back

Since version 1.16_4, the default parameter bind type is SQL_WVARCHAR
for unicode builds of DBD::ODBC. This only affects ODBC drivers which
do not support SQLDescribeParam and only then if you do not
specifically set a sql type on the bind_param method call.

The above Unicode support has been tested with the SQL Server, Oracle
9.2+ and Postgres drivers on Windows and various Easysoft ODBC drivers
on UNIX.

=head3 Unicode - What is not supported?

You cannot use unicode parameter names e.g.,

  select * from table where column = :unicode_param_name

You cannot use unicode strings in calls to prepare if you set the
odbc_execdirect attribute.

You cannot use the iODBC driver manager with DBD::ODBC built for
unicode.

=head3 Unicode - Caveats

For Unicode support on any platform in Perl you will need at least
Perl 5.8.1 - sorry but this is the way it is with Perl.

The Unicode support in DBD::ODBC expects a WCHAR to be 2 bytes (as it
is on Windows and as the ODBC specification suggests it is). Until
ODBC specifies any other Unicode support it is not envisioned this
will change.  On UNIX there are a few different ODBC driver
managers. I have only tested the unixODBC driver manager
(http://www.unixodbc.org) with Unicode support and it was built with
defaults which set WCHAR as 2 bytes.

I believe that the iODBC driver manager expects wide characters to be
wchar_t types (which are usually 4) and hence DBD::ODBC will not work
iODBC when built for unicode.

The ODBC Driver must expect Unicode data specified in SQLBindParameter
and SQLBindCol to be UTF-16 in local endianness. Similarly, in calls to
SQLPrepareW, SQLDescribeColW and SQLDriverConnectW.

You should be aware that once Unicode support is enabled it affects a
number of DBI methods (some of which you might not expect). For
instance, when listing tables, columns etc some drivers
(e.g. Microsoft SQL Server) will report the column types as wide types
even if the strings actually fit in 7-bit ASCII. As a result, there is
an overhead for retrieving this column data as 2 bytes per character
will be transmitted (compared with 1 when Unicode support is not
enabled) and these strings will be converted into UTF-8 but will end up
fitting (in most cases) into 7bit ASCII so a lot of conversion work
has been performed for nothing. If you don't have Unicode table and
column names or Unicode column data in your tables you are best
disabling Unicode support.

I am at present unsure if ChopBlanks processing on Unicode strings is
working correctly on UNIX. If nothing else the construct L' ' in
dbdimp.c might not work with all UNIX compilers. Reports of issues and
patches welcome.

=head3 Unicode implementation in DBD::ODBC

DBD::ODBC uses the wide character versions of the ODBC API and the
SQL_WCHAR ODBC type to support unicode in Perl.

Wide characters returned from the ODBC driver will be converted to
UTF-8 and the perl scalars will have the utf8 flag set (by using
sv_utf8_decode).

B<IMPORTANT>

Perl scalars which are UTF-8 and are sent through the ODBC API will be
converted to UTF-16 and passed to the ODBC wide APIs or signalled as
SQL_WCHARs (e.g., in the case of bound columns). Retrieved data which
are wide characters are converted from UTF-16 to UTF-8. However, you
should realise most ODBC drivers do not support UTF-16, ODBC only
talks about wide characters being 2 bytes and UCS-2 and UCS-2 and
UTF-16 are not the same. UCS-2 only supports Unicode characters in the
first plane (the Basic Multilangual Plane or BMP) (code points U+0000
to U+FFFF), the most frequently used characters. So why does DBD::ODBC
currently encode in UTF-16? For around 97% of Unicode characters in
the range 0-0xFFFF UCS-2 and UTF-16 are exactly the same (and where they
differ there is no valid Unicode character as the range U+D800 to U+DFFF is
reserved from use only as surrogate pairs). As the ODBC
API currently uses UCS-2 it does not support Unicode characters with
code points above 0xFFFF (if you know better I'd like to hear from
you). However, because DBD::ODBC uses UTF-16 encoding you can still
insert Unicode characters above 0xFFFF into your database and retrieve
them back correctly but they will not being treated as a single
Unicode character in your database e.g., a "select length(a_column)
from table" with a single Unicode character above 0xFFFF will most
likely return 2 and not 1 so you cannot use database functions on that
data like upper/lower/length etc but you can at least save the data in
your database and get it back. This is a fudge and I cannot say I'm
overjoyed by it but it is what the majority of people who use
DBD::ODBC have requested.

When built for unicode, DBD::ODBC will always call SQLDriverConnectW
(and not SQLDriverConnect) even if a) your connection string is not
unicode b) you have not got a DBI later than 1.607, because unixODBC
requires SQLDriverConnectW to be called if you want to call other
unicode ODBC APIs later. As a result, if you build for unicode and
pass ASCII strings to the connect method they will be converted to
UTF-16 and passed to SQLDriverConnectW. This should make no real
difference to perl not using unicode connection strings.

You will need a DBI later than 1.607 to support unicode connection
strings because until post 1.607 there was no way for DBI to pass
unicode strings to the DBD.

=head3 Unicode and Oracle

You have to set the environment variables C<NLS_NCHAR=AL32UTF8> and
C<NLS_LANG=AMERICAN_AMERICA.AL32UTF8> (or any other language setting
ending with C<.AL32UTF8>) before loading DBD::ODBC to make Oracle
return Unicode data. (See also "Oracle and Unicode" in the POD of
DBD::Oracle.)

On Windows, using the Oracle ODBC Driver you have to enable the B<Force
SQL_WCHAR support> Workaround in the data source configuration to make
Oracle return Unicode to a non-Unicode application. Alternatively, you
can include C<FWC=T> in your connect string.

Unless you need to use ODBC, if you want Unicode support with Oracle
you are better off using L<DBD::Oracle>.

=head3 Unicode and PostgreSQL

See the odbc_utf8_on parameter to treat all strings as utf8.

Some tests from the original DBD::ODBC 1.13 fail with PostgreSQL
8.0.3, so you may not want to use DBD::ODBC to connect to PostgreSQL
8.0.3.

Unicode tests fail because PostgreSQL seems not to give any hints
about Unicode, so all data is treated as non-Unicode.

Unless you need to use ODBC, if you want Unicode support with Postgres
you are better off with L<DBD::Pg> as it has a specific attribute named
C<pg_enable_utf8> to enable Unicode support.

=head3 Unicode and Easysoft ODBC Drivers

We have tested the Easysoft SQL Server, Oracle and ODBC Bridge drivers
with DBD::ODBC built for Unicode. All work as described without
modification except for the Oracle driver you will need to set you
NLS_LANG as mentioned above.

=head3 Unicode and other ODBC drivers

If you have a unicode-enabled ODBC driver and it works with DBD::ODBC
let me know and I will include it here.

=head2 ODBC Support in ODBC Drivers

=head3 Drivers without SQLDescribeParam

Some drivers do not support the C<SQLDescribeParam> ODBC API (e.g.,
Microsoft Access, FreeTDS).

DBD::ODBC uses the C<SQLDescribeParam> API when parameters are bound
to your SQL to find the types of the parameters. If the ODBC driver
does not support C<SQLDescribeParam>, DBD::ODBC assumes the parameters
are C<SQL_VARCHAR> or C<SQL_WVARCHAR> types (depending on whether
DBD::ODBC is built for unicode or not). In any case, if you bind a
parameter and specify a SQL type this overrides any type DBD::ODBC
would choose.

For ODBC drivers which do not support C<SQLDescribeParam> the default
behavior in DBD::ODBC may not be what you want. To change the default
parameter bind type set L</odbc_default_bind_type>. If, after that you
have some SQL where you need to vary the parameter types used add the
SQL type to the end of the C<bind_param> method.

  use DBI qw(:sql_types);
  $h = DBI->connect;
  # set the default bound parameter type
  $h->{odbc_default_bind_type} = SQL_VARCHAR;
  # bind a parameter with a specific type
  $s = $h->prepare(q/insert into mytable values(?)/);
  $s->bind_param(1, "\x{263a}", SQL_WVARCHAR);

=head2 Version Control

DBD::ODBC source code is under version control at svn.perl.org.  If
you would like to use the "bleeding" edge version, you can get the
latest from svn.perl.org via Subversion version control.  Note there
is no guarantee that this version is any different than what you get
from the tarball from CPAN, but it might be :)

You may read about Subversion at L<http://subversion.tigris.org>

You can get a subversion client from there and check dbd-odbc out via:

   svn checkout http://svn.perl.org/modules/dbd-odbc/trunk <your directory name here>

Which will pull all the files from the subversion trunk to your
specified directory. If you want to see what has changed since the
last release of DBD::ODBC read the Changes file or use "svn log" to
get a list of checked in changes.

=head2 Contributing

There are seven main ways you may help with the development and
maintenance of this module:

=over

=item Submitting patches

Please use Subversion (see above) to get the latest version of
DBD::ODBC from the trunk and submit any patches against that.

Please, before submitting a patch:

   svn update
   <try and included a test which demonstrates the fix/change working>
   <test your patch>
   svn diff > describe_my_diffs.patch

and send the resulting file to me and cc the dbi-users@perl.org
mailing list (if you are not a member - why not!).

=item Reporting installs

Install CPAN::Reporter and report you installations. This is easy to
do - see L</CPAN Testers Reporting>.

=item Report bugs

If you find what you believe is a bug then enter it into the
L<http://rt.cpan.org/Dist/Display.html?Name=DBD-ODBC> system. Where
possible include code which reproduces the problem including any
schema required and the versions of software you are using.

If you are unsure whether you have found a bug report it anyway or
post it to the dbi-users mailing list.

=item pod comments and corrections

If you find inaccuracies in the DBD::ODBC pod or have a comment which
you think should be added then go to L<http://annocpan.org> and submit
them there. I get an email for every comment added and will review
each one and apply any changes to the documentation.

=item Review DBD::ODBC

Add your review of DBD::ODBC on L<http://cpanratings.perl.org>.

If you are a member on ohloh then add your review or register your
use of DBD::ODBC at L<http://www.ohloh.net/projects/perl_dbd_odbc>.

=item submit test cases

Most DBDs are built against a single client library for the database.

Unlike other DBDs, DBD::ODBC works with many different ODBC drivers.
Although they all should be written with regard to the ODBC
specification drivers have bugs and in some places the specification is
open to interpretation. As a result, when changes are applied to
DBD::ODBC it is very easy to break something in one ODBC driver.

What helps enormously to identify problems in the many combinations
of DBD::ODBC and ODBC drivers is a large test suite. I would greatly
appreciate any test cases and in particular any new test cases for
databases other than MS SQL Server.

=item Test DBD::ODBC

I have a lot of problems deciding when to move a development release
to an official release since I get few test reports for development
releases. What often happens is I call for testers on various lists,
get a few and then get inundated with requests to do an official
release. Then I do an official release and loads of rts appear out of
nowhere and the cycle starts again.

DBD::ODBC by its very nature works with many ODBC Drivers and it is
impossible for me to have and test them all (this differs from other
DBDs). If you depend on DBD::ODBC you should be interested in new
releases and if you send me your email address suggesting you are
prepared to be part of the DBD::ODBC testing network I will credit you
in the Changes file and perhaps the main DBD::ODBC file.

=back

=head2 CPAN Testers Reporting

Please, please, please (is that enough), consider installing
CPAN::Reporter so that when you install perl modules a report of the
installation success or failure can be sent to cpan testers. In this
way module authors 1) get feedback on the fact that a module is being
installed 2) get to know if there are any installation problems. Also
other people like you may look at the test reports to see how
successful they are before choosing the version of a module to
install.

See this guide on how to get started with sending test reports:
L<http://wiki.cpantesters.org/wiki/QuickStart>.

=head2 Others/todo?

Level 2

    SQLColumnPrivileges
    SQLProcedureColumns
    SQLProcedures
    SQLTablePrivileges
    SQLDrivers
    SQLNativeSql

=head2 Random Links

These are in need of sorting and annotating. Some are relevant only
to ODBC developers.

You can find DBD::ODBC on ohloh now at:

L<http://www.ohloh.net/projects/perl_dbd_odbc>

If you use ohloh and DBD::ODBC please say you use it and rate it.

There is a good search engine for the various Perl DBI lists at the
following URLS:

L<http://perl.markmail.org/search/list:org.perl.dbi-users>

L<http://perl.markmail.org/search/list:org.perl.dbi-dev>

L<http://perl.markmail.org/search/list:org.perl.dbi-announce>

L<http://www.syware.com>

L<http://www.microsoft.com/odbc>

For Linux/Unix folks, compatible ODBC driver managers can be found at:

L<http://www.unixodbc.org> (unixODBC source and rpms)

L<http://www.iodbc.org> (iODBC driver manager source)

For Linux/Unix folks, you can checkout the following for ODBC Drivers and
Bridges:

L<http://www.easysoft.com>

L<http://www.openlinksw.com>

L<http://www.datadirect.com>

L<http://www.atinet.com>

=head2 Some useful tutorials:

Debugging Perl DBI:

L<http://www.easysoft.com/developer/languages/perl/dbi-debugging.html>

Enabling ODBC support in Perl with Perl DBI and DBD::ODBC:

L<http://www.easysoft.com/developer/languages/perl/dbi_dbd_odbc.html>

Perl DBI/DBD::ODBC Tutorial Part 1 - Drivers, Data Sources and Connection:

L<http://www.easysoft.com/developer/languages/perl/dbd_odbc_tutorial_part_1.html>

Perl DBI/DBD::ODBC Tutorial Part 2 - Introduction to retrieving data from your database:

L<http://www.easysoft.com/developer/languages/perl/dbd_odbc_tutorial_part_2.html>

Perl DBI/DBD::ODBC Tutorial Part 3 - Connecting Perl on UNIX or Linux to Microsoft SQL Server:

L<http://www.easysoft.com/developer/languages/perl/sql_server_unix_tutorial.html>

Perl DBI - Put Your Data On The Web:

L<http://www.easysoft.com/developer/languages/perl/tutorial_data_web.html>

Multiple Active Statements (MAS) and DBD::ODBC

L<http://www.easysoft.com/developer/languages/perl/multiple-active-statements.html>

64-bit ODBC

L<http://www.easysoft.com/developer/interfaces/odbc/64-bit.html>

=head2 Frequently Asked Questions

Frequently asked questions are now in L<DBD::ODBC::FAQ>. Run
C<perldoc DBD::ODBC::FAQ> to view them.

=head1 CONFIGURATION AND ENVIRONMENT

You should consult the documentation for the ODBC Driver Manager
you are using.

=head1 DEPENDENCIES

L<DBI>

L<Test::Simple>

=head1 INCOMPATIBILITIES

None known.

=head1 BUGS AND LIMITATIONS

None known other than the deviations from the DBI specification mentioned
above in L</Deviations from the DBI specification>.

Please report any to me via the CPAN RT system. See
L<http://rt.cpan.org/> for more details.

=head1 AUTHOR

Tim Bunce

Jeff Urlwin

Thomas K. Wenrich

Martin J. Evans

=head1 LICENSE AND COPYRIGHT

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. See L<perlartistic>. This
program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.

Portions of this software are Copyright Tim Bunce, Thomas K. Wenrich,
Jeff Urlwin and Martin J. Evans - see the source.

=head1 SEE ALSO

L<DBI>

=cut
