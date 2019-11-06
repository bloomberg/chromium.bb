#  -*-cperl-*-
#
#  Copyright (c) 2002-2019 Greg Sabino Mullane and others: see the Changes file
#  Portions Copyright (c) 2002 Jeffrey W. Baker
#  Portions Copyright (c) 1997-2001 Edmund Mergl
#  Portions Copyright (c) 1994-1997 Tim Bunce
#
#  You may distribute under the terms of either the GNU General Public
#  License or the Artistic License, as specified in the Perl README file.


use strict;
use warnings;
use 5.008001;

{
	package DBD::Pg;

	use version; our $VERSION = qv('3.8.0');

	use DBI ();
	use DynaLoader ();
	use Exporter ();
	use vars qw(@ISA %EXPORT_TAGS $err $errstr $sqlstate $drh $dbh $DBDPG_DEFAULT @EXPORT);
	@ISA = qw(DynaLoader Exporter);


	%EXPORT_TAGS =
		(
		 async => [qw(PG_ASYNC PG_OLDQUERY_CANCEL PG_OLDQUERY_WAIT)],
		 pg_types => [qw(
			PG_ACLITEM PG_ACLITEMARRAY PG_ANY PG_ANYARRAY PG_ANYELEMENT
			PG_ANYENUM PG_ANYNONARRAY PG_ANYRANGE PG_BIT PG_BITARRAY
			PG_BOOL PG_BOOLARRAY PG_BOX PG_BOXARRAY PG_BPCHAR
			PG_BPCHARARRAY PG_BYTEA PG_BYTEAARRAY PG_CHAR PG_CHARARRAY
			PG_CID PG_CIDARRAY PG_CIDR PG_CIDRARRAY PG_CIRCLE
			PG_CIRCLEARRAY PG_CSTRING PG_CSTRINGARRAY PG_DATE PG_DATEARRAY
			PG_DATERANGE PG_DATERANGEARRAY PG_EVENT_TRIGGER PG_FDW_HANDLER PG_FLOAT4
			PG_FLOAT4ARRAY PG_FLOAT8 PG_FLOAT8ARRAY PG_GTSVECTOR PG_GTSVECTORARRAY
			PG_INDEX_AM_HANDLER PG_INET PG_INETARRAY PG_INT2 PG_INT2ARRAY
			PG_INT2VECTOR PG_INT2VECTORARRAY PG_INT4 PG_INT4ARRAY PG_INT4RANGE
			PG_INT4RANGEARRAY PG_INT8 PG_INT8ARRAY PG_INT8RANGE PG_INT8RANGEARRAY
			PG_INTERNAL PG_INTERVAL PG_INTERVALARRAY PG_JSON PG_JSONARRAY
			PG_JSONB PG_JSONBARRAY PG_JSONPATH PG_JSONPATHARRAY PG_LANGUAGE_HANDLER
			PG_LINE PG_LINEARRAY PG_LSEG PG_LSEGARRAY PG_MACADDR
			PG_MACADDR8 PG_MACADDR8ARRAY PG_MACADDRARRAY PG_MONEY PG_MONEYARRAY
			PG_NAME PG_NAMEARRAY PG_NUMERIC PG_NUMERICARRAY PG_NUMRANGE
			PG_NUMRANGEARRAY PG_OID PG_OIDARRAY PG_OIDVECTOR PG_OIDVECTORARRAY
			PG_OPAQUE PG_PATH PG_PATHARRAY PG_PG_ATTRIBUTE PG_PG_CLASS
			PG_PG_DDL_COMMAND PG_PG_DEPENDENCIES PG_PG_LSN PG_PG_LSNARRAY PG_PG_MCV_LIST
			PG_PG_NDISTINCT PG_PG_NODE_TREE PG_PG_PROC PG_PG_TYPE PG_POINT
			PG_POINTARRAY PG_POLYGON PG_POLYGONARRAY PG_RECORD PG_RECORDARRAY
			PG_REFCURSOR PG_REFCURSORARRAY PG_REGCLASS PG_REGCLASSARRAY PG_REGCONFIG
			PG_REGCONFIGARRAY PG_REGDICTIONARY PG_REGDICTIONARYARRAY PG_REGNAMESPACE PG_REGNAMESPACEARRAY
			PG_REGOPER PG_REGOPERARRAY PG_REGOPERATOR PG_REGOPERATORARRAY PG_REGPROC
			PG_REGPROCARRAY PG_REGPROCEDURE PG_REGPROCEDUREARRAY PG_REGROLE PG_REGROLEARRAY
			PG_REGTYPE PG_REGTYPEARRAY PG_TABLE_AM_HANDLER PG_TEXT PG_TEXTARRAY
			PG_TID PG_TIDARRAY PG_TIME PG_TIMEARRAY PG_TIMESTAMP
			PG_TIMESTAMPARRAY PG_TIMESTAMPTZ PG_TIMESTAMPTZARRAY PG_TIMETZ PG_TIMETZARRAY
			PG_TRIGGER PG_TSM_HANDLER PG_TSQUERY PG_TSQUERYARRAY PG_TSRANGE
			PG_TSRANGEARRAY PG_TSTZRANGE PG_TSTZRANGEARRAY PG_TSVECTOR PG_TSVECTORARRAY
			PG_TXID_SNAPSHOT PG_TXID_SNAPSHOTARRAY PG_UNKNOWN PG_UUID PG_UUIDARRAY
			PG_VARBIT PG_VARBITARRAY PG_VARCHAR PG_VARCHARARRAY PG_VOID
			PG_XID PG_XIDARRAY PG_XML PG_XMLARRAY
		)]
	);

	{
		package DBD::Pg::DefaultValue;
		sub new { my $self = {}; return bless $self, shift; }
	}
	$DBDPG_DEFAULT = DBD::Pg::DefaultValue->new();
	Exporter::export_ok_tags('pg_types', 'async');
	@EXPORT = qw($DBDPG_DEFAULT PG_ASYNC PG_OLDQUERY_CANCEL PG_OLDQUERY_WAIT PG_BYTEA);

	require_version DBI 1.614;

	bootstrap DBD::Pg $VERSION;

	$err = 0;       # holds error code for DBI::err
	$errstr = '';   # holds error string for DBI::errstr
	$sqlstate = ''; # holds five character SQLSTATE code
	$drh = undef;   # holds driver handle once initialized

	## These two methods are here to allow calling before connect()
	sub parse_trace_flag {
		my ($class, $flag) = @_;
		return (0x7FFFFF00 - 0x08000000) if $flag eq 'DBD'; ## all but the prefix
		return 0x01000000 if $flag eq 'pglibpq';
		return 0x02000000 if $flag eq 'pgstart';
		return 0x04000000 if $flag eq 'pgend';
		return 0x08000000 if $flag eq 'pgprefix';
		return 0x10000000 if $flag eq 'pglogin';
		return 0x20000000 if $flag eq 'pgquote';
		return DBI::parse_trace_flag($class, $flag);
	}
	sub parse_trace_flags {
		my ($class, $flags) = @_;
		return DBI::parse_trace_flags($class, $flags);
	}

	sub CLONE {
		$drh = undef;
		return;
	}

	## Deprecated
	sub _pg_use_catalog { ## no critic (ProhibitUnusedPrivateSubroutines)
		return 'pg_catalog.';
	}

	my $methods_are_installed = 0;
	sub driver {
		return $drh if defined $drh;
		my($class, $attr) = @_;

		$class .= '::dr';

		$drh = DBI::_new_drh($class, {
			'Name'        => 'Pg',
			'Version'     => $VERSION,
			'Err'         => \$DBD::Pg::err,
			'Errstr'      => \$DBD::Pg::errstr,
			'State'       => \$DBD::Pg::sqlstate,
			'Attribution' => "DBD::Pg $VERSION by Greg Sabino Mullane and others",
		});

		if (!$methods_are_installed) {
			DBD::Pg::db->install_method('pg_cancel');
			DBD::Pg::db->install_method('pg_endcopy');
			DBD::Pg::db->install_method('pg_getline');
			DBD::Pg::db->install_method('pg_getcopydata');
			DBD::Pg::db->install_method('pg_getcopydata_async');
			DBD::Pg::db->install_method('pg_notifies');
			DBD::Pg::db->install_method('pg_putcopydata');
			DBD::Pg::db->install_method('pg_putcopyend');
			DBD::Pg::db->install_method('pg_ping');
			DBD::Pg::db->install_method('pg_putline');
			DBD::Pg::db->install_method('pg_ready');
			DBD::Pg::db->install_method('pg_release');
			DBD::Pg::db->install_method('pg_result'); ## NOT duplicated below!
			DBD::Pg::db->install_method('pg_rollback_to');
			DBD::Pg::db->install_method('pg_savepoint');
			DBD::Pg::db->install_method('pg_server_trace');
			DBD::Pg::db->install_method('pg_server_untrace');
			DBD::Pg::db->install_method('pg_type_info');

			DBD::Pg::st->install_method('pg_cancel');
			DBD::Pg::st->install_method('pg_result');
			DBD::Pg::st->install_method('pg_ready');
			DBD::Pg::st->install_method('pg_canonical_ids');
			DBD::Pg::st->install_method('pg_canonical_names');

			DBD::Pg::db->install_method('pg_lo_creat');
			DBD::Pg::db->install_method('pg_lo_open');
			DBD::Pg::db->install_method('pg_lo_write');
			DBD::Pg::db->install_method('pg_lo_read');
			DBD::Pg::db->install_method('pg_lo_lseek');
			DBD::Pg::db->install_method('pg_lo_tell');
			DBD::Pg::db->install_method('pg_lo_truncate');
			DBD::Pg::db->install_method('pg_lo_close');
			DBD::Pg::db->install_method('pg_lo_unlink');
			DBD::Pg::db->install_method('pg_lo_import');
			DBD::Pg::db->install_method('pg_lo_import_with_oid');
			DBD::Pg::db->install_method('pg_lo_export');

			$methods_are_installed++;
		}

		return $drh;

	} ## end of driver


	1;

} ## end of package DBD::Pg


{
	package DBD::Pg::dr;

	use strict;

	## Returns an array of formatted database names from the pg_database table
	sub data_sources {

		my $drh = shift;
		my $attr = shift || '';
		my $connstring = 'dbname=postgres';
		if ($ENV{DBI_DSN}) {
			($connstring = $ENV{DBI_DSN}) =~ s/dbi:Pg://i;
		}
		if (length $attr) {
			$connstring .= ";$attr";
		}

		my $dbh = DBD::Pg::dr::connect($drh, $connstring) or return;
		$dbh->{AutoCommit}=1;
		my $SQL = 'SELECT pg_catalog.quote_ident(datname) FROM pg_catalog.pg_database ORDER BY 1';
		my $sth = $dbh->prepare($SQL);
		$sth->execute() or die $DBI::errstr;
		$attr and $attr = ";$attr";
		my @sources = map { "dbi:Pg:dbname=$_->[0]$attr" } @{$sth->fetchall_arrayref()};
		$dbh->disconnect;
		return @sources;
	}


	sub connect { ## no critic (ProhibitBuiltinHomonyms)
		my ($drh, $dbname, $user, $pass, $attr) = @_;

		## Allow "db" and "database" as synonyms for "dbname"
		$dbname =~ s/\b(?:db|database)\s*=/dbname=/;

		my $name = $dbname;
		if ($dbname =~ m{dbname\s*=\s*[\"\']([^\"\']+)}) {
			$name = "'$1'";
			$dbname =~ s/\"/\'/g;
		}
		elsif ($dbname =~ m{dbname\s*=\s*([^;]+)}) {
			$name = $1;
		}

 		$user = defined($user) ? $user : defined $ENV{DBI_USER} ? $ENV{DBI_USER} : '';
		$pass = defined($pass) ? $pass : defined $ENV{DBI_PASS} ? $ENV{DBI_PASS} : '';

		my ($dbh) = DBI::_new_dbh($drh, {
			'Name'         => $dbname,
			'Username'     => $user,
			'CURRENT_USER' => $user,
		 });

		# Connect to the database..
		DBD::Pg::db::_login($dbh, $dbname, $user, $pass, $attr) or return undef;

		my $version = $dbh->{pg_server_version};
		$dbh->{private_dbdpg}{version} = $version;

		if ($attr) {
			if ($attr->{dbd_verbose}) {
				$dbh->trace('DBD');
			}
		}

		return $dbh;
	}

	sub private_attribute_info {
		return {
		};
	}

} ## end of package DBD::Pg::dr


{
	package DBD::Pg::db;

	use DBI qw(:sql_types);

	use strict;

	sub parse_trace_flag {
		my ($h, $flag) = @_;
		return DBD::Pg->parse_trace_flag($flag);
	}

	sub prepare {
		my($dbh, $statement, @attribs) = @_;

		return undef if ! defined $statement;

		# Create a 'blank' statement handle:
		my $sth = DBI::_new_sth($dbh, {
			'Statement' => $statement,
		});

		DBD::Pg::st::_prepare($sth, $statement, @attribs) || 0;

		return $sth;
	}

	sub last_insert_id {

		my ($dbh, $catalog, $schema, $table, $col, $attr) = @_;

		## Our ultimate goal is to get a sequence
		my ($sth, $count, $SQL, $sequence);

		## Cache all of our table lookups? Default is yes
		my $cache = 1;

		## Catalog and col are not used
		$schema = '' if ! defined $schema;
		$table = '' if ! defined $table;
		my $cachename = join("\0", 'lii', $schema, $table);

		if (defined $attr and length $attr) {
			## If not a hash, assume it is a sequence name
			if (! ref $attr) {
				$attr = {sequence => $attr};
			}
			elsif (ref $attr ne 'HASH') {
				$dbh->set_err(1, 'last_insert_id must be passed a hashref as the final argument');
				return undef;
			}
			## Named sequence overrides any table or schema settings
			if (exists $attr->{sequence} and length $attr->{sequence}) {
				$sequence = $attr->{sequence};
			}
			if (exists $attr->{pg_cache}) {
				$cache = $attr->{pg_cache};
			}
		}

		if (! defined $sequence and exists $dbh->{private_dbdpg}{$cachename} and $cache) {
			$sequence = $dbh->{private_dbdpg}{$cachename};
		}
		elsif (! defined $sequence) {
			## At this point, we must have a valid table name
			if (! length $table) {
				$dbh->set_err(1, 'last_insert_id needs at least a sequence or table name');
				return undef;
			}
			my @args = ($table);
			my $schemawhere;
			if (length $schema) {
				# if given a schema, use that
				$schemawhere = 'n.nspname = ?';
				push @args, $schema;
			} else {
				# otherwise it must be visible via the search path
				$schemawhere = 'pg_catalog.pg_table_is_visible(c.oid)';
			}
			## Is there a sequence associated with the table via a unique, indexed column,
			## either via ownership (e.g. serial, identity) or a manual default?
			my $idcond = $dbh->{private_dbdpg}{version} >= 100000
				? q{a.attidentity <> ''} : q{false};
			$SQL = sprintf(q{
				SELECT i.indisprimary,
					COALESCE(
						-- this takes the table name as text, not regclass
						pg_catalog.pg_get_serial_sequence(
							-- and pre-8.3 doesn't have a cast from regclass to text,
							-- and pre-9.3 doesn't have format, so do it the long way
							quote_ident(n.nspname) || '.' || quote_ident(c.relname),
							a.attname),
						(SELECT replace(substring(pg_catalog.pg_get_expr(d.adbin, d.adrelid)
											from $r$^nextval\('(.+)'::[\w\s]+\)$$r$),
										-- unescape any single quotes from the default
										$$''$$, $$'$$)
							FROM pg_catalog.pg_attrdef d
							WHERE a.atthasdef
								AND a.attrelid = d.adrelid
								AND a.attnum = d.adnum)
					) AS seqname
				FROM pg_class c
					JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace)
					-- LEFT JOIN so we can distingiuish between table not found (zero rows)
					-- and no suitable column found (at least one all-NULL row)
					LEFT JOIN pg_catalog.pg_index i
						ON c.oid = i.indrelid AND i.indisunique
					LEFT JOIN pg_catalog.pg_attribute a
						ON i.indrelid = a.attrelid AND i.indkey[0]=a.attnum
						AND (a.atthasdef OR %s)
				WHERE c.relname = ? AND %s
			}, $idcond, $schemawhere);
			my $sth = $dbh->prepare_cached($SQL);
			my $count = $sth->execute(@args);
			if (!defined $count or $count eq '0E0') {
				$sth->finish();
				my $message = qq{Could not find the table "$table"};
				length $schema and $message .= qq{ in the schema "$schema"};
				$dbh->set_err(1, $message);
				return undef;
			}
			my $info = $sth->fetchall_arrayref();

			## We have at least one with a default value. See if we found any sequences
			my @def = grep { defined $_->[1] } @$info;
			if (!@def) {
				$dbh->set_err(1, qq{No suitable column found for last_insert_id of table "$table"\n});
				return undef;
			}
			## Tiebreaker goes to the primary keys
			if (@def > 1) {
				my @pri = grep { $_->[0] } @def;
				if (1 != @pri) {
					$dbh->set_err(1, qq{No suitable column found for last_insert_id of table "$table"\n});
					return undef;
				}
				@def = @pri;
			}
			$sequence = $def[0]->[1];
			## Cache this information for subsequent calls
			$dbh->{private_dbdpg}{$cachename} = $sequence;
		}

		$sth = $dbh->prepare_cached('SELECT pg_catalog.currval(?)');
		$count = $sth->execute($sequence);
		return undef if ! defined $count;
		return $sth->fetchall_arrayref()->[0][0];

	} ## end of last_insert_id

	sub ping {
		my $dbh = shift;
		local $SIG{__WARN__} = sub { } if $dbh->FETCH('PrintError');
		my $ret = DBD::Pg::db::_ping($dbh);
		return $ret < 1 ? 0 : $ret;
	}

	sub pg_ping {
		my $dbh = shift;
		local $SIG{__WARN__} = sub { } if $dbh->FETCH('PrintError');
		return DBD::Pg::db::_ping($dbh);
	}

	sub pg_type_info {
		my($dbh,$pg_type) = @_;
		local $SIG{__WARN__} = sub { } if $dbh->FETCH('PrintError');
		my $ret = DBD::Pg::db::_pg_type_info($pg_type);
		return $ret;
	}

	# Column expected in statement handle returned.
	# table_cat, table_schem, table_name, column_name, data_type, type_name,
 	# column_size, buffer_length, DECIMAL_DIGITS, NUM_PREC_RADIX, NULLABLE,
	# REMARKS, COLUMN_DEF, SQL_DATA_TYPE, SQL_DATETIME_SUB, CHAR_OCTET_LENGTH,
	# ORDINAL_POSITION, IS_NULLABLE
	# The result set is ordered by TABLE_SCHEM, TABLE_NAME and ORDINAL_POSITION.

	sub column_info {
		my $dbh = shift;
		my ($catalog, $schema, $table, $column) = @_;

		my @search;
		## If the schema or table has an underscore or a %, use a LIKE comparison
		if (defined $schema and length $schema) {
			push @search, 'n.nspname ' . ($schema =~ /[_%]/ ? 'LIKE ' : '= ') .
				$dbh->quote($schema);
		}
		if (defined $table and length $table) {
			push @search, 'c.relname ' . ($table =~ /[_%]/ ? 'LIKE ' : '= ') .
				$dbh->quote($table);
		}
		if (defined $column and length $column) {
			push @search, 'a.attname ' . ($column =~ /[_%]/ ? 'LIKE ' : '= ') .
				$dbh->quote($column);
		}

		my $whereclause = join "\n\t\t\t\tAND ", '', @search;

		my $col_info_sql = qq!
            SELECT
                pg_catalog.quote_ident(pg_catalog.current_database()) AS "TABLE_CAT"
                , pg_catalog.quote_ident(n.nspname) AS "TABLE_SCHEM"
                , pg_catalog.quote_ident(c.relname) AS "TABLE_NAME"
                , pg_catalog.quote_ident(a.attname) AS "COLUMN_NAME"
                , a.atttypid AS "DATA_TYPE"
                , pg_catalog.format_type(a.atttypid, NULL) AS "TYPE_NAME"
                , a.attlen AS "COLUMN_SIZE"
                , NULL::text AS "BUFFER_LENGTH"
                , NULL::text AS "DECIMAL_DIGITS"
                , NULL::text AS "NUM_PREC_RADIX"
                , CASE a.attnotnull WHEN 't' THEN 0 ELSE 1 END AS "NULLABLE"
                , pg_catalog.col_description(a.attrelid, a.attnum) AS "REMARKS"
                , pg_catalog.pg_get_expr(af.adbin, af.adrelid) AS "COLUMN_DEF"
                , NULL::text AS "SQL_DATA_TYPE"
                , NULL::text AS "SQL_DATETIME_SUB"
                , NULL::text AS "CHAR_OCTET_LENGTH"
                , a.attnum AS "ORDINAL_POSITION"
                , CASE a.attnotnull WHEN 't' THEN 'NO' ELSE 'YES' END AS "IS_NULLABLE"
                , pg_catalog.format_type(a.atttypid, a.atttypmod) AS "pg_type"
                , '?' AS "pg_constraint"
                , n.nspname AS "pg_schema"
                , c.relname AS "pg_table"
                , a.attname AS "pg_column"
                , a.attrelid AS "pg_attrelid"
                , a.attnum AS "pg_attnum"
                , a.atttypmod AS "pg_atttypmod"
                , t.typtype AS "_pg_type_typtype"
                , t.oid AS "_pg_type_oid"
            FROM
                pg_catalog.pg_type t
                JOIN pg_catalog.pg_attribute a ON (t.oid = a.atttypid)
                JOIN pg_catalog.pg_class c ON (a.attrelid = c.oid)
                LEFT JOIN pg_catalog.pg_attrdef af ON (a.attnum = af.adnum AND a.attrelid = af.adrelid)
                JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace)
            WHERE
                a.attnum >= 0
                AND c.relkind IN ('r','v','m','f')
                $whereclause
            ORDER BY "TABLE_SCHEM", "TABLE_NAME", "ORDINAL_POSITION"
            !;

		my $data = $dbh->selectall_arrayref($col_info_sql) or return undef;

		# To turn the data back into a statement handle, we need 
		# to fetch the data as an array of arrays, and also have a
		# a matching array of all the column names
		my %col_map = (qw/
			TABLE_CAT             0
			TABLE_SCHEM           1
			TABLE_NAME            2
			COLUMN_NAME           3
			DATA_TYPE             4
			TYPE_NAME             5
			COLUMN_SIZE           6
			BUFFER_LENGTH         7
			DECIMAL_DIGITS        8
			NUM_PREC_RADIX        9
			NULLABLE             10
			REMARKS              11
			COLUMN_DEF           12
			SQL_DATA_TYPE        13
			SQL_DATETIME_SUB     14
			CHAR_OCTET_LENGTH    15
			ORDINAL_POSITION     16
			IS_NULLABLE          17
			pg_type              18
			pg_constraint        19
			pg_schema            20
			pg_table             21
			pg_column            22
			pg_enum_values       23
			/);

		for my $row (@$data) {
			my $typoid = pop @$row;
			my $typtype = pop @$row;
			my $typmod = pop @$row;
			my $attnum = pop @$row;
			my $aid = pop @$row;

			$row->[$col_map{COLUMN_SIZE}] =
 				_calc_col_size($typmod,$row->[$col_map{COLUMN_SIZE}]);

			# Replace the Pg type with the SQL_ type
			$row->[$col_map{DATA_TYPE}] = DBD::Pg::db::pg_type_info($dbh,$row->[$col_map{DATA_TYPE}]);

			# Add pg_constraint
			my $SQL = q{SELECT pg_catalog.pg_get_constraintdef(oid) }.
				q{FROM pg_catalog.pg_constraint WHERE contype = 'c' AND }.
				qq{conrelid = $aid AND conkey = '{$attnum}'};
			my $info = $dbh->selectall_arrayref($SQL);
			if (@$info) {
				$row->[$col_map{pg_constraint}] = $info->[0][0];
			}
			else {
				$row->[$col_map{pg_constraint}] = undef;
			}

			if ( $typtype eq 'e' ) {
				my $order_column = $dbh->{private_dbdpg}{version} >= 90100
					? 'enumsortorder' : 'oid';
				$SQL = "SELECT enumlabel FROM pg_catalog.pg_enum WHERE enumtypid = $typoid ORDER BY $order_column";
				$row->[$col_map{pg_enum_values}] = $dbh->selectcol_arrayref($SQL);
			}
			else {
				$row->[$col_map{pg_enum_values}] = undef;
			}
		}

		# Since we've processed the data in Perl, we have to jump through a hoop
		# To turn it back into a statement handle
		#
		return _prepare_from_data
			(
			 'column_info',
			 $data,
			 [ sort { $col_map{$a} <=> $col_map{$b} } keys %col_map],
			 );
	}

	sub _prepare_from_data {
		my ($statement, $data, $names, %attr) = @_;
		my $sponge = DBI->connect('dbi:Sponge:', '', '', { RaiseError => 1 });
		my $sth = $sponge->prepare($statement, { rows=>$data, NAME=>$names, %attr });
		return $sth;
	}

	sub statistics_info {

		my $dbh = shift;
		my ($catalog, $schema, $table, $unique_only, $quick, $attr) = @_;

		## Catalog is ignored, but table is mandatory
		return undef unless defined $table and length $table;

		my $schema_where = '';
		my @exe_args = ($table);

		my $input_schema = (defined $schema and length $schema) ? 1 : 0;

		if ($input_schema) {
			$schema_where = 'AND n.nspname = ? AND n.oid = d.relnamespace';
			push(@exe_args, $schema);
		}
		else {
			$schema_where = 'AND n.oid = d.relnamespace';
		}

		my $table_stats_sql = qq{
            SELECT d.relpages, d.reltuples, n.nspname,
                   pg_catalog.current_database() as catname
            FROM   pg_catalog.pg_class d, pg_catalog.pg_namespace n
            WHERE  d.relname = ? $schema_where
        };

		my $colnames_sql = qq{
            SELECT
                a.attnum, a.attname
            FROM
                pg_catalog.pg_attribute a, pg_catalog.pg_class d, pg_catalog.pg_namespace n
            WHERE
                a.attrelid = d.oid AND d.relname = ? $schema_where
        };

		my $stats_sql = qq{
            SELECT
                pg_catalog.current_database() as catname,
                c.relname, i.indkey, i.indisunique, i.indisclustered, a.amname,
                n.nspname, c.relpages, c.reltuples, i.indexprs, i.indnatts, i.indexrelid,
                pg_catalog.pg_get_expr(i.indpred,i.indrelid) as predicate,
                pg_catalog.pg_get_expr(i.indexprs,i.indrelid, true) AS indexdef
            FROM
                pg_catalog.pg_index i, pg_catalog.pg_class c,
                pg_catalog.pg_class d, pg_catalog.pg_am a,
                pg_catalog.pg_namespace n
            WHERE
                d.relname = ? $schema_where AND d.oid = i.indrelid
                AND i.indexrelid = c.oid AND c.relam = a.oid
            ORDER BY
                i.indisunique desc, a.amname, c.relname
        };

		my $indexdef_sql = q{
            SELECT
                pg_catalog.pg_get_indexdef(indexrelid,x,true)
            FROM
              pg_index
            JOIN pg_catalog.generate_series(1,?) s(x) ON indexrelid = ?
        };

		my @output_rows;

		# Table-level stats
		if (!$unique_only) {
			my $table_stats_sth = $dbh->prepare($table_stats_sql);
			$table_stats_sth->execute(@exe_args) or return undef;
			my $tst = $table_stats_sth->fetchrow_hashref or return undef;
			push(@output_rows, [
				$tst->{catname},  # TABLE_CAT
				$tst->{nspname},  # TABLE_SCHEM
				$table,           # TABLE_NAME
				undef,            # NON_UNIQUE
				undef,            # INDEX_QUALIFIER
				undef,            # INDEX_NAME
				'table',          # TYPE
				undef,            # ORDINAL_POSITION
				undef,            # COLUMN_NAME
				undef,            # ASC_OR_DESC
				$tst->{reltuples},# CARDINALITY
				$tst->{relpages}, # PAGES
				undef,            # FILTER_CONDITION
                undef,            # pg_expression
			]);
		}

		# Fetch the column names for later use
		my $colnames_sth = $dbh->prepare($colnames_sql);
		$colnames_sth->execute(@exe_args) or return undef;
		my $colnames = $colnames_sth->fetchall_hashref('attnum');

		# Fetch the individual parts of the index
		my $sth_indexdef = $dbh->prepare($indexdef_sql);

		# Fetch the index definitions
		my $sth = $dbh->prepare($stats_sql);
		$sth->execute(@exe_args) or return undef;

		STAT_ROW:
		while (my $row = $sth->fetchrow_hashref) {

			next if $unique_only and !$row->{indisunique};

			my $indtype = $row->{indisclustered}
				? 'clustered'
				: ( $row->{amname} eq 'btree' )
					? 'btree'
					: ($row->{amname} eq 'hash' )
						? 'hashed' : 'other';

			my $nonunique = $row->{indisunique} ? 0 : 1;

			my @index_row = (
				$row->{catname},   # TABLE_CAT         0
				$row->{nspname},   # TABLE_SCHEM       1
				$table,            # TABLE_NAME        2
				$nonunique,        # NON_UNIQUE        3
				undef,             # INDEX_QUALIFIER   4
				$row->{relname},   # INDEX_NAME        5
				$indtype,          # TYPE              6
				undef,             # ORDINAL_POSITION  7
				undef,             # COLUMN_NAME       8
				'A',               # ASC_OR_DESC       9
				$row->{reltuples}, # CARDINALITY      10
				$row->{relpages},  # PAGES            11
				$row->{predicate}, # FILTER_CONDITION 12
                undef,             # pg_expression    13
			);

			## Grab expression information
			$sth_indexdef->execute($row->{indnatts}, $row->{indexrelid});
			my $expression = $sth_indexdef->fetchall_arrayref();

			my $col_nums = $row->{indkey};
			$col_nums =~ s/^\s+//;
			my @col_nums = split(/\s+/, $col_nums);

			my $ord_pos = 1;
			for my $col_num (@col_nums) {
				my @copy = @index_row;
				$copy[7] = $ord_pos; # ORDINAL_POSITION
				$copy[8] = $colnames->{$col_num}->{attname}; # COLUMN_NAME
				$copy[13] = $expression->[$ord_pos-1][0];
				push(@output_rows, \@copy);
				$ord_pos++;
			}
		}

		my @output_colnames = qw/ TABLE_CAT TABLE_SCHEM TABLE_NAME NON_UNIQUE INDEX_QUALIFIER
					INDEX_NAME TYPE ORDINAL_POSITION COLUMN_NAME ASC_OR_DESC
					CARDINALITY PAGES FILTER_CONDITION pg_expression /;

		return _prepare_from_data('statistics_info', \@output_rows, \@output_colnames);
	}

	sub primary_key_info {

		my $dbh = shift;
		my ($catalog, $schema, $table, $attr) = @_;

		## Catalog is ignored, but table is mandatory
		return undef unless defined $table and length $table;

		my $whereclause = 'AND c.relname = ' . $dbh->quote($table);

		if (defined $schema and length $schema) {
			$whereclause .= "\n\t\t\tAND n.nspname = " . $dbh->quote($schema);
		}

		my $pri_key_sql = qq{
            SELECT
                  c.oid
                , pg_catalog.quote_ident(n.nspname)
                , pg_catalog.quote_ident(c.relname)
                , pg_catalog.quote_ident(c2.relname)
                , i.indkey
                , pg_catalog.quote_ident(t.spcname)
                , pg_catalog.quote_ident(t.spclocation)
                , n.nspname, c.relname, c2.relname
                , pg_catalog.quote_ident(pg_catalog.current_database())
            FROM
                pg_catalog.pg_class c
                JOIN pg_catalog.pg_index i ON (i.indrelid = c.oid)
                JOIN pg_catalog.pg_class c2 ON (c2.oid = i.indexrelid)
                LEFT JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace)
                LEFT JOIN pg_catalog.pg_tablespace t ON (t.oid = c.reltablespace)
            WHERE
                i.indisprimary IS TRUE
            $whereclause
        };

		if ($dbh->{private_dbdpg}{version} >= 90200) {
			$pri_key_sql =~ s/t.spclocation/pg_catalog.pg_tablespace_location(t.oid)/;
		}

		my $sth = $dbh->prepare($pri_key_sql) or return undef;
		$sth->execute();
		my $info = $sth->fetchall_arrayref()->[0];
		return undef if ! defined $info;

		# Get the attribute information
		my $indkey = join ',', split /\s+/, $info->[4];
		my $sql = qq{
            SELECT a.attnum, pg_catalog.quote_ident(a.attname) AS colname,
                pg_catalog.quote_ident(t.typname) AS typename
            FROM pg_catalog.pg_attribute a, pg_catalog.pg_type t
            WHERE a.attrelid = '$info->[0]'
            AND a.atttypid = t.oid
            AND attnum IN ($indkey);
        };
		$sth = $dbh->prepare($sql) or return undef;
		$sth->execute();
		my $attribs = $sth->fetchall_hashref('attnum');

		my $pkinfo = [];

		## Normal way: complete "row" per column in the primary key
		if (!exists $attr->{'pg_onerow'}) {
			my $x=0;
			my @key_seq = split/\s+/, $info->[4];
			for (@key_seq) {
				# TABLE_CAT
				$pkinfo->[$x][0] = $info->[10];
				# SCHEMA_NAME
				$pkinfo->[$x][1] = $info->[1];
				# TABLE_NAME
				$pkinfo->[$x][2] = $info->[2];
				# COLUMN_NAME
				$pkinfo->[$x][3] = $attribs->{$_}{colname};
				# KEY_SEQ
				$pkinfo->[$x][4] = $_;
				# PK_NAME
				$pkinfo->[$x][5] = $info->[3];
				# DATA_TYPE
				$pkinfo->[$x][6] = $attribs->{$_}{typename};
				$pkinfo->[$x][7] = $info->[5];
				$pkinfo->[$x][8] = $info->[6];
				$pkinfo->[$x][9] = $info->[7];
				$pkinfo->[$x][10] = $info->[8];
				$pkinfo->[$x][11] = $info->[9];
				$x++;
			}
		}
		else { ## Nicer way: return only one row

			# TABLE_CAT
			$info->[0] = $info->[10];
			# TABLESPACES
			$info->[7] = $info->[5];
			$info->[8] = $info->[6];
			# Unquoted names
			$info->[9] = $info->[7];
			$info->[10] = $info->[8];
			$info->[11] = $info->[9];
			# PK_NAME
			$info->[5] = $info->[3];
			# COLUMN_NAME
			$info->[3] = 2==$attr->{'pg_onerow'} ?
				[ map { $attribs->{$_}{colname} } split /\s+/, $info->[4] ] :
					join ', ', map { $attribs->{$_}{colname} } split /\s+/, $info->[4];
			# DATA_TYPE
			$info->[6] = 2==$attr->{'pg_onerow'} ?
				[ map { $attribs->{$_}{typename} } split /\s+/, $info->[4] ] :
					join ', ', map { $attribs->{$_}{typename} } split /\s+/, $info->[4];
			# KEY_SEQ
			$info->[4] = 2==$attr->{'pg_onerow'} ?
				[ split /\s+/, $info->[4] ] :
					join ', ', split /\s+/, $info->[4];

			$pkinfo = [$info];
		}

		my @cols = (qw(TABLE_CAT TABLE_SCHEM TABLE_NAME COLUMN_NAME
									 KEY_SEQ PK_NAME DATA_TYPE));
		push @cols, 'pg_tablespace_name', 'pg_tablespace_location';
		push @cols, 'pg_schema', 'pg_table', 'pg_column';

		return _prepare_from_data('primary_key_info', $pkinfo, \@cols);

	}

	sub primary_key {
		my $sth = primary_key_info(@_[0..3], {pg_onerow => 2});
		return defined $sth ? @{$sth->fetchall_arrayref()->[0][3]} : ();
	}


	sub foreign_key_info {

		my $dbh = shift;

		## PK: catalog, schema, table, FK: catalog, schema, table, attr
		## Each of these may be undef or empty
		my $pschema = $_[1] || '';
		my $ptable = $_[2] || '';
		my $fschema = $_[4] || '';
		my $ftable = $_[5] || '';
		my $args = $_[6];

		## Must have at least one named table
		return undef if !length($ptable) and !length($ftable);

		## If only the primary table is given, we return only those columns
		## that are used as foreign keys, even if that means that we return
		## unique keys but not primary one. We also return all the foreign
		## tables/columns that are referencing them, of course.
		## If no schema is given, respect search_path by using pg_table_is_visible()
		my @where;
		for ([$ptable, $pschema, 'uk'], [$ftable, $fschema, 'fk']) {
			my ($table, $schema, $type) = @$_;
			if (length $table) {
				push @where, "${type}_class.relname = " . $dbh->quote($table);
				if (length $schema) {
					push @where, "${type}_ns.nspname = " . $dbh->quote($schema);
				}
				else {
					push @where, "pg_catalog.pg_table_is_visible(${type}_class.oid)"
				}
			}
		}

		my $WHERE = join ' AND ', @where;
		my $SQL = qq{
			SELECT
				pg_catalog.quote_ident(pg_catalog.current_database()),
				pg_catalog.quote_ident(uk_ns.nspname),
				pg_catalog.quote_ident(uk_class.relname),
				pg_catalog.quote_ident(uk_col.attname),
				pg_catalog.quote_ident(pg_catalog.current_database()),
				pg_catalog.quote_ident(fk_ns.nspname),
				pg_catalog.quote_ident(fk_class.relname),
				pg_catalog.quote_ident(fk_col.attname),
				colnum.i,
				CASE constr.confupdtype
					WHEN 'c' THEN 0 WHEN 'r' THEN 1 WHEN 'n' THEN 2 WHEN 'a' THEN 3 WHEN 'd' THEN 4 ELSE -1
				END,
				CASE constr.confdeltype
					WHEN 'c' THEN 0 WHEN 'r' THEN 1 WHEN 'n' THEN 2 WHEN 'a' THEN 3 WHEN 'd' THEN 4 ELSE -1
				END,
				pg_catalog.quote_ident(constr.conname), pg_catalog.quote_ident(uk_constr.conname),
				CASE
					WHEN constr.condeferrable = 'f' THEN 7
					WHEN constr.condeferred = 't' THEN 6
					WHEN constr.condeferred = 'f' THEN 5
					ELSE -1
				END,
				CASE coalesce(uk_constr.contype, 'u')
					WHEN 'u' THEN 'UNIQUE' WHEN 'p' THEN 'PRIMARY'
				END,
				pg_catalog.quote_ident(uk_type.typname), pg_catalog.quote_ident(fk_type.typname)
			FROM pg_catalog.pg_constraint constr
				JOIN pg_catalog.pg_class uk_class ON constr.confrelid = uk_class.oid
				JOIN pg_catalog.pg_namespace uk_ns ON uk_class.relnamespace = uk_ns.oid
				JOIN pg_catalog.pg_class fk_class ON constr.conrelid = fk_class.oid
				JOIN pg_catalog.pg_namespace fk_ns ON fk_class.relnamespace = fk_ns.oid
				-- can't do unnest() until 8.4, and would need WITH ORDINALITY to get the array indices,
				-- wich isn't available until 9.4 at the earliest, so we join against a series table instead
				JOIN pg_catalog.generate_series(1, pg_catalog.current_setting('max_index_keys')::integer) colnum(i)
					ON colnum.i <= pg_catalog.array_upper(constr.conkey,1)
				JOIN pg_catalog.pg_attribute uk_col ON uk_col.attrelid = constr.confrelid AND uk_col.attnum = constr.confkey[colnum.i]
				JOIN pg_catalog.pg_type uk_type ON uk_col.atttypid = uk_type.oid
				JOIN pg_catalog.pg_attribute fk_col ON fk_col.attrelid = constr.conrelid AND fk_col.attnum = constr.conkey[colnum.i]
				JOIN pg_catalog.pg_type fk_type ON fk_col.atttypid = fk_type.oid

				-- We can't match confkey from the fk constraint to conkey of the unique constraint,
				-- because the unique constraint might not exist or there might be more than one
				-- matching one. However, there must be at least a unique _index_ on the key
				-- columns, so we look for that; but we can't find it via pg_index, since there may
				-- again be more than one matching index.

				-- So instead, we look at pg_depend for the dependency that was created by the fk
				-- constraint. This dependency is of type 'n' (normal) and ties the pg_constraint
				-- row oid to the pg_class oid for the index relation (a single arbitrary one if
				-- more than one matching unique index existed at the time the constraint was
				-- created).  Fortunately, the constraint does not create dependencies on the
				-- referenced table itself, but on the _columns_ of the referenced table, so the
				-- index can be distinguished easily.  Then we look for another pg_depend entry,
				-- this time an 'i' (implementation) dependency from a pg_constraint oid (the unique
				-- constraint if one exists) to the index oid; but we have to allow for the
				-- possibility that this one doesn't exist.          - Andrew Gierth (RhodiumToad)

				JOIN pg_catalog.pg_depend dep ON (
					dep.classid = 'pg_catalog.pg_constraint'::regclass
					AND dep.objid = constr.oid
					AND dep.objsubid = 0
					AND dep.deptype = 'n'
					AND dep.refclassid = 'pg_catalog.pg_class'::regclass
					AND dep.refobjsubid=0
				)
				JOIN pg_catalog.pg_class idx ON (
					idx.oid = dep.refobjid AND idx.relkind='i'
				)
				LEFT JOIN pg_catalog.pg_depend dep2 ON (
					dep2.classid = 'pg_catalog.pg_class'::regclass
					AND dep2.objid = idx.oid
					AND dep2.objsubid = 0
					AND dep2.deptype = 'i'
					AND dep2.refclassid = 'pg_catalog.pg_constraint'::regclass
					AND dep2.refobjsubid = 0
				)
				LEFT JOIN pg_catalog.pg_constraint uk_constr ON (
					uk_constr.oid = dep2.refobjid AND uk_constr.contype IN ('p','u')
				)
			WHERE $WHERE
				AND uk_class.relkind = 'r'
				AND fk_class.relkind = 'r'
				AND constr.contype = 'f'
			ORDER BY constr.conname, colnum.i
		};
		my $fkinfo = $dbh->selectall_arrayref($SQL);

		return undef unless $fkinfo && @{$fkinfo};

		my @cols = (qw(
			UK_TABLE_CAT UK_TABLE_SCHEM UK_TABLE_NAME UK_COLUMN_NAME
			FK_TABLE_CAT FK_TABLE_SCHEM FK_TABLE_NAME FK_COLUMN_NAME
			ORDINAL_POSITION UPDATE_RULE DELETE_RULE FK_NAME UK_NAME
			DEFERABILITY UNIQUE_OR_PRIMARY UK_DATA_TYPE FK_DATA_TYPE
		));

		if ($dbh->{FetchHashKeyName} eq 'NAME_lc') {
			for my $col (@cols) {
				$col = lc $col;
			}
		}

		return _prepare_from_data('foreign_key_info', $fkinfo, \@cols);

	}


	sub table_info {

		my $dbh = shift;
		my ($catalog, $schema, $table, $type) = @_;

		my $tbl_sql = ();

		my $extracols = q{,NULL::text AS pg_schema, NULL::text AS pg_table};
		if ( # Rule 19a
				(defined $catalog and $catalog eq '%')
				and (defined $schema and $schema eq '')
				and (defined $table and $table eq '')
			 ) {
			$tbl_sql = qq{
                    SELECT
                       pg_catalog.quote_ident(pg_catalog.current_database()) AS "TABLE_CAT"
                     , NULL::text AS "TABLE_SCHEM"
                     , NULL::text AS "TABLE_NAME"
                     , NULL::text AS "TABLE_TYPE"
                     , NULL::text AS "REMARKS" $extracols
                    };
		}
		elsif (# Rule 19b
					 (defined $catalog and $catalog eq '')
					 and (defined $schema and $schema eq '%')
					 and (defined $table and $table eq '')
					) {
			$extracols = q{,n.nspname AS pg_schema, NULL::text AS pg_table};
			$tbl_sql = qq{SELECT
                       NULL::text AS "TABLE_CAT"
                     , pg_catalog.quote_ident(n.nspname) AS "TABLE_SCHEM"
                     , NULL::text AS "TABLE_NAME"
                     , NULL::text AS "TABLE_TYPE"
                     , CASE WHEN n.nspname ~ '^pg_' THEN 'system schema' ELSE 'owned by ' || pg_catalog.pg_get_userbyid(n.nspowner) END AS "REMARKS" $extracols
                    FROM pg_catalog.pg_namespace n
                    ORDER BY "TABLE_SCHEM"
                    };
		}
		elsif (# Rule 19c
					 (defined $catalog and $catalog eq '')
					 and (defined $schema and $schema eq '')
					 and (defined $table and $table eq '')
					 and (defined $type and $type eq '%')
					) {
			$tbl_sql = q{
                    SELECT "TABLE_CAT"
                         , "TABLE_SCHEM"
                         , "TABLE_NAME"
                         , "TABLE_TYPE"
                         , "REMARKS"
                    FROM
                      (SELECT NULL::text AS "TABLE_CAT"
                            , NULL::text AS "TABLE_SCHEM"
                            , NULL::text AS "TABLE_NAME") dummy_cols
                    CROSS JOIN
                      (SELECT 'TABLE'        AS "TABLE_TYPE"
                            , 'relkind: r'   AS "REMARKS"
                       UNION
                       SELECT 'SYSTEM TABLE'
                            , 'relkind: r; nspname ~ ^pg_(catalog|toast)$'
                       UNION
                       SELECT 'VIEW'
                            , 'relkind: v'
                       UNION
                       SELECT 'SYSTEM VIEW'
                            , 'relkind: v; nspname ~ ^pg_(catalog|toast)$'
                       UNION
                       SELECT 'MATERIALIZED VIEW'
                            , 'relkind: m'
                       UNION
                       SELECT 'SYSTEM MATERIALIZED VIEW'
                            , 'relkind: m; nspname ~ ^pg_(catalog|toast)$'
                       UNION
                       SELECT 'FOREIGN TABLE'
                            , 'relkind: f'
                       UNION
                       SELECT 'SYSTEM FOREIGN TABLE'
                            , 'relkind: f; nspname ~ ^pg_(catalog|toast)$'
                       UNION
                       SELECT 'LOCAL TEMPORARY'
                            , 'relkind: r; nspname ~ ^pg_(toast_)?temp') type_info
                     ORDER BY "TABLE_TYPE" ASC
                };
        }
        else {
            # Default SQL
            $extracols = q{,n.nspname AS pg_schema, c.relname AS pg_table};
            my @search = (q|c.relkind IN ('r', 'v', 'm', 'f')|, # No sequences, etc. for now
                          q|NOT (pg_catalog.quote_ident(n.nspname) ~ '^pg_(toast_)?temp_' AND NOT pg_catalog.has_schema_privilege(n.nspname, 'USAGE'))|);   # No others' temp objects
            my $showtablespace = ', pg_catalog.quote_ident(t.spcname) AS "pg_tablespace_name", pg_catalog.quote_ident(t.spclocation) AS "pg_tablespace_location"';
            if ($dbh->{private_dbdpg}{version} >= 90200) {
                $showtablespace = ', pg_catalog.quote_ident(t.spcname) AS "pg_tablespace_name", pg_catalog.quote_ident(pg_catalog.pg_tablespace_location(t.oid)) AS "pg_tablespace_location"';
            }

            ## If the schema or table has an underscore or a %, use a LIKE comparison
            if (defined $schema and length $schema) {
                    push @search, 'n.nspname ' . ($schema =~ /[_%]/ ? 'LIKE ' : '= ') . $dbh->quote($schema);
            }
            if (defined $table and length $table) {
                    push @search, 'c.relname ' . ($table =~ /[_%]/ ? 'LIKE ' : '= ') . $dbh->quote($table);
            }

            my $whereclause = join "\n\t\t\t\t\t AND " => @search;
            $tbl_sql = qq{
                SELECT pg_catalog.quote_ident(pg_catalog.current_database()) AS "TABLE_CAT"
                     , pg_catalog.quote_ident(n.nspname) AS "TABLE_SCHEM"
                     , pg_catalog.quote_ident(c.relname) AS "TABLE_NAME"
                       -- any temp table or temp view is LOCAL TEMPORARY for us
                     , CASE WHEN pg_catalog.quote_ident(n.nspname) ~ '^pg_(toast_)?temp_' THEN
                                 'LOCAL TEMPORARY'
                            WHEN c.relkind = 'r' THEN
                                 CASE WHEN pg_catalog.quote_ident(n.nspname) ~ '^pg_' THEN
                                           'SYSTEM TABLE'
                                      ELSE 'TABLE'
                                  END
                            WHEN c.relkind = 'v' THEN
                                 CASE WHEN pg_catalog.quote_ident(n.nspname) ~ '^pg_' THEN
                                           'SYSTEM VIEW'
                                      ELSE 'VIEW'
                                  END
                            WHEN c.relkind = 'm' THEN
                                 CASE WHEN pg_catalog.quote_ident(n.nspname) ~ '^pg_' THEN
                                           'SYSTEM MATERIALIZED VIEW'
                                      ELSE 'MATERIALIZED VIEW'
                                  END
                            WHEN c.relkind = 'f' THEN
                                 CASE WHEN pg_catalog.quote_ident(n.nspname) ~ '^pg_' THEN
                                           'SYSTEM FOREIGN TABLE'
                                      ELSE 'FOREIGN TABLE'
                                  END
                            ELSE 'UNKNOWN'
                         END AS "TABLE_TYPE"
                     , d.description AS "REMARKS" $showtablespace $extracols
                  FROM pg_catalog.pg_class AS c
                  LEFT JOIN pg_catalog.pg_description AS d
                       ON (c.oid = d.objoid AND c.tableoid = d.classoid AND d.objsubid = 0)
                  LEFT JOIN pg_catalog.pg_namespace n ON (n.oid = c.relnamespace)
                  LEFT JOIN pg_catalog.pg_tablespace t ON (t.oid = c.reltablespace)
                 WHERE $whereclause
                 ORDER BY "TABLE_TYPE", "TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME"
                };

            if (defined($type) and length($type) and $type ne '%') {
                my $type_restrict = join ', ' =>
                                      map { /^'/ ? $_ : $dbh->quote($_) }
                                        grep {length}
                                          split(',', $type); ## no critic
                $tbl_sql = qq{SELECT * FROM ($tbl_sql) ti WHERE "TABLE_TYPE" IN ($type_restrict)};
            }
        }
        my $sth = $dbh->prepare( $tbl_sql ) or return undef;
        $sth->execute();

        return $sth;
    }

    sub tables {
            my ($dbh, @args) = @_;
            my $attr = $args[4];
            my $sth = $dbh->table_info(@args) or return;
            my $tables = $sth->fetchall_arrayref() or return;
            my @tables = map { (! (ref $attr eq 'HASH' and $attr->{pg_noprefix})) ?
                        "$_->[1].$_->[2]" : $_->[2] } @$tables;
            return @tables;
    }

    sub table_attributes {
        my ($dbh, $table) = @_;

        my $sth = $dbh->column_info(undef,undef,$table,undef);

        my %convert = (
            COLUMN_NAME   => 'NAME',
            DATA_TYPE     => 'TYPE',
            COLUMN_SIZE   => 'SIZE',
            NULLABLE      => 'NOTNULL',
            REMARKS       => 'REMARKS',
            COLUMN_DEF    => 'DEFAULT',
            pg_constraint => 'CONSTRAINT',
        );

        my $attrs = $sth->fetchall_arrayref(\%convert);

        for my $row (@$attrs) {
            # switch the column names
            for my $name (keys %$row) {
                $row->{ $convert{$name} } = $row->{$name};

                ## Keep some original columns
                delete $row->{$name} unless ($name eq 'REMARKS' or $name eq 'NULLABLE');

            }
            # Moved check outside of loop as it was inverting the NOTNULL value for
            # attribute.
            # NOTNULL inverts the sense of NULLABLE
            $row->{NOTNULL} = ($row->{NOTNULL} ? 0 : 1);

            my @pri_keys = $dbh->primary_key( undef, undef, $table );
            $row->{PRIMARY_KEY} = scalar(grep { /^$row->{NAME}$/i } @pri_keys) ? 1 : 0;
        }

        return $attrs;

    }

    sub _calc_col_size {

        my $mod = shift;
        my $size = shift;


        if ((defined $size) and ($size > 0)) {
            return $size;
        } elsif ($mod > 0xffff) {
            my $prec = ($mod & 0xffff) - 4;
            $mod >>= 16;
            my $dig = $mod;
            return "$prec,$dig";
        } elsif ($mod >= 4) {
            return $mod - 4;
        } # else {
            # $rtn = $mod;
            # $rtn = undef;
        # }

        return;
    }


    sub type_info_all {
        my ($dbh) = @_;

        my $names =
            {
             TYPE_NAME          => 0,
             DATA_TYPE          => 1,
             COLUMN_SIZE        => 2,
             LITERAL_PREFIX     => 3,
             LITERAL_SUFFIX     => 4,
             CREATE_PARAMS      => 5,
             NULLABLE           => 6,
             CASE_SENSITIVE     => 7,
             SEARCHABLE         => 8,
             UNSIGNED_ATTRIBUTE => 9,
             FIXED_PREC_SCALE   => 10,
             AUTO_UNIQUE_VALUE  => 11,
             LOCAL_TYPE_NAME    => 12,
             MINIMUM_SCALE      => 13,
             MAXIMUM_SCALE      => 14,
             SQL_DATA_TYPE      => 15,
             SQL_DATETIME_SUB   => 16,
             NUM_PREC_RADIX     => 17,
             INTERVAL_PRECISION => 18,
            };

        ## This list is derived from dbi_sql.h in DBI, from types.c and types.h, and from the PG docs

        ## Aids to make the list more readable:
        my $GIG = 1073741824;
        my $PS = 'precision/scale';
        my $LEN = 'length';
        my $UN;
        my $ti =
            [
             $names,
# name     sql_type          size   pfx/sfx crt   n/c/s    +-/P/I   local       min max  sub rdx itvl

['unknown',  SQL_UNKNOWN_TYPE,  0,    $UN,$UN,   $UN,  1,0,0, $UN,0,0, 'UNKNOWN',   $UN,$UN,
             SQL_UNKNOWN_TYPE,                                                             $UN, $UN, $UN ],
['bytea',    SQL_VARBINARY,     $GIG, q{'},q{'}, $UN,  1,0,3, $UN,0,0, 'BYTEA',     $UN,$UN,
             SQL_VARBINARY,                                                                $UN, $UN, $UN ],
['bpchar',   SQL_CHAR,          $GIG, q{'},q{'}, $LEN, 1,1,3, $UN,0,0, 'CHARACTER', $UN,$UN,
             SQL_CHAR,                                                                     $UN, $UN, $UN ],
['numeric',  SQL_DECIMAL,       1000, $UN,$UN,   $PS,  1,0,2, 0,0,0,   'FLOAT',     0,1000,
             SQL_DECIMAL,                                                                  $UN, $UN, $UN ],
['numeric',  SQL_NUMERIC,       1000, $UN,$UN,   $PS,  1,0,2, 0,0,0,   'FLOAT',     0,1000,
             SQL_NUMERIC,                                                                  $UN, $UN, $UN ],
['int4',     SQL_INTEGER,       10,   $UN,$UN,   $UN,  1,0,2, 0,0,0,   'INTEGER',   0,0,
             SQL_INTEGER,                                                                  $UN, $UN, $UN ],
['int2',     SQL_SMALLINT,      5,    $UN,$UN,   $UN,  1,0,2, 0,0,0,   'SMALLINT',  0,0,
             SQL_SMALLINT,                                                                 $UN, $UN, $UN ],
['float4',   SQL_FLOAT,         6,    $UN,$UN,   $PS,  1,0,2, 0,0,0,   'FLOAT',     0,6,
             SQL_FLOAT,                                                                    $UN, $UN, $UN ],
['float8',   SQL_REAL,          15,   $UN,$UN,   $PS,  1,0,2, 0,0,0,   'REAL',      0,15,
             SQL_REAL,                                                                     $UN, $UN, $UN ],
['int8',     SQL_BIGINT,        20,   $UN,$UN,   $UN,  1,0,2, 0,0,0,   'INT8',   0,0,
             SQL_BIGINT,                                                                   $UN, $UN, $UN ],
['date',     SQL_DATE,          10,   q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'DATE',      0,0,
             SQL_DATE,                                                                     $UN, $UN, $UN ],
['tinterval',SQL_TIME,          18,   q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'TINTERVAL', 0,6,
             SQL_TIME,                                                                     $UN, $UN, $UN ],
['timestamp',SQL_TIMESTAMP,     29,   q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'TIMESTAMP', 0,6,
             SQL_TIMESTAMP,                                                                $UN, $UN, $UN ],
['text',     SQL_LONGVARCHAR,       $GIG, q{'},q{'}, $LEN, 1,1,3, $UN,0,0, 'TEXT',      $UN,$UN,
             SQL_LONGVARCHAR,                                                                  $UN, $UN, $UN ],
['bool',     SQL_BOOLEAN,       1,    q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'BOOLEAN',   $UN,$UN,
             SQL_BOOLEAN,                                                                  $UN, $UN, $UN ],
['array',    SQL_ARRAY,         1,    q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'ARRAY',     $UN,$UN,
             SQL_ARRAY,                                                                    $UN, $UN, $UN ],
['date',     SQL_TYPE_DATE,     10,   q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'DATE',      0,0,
             SQL_TYPE_DATE,                                                                $UN, $UN, $UN ],
['time',     SQL_TYPE_TIME,     18,   q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'TIME',      0,6,
             SQL_TYPE_TIME,                                                                $UN, $UN, $UN ],
['timestamp',SQL_TYPE_TIMESTAMP,29,   q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'TIMESTAMP', 0,6,
             SQL_TYPE_TIMESTAMP,                                                           $UN, $UN, $UN ],
['timetz',   SQL_TYPE_TIME_WITH_TIMEZONE,
                                29,   q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'TIMETZ',    0,6,
             SQL_TYPE_TIME_WITH_TIMEZONE,                                                  $UN, $UN, $UN ],
['timestamptz',SQL_TYPE_TIMESTAMP_WITH_TIMEZONE,
                                29,   q{'},q{'}, $UN,  1,0,2, $UN,0,0, 'TIMESTAMPTZ',0,6,
             SQL_TYPE_TIMESTAMP_WITH_TIMEZONE,                                             $UN, $UN, $UN ],
        #
        # intentionally omitted: char, all geometric types, internal types
    ];
    return $ti;
    }


    # Characters that need to be escaped by quote().
    my %esc = (
        q{'}  => '\\047', # '\\' . sprintf("%03o", ord("'")), # ISO SQL 2
        '\\' => '\\134', # '\\' . sprintf("%03o", ord("\\")),
    );

    # Set up lookup for SQL types we don't want to escape.
    my %no_escape = map { $_ => 1 }
        DBI::SQL_INTEGER, DBI::SQL_SMALLINT, DBI::SQL_BIGINT, DBI::SQL_DECIMAL,
        DBI::SQL_FLOAT, DBI::SQL_REAL, DBI::SQL_DOUBLE, DBI::SQL_NUMERIC;

    my %get_info_type = (

## Driver information:

     116 => ['SQL_ACTIVE_ENVIRONMENTS',             0                         ], ## unlimited
   10021 => ['SQL_ASYNC_MODE',                      2                         ], ## SQL_AM_STATEMENT
     120 => ['SQL_BATCH_ROW_COUNT',                 2                         ], ## SQL_BRC_EXPLICIT
     121 => ['SQL_BATCH_SUPPORT',                   3                         ], ## 12 SELECT_PROC + ROW_COUNT_PROC
       2 => ['SQL_DATA_SOURCE_NAME',                sub { sprintf 'dbi:Pg:%s', shift->{Name} } ],
       3 => ['SQL_DRIVER_HDBC',                     0                         ], ## not applicable
     135 => ['SQL_DRIVER_HDESC',                    0                         ], ## not applicable
       4 => ['SQL_DRIVER_HENV',                     0                         ], ## not applicable
      76 => ['SQL_DRIVER_HLIB',                     0                         ], ## not applicable
       5 => ['SQL_DRIVER_HSTMT',                    0                         ], ## not applicable
       ## Not clear what should go here. Some things suggest 'Pg', others 'Pg.pm'. We'll use DBD::Pg for now
       6 => ['SQL_DRIVER_NAME',                     'DBD::Pg'                 ],
      77 => ['SQL_DRIVER_ODBC_VERSION',             '03.00'                   ],
       7 => ['SQL_DRIVER_VER',                      'DBDVERSION'              ], ## magic word
     144 => ['SQL_DYNAMIC_CURSOR_ATTRIBUTES1',      0                         ], ## we can FETCH, but not via methods
     145 => ['SQL_DYNAMIC_CURSOR_ATTRIBUTES2',      0                         ], ## same as above
      84 => ['SQL_FILE_USAGE',                      0                         ], ## SQL_FILE_NOT_SUPPORTED (this is good)
     146 => ['SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1', 519                       ], ## not clear what this refers to in DBD context
     147 => ['SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2', 5209                      ], ## see above
      81 => ['SQL_GETDATA_EXTENSIONS',              15                        ], ## 1+2+4+8
     149 => ['SQL_INFO_SCHEMA_VIEWS',               3932149                   ], ## not: assert, charset, collat, trans
     150 => ['SQL_KEYSET_CURSOR_ATTRIBUTES1',       0                         ], ## applies to us?
     151 => ['SQL_KEYSET_CURSOR_ATTRIBUTES2',       0                         ], ## see above
   10022 => ['SQL_MAX_ASYNC_CONCURRENT_STATEMENTS', 0                         ], ## unlimited, probably
       0 => ['SQL_MAX_DRIVER_CONNECTIONS',          \'SHOW max_connections'   ],
     152 => ['SQL_ODBC_INTERFACE_CONFORMANCE',      1                         ], ## SQL_OIC_LEVEL_1
      10 => ['SQL_ODBC_VER',                        '03.00.0000'              ],
     153 => ['SQL_PARAM_ARRAY_ROW_COUNTS',          2                         ], ## correct?
     154 => ['SQL_PARAM_ARRAY_SELECTS',             3                         ], ## PAS_NO_SELECT
      11 => ['SQL_ROW_UPDATES',                     'N'                       ],
      14 => ['SQL_SEARCH_PATTERN_ESCAPE',           '\\'                      ],
      13 => ['SQL_SERVER_NAME',                     \'SELECT pg_catalog.current_database()' ],
     166 => ['SQL_STANDARD_CLI_CONFORMANCE',        2                         ], ## ??
     167 => ['SQL_STATIC_CURSOR_ATTRIBUTES1',       519                       ], ## ??
     168 => ['SQL_STATIC_CURSOR_ATTRIBUTES2',       5209                      ], ## ??
    9000 => ['9000',                                1                         ], ## can escape placeholders

## DBMS Information

      16 => ['SQL_DATABASE_NAME',                   \'SELECT pg_catalog.current_database()' ],
      17 => ['SQL_DBMS_NAME',                       'PostgreSQL'              ],
      18 => ['SQL_DBMS_VERSION',                    'ODBCVERSION'             ], ## magic word

## Data source information

      20 => ['SQL_ACCESSIBLE_PROCEDURES',           'Y'                       ], ## is this really true?
      19 => ['SQL_ACCESSIBLE_TABLES',               'Y'                       ], ## is this really true?
      82 => ['SQL_BOOKMARK_PERSISTENCE',            0                         ],
      42 => ['SQL_CATALOG_TERM',                    ''                        ], ## empty = catalogs are not supported
   10004 => ['SQL_COLLATION_SEQ',                   \'SHOW server_encoding'   ],
      22 => ['SQL_CONCAT_NULL_BEHAVIOR',            0                         ], ## SQL_CB_NULL
      23 => ['SQL_CURSOR_COMMIT_BEHAVIOR',          1                         ], ## SQL_CB_CLOSE
      24 => ['SQL_CURSOR_ROLLBACK_BEHAVIOR',        1                         ], ## SQL_CB_CLOSE
   10001 => ['SQL_CURSOR_SENSITIVITY',              1                         ], ## SQL_INSENSITIVE
      25 => ['SQL_DATA_SOURCE_READ_ONLY',           'READONLY'                ], ## magic word
      26 => ['SQL_DEFAULT_TXN_ISOLATION',           'DEFAULTTXN'              ], ## magic word (2 or 8)
   10002 => ['SQL_DESCRIBE_PARAMETER',              'Y'                       ],
      36 => ['SQL_MULT_RESULT_SETS',                'Y'                       ],
      37 => ['SQL_MULTIPLE_ACTIVE_TXN',             'Y'                       ],
     111 => ['SQL_NEED_LONG_DATA_LEN',              'N'                       ],
      85 => ['SQL_NULL_COLLATION',                  0                         ], ## SQL_NC_HIGH
      40 => ['SQL_PROCEDURE_TERM',                  'function'                ], ## for now
      39 => ['SQL_SCHEMA_TERM',                     'schema'                  ],
      44 => ['SQL_SCROLL_OPTIONS',                  8                         ], ## not really for DBD?
      45 => ['SQL_TABLE_TERM',                      'table'                   ],
      46 => ['SQL_TXN_CAPABLE',                     2                         ], ## SQL_TC_ALL
      72 => ['SQL_TXN_ISOLATION_OPTION',            10                        ], ## 2+8
      47 => ['SQL_USER_NAME',                       sub { shift->{CURRENT_USER} } ],

## Supported SQL

     169  => ['SQL_AGGREGATE_FUNCTIONS',            127                       ], ## all of 'em
     117  => ['SQL_ALTER_DOMAIN',                   31                        ], ## all but deferred
      86  => ['SQL_ALTER_TABLE',                    32639                     ], ## no collate
     114  => ['SQL_CATALOG_LOCATION',               0                         ],
   10003  => ['SQL_CATALOG_NAME',                   'N'                       ],
      41  => ['SQL_CATALOG_NAME_SEPARATOR',         ''                        ],
      92  => ['SQL_CATALOG_USAGE',                  0                         ],
      87  => ['SQL_COLUMN_ALIAS',                   'Y'                       ],
      74  => ['SQL_CORRELATION_NAME',               2                         ], ## SQL_CN_ANY
     127  => ['SQL_CREATE_ASSERTION',               0                         ],
     128  => ['SQL_CREATE_CHARACTER_SET',           0                         ],
     129  => ['SQL_CREATE_COLLATION',               0                         ],
     130  => ['SQL_CREATE_DOMAIN',                  23                        ], ## no collation, no defer
     131  => ['SQL_CREATE_SCHEMA',                  3                         ], ## 1+2 schema + authorize
     132  => ['SQL_CREATE_TABLE',                   13845                     ], ## no collation
     133  => ['SQL_CREATE_TRANSLATION',             0                         ],
     134  => ['SQL_CREATE_VIEW',                    9                         ], ## local + create?
     119  => ['SQL_DATETIME_LITERALS',              65535                     ], ## all?
     170  => ['SQL_DDL_INDEX',                      3                         ], ## create + drop
     136  => ['SQL_DROP_ASSERTION',                 0                         ],
     137  => ['SQL_DROP_CHARACTER_SET',             0                         ],
     138  => ['SQL_DROP_COLLATION',                 0                         ],
     139  => ['SQL_DROP_DOMAIN',                    7                         ],
     140  => ['SQL_DROP_SCHEMA',                    7                         ],
     141  => ['SQL_DROP_TABLE',                     7                         ],
     142  => ['SQL_DROP_TRANSLATION',               0                         ],
     143  => ['SQL_DROP_VIEW',                      7                         ],
      27  => ['SQL_EXPRESSIONS_IN_ORDERBY',         'Y'                       ],
      88  => ['SQL_GROUP_BY',                       2                         ], ## GROUP_BY_CONTAINS_SELECT
      28  => ['SQL_IDENTIFIER_CASE',                2                         ], ## SQL_IC_LOWER
      29  => ['SQL_IDENTIFIER_QUOTE_CHAR',          q{"}                      ],
     148  => ['SQL_INDEX_KEYWORDS',                 0                         ], ## not needed for Pg
     172  => ['SQL_INSERT_STATEMENT',               7                         ], ## 1+2+4 = all
      73  => ['SQL_INTEGRITY',                      'Y'                       ], ## e.g. ON DELETE CASCADE?
      89  => ['SQL_KEYWORDS',                       'KEYWORDS'                ], ## magic word
     113  => ['SQL_LIKE_ESCAPE_CLAUSE',             'Y'                       ],
      75  => ['SQL_NON_NULLABLE_COLUMNS',           1                         ], ## NNC_NOT_NULL
     115  => ['SQL_OJ_CAPABILITIES',                127                       ], ## all
      90  => ['SQL_ORDER_BY_COLUMNS_IN_SELECT',     'N'                       ],
      38  => ['SQL_OUTER_JOINS',                    'Y'                       ],
      21  => ['SQL_PROCEDURES',                     'Y'                       ],
      93  => ['SQL_QUOTED_IDENTIFIER_CASE',         3                         ], ## SQL_IC_SENSITIVE
      91  => ['SQL_SCHEMA_USAGE',                   31                        ], ## all
      94  => ['SQL_SPECIAL_CHARACTERS',             '$'                       ], ## there are actually many more...
     118  => ['SQL_SQL_CONFORMANCE',                4                         ], ## SQL92_INTERMEDIATE ??
      95  => ['SQL_SUBQUERIES',                     31                        ], ## all
      96  => ['SQL_UNION',                          3                         ], ## 1+2 = all

## SQL limits

     112  => ['SQL_MAX_BINARY_LITERAL_LEN',         0                         ],
      34  => ['SQL_MAX_CATALOG_NAME_LEN',           0                         ],
     108  => ['SQL_MAX_CHAR_LITERAL_LEN',           0                         ],
      30  => ['SQL_MAX_COLUMN_NAME_LEN',            'NAMEDATALEN'             ], ## magic word
      97  => ['SQL_MAX_COLUMNS_IN_GROUP_BY',        0                         ],
      98  => ['SQL_MAX_COLUMNS_IN_INDEX',           0                         ],
      99  => ['SQL_MAX_COLUMNS_IN_ORDER_BY',        0                         ],
     100  => ['SQL_MAX_COLUMNS_IN_SELECT',          0                         ],
     101  => ['SQL_MAX_COLUMNS_IN_TABLE',           250                       ], ## 250-1600 (depends on column types)
      31  => ['SQL_MAX_CURSOR_NAME_LEN',            'NAMEDATALEN'             ], ## magic word
   10005  => ['SQL_MAX_IDENTIFIER_LEN',             'NAMEDATALEN'             ], ## magic word
     102  => ['SQL_MAX_INDEX_SIZE',                 0                         ],
     102  => ['SQL_MAX_PROCEDURE_NAME_LEN',         'NAMEDATALEN'             ], ## magic word
     104  => ['SQL_MAX_ROW_SIZE',                   0                         ], ## actually 1.6 TB, but too big to represent here
     103  => ['SQL_MAX_ROW_SIZE_INCLUDES_LONG',     'Y'                       ],
      32  => ['SQL_MAX_SCHEMA_NAME_LEN',            'NAMEDATALEN'             ], ## magic word
     105  => ['SQL_MAX_STATEMENT_LEN',              0                         ],
      35  => ['SQL_MAX_TABLE_NAME_LEN',             'NAMEDATALEN'             ], ## magic word
     106  => ['SQL_MAX_TABLES_IN_SELECT',           0                         ],
     107  => ['SQL_MAX_USER_NAME_LEN',              'NAMEDATALEN'             ], ## magic word

## Scalar function information

      48  => ['SQL_CONVERT_FUNCTIONS',              2                         ], ## CVT_CAST only?
      49  => ['SQL_NUMERIC_FUNCTIONS',              16777215                  ], ## ?? all but some naming clashes: rand(om), trunc(ate), log10=ln, etc.
      50  => ['SQL_STRING_FUNCTIONS',               16280984                  ], ## ??
      51  => ['SQL_SYSTEM_FUNCTIONS',               0                         ], ## ??
     109  => ['SQL_TIMEDATE_ADD_INTERVALS',         0                         ], ## ?? no explicit timestampadd?
     110  => ['SQL_TIMEDATE_DIFF_INTERVALS',        0                         ], ## ??
      52  => ['SQL_TIMEDATE_FUNCTIONS',             1966083                   ],

## Conversion information - all but BIT, LONGVARBINARY, and LONGVARCHAR

      53  => ['SQL_CONVERT_BIGINT',                 1830399                    ],
      54  => ['SQL_CONVERT_BINARY',                 1830399                    ],
      55  => ['SQL_CONVERT_BIT',                    0                          ],
      56  => ['SQL_CONVERT_CHAR',                   1830399                    ],
      57  => ['SQL_CONVERT_DATE',                   1830399                    ],
      58  => ['SQL_CONVERT_DECIMAL',                1830399                    ],
      59  => ['SQL_CONVERT_DOUBLE',                 1830399                    ],
      60  => ['SQL_CONVERT_FLOAT',                  1830399                    ],
      61  => ['SQL_CONVERT_INTEGER',                1830399                    ],
     123  => ['SQL_CONVERT_INTERVAL_DAY_TIME',      1830399                    ],
     124  => ['SQL_CONVERT_INTERVAL_YEAR_MONTH',    1830399                    ],
      71  => ['SQL_CONVERT_LONGVARBINARY',          0                          ],
      62  => ['SQL_CONVERT_LONGVARCHAR',            0                          ],
      63  => ['SQL_CONVERT_NUMERIC',                1830399                    ],
      64  => ['SQL_CONVERT_REAL',                   1830399                    ],
      65  => ['SQL_CONVERT_SMALLINT',               1830399                    ],
      66  => ['SQL_CONVERT_TIME',                   1830399                    ],
      67  => ['SQL_CONVERT_TIMESTAMP',              1830399                    ],
      68  => ['SQL_CONVERT_TINYINT',                1830399                    ],
      69  => ['SQL_CONVERT_VARBINARY',              0                          ],
      70  => ['SQL_CONVERT_VARCHAR',                1830399                    ],
     122  => ['SQL_CONVERT_WCHAR',                  0                          ],
     125  => ['SQL_CONVERT_WLONGVARCHAR',           0                          ],
     126  => ['SQL_CONVERT_WVARCHAR',               0                          ],
    ); ## end of %get_info_type
    ## Add keys for names into the hash
    for (keys %get_info_type) {
        $get_info_type{$get_info_type{$_}->[0]} = $get_info_type{$_};
    }

    sub get_info {

        my ($dbh,$type) = @_;

        return undef unless defined $type;
        return undef unless exists $get_info_type{$type};

        my $ans = $get_info_type{$type}->[1];

        if (ref $ans eq 'CODE') {
            $ans = $ans->($dbh);
        }
        elsif (ref $ans eq 'SCALAR') { # SQL
            return $dbh->selectall_arrayref($$ans)->[0][0];
        }
        elsif ($ans eq 'NAMEDATALEN') {
            return $dbh->selectall_arrayref('SHOW max_identifier_length')->[0][0];
        }
        elsif ($ans eq 'ODBCVERSION') {
            my $version = $dbh->{private_dbdpg}{version};
            return '00.00.0000' unless $version =~ /^(\d\d?)(\d\d)(\d\d)$/;
            return sprintf '%02d.%02d.%.2d00', $1,$2,$3;
        }
        elsif ($ans eq 'DBDVERSION') {
            my $simpleversion = $DBD::Pg::VERSION;
            $simpleversion =~ s/_/./g;
            no if $] >= 5.022, warnings => 'redundant';
            return sprintf '%02d.%02d.%1d%1d%1d%1d', split (/\./, "$simpleversion.0.0.0.0.0.0");
        }
         elsif ($ans eq 'KEYWORDS') {
            ## http://www.postgresql.org/docs/current/static/sql-keywords-appendix.html
            ## Basically, we want ones that are 'reserved' for PostgreSQL but not 'reserved' in SQL:2003
            ## 
            return join ',' => (qw(ANALYSE ANALYZE ASC DEFERRABLE DESC DO FREEZE ILIKE INITIALLY ISNULL LIMIT NOTNULL OFF OFFSET PLACING RETURNING VERBOSE));
         }
         elsif ($ans eq 'READONLY') {
             my $SQL = q{SELECT CASE WHEN setting = 'on' THEN 'Y' ELSE 'N' END FROM pg_settings WHERE name = 'transaction_read_only'};
             my $info = $dbh->selectall_arrayref($SQL);
             return defined $info->[0] ? $info->[0][0] : 'N';
         }
         elsif ($ans eq 'DEFAULTTXN') {
             my $SQL = q{SELECT CASE WHEN setting = 'read committed' THEN 2 ELSE 8 END FROM pg_settings WHERE name = 'default_transaction_isolation'};
             my $info = $dbh->selectall_arrayref($SQL);
             return defined $info->[0] ? $info->[0][0] : 2;
         }

         return $ans;
    } # end of get_info

    sub private_attribute_info {
        return {
                pg_async_status                => undef,
                pg_bool_tf                     => undef,
                pg_db                          => undef,
                pg_default_port                => undef,
                pg_enable_utf8                 => undef,
                pg_utf8_flag                   => undef,
                pg_errorlevel                  => undef,
                pg_expand_array                => undef,
                pg_host                        => undef,
                pg_INV_READ                    => undef,
                pg_INV_WRITE                   => undef,
                pg_lib_version                 => undef,
                pg_options                     => undef,
                pg_pass                        => undef,
                pg_pid                         => undef,
                pg_placeholder_dollaronly      => undef,
                pg_placeholder_nocolons        => undef,
                pg_placeholder_escaped         => undef,
                pg_port                        => undef,
                pg_prepare_now                 => undef,
                pg_protocol                    => undef,
                pg_server_prepare              => undef,
                pg_server_version              => undef,
                pg_socket                      => undef,
                pg_standard_conforming_strings => undef,
                pg_switch_prepared             => undef,
                pg_user                        => undef,
        };
    }
}


{
    package DBD::Pg::st;

    sub parse_trace_flag {
        my ($h, $flag) = @_;
        return DBD::Pg->parse_trace_flag($flag);
    }

    sub bind_param_array {

        ## Binds an array of data to a specific placeholder in a statement
        ## The DBI version is broken, so we implement a near-copy here

        my $sth = shift;
        my ($p_id, $value_array, $attr) = @_;

        ## Bail if the second arg is not undef or an arrayref
        return $sth->set_err(1, "Value for parameter $p_id must be a scalar or an arrayref, not a ".ref($value_array))
            if defined $value_array and ref $value_array and ref $value_array ne 'ARRAY';

        ## Bail if the first arg is not a number
        return $sth->set_err(1, q{Can't use named placeholders for non-driver supported bind_param_array})
            unless DBI::looks_like_number($p_id); # because we rely on execute(@ary) here

        ## Store the list of items in the hash (will be undef or an arrayref)
        $sth->{ParamArrays}{$p_id} = $value_array;

        ## If any attribs were passed in, we need to call bind_param
        return $sth->bind_param($p_id, '', $attr) if $attr; ## This is the big change so -w does not complain

        return 1;
    } ## end bind_param_array

     sub private_attribute_info {
        return {
                pg_async                  => undef,
                pg_bound                  => undef,
                pg_current_row            => undef,
                pg_direct                 => undef,
                pg_numbound               => undef,
                pg_cmd_status             => undef,
                pg_oid_status             => undef,
                pg_placeholder_dollaronly => undef,
                pg_placeholder_nocolons   => undef,
                pg_prepare_name           => undef,
                pg_prepare_now            => undef,
                pg_segments               => undef,
                pg_server_prepare         => undef,
                pg_size                   => undef,
                pg_switch_prepared        => undef,
                pg_type                   => undef,
        };
    }

} ## end st section

1;

__END__

=head1 NAME

DBD::Pg - PostgreSQL database driver for the DBI module

=head1 SYNOPSIS

  use DBI;

  $dbh = DBI->connect("dbi:Pg:dbname=$dbname", '', '', {AutoCommit => 0});
  # The AutoCommit attribute should always be explicitly set

  # For some advanced uses you may need PostgreSQL type values:
  use DBD::Pg qw(:pg_types);

  # For asynchronous calls, import the async constants:
  use DBD::Pg qw(:async);

  $dbh->do('INSERT INTO mytable(a) VALUES (1)');

  $sth = $dbh->prepare('INSERT INTO mytable(a) VALUES (?)');
  $sth->execute();

=head1 VERSION

This documents version 3.8.0 of the DBD::Pg module

=head1 DESCRIPTION

DBD::Pg is a Perl module that works with the DBI module to provide access to
PostgreSQL databases.

=head1 MODULE DOCUMENTATION

This documentation describes driver specific behavior and restrictions. It is
not supposed to be used as the only reference for the user. In any case
consult the B<DBI> documentation first!

L<Latest DBI documentation.|DBI>

=head1 THE DBI CLASS

=head2 DBI Class Methods

=head3 B<connect>

This method creates a database handle by connecting to a database, and is the DBI 
equivalent of the "new" method. To connect to a Postgres database with a minimum of parameters, 
use the following syntax:

  $dbh = DBI->connect("dbi:Pg:dbname=$dbname", '', '', {AutoCommit => 0});

This connects to the database named in the C<$dbname> variable on the default port (usually 5432) 
without any user authentication.

The following connect statement shows almost all possible parameters:

  $dbh = DBI->connect("dbi:Pg:dbname=$dbname;host=$host;port=$port;options=$options",
                      $username,
                      $password,
                      {AutoCommit => 0, RaiseError => 1, PrintError => 0}
                     );

Parameters containing unusual characters such as spaces can be wrapped in single quotes 
around the value e.g. "dbi:Pg:dbname='spacey name';host=$host"

If a parameter is not given, the connect() method will first look for 
specific environment variables, and then fall back to hard-coded defaults:

  parameter    environment variable    hard coded default
  ------------------------------------------------------
  host         PGHOST                  local domain socket
  hostaddr     PGHOSTADDR              local domain socket
  port         PGPORT                  5432
  dbname*      PGDATABASE              current userid
  username     PGUSER                  current userid
  password     PGPASSWORD              (none)
  options      PGOPTIONS               (none)
  service      PGSERVICE               (none)
  sslmode      PGSSLMODE               (none)

* May also use the aliases C<db> or C<database>

If the username and password values passed via C<connect()> are undefined (as opposed 
to merely being empty strings), DBI will use the environment variables I<DBI_USER> 
and I<DBI_PASS> if they exist.

You can also connect by using a service connection file, which is named 
F<pg_service.conf>. The location of this file can be controlled by 
setting the I<PGSYSCONFDIR> environment variable. To use one of the named 
services within the file, set the name by using either the I<service> parameter 
or the environment variable I<PGSERVICE>. Note that when connecting this way, 
only the minimum parameters should be used. For example, to connect to a 
service named "zephyr", you could use:

  $dbh = DBI->connect("dbi:Pg:service=zephyr", '', '');

You could also set C<$ENV{PGSERVICE}> to "zephyr" and connect like this:

  $dbh = DBI->connect("dbi:Pg:", '', '');

The format of the F<pg_service.conf> file is simply a bracketed service 
name, followed by one parameter per line in the format name=value.
For example:

  [zephyr]
  dbname=winds
  user=wisp
  password=W$2Hc00YSgP
  port=6543

There are four valid arguments to the I<sslmode> parameter, which controls 
whether to use SSL to connect to the database:

=over 4

=item * disable: SSL connections are never used

=item * allow: try non-SSL, then SSL

=item * prefer: try SSL, then non-SSL

=item * require: connect only with SSL

=back

You can also connect using sockets in a specific directory. This 
may be needed if the server you are connecting to has a different 
default socket directory from the one used to compile DBD::Pg. 
Use the complete path to the socket directory as the name of the 
host, like this:

  $dbh = DBI->connect('dbi:Pg:dbname=foo;host=/var/tmp/socket',
    $username,
    $password,
    {AutoCommit => 0, RaiseError => 1});

The attribute hash can also contain a key named C<dbd_verbose>, which 
simply calls C<< $dbh->trace('DBD') >> after the handle is created. This attribute 
is not recommended, as it is clearer to simply explicitly call C<trace> explicitly 
in your script.

=head3 B<connect_cached>

  $dbh = DBI->connect_cached("dbi:Pg:dbname=$dbname", $username, $password, \%options);

Implemented by DBI, no driver-specific impact.

=head3 B<data_sources>

  @data_sources = DBI->data_sources('Pg');
  @data_sources = $dbh->data_sources();

Returns a list of available databases. Unless the environment variable C<DBI_DSN> is set, 
a connection will be attempted to the database C<template1>. The normal connection 
environment variables also apply, such as C<PGHOST>, C<PGPORT>, C<DBI_USER>, 
C<DBI_PASS>, and C<PGSERVICE>.

You can also pass in options to add to the connection string For example, to specify 
an alternate port and host:

  @data_sources = DBI->data_sources('Pg', 'port=5824;host=example.com');

  or:

  @data_sources = $dbh->data_sources('port=5824;host=example.com');


=head2 Methods Common To All Handles

For all of the methods below, B<$h> can be either a database handle (B<$dbh>) 
or a statement handle (B<$sth>). Note that I<$dbh> and I<$sth> can be replaced with 
any variable name you choose: these are just the names most often used. Another 
common variable used in this documentation is $I<rv>, which stands for "return value".

=head3 B<err>

  $rv = $h->err;

Returns the error code from the last method called. For the connect method it returns
C<PQstatus>, which is a number used by I<libpq> (the Postgres connection library). A value of 0 
indicates no error (CONNECTION_OK), while any other number indicates a failed connection. The 
only other number commonly seen is 1 (CONNECTION_BAD). See the libpq documentation for the 
complete list of return codes.

In all other non-connect methods C<< $h->err >> returns the C<PQresultStatus> of the current
handle. This is a number used by libpq and is one of:

  0  Empty query string
  1  A command that returns no data successfully completed.
  2  A command that returns data successfully completed.
  3  A COPY OUT command is still in progress.
  4  A COPY IN command is still in progress.
  5  A bad response was received from the backend.
  6  A nonfatal error occurred (a notice or warning message)
  7  A fatal error was returned: the last query failed.

=head3 B<errstr>

  $str = $h->errstr;

Returns the last error that was reported by Postgres. This message is affected 
by the L<pg_errorlevel|/pg_errorlevel_(integer)> setting.

=head3 B<state>

  $str = $h->state;

Returns a five-character "SQLSTATE" code. Success is indicated by a C<00000> code, which 
gets mapped to an empty string by DBI. A code of C<S8006> indicates a connection failure, 
usually because the connection to the Postgres server has been lost.

While this method can be called as either C<< $sth->state >> or C<< $dbh->state >>, it 
is usually clearer to always use C<< $dbh->state >>.

The list of codes used by PostgreSQL can be found at:
L<http://www.postgresql.org/docs/current/static/errcodes-appendix.html>

Note that these codes are part of the SQL standard and only a small number 
of them will be used by PostgreSQL.

Common codes:

  00000 Successful completion
  25P01 No active SQL transaction
  25P02 In failed SQL transaction
  S8006 Connection failure

=head3 B<trace>

  $h->trace($trace_settings);
  $h->trace($trace_settings, $trace_filename);
  $trace_settings = $h->trace;

Changes the trace settings on a database or statement handle. 
The optional second argument specifies a file to write the 
trace information to. If no filename is given, the information 
is written to F<STDERR>. Note that tracing can be set globally as 
well by setting C<< DBI->trace >>, or by using the environment 
variable I<DBI_TRACE>.

The value is either a numeric level or a named flag. For the 
flags that DBD::Pg uses, see L<parse_trace_flag|/parse_trace_flag and parse_trace_flags>.

=head3 B<trace_msg>

  $h->trace_msg($message_text);
  $h->trace_msg($message_text, $min_level);

Writes a message to the current trace output (as set by the L</trace> method). If a second argument 
is given, the message is only written if the current tracing level is equal to or greater than 
the C<$min_level>.

=head3 B<parse_trace_flag> and B<parse_trace_flags>

  $h->trace($h->parse_trace_flags('SQL|pglibpq'));
  $h->trace($h->parse_trace_flags('1|pgstart'));

  ## Simpler:
  $h->trace('SQL|pglibpq');
  $h->trace('1|pgstart');

  my $value = DBD::Pg->parse_trace_flag('pglibpq');
  DBI->trace($value);

The parse_trace_flags method is used to convert one or more named 
flags to a number which can passed to the L</trace> method.
DBD::Pg currently supports the DBI-specific flag, C<SQL>, 
as well as the ones listed below.

Flags can be combined by using the parse_trace_flags method, 
which simply calls C<parse_trace_flag> on each item and 
combines them.

Sometimes you may wish to turn the tracing on before you connect 
to the database. The second example above shows a way of doing this: 
the call to C<< DBD::Pg->parse_trace_flags >> provides a number than can 
be fed to C<< DBI->trace >> before you create a database handle.

DBD::Pg supports the following trace flags:

=over 4

=item SQL

Outputs all SQL statements. Note that the output provided will not 
necessarily be in a form suitable to passing directly to Postgres, 
as server-side prepared statements are used extensively by DBD::Pg.
For maximum portability of output (but with a potential performance 
hit), use with C<< $dbh->{pg_server_prepare} = 0 >>.

=item DBD

Turns on all non-DBI flags, in other words, only the ones that are specific 
to DBD::Pg (all those below which start with the letters 'pg').

=item pglibpq

Outputs the name of each libpq function (without arguments) immediately 
before running it. This is a good way to trace the flow of your program 
at a low level. This information is also output if the trace level 
is set to 4 or greater.

=item pgstart

Outputs the name of each internal DBD::Pg function, and other information such as 
the function arguments or important global variables, as each function starts. This 
information is also output if the trace level is set to 4 or greater.

=item pgend

Outputs a simple message at the very end of each internal DBD::Pg function. This is also 
output if the trace level is set to 4 or greater.

=item pgprefix

Forces each line of trace output to begin with the string B<C<dbdpg: >>. This helps to 
differentiate it from the normal DBI trace output.

=item pglogin

Outputs a message showing the connection string right before a new database connection 
is attempted, a message when the connection was successful, and a message right after 
the database has been disconnected. Also output if trace level is 5 or greater.

=back

See the L<DBI section on TRACING|DBI/TRACING> for more information.

=head3 B<func>

DBD::Pg uses the C<func> method to support a variety of functions. 
Note that the name of the function comes I<last>, after the arguments.

=over

=item table_attributes

  $attrs = $dbh->func($table, 'table_attributes');

Use of the tables_attributes function is no longer recommended. Instead,
you can use the more portable C<column_info> and C<primary_key> methods
to access the same information.

The table_attributes method returns, for the given table argument, a
reference to an array of hashes, each of which contains the following keys:

  NAME        attribute name
  TYPE        attribute type
  SIZE        attribute size (-1 for variable size)
  NULLABLE    flag nullable
  DEFAULT     default value
  CONSTRAINT  constraint
  PRIMARY_KEY flag is_primary_key
  REMARKS     attribute description

=item pg_lo_creat

  $lobjId = $dbh->pg_lo_creat($mode);

Creates a new large object and returns the object-id. C<$mode> is a bitmask
describing read and write access to the new object. This setting is ignored
since Postgres version 8.1. For backwards compatibility, however, you should 
set a valid mode anyway (see L</pg_lo_open> for a list of valid modes).

Upon failure it returns C<undef>. This function cannot be used if AutoCommit is enabled.

The old way of calling large objects functions is deprecated: $dbh->func(.., 'lo_);

=item pg_lo_open

  $lobj_fd = $dbh->pg_lo_open($lobjId, $mode);

Opens an existing large object and returns an object-descriptor for use in
subsequent C<pg_lo_*> calls. C<$mode> is a bitmask describing read and write
access to the opened object. It may be one of: 

  $dbh->{pg_INV_READ}
  $dbh->{pg_INV_WRITE}
  $dbh->{pg_INV_READ} | $dbh->{pg_INV_WRITE}

C<pg_INV_WRITE> and C<pg_INV_WRITE | pg_INV_READ> modes are identical; in
both modes, the large object can be read from or written to.
Reading from the object will provide the object as written in other committed
transactions, along with any writes performed by the current transaction.
Objects opened with C<pg_INV_READ> cannot be written to. Reading from this
object will provide the stored data at the time of the transaction snapshot
which was active when C<pg_lo_write> was called.

Returns C<undef> upon failure. Note that 0 is a perfectly correct (and common)
object descriptor! This function cannot be used if AutoCommit is enabled.

=item pg_lo_write

  $nbytes = $dbh->pg_lo_write($lobj_fd, $buffer, $len);

Writes C<$len> bytes of c<$buffer> into the large object C<$lobj_fd>. Returns the number
of bytes written and C<undef> upon failure. This function cannot be used if AutoCommit is enabled.

=item pg_lo_read

  $nbytes = $dbh->pg_lo_read($lobj_fd, $buffer, $len);

Reads C<$len> bytes into c<$buffer> from large object C<$lobj_fd>. Returns the number of
bytes read and C<undef> upon failure. This function cannot be used if AutoCommit is enabled.

=item pg_lo_lseek

  $loc = $dbh->pg_lo_lseek($lobj_fd, $offset, $whence);

Changes the current read or write location on the large object
C<$obj_id>. Currently C<$whence> can only be 0 (which is L_SET). Returns the current
location and C<undef> upon failure. This function cannot be used if AutoCommit is enabled.

=item pg_lo_tell

  $loc = $dbh->pg_lo_tell($lobj_fd);

Returns the current read or write location on the large object C<$lobj_fd> and C<undef> upon failure.
This function cannot be used if AutoCommit is enabled.

=item pg_lo_truncate

  $loc = $dbh->pg_lo_truncate($lobj_fd, $len);

Truncates the given large object to the new size. Returns C<undef> on failure, and 0 on success.
This function cannot be used if AutoCommit is enabled.

=item pg_lo_close

  $lobj_fd = $dbh->pg_lo_close($lobj_fd);

Closes an existing large object. Returns true upon success and false upon failure.
This function cannot be used if AutoCommit is enabled.

=item pg_lo_unlink

  $ret = $dbh->pg_lo_unlink($lobjId);

Deletes an existing large object. Returns true upon success and false upon failure.
This function cannot be used if AutoCommit is enabled.

=item pg_lo_import

  $lobjId = $dbh->pg_lo_import($filename);

Imports a Unix file as a large object and returns the object id of the new
object or C<undef> upon failure.

=item pg_lo_import_with_oid

  $lobjId = $dbh->pg_lo_import($filename, $OID);

Same as pg_lo_import, but attempts to use the supplied OID as the 
large object number. If this number is 0, it falls back to the 
behavior of pg_lo_import (which assigns the next available OID).

This is only available when DBD::Pg is compiled against a Postgres 
server version 8.4 or later.

=item pg_lo_export

  $ret = $dbh->pg_lo_export($lobjId, $filename);

Exports a large object into a Unix file. Returns false upon failure, true otherwise.

=item getfd

  $fd = $dbh->func('getfd');

Deprecated, use $dbh->{pg_socket} instead.

=back

=head3 B<private_attribute_info>

  $hashref = $dbh->private_attribute_info();
  $hashref = $sth->private_attribute_info();

Returns a hash of all private attributes used by DBD::Pg, for either 
a database or a statement handle. Currently, all the hash values are undef.

=head1 ATTRIBUTES COMMON TO ALL HANDLES

=head3 B<InactiveDestroy> (boolean)

If set to true, then the L</disconnect> method will not be automatically called when 
the database handle goes out of scope. This is required if you are forking, and even 
then you must tread carefully and ensure that either the parent or the child (but not 
both!) handles all database calls from that point forwards, so that messages from the 
Postgres backend are only handled by one of the processes. If you don't set things up 
properly, you will see messages such as "I<server closed the connection unexpectedly>", 
and "I<message type 0x32 arrived from server while idle>". The best solution is to either 
have the child process reconnect to the database with a fresh database handle, or to 
rewrite your application not to use forking. See the section on L</Asynchronous Queries> 
for a way to have your script continue to work while the database is processing a request.

=head3 B<AutoInactiveDestroy> (boolean)

The InactiveDestroy attribute, described above, needs to be explicitly set in the child 
process after a fork. If the code that performs the fork is in a third party module such 
as Sys::Syslog, this can present a problem. Use AutoInactiveDestroy to get around this 
problem.

=head3 B<RaiseError> (boolean, inherited)

Forces errors to always raise an exception. Although it defaults to off, it is recommended that this 
be turned on, as the alternative is to check the return value of every method (prepare, execute, fetch, etc.) 
manually, which is easy to forget to do.

=head3 B<PrintError> (boolean, inherited)

Forces database errors to also generate warnings, which can then be filtered with methods such as 
locally redefining I<$SIG{__WARN__}> or using modules such as C<CGI::Carp>. This attribute is on 
by default.

=head3 B<ShowErrorStatement> (boolean, inherited)

Appends information about the current statement to error messages. If placeholder information 
is available, adds that as well. Defaults to false.

=head3 B<Warn> (boolean, inherited)

Enables warnings. This is on by default, and should only be turned off in a local block 
for a short a time only when absolutely needed.

=head3 B<Executed> (boolean, read-only)

Indicates if a handle has been executed. For database handles, this value is true after the L</do> method has been called, or 
when one of the child statement handles has issued an L</execute>. Issuing a L</commit> or L</rollback> always resets the 
attribute to false for database handles. For statement handles, any call to L</execute> or its variants will flip the value to 
true for the lifetime of the statement handle.

=head3 B<TraceLevel> (integer, inherited)

Sets the trace level, similar to the L</trace> method. See the sections on 
L</trace> and L<parse_trace_flag|/parse_trace_flag and parse_trace_flags> for more details.

=head3 B<Active> (boolean, read-only)

Indicates if a handle is active or not. For database handles, this indicates if the database has 
been disconnected or not. For statement handles, it indicates if all the data has been fetched yet 
or not. Use of this attribute is not encouraged.

=head3 B<Kids> (integer, read-only)

Returns the number of child processes created for each handle type. For a driver handle, indicates the number 
of database handles created. For a database handle, indicates the number of statement handles created. For 
statement handles, it always returns zero, because statement handles do not create kids.

=head3 B<ActiveKids> (integer, read-only)

Same as C<Kids>, but only returns those that are active.

=head3 B<CachedKids> (hash ref)

Returns a hashref of handles. If called on a database handle, returns all statement handles created by use of the 
C<prepare_cached> method. If called on a driver handle, returns all database handles created by the L</connect_cached> 
method.

=head3 B<ChildHandles> (array ref)

Implemented by DBI, no driver-specific impact.

=head3 B<PrintWarn> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head3 B<HandleError> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head3 B<HandleSetErr> (code ref, inherited)

Implemented by DBI, no driver-specific impact.

=head3 B<ErrCount> (unsigned integer)

Implemented by DBI, no driver-specific impact.

=head3 B<FetchHashKeyName> (string, inherited)

Implemented by DBI, no driver-specific impact.

=head3 B<ChopBlanks> (boolean, inherited)

Supported by DBD::Pg as proposed by DBI. This method is similar to the
SQL function C<RTRIM>.

=head3 B<Taint> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head3 B<TaintIn> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head3 B<TaintOut> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head3 B<Profile> (inherited)

Implemented by DBI, no driver-specific impact.

=head3 B<Type> (scalar)

Returns C<dr> for a driver handle, C<db> for a database handle, and C<st> for a statement handle. 
Should be rarely needed.

=head3 B<LongReadLen>

Not used by DBD::Pg

=head3 B<LongTruncOk>

Not used by DBD::Pg

=head3 B<CompatMode>

Not used by DBD::Pg

=head1 DBI DATABASE HANDLE OBJECTS

=head2 Database Handle Methods

=head3 B<selectall_arrayref>

  $ary_ref = $dbh->selectall_arrayref($sql);
  $ary_ref = $dbh->selectall_arrayref($sql, \%attr);
  $ary_ref = $dbh->selectall_arrayref($sql, \%attr, @bind_values);

Returns a reference to an array containing the rows returned by preparing and executing the SQL string.
See the DBI documentation for full details. 

=head3 B<selectall_hashref>

  $hash_ref = $dbh->selectall_hashref($sql, $key_field);

Returns a reference to a hash containing the rows returned by preparing and executing the SQL string.
See the DBI documentation for full details. 

=head3 B<selectcol_arrayref>

  $ary_ref = $dbh->selectcol_arrayref($sql, \%attr, @bind_values);

Returns a reference to an array containing the first column 
from each rows returned by preparing and executing the SQL string. It is possible to specify exactly 
which columns to return. See the DBI documentation for full details. 

=head3 B<prepare>

  $sth = $dbh->prepare($statement, \%attr);

WARNING: DBD::Pg now (as of version 1.40) uses true prepared statements by sending them 
to the backend to be prepared by the Postgres server. Statements 
that were legal before may no longer work. See below for details.

The prepare method prepares a statement for later execution. PostgreSQL supports 
prepared statements, which enables DBD::Pg to only send the query once, and
simply send the arguments for every subsequent call to L</execute>.
DBD::Pg can use these server-side prepared statements, or it can
just send the entire query to the server each time. The best way
is automatically chosen for each query. This will be sufficient for
most users: keep reading for a more detailed explanation and some
optional flags.

Queries that do not begin with the word "SELECT", "INSERT", 
"UPDATE", or "DELETE" are never sent as server-side prepared statements.

Deciding whether or not to use prepared statements depends on many factors, 
but you can force them to be used or not used by using the 
L<pg_server_prepare|/pg_server_prepare_(boolean)> attribute when calling L</prepare>.
Setting this to false means to never use
prepared statements. Setting pg_server_prepare to true means that prepared
statements should be used whenever possible. This is the default.

The pg_server_prepare attribute can also be set at connection time like so:

  $dbh = DBI->connect($DBNAME, $DBUSER, $DBPASS,
                      { AutoCommit => 0,
                        RaiseError => 1,
                        pg_server_prepare => 0,
                      });

or you may set it after your database handle is created:

  $dbh->{pg_server_prepare} = 1;

To enable it for just one particular statement:

  $sth = $dbh->prepare("SELECT id FROM mytable WHERE val = ?",
                       { pg_server_prepare => 1 });

You can even toggle between the two as you go:

  $sth->{pg_server_prepare} = 1;
  $sth->execute(22);
  $sth->{pg_server_prepare} = 0;
  $sth->execute(44);
  $sth->{pg_server_prepare} = 1;
  $sth->execute(66);

In the above example, the first execute will use the previously prepared statement.
The second execute will not, but will build the query into a single string and send
it to the server. The third one will act like the first and only send the arguments.
Even if you toggle back and forth, a statement is only prepared once.

Using prepared statements is in theory quite a bit faster: not only does the
PostgreSQL backend only have to prepare the query only once, but DBD::Pg no
longer has to worry about quoting each value before sending it to the server.

However, there are some drawbacks. The server cannot always choose the ideal
parse plan because it will not know the arguments before hand. But for most
situations in which you will be executing similar data many times, the default
plan will probably work out well. Programs such as PgBouncer which cache connections 
at a low level should not use prepared statements via DBD::Pg, or must take 
extra care in the application to account for the fact that prepared statements 
are not shared across database connections. Further discussion on this subject is beyond
the scope of this documentation: please consult the pgsql-performance mailing
list, L<http://archives.postgresql.org/pgsql-performance/>

Only certain commands will be sent to a server-side prepare: currently these
include C<SELECT>, C<INSERT>, C<UPDATE>, and C<DELETE>. DBD::Pg uses a simple
naming scheme for the prepared statements themselves: B<dbdpg_XY_Z>, where B<Y> is the current 
PID, B<X> is either 'p' or 'n' (depending on if the PID is a positive or negative 
number), and B<Z> is a number that starts at 1 and increases each time a new statement 
is prepared. This number is tracked at the database handle level, so multiple
statement handles will not collide.

You cannot send more than one command at a time in the same prepare command 
(by separating them with semi-colons) when using server-side prepares.

The actual C<PREPARE> is usually not performed until the first execute is called, due
to the fact that information on the data types (provided by L</bind_param>) may
be provided after the prepare but before the execute.

A server-side prepare may happen before the first L</execute>, but only if the server can
handle the server-side prepare, and the statement contains no placeholders. It will 
also be prepared if the L<pg_prepare_now|/pg_prepare_now_(boolean)> attribute is passed in and set to a true 
value. Similarly, the pg_prepare_now attribute can be set to 0 to ensure that
the statement is B<not> prepared immediately, although the cases in which you would
want this are very rare. Finally, you can set the default behavior of all prepare
statements by setting the pg_prepare_now attribute on the database handle:

  $dbh->{pg_prepare_now} = 1;

The following two examples will be prepared right away:

  $sth->prepare("SELECT 123"); ## no placeholders

  $sth->prepare("SELECT 123, ?", {pg_prepare_now => 1});

The following two examples will NOT be prepared right away:

  $sth->prepare("SELECT 123, ?"); ## has a placeholder

  $sth->prepare("SELECT 123", {pg_prepare_now => 0});

There are times when you may want to prepare a statement yourself. To do this,
simply send the C<PREPARE> statement directly to the server (e.g. with
the L</do> method). Create a statement handle and set the prepared name via
the L<pg_prepare_name|/pg_prepare_name_(string)> attribute. The statement handle can be created with a dummy
statement, as it will not be executed. However, it should have the same
number of placeholders as your prepared statement. Example:

  $dbh->do('PREPARE mystat AS SELECT COUNT(*) FROM pg_class WHERE reltuples < ?');
  $sth = $dbh->prepare('SELECT ?');
  $sth->bind_param(1, 1, SQL_INTEGER);
  $sth->{pg_prepare_name} = 'mystat';
  $sth->execute(123);

The above will run the equivalent of this query on the backend:

  EXECUTE mystat(123);

which is the equivalent of:

  SELECT COUNT(*) FROM pg_class WHERE reltuples < 123;

You can force DBD::Pg to send your query directly to the server by adding
the L<pg_direct|/pg_direct_(boolean)> attribute to your prepare call. This is not recommended,
but is added just in case you need it.

=head4 B<Placeholders>

There are three types of placeholders that can be used in DBD::Pg. The first is
the "question mark" type, in which each placeholder is represented by a single
question mark character. This is the method recommended by the DBI specs and is the most
portable. Each question mark is internally replaced by a "dollar sign number" in the order
in which they appear in the query (important when using L</bind_param>).

The second type of placeholder is "dollar sign numbers". This is the method
that Postgres uses internally and is overall probably the best method to use
if you do not need compatibility with other database systems. DBD::Pg, like
PostgreSQL, allows the same number to be used more than once in the query.
Numbers must start with "1" and increment by one value (but can appear in any order 
within the query). If the same number appears more than once in a query, it is treated as a 
single parameter and all instances are replaced at once. Examples:

Not legal:

  $SQL = 'SELECT count(*) FROM pg_class WHERE relpages > $2'; # Does not start with 1

  $SQL = 'SELECT count(*) FROM pg_class WHERE relpages BETWEEN $1 AND $3'; # Missing 2

Legal:

  $SQL = 'SELECT count(*) FROM pg_class WHERE relpages > $1';

  $SQL = 'SELECT count(*) FROM pg_class WHERE relpages BETWEEN $1 AND $2';

  $SQL = 'SELECT count(*) FROM pg_class WHERE relpages BETWEEN $2 AND $1'; # legal but confusing

  $SQL = 'SELECT count(*) FROM pg_class WHERE relpages BETWEEN $1 AND $2 AND reltuples > $1';

  $SQL = 'SELECT count(*) FROM pg_class WHERE relpages > $1 AND reltuples > $1';

In the final statement above, DBI thinks there is only one placeholder, so this
statement will replace both placeholders:

  $sth->bind_param(1, 2045);

While a simple execute with no bind_param calls requires only a single argument as well:

  $sth->execute(2045);

The final placeholder type is "named parameters" in the format ":foo". While this
syntax is supported by DBD::Pg, its use is discouraged in favor of 
dollar-sign numbers.

The different types of placeholders cannot be mixed within a statement, but you may
use different ones for each statement handle you have. This is confusing at best, so 
stick to one style within your program.

If your queries use operators that contain question marks (e.g. some of the native 
Postgres geometric operators and JSON operators) or array slices (e.g. C<data[100:300]>), 
there are methods to instruct DBD::Pg to not treat some symbols as placeholders. First, you 
may simply add a backslash before the start of a placeholder, and DBD::Pg will strip the 
backslash and not treat the character as a placeholder. 

You can also tell DBD::Pg to ignore any non-dollar sign placeholders by setting the 
L<pg_placeholder_dollaronly|/pg_placeholder_dollaronly_(boolean)> attribute at either the database handle or the statement 
handle level. Examples:

  $dbh->{pg_placeholder_dollaronly} = 1;
  $sth = $dbh->prepare(q{SELECT * FROM mytable WHERE lseg1 ?# lseg2 AND name = $1});
  $sth->execute('segname');

Alternatively, you can set it at prepare time:

  $sth = $dbh->prepare(q{SELECT * FROM mytable WHERE lseg1 ?-| lseg2 AND name = $1},
    {pg_placeholder_dollaronly => 1});
  $sth->execute('segname');

If your queries use array slices but you still want to use question marks as
placeholders, you can tell DBD::Pg to ignore just colon placeholders by setting
the L</pg_placeholder_nocolons> attribute in the same way. Examples:

  $dbh->{pg_placeholder_nocolons} = 1;
  $sth = $dbh->prepare(q{SELECT array[1:2] FROM mytable WHERE id = ?});
  $sth->execute(1);

Again, you may set it param time as well:

  $sth = $dbh->prepare(q{SELECT array[1:2] FROM mytable WHERE id = ?},
    {pg_placeholder_nocolons => 1});
  $sth->execute(1);

It should be noted that placeholders only work when used outside of a literal string context; i.e.,
the following examples will B<not> define/use any placeholders due to appearing inside strings
within the SQL:

  $sth = $dbh->prepare(q{SELECT id FROM mytable WHERE text LIKE '%?'});
  $dbh->do(q{DO LANGUAGE plpgsql $$ BEGIN RAISE NOTICE ?; END $$}, undef, $message);

See the DBI placeholder documentation for more details.

=head3 B<prepare_cached>

  $sth = $dbh->prepare_cached($statement, \%attr);

Implemented by DBI, no driver-specific impact. This method is most useful
when using a server that supports server-side prepares, and you have asked
the prepare to happen immediately via the L<pg_prepare_now|/pg_prepare_now_(boolean)> attribute.

=head3 B<do>

  $rv = $dbh->do($statement);
  $rv = $dbh->do($statement, \%attr);
  $rv = $dbh->do($statement, \%attr, @bind_values);

Prepare and execute a single statement. Returns the number of rows affected if the 
query was successful, returns undef if an error occurred, and returns -1 if the 
number of rows is unknown or not available. Note that this method will return B<0E0> instead
of 0 for 'no rows were affected', in order to always return a true value if no error occurred.

If neither C<\%attr> nor C<@bind_values> is given, the query will be sent directly
to the server without the overhead of internally creating a statement handle and
running prepare and execute, for a measurable speed increase.

Note that an empty statement (a string with no length) will not be passed to
the server; if you want a simple test, use "SELECT 123" or the L</ping> method.

=head3 B<last_insert_id>

  $rv = $dbh->last_insert_id(undef, $schema, $table, undef);
  $rv = $dbh->last_insert_id(undef, $schema, $table, undef, {sequence => $seqname});

Attempts to return the id of the last value to be inserted into a table.
You can either provide a sequence name (preferred) or provide a table
name with optional schema, and DBD::Pg will attempt to find the sequence itself. 
The current value of the sequence is returned by a call to the C<CURRVAL()> 
PostgreSQL function. This will fail if the sequence has not yet been used in the 
current database connection.

If you do not know the name of the sequence, you can provide a table name and
DBD::Pg will attempt to return the correct value. To do this, there must be at
least one column in the table with a C<NOT NULL> constraint, that has a unique
constraint, and which uses a sequence as a default value (either manually, or via
the C<SERIAL> pseudotype or C<GENERATED ... AS IDENTITY>). If more than one column
meets these conditions, the primary key will be used. This involves some
looking up of things in the system table, so DBD::Pg will cache the sequence
name for subsequent calls. If you need to disable this caching for some reason,
(such as the sequence name changing), you can control it by adding C<< pg_cache => 0 >>
to the final (hashref) argument for last_insert_id.

Please keep in mind that this method is far from foolproof, so make your
script use it properly. Specifically, make sure that it is called
immediately after the insert, and that the insert does not add a value
to the column that is using the sequence as a default value. However, because 
we are using sequences, you can be sure that the value you got back has not 
been used by any other process.

Some examples:

  $dbh->do('CREATE SEQUENCE lii_seq START 1');
  $dbh->do(q{CREATE TABLE lii (
    foobar INTEGER NOT NULL UNIQUE DEFAULT nextval('lii_seq'),
    baz VARCHAR)});
  $SQL = 'INSERT INTO lii(baz) VALUES (?)';
  $sth = $dbh->prepare($SQL);
  for (qw(uno dos tres cuatro)) {
    $sth->execute($_);
    my $newid = $dbh->last_insert_id(undef,undef,undef,undef,{sequence=>'lii_seq'});
    print "Last insert id was $newid\n";
  }

If you did not want to worry about the sequence name:

  $dbh->do('CREATE TABLE lii2 (
    foobar SERIAL UNIQUE,
    baz VARCHAR)');
  $SQL = 'INSERT INTO lii2(baz) VALUES (?)';
  $sth = $dbh->prepare($SQL);
  for (qw(uno dos tres cuatro)) {
    $sth->execute($_);
    my $newid = $dbh->last_insert_id(undef,undef,"lii2",undef);
    print "Last insert id was $newid\n";
  }

=head3 B<commit>

  $rv = $dbh->commit;

Issues a COMMIT to the server, indicating that the current transaction is finished and that 
all changes made will be visible to other processes. If AutoCommit is enabled, then 
a warning is given and no COMMIT is issued. Returns true on success, false on error.
See also the section on L</Transactions>.

=head3 B<rollback>

  $rv = $dbh->rollback;

Issues a ROLLBACK to the server, which discards any changes made in the current transaction. If AutoCommit 
is enabled, then a warning is given and no ROLLBACK is issued. Returns true on success, and 
false on error. See also the the section on L</Transactions>.

=head3 B<begin_work>

This method turns on transactions until the next call to L</commit> or L</rollback>, if L<AutoCommit|/AutoCommit_(boolean)> is 
currently enabled. If it is not enabled, calling begin_work will issue an error. Note that the 
transaction will not actually begin until the first statement after begin_work is called.
Example:

  $dbh->{AutoCommit} = 1;
  $dbh->do('INSERT INTO foo VALUES (123)'); ## Changes committed immediately
  $dbh->begin_work();
  ## Not in a transaction yet, but AutoCommit is set to 0

  $dbh->do("INSERT INTO foo VALUES (345)");
  ## DBD::PG actually issues two statements here:
  ## BEGIN;
  ## INSERT INTO foo VALUES (345)
  ## We are now in a transaction

  $dbh->commit();
  ## AutoCommit is now set to 1 again

=head3 B<disconnect>

  $rv = $dbh->disconnect;

Disconnects from the Postgres database. Any uncommitted changes will be rolled back upon disconnection. It's 
good policy to always explicitly call commit or rollback at some point before disconnecting, rather than 
relying on the default rollback behavior.

This method may give warnings about "disconnect invalidates X active statement handle(s)". This means that 
you called C<< $sth->execute() >> but did not finish fetching all the rows from them. To avoid seeing this 
warning, either fetch all the rows or call C<< $sth->finish() >> for each executed statement handle.

If the script exits before disconnect is called (or, more precisely, if the database handle is no longer 
referenced by anything), then the database handle's DESTROY method will call the rollback() and disconnect() 
methods automatically. It is best to explicitly disconnect rather than rely on this behavior.

=head3 B<quote>

  $rv = $dbh->quote($value, $data_type);

This module implements its own C<quote> method. For simple string types, both backslashes 
and single quotes are doubled. You may also quote arrayrefs and receive a string 
suitable for passing into Postgres array columns.

If the value contains backslashes, and the server is version 8.1 or higher, 
then the escaped string syntax will be used (which places a capital E before 
the first single quote). This syntax is always used when quoting bytea values 
on servers 8.1 and higher.

The C<data_type> argument is optional and should be one of the type constants 
exported by DBD::Pg (such as PG_BYTEA). In addition to string, bytea, char, bool, 
and other standard types, the following geometric types are supported: point, line, 
lseg, box, path, polygon, and circle (PG_POINT, PG_LINE, PG_LSEG, PG_BOX, 
PG_PATH, PG_POLYGON, and PG_CIRCLE respectively). To quote a Postgres-specific 
data type, you must use a 'hashref' argument like so:

  my $quotedval = $dbh->quote($value, { pg_type => PG_VARCHAR });

B<NOTE:> The undocumented (and invalid) support for the C<SQL_BINARY> data
type is officially deprecated. Use C<PG_BYTEA> with C<bind_param()> instead:

  $rv = $sth->bind_param($param_num, $bind_value,
                         { pg_type => PG_BYTEA });

=head3 B<quote_identifier>

  $string = $dbh->quote_identifier( $name );
  $string = $dbh->quote_identifier( undef, $schema, $table);

Returns a quoted version of the supplied string, which is commonly a schema, 
table, or column name. The three argument form will return the schema and 
the table together, separated by a dot. Examples:

  print $dbh->quote_identifier('grapefruit'); ## Prints: "grapefruit"

  print $dbh->quote_identifier('juicy fruit'); ## Prints: "juicy fruit"

  print $dbh->quote_identifier(undef, 'public', 'pg_proc');
  ## Prints: "public"."pg_proc"

=head3 B<pg_notifies>

  $ret = $dbh->pg_notifies;

Looks for any asynchronous notifications received and returns either C<undef> 
or a reference to a three-element array consisting of an event name, the PID 
of the backend that sent the NOTIFY command, and the optional payload string. 
Note that this does not check if the connection to the database is still valid first - 
for that, use the c<ping> method. You may need to commit if not in autocommit mode - 
new notices will not be picked up while in the middle of a transaction. An example:

  $dbh->do("LISTEN abc");
  $dbh->do("LISTEN def");

  ## Hang around until we get the message we want
  LISTENLOOP: {
    while (my $notify = $dbh->pg_notifies) {
      my ($name, $pid, $payload) = @$notify;
      print qq{I received notice "$name" from PID $pid, payload was "$payload"\n};
      ## Do something based on the notice received
    }
    $dbh->ping() or die qq{Ping failed!};
    $dbh->commit();
    sleep(5);
    redo;
  }

Payloads will always be an empty string unless you are connecting to a Postgres 
server version 9.0 or higher.

=head3 B<ping>

  $rv = $dbh->ping;

The C<ping> method determines if there is a working connection to an active 
database server. It does this by sending a small query to the server, currently 
B<'DBD::Pg ping test v3.8.0'>. It returns 0 (false) if the connection is not valid, 
otherwise it returns a positive number (true). The value returned indicates the 
current state:

  Value    Meaning
  --------------------------------------------------
    1      Database is idle (not in a transaction)
    2      Database is active, there is a command in progress (usually seen after a COPY command)
    3      Database is idle within a transaction
    4      Database is idle, within a failed transaction

Additional information on why a handle is not valid can be obtained by using the 
L</pg_ping> method.

=head3 B<pg_ping>

  $rv = $dbh->pg_ping;

This is a DBD::Pg-specific extension to the L</ping> method. This will check the 
validity of a database handle in exactly the same way as C<ping>, but instead of 
returning a 0 for an invalid connection, it will return a negative number. So in 
addition to returning the positive numbers documented for C<ping>, it may also 
return the following:

  Value    Meaning
  --------------------------------------------------
   -1      There is no connection to the database at all (e.g. after disconnect)
   -2      An unknown transaction status was returned (e.g. after forking)
   -3      The test query failed (PQexec returned null)
   -4      PQstatus returned a CONNECTION_BAD

=head3 B<get_info>

  $value = $dbh->get_info($info_type);

Supports a very large set (> 250) of the information types, including the minimum 
recommended by DBI.

=head3 B<table_info>

  $sth = $dbh->table_info(undef, $schema, $table, $type);

Returns all tables and views visible to the current user.  The schema and table
arguments will do a C<LIKE> search if a percent sign (C<%>) or an underscore
(C<_>) is detected in the argument. The C<$type> argument accepts any
comma-separated combination of "TABLE", "VIEW", "SYSTEM TABLE", "SYSTEM VIEW",
"MATERIALIZED VIEW", "SYSTEM MATERIALIZED VIEW", "FOREIGN TABLE", "SYSTEM FOREIGN TABLE",
or "LOCAL TEMPORARY".  (Using all is the default action.)

Note that a statement handle is returned, and not a direct list of tables. See
the examples below for ways to handle this.

The following fields are returned:

B<TABLE_CAT>: The name of the database that the table or view is in
(always the current database).

B<TABLE_SCHEM>: The name of the schema that the table or view is in.

B<TABLE_NAME>: The name of the table or view.

B<TABLE_TYPE>: The type of object returned. Will be one of "TABLE", "VIEW",
"MATERIALIZED VIEW", "SYSTEM VIEW", "SYSTEM MATERIALIZED VIEW", "SYSTEM TABLE", 
"FOREIGN TABLE", "SYSTEM FOREIGN TABLE", or "LOCAL TEMPORARY".

The TABLE_SCHEM and TABLE_NAME will be quoted via C<quote_ident()>.

Four additional fields specific to DBD::Pg are returned:

B<pg_schema>: the unquoted name of the schema

B<pg_table>: the unquoted name of the table

B<pg_tablespace_name>: the name of the tablespace the table is in

B<pg_tablespace_location>: the location of the tablespace the table is in

Tables that have not been assigned to a particular tablespace (or views)
will return NULL (C<undef>) for both of the above field.

Rows are returned alphabetically, with all tables first, and then all views.

Examples of use:

  ## Display all tables and views in the public schema:
  $sth = $dbh->table_info('', 'public', undef, undef);
  for my $rel (@{$sth->fetchall_arrayref({})}) {
    print "$rel->{TABLE_TYPE} name is $rel->{TABLE_NAME}\n";
  }


  # Display the schema of all tables named 'foo':
  $sth = $dbh->table_info('', undef, 'foo', 'TABLE');
  for my $rel (@{$sth->fetchall_arrayref({})}) {
    print "Table name is $rel->{TABLE_SCHEM}.$rel->{TABLE_NAME}\n";
  }

=head3 B<column_info>

  $sth = $dbh->column_info( undef, $schema, $table, $column );

Supported by this driver as proposed by DBI with the follow exceptions.
These fields are currently always returned with NULL (C<undef>) values:

   BUFFER_LENGTH
   DECIMAL_DIGITS
   NUM_PREC_RADIX
   SQL_DATA_TYPE
   SQL_DATETIME_SUB
   CHAR_OCTET_LENGTH

Also, six additional non-standard fields are returned:

B<pg_type>: data type with additional info i.e. "character varying(20)"

B<pg_constraint>: holds column constraint definition

B<pg_schema>: the unquoted name of the schema

B<pg_table>: the unquoted name of the table

B<pg_column>: the unquoted name of the column

B<pg_enum_values>: an array reference of allowed values for an enum column

Note that the TABLE_SCHEM, TABLE_NAME, and COLUMN_NAME fields all return 
output wrapped in quote_ident(). If you need the unquoted version, use 
the pg_ fields above.

=head3 B<primary_key_info>

  $sth = $dbh->primary_key_info( undef, $schema, $table, \%attr );

Supported by this driver as proposed by DBI. There are no search patterns allowed, but leaving the 
$schema argument blank will cause the first table found in the schema 
search path to be used. An additional field, "DATA_TYPE", is returned and 
shows the data type for each of the arguments in the "COLUMN_NAME" field.

This method will also return tablespace information for servers that support
tablespaces. See the L</table_info> entry for more information.

The five additional custom fields returned are:

B<pg_tablespace_name>: name of the tablespace, if any

B<pg_tablespace_location>: location of the tablespace

B<pg_schema>: the unquoted name of the schema

B<pg_table>: the unquoted name of the table

B<pg_column>: the unquoted name of the column

In addition to the standard format of returning one row for each column
found for the primary key, you can pass the C<pg_onerow> attribute to force
a single row to be used. If the primary key has multiple columns, the
"KEY_SEQ", "COLUMN_NAME", and "DATA_TYPE" fields will return a comma-delimited
string. If the C<pg_onerow> attribute is set to "2", the fields will be
returned as an arrayref, which can be useful when multiple columns are
involved:

  $sth = $dbh->primary_key_info('', '', 'dbd_pg_test', {pg_onerow => 2});
  if (defined $sth) {
    my $pk = $sth->fetchall_arrayref()->[0];
    print "Table $pk->[2] has a primary key on these columns:\n";
    for (my $x=0; defined $pk->[3][$x]; $x++) {
      print "Column: $pk->[3][$x]  (data type: $pk->[6][$x])\n";
    }
  }

=head3 B<primary_key>

  @key_column_names = $dbh->primary_key(undef, $schema, $table);

Simple interface to the L</primary_key_info> method. Returns a list of the column names that 
comprise the primary key of the specified table. The list is in primary key column sequence 
order. If there is no primary key then an empty list is returned.

=head3 B<foreign_key_info>

  $sth = $dbh->foreign_key_info( $pk_catalog, $pk_schema, $pk_table,
                                 $fk_catalog, $fk_schema, $fk_table );

Supported by this driver as proposed by DBI, using the SQL/CLI variant.
There are no search patterns allowed, but leaving the C<$schema> argument
blank will cause the first table found in the schema search path to be
used. Two additional fields, "UK_DATA_TYPE" and "FK_DATA_TYPE", are returned
to show the data type for the unique and foreign key columns. Foreign
keys that have no named constraint (where the referenced column only has
an unique index) will return C<undef> for the "UK_NAME" field.

=head3 B<statistics_info>

  $sth = $dbh->statistics_info( undef, $schema, $table, $unique_only, $quick );

Returns a statement handle that can be fetched from to give statistics information 
on a specific table and its indexes. The C<$table> argument is mandatory. The 
C<$schema> argument is optional but recommended. The C<$unique_only> argument, if true, 
causes only information about unique indexes to be returned. The C<$quick> argument is 
not used by DBD::Pg. For information on the format of the standard rows returned, please 
see the DBI documentation.

L<DBI section on statistics_info|DBI/statistics_info>

In addition, the following Postgres specific columns are returned:

=over 4

=item pg_expression

Postgres allows indexes on functions and scalar expressions based on one or more columns. This field 
will always be populated if an index, but the lack of an entry in the COLUMN_NAME should indicate 
that this is an index expression.

=back


=head3 B<tables>

  @names = $dbh->tables( undef, $schema, $table, $type, \%attr );

Supported by this driver as proposed by DBI. This method returns all tables
and/or views (including foreign tables and materialized views) which are
visible to the current user: see L</table_info> for more information about
the arguments. The name of the schema appears before the table or view
name. This can be turned off by adding in the C<pg_noprefix> attribute:

  my @tables = $dbh->tables( '', '', 'dbd_pg_test', '', {pg_noprefix => 1} );

=head3 B<type_info_all>

  $type_info_all = $dbh->type_info_all;

Supported by this driver as proposed by DBI. Information is only provided for
SQL datatypes and for frequently used datatypes. The mapping between the
PostgreSQL typename and the SQL92 datatype (if possible) has been done
according to the following table:

  +---------------+------------------------------------+
  | typname       | SQL92                              |
  |---------------+------------------------------------|
  | bool          | BOOL                               |
  | text          | /                                  |
  | bpchar        | CHAR(n)                            |
  | varchar       | VARCHAR(n)                         |
  | int2          | SMALLINT                           |
  | int4          | INT                                |
  | int8          | BIGINT                             |
  | money         | /                                  |
  | float4        | FLOAT(p)   p<7=float4, p<16=float8 |
  | float8        | REAL                               |
  | abstime       | /                                  |
  | reltime       | /                                  |
  | tinterval     | /                                  |
  | date          | /                                  |
  | time          | /                                  |
  | datetime      | /                                  |
  | timespan      | TINTERVAL                          |
  | timestamp     | TIMESTAMP                          |
  +---------------+------------------------------------+

=head3 B<type_info>

  @type_info = $dbh->type_info($data_type);

Returns a list of hash references holding information about one or more variants of $data_type. 
See the DBI documentation for more details.

=head3 B<pg_server_trace>

  $dbh->pg_server_trace($filehandle);

Writes debugging information from the PostgreSQL backend to a file. This is
not related to the DBI L</trace> method and you should not use this method unless
you know what you are doing. If you do enable this, be aware that the file
will grow very large, very quick. To stop logging to the file, use the
L</pg_server_untrace> method. The first argument must be a file handle, not
a filename. Example:

  my $pid = $dbh->{pg_pid};
  my $file = "pgbackend.$pid.debug.log";
  open(my $fh, ">$file") or die qq{Could not open "$file": $!\n};
  $dbh->pg_server_trace($fh);
  ## Run code you want to trace here
  $dbh->pg_server_untrace;
  close($fh);

=head3 B<pg_server_untrace>

  $dbh->pg_server_untrace;

Stop server logging to a previously opened file.

=head3 B<selectrow_array>

  @row_ary = $dbh->selectrow_array($sql);
  @row_ary = $dbh->selectrow_array($sql, \%attr);
  @row_ary = $dbh->selectrow_array($sql, \%attr, @bind_values);

Returns an array of row information after preparing and executing the provided SQL string. The rows are returned 
by calling L</fetchrow_array>. The string can also be a statement handle generated by a previous prepare. Note that 
only the first row of data is returned. If called in a scalar context, only the first column of the first row is 
returned. Because this is not portable, it is not recommended that you use this method in that way.

=head3 B<selectrow_arrayref>

  $ary_ref = $dbh->selectrow_arrayref($statement);
  $ary_ref = $dbh->selectrow_arrayref($statement, \%attr);
  $ary_ref = $dbh->selectrow_arrayref($statement, \%attr, @bind_values);

Exactly the same as L</selectrow_array>, except that it returns a reference to an array, by internal use of 
the L</fetchrow_arrayref> method.

=head3 B<selectrow_hashref>

  $hash_ref = $dbh->selectrow_hashref($sql);
  $hash_ref = $dbh->selectrow_hashref($sql, \%attr);
  $hash_ref = $dbh->selectrow_hashref($sql, \%attr, @bind_values);

Exactly the same as L</selectrow_array>, except that it returns a reference to an hash, by internal use of 
the L</fetchrow_hashref> method.

=head3 B<clone>

  $other_dbh = $dbh->clone();

Creates a copy of the database handle by connecting with the same parameters as the original 
handle, then trying to merge the attributes. See the DBI documentation for complete usage.

=head2 Database Handle Attributes

=head3 B<AutoCommit> (boolean)

Supported by DBD::Pg as proposed by DBI. According to the classification of
DBI, PostgreSQL is a database in which a transaction must be explicitly
started. Without starting a transaction, every change to the database becomes
immediately permanent. The default of AutoCommit is on, but this may change
in the future, so it is highly recommended that you explicitly set it when
calling L</connect>. For details see the notes about L</Transactions>
elsewhere in this document.

=head3 B<pg_bool_tf> (boolean)

DBD::Pg specific attribute. If true, boolean values will be returned
as the characters 't' and 'f' instead of '1' and '0'.

=head3 B<ReadOnly> (boolean)

$dbh->{ReadOnly} = 1;

Specifies if the current database connection should be in read-only mode or not. 
In this mode, changes that change the database are not allowed and will throw 
an error. Note: this method will B<not> work if L</AutoCommit> is true. The 
read-only effect is accomplished by sending a S<SET TRANSACTION READ ONLY> after 
every begin. For more details, please see:

http://www.postgresql.org/docs/current/interactive/sql-set-transaction.html

Please not that this method is not foolproof: there are still ways to update the 
database. Consider this a safety net to catch applications that should not be 
issuing commands such as INSERT, UPDATE, or DELETE.

This method requires DBI version 1.55 or better.

=head3 B<pg_server_prepare> (boolean)

DBD::Pg specific attribute. Indicates if DBD::Pg should attempt to use server-side 
prepared statements. The default value, true, indicates that prepared statements should
be used whenever possible. See the section on the L</prepare> method for more information.

=head3 B<pg_switch_prepared> (integer)

DBD::Pg specific attribute. Indicates when DBD::Pg will internally switch from using 
PQexecParams to PQexecPrepared. In other words, when it will start using server-side 
prepared statements (assuming all other requirements for them are met). The default value, 
2, means that a prepared statement will be prepared and used the second and subsequent 
time execute is called. To always use PQexecPrepared instead of PQexecParams, set 
pg_switch_prepared to 1 (this was the default behavior in earlier versions). 
Setting pg_switch_prepared to 0 will force DBD::Pg to always use PQexecParams.

=head3 B<pg_placeholder_dollaronly> (boolean)

DBD::Pg specific attribute. Defaults to false. When true, question marks inside of statements 
are not treated as L<placeholders|/Placeholders>. Useful for statements that contain unquoted question 
marks, such as geometric operators. Note that you may also simply escape question marks with 
a backslash to prevent them from being treated as placeholders.

=head3 B<pg_placeholder_nocolons> (boolean)

DBD::Pg specific attribute. Defaults to false. When true, colons inside of statements
are not treated as L<placeholders|/Placeholders>. Useful for statements that contain an
array slice. You may also place a backslash directly before the colon to prevent it from 
being treated as a placeholder.

=head3 B<pg_enable_utf8> (integer)

DBD::Pg specific attribute. The behavior of DBD::Pg with regards to this flag has 
changed as of version 3.0.0. The default value for this attribute, -1, indicates 
that the internal Perl C<utf8> flag will be turned on for all strings coming back 
from the database if the client_encoding is set to 'UTF8'. Use of this default 
is highly encouraged. If your code was previously using pg_enable_utf8, you can 
probably remove mention of it entirely.

If this attribute is set to 0, then the internal C<utf8> flag will *never* be 
turned on for returned data, regardless of the current client_encoding. 

If this attribute is set to 1, then the internal C<utf8> flag will *always* 
be turned on for returned data, regardless of the current client_encoding 
(with the exception of bytea data).

Note that the value of client_encoding is only checked on connection time. If 
you change the client_encoding to/from 'UTF8' after connecting, you can set 
pg_enable_utf8 to -1 to force DBD::Pg to read in the new client_encoding and 
act accordingly.

=head3 B<pg_errorlevel> (integer)

DBD::Pg specific attribute. Sets the amount of information returned by the server's 
error messages. Valid entries are 0, 1, and 2. Any other number will be forced to the 
default value of 1.

A value of 0 ("TERSE") will show severity, primary text, and position only
and will usually fit on a single line. A value of 1 ("DEFAULT") will also
show any detail, hint, or context fields. A value of 2 ("VERBOSE") will
show all available information.

=head3 B<pg_lib_version> (integer, read-only)

DBD::Pg specific attribute. Indicates which version of PostgreSQL that 
DBD::Pg was compiled against. In other words, which libraries were used. 
Returns a number with major, minor, and revision together; version 8.1.4 
would be returned as C<80104>.

=head3 B<pg_server_version> (integer, read-only)

DBD::Pg specific attribute. Indicates which version of PostgreSQL that 
the current database handle is connected to. Returns a number with major, 
minor, and revision together; version 8.0.1 would be C<80001>.

=head3 B<Name> (string, read-only)

Returns the name of the current database. This is the same as the DSN, without the 
"dbi:Pg:" part. Before version 2.0.0, this only returned the bare database name 
(e.g. 'foo'). From version 2.0.0 onwards, it returns the more correct 
output (e.g. 'dbname=foo')

=head3 B<Username> (string, read-only)

Returns the name of the user connected to the database.

=head3 B<pg_db> (string, read-only)

DBD::Pg specific attribute. Returns the name of the current database.

=head3 B<pg_user> (string, read-only)

DBD::Pg specific attribute. Returns the name of the user that
connected to the server.

=head3 B<pg_host> (string, read-only)

DBD::Pg specific attribute. Returns the host of the current
server connection. Locally connected hosts will return an empty
string.

=head3 B<pg_port> (integer, read-only)

DBD::Pg specific attribute. Returns the port of the connection to
the server.

=head3 B<pg_socket> (integer, read-only)

DBD::Pg specific attribute. Returns the file description number of
the connection socket to the server.

=head3 B<pg_pass> (string, read-only)

DBD::Pg specific attribute. Returns the password used to connect
to the server.

=head3 B<pg_options> (string, read-only)

DBD::Pg specific attribute. Returns the command-line options passed
to the server. May be an empty string.

=head3 B<pg_default_port> (integer, read-only)

DBD::Pg specific attribute. Returns the default port used if none is
specifically given.

=head3 B<pg_pid> (integer, read-only)

DBD::Pg specific attribute. Returns the process id (PID) of the
backend server process handling the connection.

=head3 B<pg_prepare_now> (boolean)

DBD::Pg specific attribute. Default is off. If true, then the L</prepare> method will 
immediately prepare commands, rather than waiting until the first execute.

=head3 B<pg_expand_array> (boolean)

DBD::Pg specific attribute. Defaults to true. If false, arrays returned from the server will 
not be changed into a Perl arrayref, but remain as a string.

=head3 B<pg_async_status> (integer, read-only)

DBD::Pg specific attribute. Returns the current status of an L<asynchronous|/Asynchronous Queries>
command. 0 indicates no asynchronous command is in progress, 1 indicates that 
an asynchronous command has started and -1 indicated that an asynchronous command 
has been cancelled.

=head3 B<pg_standard_conforming_strings> (boolean, read-only)

DBD::Pg specific attribute. Returns true if the server is currently using 
standard conforming strings. Only available if the target 
server is version 8.2 or better.

=head3 B<pg_INV_READ> (integer, read-only)

Constant to be used for the mode in L</pg_lo_creat> and L</pg_lo_open>.

=head3 B<pg_INV_WRITE> (integer, read-only)

Constant to be used for the mode in L</pg_lo_creat> and L</pg_lo_open>.

=head3 B<Driver> (handle, read-only)

Holds the handle of the parent driver. The only recommended use for this is to find the name 
of the driver using:

  $dbh->{Driver}->{Name}

=head3 B<pg_protocol> (integer, read-only)

DBD::Pg specific attribute. Returns the version of the PostgreSQL server.
If DBD::Pg is unable to figure out the version, it will return a "0". Otherwise,
a "3" is returned.

=head3 B<RowCacheSize>

Not used by DBD::Pg

=head1 DBI STATEMENT HANDLE OBJECTS

=head2 Statement Handle Methods

=head3 B<bind_param>

  $rv = $sth->bind_param($param_num, $bind_value);
  $rv = $sth->bind_param($param_num, $bind_value, $bind_type);
  $rv = $sth->bind_param($param_num, $bind_value, \%attr);

Allows the user to bind a value and/or a data type to a placeholder. This is
especially important when using server-side prepares. See the 
L</prepare> method for more information.

The value of C<$param_num> is a number if using the '?' or '$1' style
placeholders. If using ":foo" style placeholders, the complete name
(e.g. ":foo") must be given. For numeric values, you can either use a
number or use a literal '$1'. See the examples below.

The C<$bind_value> argument is fairly self-explanatory. A value of C<undef> will
bind a C<NULL> to the placeholder. Using C<undef> is useful when you want
to change just the type and will be overwriting the value later.
(Any value is actually usable, but C<undef> is easy and efficient).

The C<\%attr> hash is used to indicate the data type of the placeholder.
The default value is "varchar". If you need something else, you must
use one of the values provided by DBI or by DBD::Pg. To use a SQL value,
modify your "use DBI" statement at the top of your script as follows:

  use DBI qw(:sql_types);

This will import some constants into your script. You can plug those
directly into the L</bind_param> call. Some common ones that you will
encounter are:

  SQL_INTEGER

To use PostgreSQL data types, import the list of values like this:

  use DBD::Pg qw(:pg_types);

You can then set the data types by setting the value of the C<pg_type>
key in the hash passed to L</bind_param>.
The current list of Postgres data types exported is:

 PG_ACLITEM PG_ACLITEMARRAY PG_ANY PG_ANYARRAY PG_ANYELEMENT PG_ANYENUM
 PG_ANYNONARRAY PG_ANYRANGE PG_BIT PG_BITARRAY PG_BOOL PG_BOOLARRAY
 PG_BOX PG_BOXARRAY PG_BPCHAR PG_BPCHARARRAY PG_BYTEA PG_BYTEAARRAY
 PG_CHAR PG_CHARARRAY PG_CID PG_CIDARRAY PG_CIDR PG_CIDRARRAY
 PG_CIRCLE PG_CIRCLEARRAY PG_CSTRING PG_CSTRINGARRAY PG_DATE PG_DATEARRAY
 PG_DATERANGE PG_DATERANGEARRAY PG_EVENT_TRIGGER PG_FDW_HANDLER PG_FLOAT4 PG_FLOAT4ARRAY
 PG_FLOAT8 PG_FLOAT8ARRAY PG_GTSVECTOR PG_GTSVECTORARRAY PG_INDEX_AM_HANDLER PG_INET
 PG_INETARRAY PG_INT2 PG_INT2ARRAY PG_INT2VECTOR PG_INT2VECTORARRAY PG_INT4
 PG_INT4ARRAY PG_INT4RANGE PG_INT4RANGEARRAY PG_INT8 PG_INT8ARRAY PG_INT8RANGE
 PG_INT8RANGEARRAY PG_INTERNAL PG_INTERVAL PG_INTERVALARRAY PG_JSON PG_JSONARRAY
 PG_JSONB PG_JSONBARRAY PG_JSONPATH PG_JSONPATHARRAY PG_LANGUAGE_HANDLER PG_LINE
 PG_LINEARRAY PG_LSEG PG_LSEGARRAY PG_MACADDR PG_MACADDR8 PG_MACADDR8ARRAY
 PG_MACADDRARRAY PG_MONEY PG_MONEYARRAY PG_NAME PG_NAMEARRAY PG_NUMERIC
 PG_NUMERICARRAY PG_NUMRANGE PG_NUMRANGEARRAY PG_OID PG_OIDARRAY PG_OIDVECTOR
 PG_OIDVECTORARRAY PG_OPAQUE PG_PATH PG_PATHARRAY PG_PG_ATTRIBUTE PG_PG_CLASS
 PG_PG_DDL_COMMAND PG_PG_DEPENDENCIES PG_PG_LSN PG_PG_LSNARRAY PG_PG_MCV_LIST PG_PG_NDISTINCT
 PG_PG_NODE_TREE PG_PG_PROC PG_PG_TYPE PG_POINT PG_POINTARRAY PG_POLYGON
 PG_POLYGONARRAY PG_RECORD PG_RECORDARRAY PG_REFCURSOR PG_REFCURSORARRAY PG_REGCLASS
 PG_REGCLASSARRAY PG_REGCONFIG PG_REGCONFIGARRAY PG_REGDICTIONARY PG_REGDICTIONARYARRAY PG_REGNAMESPACE
 PG_REGNAMESPACEARRAY PG_REGOPER PG_REGOPERARRAY PG_REGOPERATOR PG_REGOPERATORARRAY PG_REGPROC
 PG_REGPROCARRAY PG_REGPROCEDURE PG_REGPROCEDUREARRAY PG_REGROLE PG_REGROLEARRAY PG_REGTYPE
 PG_REGTYPEARRAY PG_TABLE_AM_HANDLER PG_TEXT PG_TEXTARRAY PG_TID PG_TIDARRAY
 PG_TIME PG_TIMEARRAY PG_TIMESTAMP PG_TIMESTAMPARRAY PG_TIMESTAMPTZ PG_TIMESTAMPTZARRAY
 PG_TIMETZ PG_TIMETZARRAY PG_TRIGGER PG_TSM_HANDLER PG_TSQUERY PG_TSQUERYARRAY
 PG_TSRANGE PG_TSRANGEARRAY PG_TSTZRANGE PG_TSTZRANGEARRAY PG_TSVECTOR PG_TSVECTORARRAY
 PG_TXID_SNAPSHOT PG_TXID_SNAPSHOTARRAY PG_UNKNOWN PG_UUID PG_UUIDARRAY PG_VARBIT
 PG_VARBITARRAY PG_VARCHAR PG_VARCHARARRAY PG_VOID PG_XID PG_XIDARRAY
 PG_XML PG_XMLARRAY

Data types are "sticky," in that once a data type is set to a certain placeholder,
it will remain for that placeholder, unless it is explicitly set to something
else afterwards. If the statement has already been prepared, and you switch the
data type to something else, DBD::Pg will re-prepare the statement for you before
doing the next execute.

Examples:

  use DBI qw(:sql_types);
  use DBD::Pg qw(:pg_types);

  $SQL = "SELECT id FROM ptable WHERE size > ? AND title = ?";
  $sth = $dbh->prepare($SQL);

  ## Both arguments below are bound to placeholders as "varchar"
  $sth->execute(123, "Merk");

  ## Reset the datatype for the first placeholder to an integer
  $sth->bind_param(1, undef, SQL_INTEGER);

  ## The "undef" bound above is not used, since we supply params to execute
  $sth->execute(123, "Merk");

  ## Set the first placeholder's value and data type
  $sth->bind_param(1, 234, { pg_type => PG_TIMESTAMP });

  ## Set the second placeholder's value and data type.
  ## We don't send a third argument, so the default "varchar" is used
  $sth->bind_param('$2', "Zool");

  ## We realize that the wrong data type was set above, so we change it:
  $sth->bind_param('$1', 234, { pg_type => SQL_INTEGER });

  ## We also got the wrong value, so we change that as well.
  ## Because the data type is sticky, we don't need to change it
  $sth->bind_param(1, 567);

  ## This executes the statement with 567 (integer) and "Zool" (varchar)
  $sth->execute();

=head3 B<bind_param_inout>

  $rv = $sth->bind_param_inout($param_num, \$scalar, 0);


Experimental support for this feature is provided. The first argument to 
bind_param_inout should be a placeholder number. The second argument 
should be a reference to a scalar variable in your script. The third argument 
is not used and should simply be set to 0. Note that what this really does is 
assign a returned column to the variable, in the order in which the column 
appears. For example:

  my $foo = 123;
  $sth = $dbh->prepare("SELECT 1+?::int");
  $sth->bind_param_inout(1, \$foo, 0);
  $foo = 222;
  $sth->execute(444);
  $sth->fetch;

The above will cause $foo to have a new value of "223" after the final fetch.
Note that the variables bound in this manner are very sticky, and will trump any 
values passed in to execute. This is because the binding is done as late as possible, 
at the execute() stage, allowing the value to be changed between the time it was bound 
and the time the query is executed. Thus, the above execute is the same as:

  $sth->execute();

=head3 B<bind_param_array>


  $rv = $sth->bind_param_array($param_num, $array_ref_or_value)
  $rv = $sth->bind_param_array($param_num, $array_ref_or_value, $bind_type)
  $rv = $sth->bind_param_array($param_num, $array_ref_or_value, \%attr)

Binds an array of values to a placeholder, so that each is used in turn by a call 
to the L</execute_array> method.

=head3 B<execute>

  $rv = $sth->execute(@bind_values);

Executes a previously prepared statement. In addition to C<UPDATE>, C<DELETE>,
C<INSERT> statements, for which it returns always the number of affected rows,
the C<execute> method can also be used for C<SELECT ... INTO table> statements.

The "prepare/bind/execute" process has changed significantly for PostgreSQL
servers 7.4 and later: please see the C<prepare()> and C<bind_param()> entries for
much more information.

Setting one of the bind_values to "undef" is the equivalent of setting the value 
to NULL in the database. Setting the bind_value to $DBDPG_DEFAULT is equivalent 
to sending the literal string 'DEFAULT' to the backend. Note that using this 
option will force server-side prepares off until such time as PostgreSQL 
supports using DEFAULT in prepared statements.

DBD::Pg also supports passing in arrays to execute: simply pass in an arrayref, 
and DBD::Pg will flatten it into a string suitable for input on the backend.

If you are using Postgres version 8.2 or greater, you can also use any of the 
fetch methods to retrieve the values of a C<RETURNING> clause after you execute 
an C<UPDATE>, C<DELETE>, or C<INSERT>. For example:

  $dbh->do(q{CREATE TABLE abc (id SERIAL, country TEXT)});
  $SQL = q{INSERT INTO abc (country) VALUES (?) RETURNING id};
  $sth = $dbh->prepare($SQL);
  $sth->execute('France');
  $countryid = $sth->fetch()->[0];
  $sth->execute('New Zealand');
  $countryid = $sth->fetch()->[0];

=head3 B<execute_array>

  $tuples = $sth->execute_array() or die $sth->errstr;
  $tuples = $sth->execute_array(\%attr) or die $sth->errstr;
  $tuples = $sth->execute_array(\%attr, @bind_values) or die $sth->errstr;

  ($tuples, $rows) = $sth->execute_array(\%attr) or die $sth->errstr;
  ($tuples, $rows) = $sth->execute_array(\%attr, @bind_values) or die $sth->errstr;

Execute a prepared statement once for each item in a passed-in hashref, or items that 
were previously bound via the L</bind_param_array> method. See the DBI documentation 
for more details.

=head3 B<execute_for_fetch>

  $tuples = $sth->execute_for_fetch($fetch_tuple_sub);
  $tuples = $sth->execute_for_fetch($fetch_tuple_sub, \@tuple_status);

  ($tuples, $rows) = $sth->execute_for_fetch($fetch_tuple_sub);
  ($tuples, $rows) = $sth->execute_for_fetch($fetch_tuple_sub, \@tuple_status);

Used internally by the L</execute_array> method, and rarely used directly. See the 
DBI documentation for more details.

=head3 B<fetchrow_arrayref>

  $ary_ref = $sth->fetchrow_arrayref;

Fetches the next row of data from the statement handle, and returns a reference to an array 
holding the column values. Any columns that are NULL are returned as undef within the array.

If there are no more rows or if an error occurs, then this method return undef. You should 
check C<< $sth->err >> afterwards (or use the L<RaiseError|/RaiseError_(boolean,_inherited)> attribute) to discover if the undef returned 
was due to an error.

Note that the same array reference is returned for each fetch, so don't store the reference and 
then use it after a later fetch. Also, the elements of the array are also reused for each row, 
so take care if you want to take a reference to an element. See also L</bind_columns>.

=head3 B<fetchrow_array>

  @ary = $sth->fetchrow_array;

Similar to the L</fetchrow_arrayref> method, but returns a list of column information rather than 
a reference to a list. Do not use this in a scalar context.

=head3 B<fetchrow_hashref>

  $hash_ref = $sth->fetchrow_hashref;
  $hash_ref = $sth->fetchrow_hashref($name);

Fetches the next row of data and returns a hashref containing the name of the columns as the keys 
and the data itself as the values. Any NULL value is returned as an undef value.

If there are no more rows or if an error occurs, then this method return undef. You should 
check C<< $sth->err >> afterwards (or use the L<RaiseError|/RaiseError_(boolean,_inherited)> attribute) to discover if the undef returned 
was due to an error.

The optional C<$name> argument should be either C<NAME>, C<NAME_lc> or C<NAME_uc>, and indicates 
what sort of transformation to make to the keys in the hash.

=head3 B<fetchall_arrayref>

  $tbl_ary_ref = $sth->fetchall_arrayref();
  $tbl_ary_ref = $sth->fetchall_arrayref( $slice );
  $tbl_ary_ref = $sth->fetchall_arrayref( $slice, $max_rows );

Returns a reference to an array of arrays that contains all the remaining rows to be fetched from the 
statement handle. If there are no more rows, an empty arrayref will be returned. If an error occurs, 
the data read in so far will be returned. Because of this, you should always check C<< $sth->err >> after 
calling this method, unless L<RaiseError|/RaiseError_(boolean,_inherited)> has been enabled.

If C<$slice> is an array reference, fetchall_arrayref uses the L</fetchrow_arrayref> method to fetch each 
row as an array ref. If the C<$slice> array is not empty then it is used as a slice to select individual 
columns by perl array index number (starting at 0, unlike column and parameter numbers which start at 1).

With no parameters, or if $slice is undefined, fetchall_arrayref acts as if passed an empty array ref.

If C<$slice> is a hash reference, fetchall_arrayref uses L</fetchrow_hashref> to fetch each row as a hash reference.

See the DBI documentation for a complete discussion.

=head3 B<fetchall_hashref>

  $hash_ref = $sth->fetchall_hashref( $key_field );

Returns a hashref containing all rows to be fetched from the statement handle. See the DBI documentation for 
a full discussion.

=head3 B<finish>

  $rv = $sth->finish;

Indicates to DBI that you are finished with the statement handle and are not going to use it again. Only needed 
when you have not fetched all the possible rows.

=head3 B<rows>

  $rv = $sth->rows;

Returns the number of rows returned by the last query. In contrast to many other DBD modules, 
the number of rows is available immediately after calling C<< $sth->execute >>. Note that 
the L</execute> method itself returns the number of rows itself, which means that this 
method is rarely needed.

=head3 B<bind_col>

  $rv = $sth->bind_col($column_number, \$var_to_bind);
  $rv = $sth->bind_col($column_number, \$var_to_bind, \%attr );
  $rv = $sth->bind_col($column_number, \$var_to_bind, $bind_type );

Binds a Perl variable and/or some attributes to an output column of a SELECT statement. 
Column numbers count up from 1. You do not need to bind output columns in order to fetch data.

See the DBI documentation for a discussion of the optional parameters C<\%attr> and C<$bind_type>

=head3 B<bind_columns>

  $rv = $sth->bind_columns(@list_of_refs_to_vars_to_bind);

Calls the L</bind_col> method for each column in the SELECT statement, using the supplied list.

=head3 B<dump_results>

  $rows = $sth->dump_results($maxlen, $lsep, $fsep, $fh);

Fetches all the rows from the statement handle, calls C<DBI::neat_list> for each row, and 
prints the results to C<$fh> (which defaults to F<STDOUT>). Rows are separated by C<$lsep> (which defaults 
to a newline). Columns are separated by C<$fsep> (which defaults to a comma). The C<$maxlen> controls 
how wide the output can be, and defaults to 35.

This method is designed as a handy utility for prototyping and testing queries. Since it uses 
"neat_list" to format and edit the string for reading by humans, it is not recommended 
for data transfer applications.

=head3 B<blob_read>

  $blob = $sth->blob_read($id, $offset, $len);

Supported by DBD::Pg. This method is implemented by DBI but not
currently documented by DBI, so this method might change.

This method seems to be heavily influenced by the current implementation of
blobs in Oracle. Nevertheless we try to be as compatible as possible. Whereas
Oracle suffers from the limitation that blobs are related to tables and every
table can have only one blob (datatype LONG), PostgreSQL handles its blobs
independent of any table by using so-called object identifiers. This explains
why the C<blob_read> method is blessed into the STATEMENT package and not part of
the DATABASE package. Here the field parameter has been used to handle this
object identifier. The offset and len parameters may be set to zero, in which
case the whole blob is fetched at once.

See also the PostgreSQL-specific functions concerning blobs, which are
available via the C<func> interface.

For further information and examples about blobs, please read the chapter
about Large Objects in the PostgreSQL Programmer's Guide at
L<http://www.postgresql.org/docs/current/static/largeobjects.html>.

=head3 B<pg_canonical_ids>

  $data = $sth->pg_canonical_ids;

DBD::Pg specific method. It returns Oid of table and position in table for
every column in result set.

Returns array of arrays with F<Table Oid> and F<Column Position> for every
column in result set or undef if current column is not a simple reference.

=head3 B<pg_canonical_names>

  $data = $sth->pg_canonical_names;

DBD::Pg specific method. It returns array of original (or canonical) names
(from where this data is actually came from) of columns in
F<Schema>.F<Table>.F<Column> format or undef if current column is not a
simple reference.

Note that this method is quite slow because it need additional information from
server for every column that is simple reference. Consider to use L</pg_canonical_ids>
instead.

=head2 Statement Handle Attributes

=head3 B<NUM_OF_FIELDS> (integer, read-only)

Returns the number of columns returned by the current statement. A number will only be returned for 
SELECT statements, for SHOW statements (which always return C<1>), and for INSERT, 
UPDATE, and DELETE statements which contain a RETURNING clause.
This method returns undef if called before C<execute()>.

=head3 B<NUM_OF_PARAMS> (integer, read-only)

Returns the number of placeholders in the current statement.

=head3 B<NAME> (arrayref, read-only)

Returns an arrayref of column names for the current statement. This 
method will only work for SELECT statements, for SHOW statements, and for 
INSERT, UPDATE, and DELETE statements which contain a RETURNING clause.
This method returns undef if called before C<execute()>.

=head3 B<NAME_lc> (arrayref, read-only)

The same as the C<NAME> attribute, except that all column names are forced to lower case.

=head3 B<NAME_uc>  (arrayref, read-only)

The same as the C<NAME> attribute, except that all column names are forced to upper case.

=head3 B<NAME_hash> (hashref, read-only)

Similar to the C<NAME> attribute, but returns a hashref of column names instead of an arrayref. The names of the columns 
are the keys of the hash, and the values represent the order in which the columns are returned, starting at 0.
This method returns undef if called before C<execute()>.

=head3 B<NAME_lc_hash> (hashref, read-only)

The same as the C<NAME_hash> attribute, except that all column names are forced to lower case.

=head3 B<NAME_uc_hash> (hashref, read-only)

The same as the C<NAME_hash> attribute, except that all column names are forced to lower case.

=head3 B<TYPE> (arrayref, read-only)

Returns an arrayref indicating the data type for each column in the statement. 
This method returns undef if called before C<execute()>.

=head3 B<PRECISION> (arrayref, read-only)

Returns an arrayref of integer values for each column returned by the statement. 
The number indicates the precision for C<NUMERIC> columns, the size in number of 
characters for C<CHAR> and C<VARCHAR> columns, and for all other types of columns 
it returns the number of I<bytes>.
This method returns undef if called before C<execute()>.

=head3 B<SCALE> (arrayref, read-only)

Returns an arrayref of integer values for each column returned by the statement. The number 
indicates the scale of the that column. The only type that will return a value is C<NUMERIC>.
This method returns undef if called before C<execute()>.

=head3 B<NULLABLE> (arrayref, read-only)

Returns an arrayref of integer values for each column returned by the statement. The number 
indicates if the column is nullable or not. 0 = not nullable, 1 = nullable, 2 = unknown. 
This method returns undef if called before C<execute()>.

=head3 B<Database> (dbh, read-only)

Returns the database handle this statement handle was created from.

=head3 B<ParamValues> (hash ref, read-only)

Returns a reference to a hash containing the values currently bound to placeholders. If the "named parameters" 
type of placeholders are being used (such as ":foo"), then the keys of the hash will be the names of the 
placeholders (without the colon). If the "dollar sign numbers" type of placeholders are being used, the keys of the hash will 
be the numbers, without the dollar signs. If the "question mark" type is used, integer numbers will be returned, 
starting at one and increasing for every placeholder.

If this method is called before L</execute>, the literal values passed in are returned. If called after 
L</execute>, then the quoted versions of the values are returned.

=head3 B<ParamTypes> (hash ref, read-only)

Returns a reference to a hash containing the type names currently bound to placeholders. The keys 
are the same as returned by the ParamValues method. The values are hashrefs containing a single key value 
pair, in which the key is either 'TYPE' if the type has a generic SQL equivalent, and 'pg_type' if the type can 
only be expressed by a Postgres type. The value is the internal number corresponding to the type originally 
passed in. (Placeholders that have not yet been bound will return undef as the value). This allows the output of 
ParamTypes to be passed back to the L</bind_param> method.

=head3 B<Statement> (string, read-only)

Returns the statement string passed to the most recent "prepare" method called in this database handle, even if that method
failed. This is especially useful where "RaiseError" is enabled and the exception handler checks $@ and sees that a C<prepare>
method call failed.

=head3 B<pg_current_row> (integer, read-only)

DBD::Pg specific attribute. Returns the number of the tuple (row) that was
most recently fetched. Returns zero before and after fetching is performed.

=head3 B<pg_numbound> (integer, read-only)

DBD::Pg specific attribute. Returns the number of placeholders
that are currently bound (via bind_param).

=head3 B<pg_bound> (hashref, read-only)

DBD::Pg specific attribute. Returns a hash of all named placeholders. The
key is the name of the placeholder, and the value is a 0 or a 1, indicating if
the placeholder has been bound yet (e.g. via bind_param)

=head3 B<pg_size> (arrayref, read-only)

DBD::Pg specific attribute. It returns a reference to an array of integer
values for each column. The integer shows the size of the column in
bytes. Variable length columns are indicated by -1.

=head3 B<pg_type> (arrayref, read-only)

DBD::Pg specific attribute. It returns a reference to an array of strings
for each column. The string shows the name of the data_type.

=head3 B<pg_segments> (arrayref, read-only)

DBD::Pg specific attribute. Returns an arrayref of the query split on the 
placeholders.

=head3 B<pg_oid_status> (integer, read-only)

DBD::Pg specific attribute. It returns the OID of the last INSERT command.

=head3 B<pg_cmd_status> (integer, read-only)

DBD::Pg specific attribute. It returns the type of the last
command. Possible types are: "INSERT", "DELETE", "UPDATE", "SELECT".

=head3 B<pg_direct> (boolean)

DBD::Pg specific attribute. Default is false. If true, the query is passed 
directly to the backend without parsing for placeholders.

=head3 B<pg_prepare_now> (boolean)

DBD::Pg specific attribute. Default is off. If true, the query will be immediately 
prepared, rather than waiting for the L</execute> call.

=head3 B<pg_prepare_name> (string)

DBD::Pg specific attribute. Specifies the name of the prepared statement to use for this 
statement handle. Not normally needed, see the section on the L</prepare> method for 
more information.

=head3 B<pg_server_prepare> (boolean)

DBD::Pg specific attribute. Indicates if DBD::Pg should attempt to use server-side 
prepared statements for this statement handle. The default value, true, indicates that prepared
statements should be used whenever possible. See the section on the L</prepare> method for 
more information.

=head3 B<pg_switch_prepared> (integer)

DBD::Pg specific attribute. Indicates when DBD::Pg will internally switch from using 
PQexecParams to PQexecPrepared. In other words, when it will start using server-side 
prepared statements (assuming all other requirements for them are met). The default value, 
2, means that a prepared statement will be prepared and used the second and subsequent 
time execute is called. To always use PQexecPrepared instead of PQexecParams, set 
pg_switch_prepared to 1 (this was the default behavior in earlier versions). 
Setting pg_switch_prepared to 0 will force DBD::Pg to always use PQexecParams.

=head3 B<pg_placeholder_dollaronly> (boolean)

DBD::Pg specific attribute. Defaults to false. When true, question marks inside of the query 
being prepared are not treated as placeholders. Useful for statements that contain unquoted question 
marks, such as geometric operators. Note that you may also simply escape question marks with 
a backslash to prevent them from being treated as placeholders.

=head3 B<pg_placeholder_nocolons> (boolean)

DBD::Pg specific attribute. Defaults to false. When true, colons inside of statements
are not treated as L<placeholders|/Placeholders>. Useful for statements that contain an
array slice. You may also place a backslash directly before the colon to prevent it from 
being treated as a placeholder.

=head3 B<pg_async> (integer)

DBD::Pg specific attribute. Indicates the current behavior for asynchronous queries. See the section 
on L</Asynchronous Constants> for more information.

=head3 B<pg_async_status> (integer, read-only)

DBD::Pg specific attribute. Returns the current status of an L<asynchronous|/Asynchronous Queries>
command. 0 indicates no asynchronous command is in progress, 1 indicates that 
an asynchronous command has started and -1 indicated that an asynchronous command 
has been cancelled.

=head3 B<RowsInCache>

Not used by DBD::Pg

=head3 B<RowCache>

Not used by DBD::Pg

=head3 B<CursorName>

Not used by DBD::Pg. See the note about L</Cursors> elsewhere in this document.

=head1 FURTHER INFORMATION

=head2 Encoding

DBD::Pg has extensive support for a client_encoding of UTF-8, and most 
things like encoding and decoding should happen automatically. If you are 
using a different encoding, you will need do the encoding and decoding 
yourself. For this reason, it is highly recommended to always use a 
client_encoding of UTF-8. The server_encoding can be anything, and no 
recommendations are made there, other than avoid SQL_ASCII whenever 
possible.

=head2 Transactions

Transaction behavior is controlled via the L</AutoCommit> attribute. For a
complete definition of C<AutoCommit> please refer to the DBI documentation.

According to the DBI specification the default for C<AutoCommit> is a true
value. In this mode, any change to the database becomes valid immediately. Any
C<BEGIN>, C<COMMIT> or C<ROLLBACK> statements will be rejected. Note that 
preparing a statement does not always contact the server, as the actual 
C<PREPARE> is usually postponed until the first call to L</execute>.

=head2 Savepoints

PostgreSQL version 8.0 introduced the concept of savepoints, which allows 
transactions to be rolled back to a certain point without affecting the 
rest of the transaction. DBD::Pg encourages using the following methods to 
control savepoints:

=head3 C<pg_savepoint>

Creates a savepoint. This will fail unless you are inside of a transaction. The 
only argument is the name of the savepoint. Note that PostgreSQL DOES allow 
multiple savepoints with the same name to exist.

  $dbh->pg_savepoint("mysavepoint");

=head3 C<pg_rollback_to>

Rolls the database back to a named savepoint, discarding any work performed after 
that point. If more than one savepoint with that name exists, rolls back to the 
most recently created one.

  $dbh->pg_rollback_to("mysavepoint");

=head3 C<pg_release>

Releases (or removes) a named savepoint. If more than one savepoint with that name 
exists, it will only destroy the most recently created one. Note that all savepoints 
created after the one being released are also destroyed.

  $dbh->pg_release("mysavepoint");

=head2 Asynchronous Queries

It is possible to send a query to the backend and have your script do other work while the query is 
running on the backend. Both queries sent by the L</do> method, and by the L</execute> method can be 
sent asynchronously. The basic usage is as follows:

  use DBD::Pg ':async';

  print "Async do() example:\n";
  $dbh->do("SELECT long_running_query()", {pg_async => PG_ASYNC});
  do_something_else();
  {
    if ($dbh->pg_ready()) {
      $res = $dbh->pg_result();
      print "Result of do(): $res\n";
    }
    print "Query is still running...\n";
    if (cancel_request_received) {
      $dbh->pg_cancel();
    }
    sleep 1;
    redo;
  }

  print "Async prepare/execute example:\n";
  $sth = $dbh->prepare("SELECT long_running_query(1)", {pg_async => PG_ASYNC});
  $sth->execute();

  ## Changed our mind, cancel and run again:
  $sth = $dbh->prepare("SELECT 678", {pg_async => PG_ASYNC + PG_OLDQUERY_CANCEL});
  $sth->execute();

  do_something_else();

  if (!$sth->pg_ready) {
    do_another_thing();
  }

  ## We wait until it is done, and get the result:
  $res = $dbh->pg_result();

=head3 Asynchronous Constants

There are currently three asynchronous constants exported by DBD::Pg. You can import all of them by putting 
either of these at the top of your script:

  use DBD::Pg;

  use DBD::Pg ':async';

You may also use the numbers instead of the constants, but using the constants is recommended as it 
makes your script more readable.

=over 4

=item PG_ASYNC

This is a constant for the number 1. It is passed to either the L</do> or the L</prepare> method as a value 
to the pg_async key and indicates that the query should be sent asynchronously.

=item PG_OLDQUERY_CANCEL

This is a constant for the number 2. When passed to either the L</do> or the L</prepare> method, it causes any 
currently running asynchronous query to be cancelled and rolled back. It has no effect if no asynchronous 
query is currently running.

=item PG_OLDQUERY_WAIT

This is a constant for the number 4. When passed to either the L</do> or the L</prepare> method, it waits for any 
currently running asynchronous query to complete. It has no effect if there is no asynchronous query currently running.

=back

=head3 Asynchronous Methods

=over 4

=item B<pg_cancel>

This database-level method attempts to cancel any currently running asynchronous query. It returns true if 
the cancel succeeded, and false otherwise. Note that a query that has finished before this method is executed 
will also return false. B<WARNING>: a successful cancellation may leave the database in an unusable state, 
so you may need to ROLLBACK or ROLLBACK TO a savepoint. As of version 2.17.0 of DBD::Pg, rollbacks are 
not done automatically.

  $result = $dbh->pg_cancel();

=item B<pg_ready>

This method can be called as a database handle method or (for convenience) as a statement handle method. Both simply 
see if a previously issued asynchronous query has completed yet. It returns true if the statement has finished, in which 
case you should then call the L</pg_result> method. Calls to C<pg_ready()> should only be used when you have other 
things to do while the query is running. If you simply want to wait until the query is done, do not call pg_ready()
over and over, but simply call the pg_result() method.

  my $time = 0;
  while (!$dbh->pg_ready) {
    print "Query is still running. Seconds: $time\n";
    $time++;
    sleep 1;
  }
  $result = $dbh->pg_result;

=item B<pg_result>

This database handle method returns the results of a previously issued asynchronous query. If the query is still 
running, this method will wait until it has finished. The result returned is the number of rows: the same thing 
that would have been returned by the asynchronous L</do> or L</execute> if it had been called without an asynchronous flag.

  $result = $dbh->pg_result;

=back

=head3 Asynchronous Examples

Here are some working examples of asynchronous queries. Note that we'll use the B<pg_sleep> function to emulate a 
long-running query.

  use strict;
  use warnings;
  use Time::HiRes 'sleep';
  use DBD::Pg ':async';

  my $dbh = DBI->connect('dbi:Pg:dbname=postgres', 'postgres', '', {AutoCommit=>0,RaiseError=>1});

  ## Kick off a long running query on the first database:
  my $sth = $dbh->prepare("SELECT pg_sleep(?)", {pg_async => PG_ASYNC});
  $sth->execute(5);

  ## While that is running, do some other things
  print "Your query is processing. Thanks for waiting\n";
  check_on_the_kids(); ## Expensive sub, takes at least three seconds.

  while (!$dbh->pg_ready) {
    check_on_the_kids();
    ## If the above function returns quickly for some reason, we add a small sleep
    sleep 0.1;
  }

  print "The query has finished. Gathering results\n";
  my $result = $sth->pg_result;
  print "Result: $result\n";
  my $info = $sth->fetchall_arrayref();

Without asynchronous queries, the above script would take about 8 seconds to run: five seconds waiting 
for the execute to finish, then three for the check_on_the_kids() function to return. With asynchronous 
queries, the script takes about 6 seconds to run, and gets in two iterations of check_on_the_kids in 
the process.

Here's an example showing the ability to cancel a long-running query. Imagine two slave databases in 
different geographic locations over a slow network. You need information as quickly as possible, so 
you query both at once. When you get an answer, you tell the other one to stop working on your query, 
as you don't need it anymore.

  use strict;
  use warnings;
  use Time::HiRes 'sleep';
  use DBD::Pg ':async';

  my $dbhslave1 = DBI->connect('dbi:Pg:dbname=postgres;host=slave1', 'postgres', '', {AutoCommit=>0,RaiseError=>1});
  my $dbhslave2 = DBI->connect('dbi:Pg:dbname=postgres;host=slave2', 'postgres', '', {AutoCommit=>0,RaiseError=>1});

  $SQL = "SELECT count(*) FROM largetable WHERE flavor='blueberry'";

  my $sth1 = $dbhslave1->prepare($SQL, {pg_async => PG_ASYNC});
  my $sth2 = $dbhslave2->prepare($SQL, {pg_async => PG_ASYNC});

  $sth1->execute();
  $sth2->execute();

  my $winner;
  while (!defined $winner) {
    if ($sth1->pg_ready) {
      $winner = 1;
    }
    elsif ($sth2->pg_ready) {
      $winner = 2;
    }
    Time::HiRes::sleep 0.05;
  }

  my $count;
  if ($winner == 1) {
    $sth2->pg_cancel();
    $sth1->pg_result();
    $count = $sth1->fetchall_arrayref()->[0][0];
  }
  else {
    $sth1->pg_cancel();
    $sth2->pg_result();
    $count = $sth2->fetchall_arrayref()->[0][0];
  }

=head2 Array support

DBD::Pg allows arrays (as arrayrefs) to be passed in to both 
the L</quote> and the L</execute> methods. In both cases, the array is 
flattened into a string representing a Postgres array.

When fetching rows from a table that contains a column with an 
array type, the result will be passed back to your script as an arrayref.

To turn off the automatic parsing of returned arrays into arrayrefs, 
you can set the attribute L<pg_expand_array|/pg_expand_array_(boolean)>, which is true by default.

  $dbh->{pg_expand_array} = 0;


=head2 COPY support

DBD::Pg allows for quick (bulk) reading and storing of data by using 
the B<COPY> command. The basic process is to use C<< $dbh->do >> to issue a 
COPY command, and then to either add rows using L</pg_putcopydata>, or to 
read them by using L</pg_getcopydata>.

The first step is to put the server into "COPY" mode. This is done by 
sending a complete COPY command to the server, by using the L</do> method. 
For example:

  $dbh->do("COPY foobar FROM STDIN");

This would tell the server to enter a COPY IN mode (yes, that's confusing, but 
the I<mode> is COPY IN because of the I<command> COPY FROM). It is now ready to 
receive information via the L</pg_putcopydata> method. The complete syntax of the 
COPY command is more complex and not documented here: the canonical 
PostgreSQL documentation for COPY can be found at:

http://www.postgresql.org/docs/current/static/sql-copy.html

Once a COPY command has been issued, no other SQL commands are allowed 
until L</pg_putcopyend> has been issued (for COPY FROM), or the final 
L</pg_getcopydata> has been called (for COPY TO).

Note: All other COPY methods (pg_putline, pg_getline, etc.) are now 
heavily deprecated in favor of the pg_getcopydata, pg_putcopydata, and 
pg_putcopyend methods.

=head3 B<pg_getcopydata>

Used to retrieve data from a table after the server has been put into a 
COPY OUT mode by calling "COPY tablename TO STDOUT". Data is always returned 
one data row at a time. The first argument to pg_getcopydata 
is the variable into which the data will be stored (this variable should not 
be undefined, or it may throw a warning, although it may be a reference). The 
pg_getcopydata method returns a number greater than 1 indicating the new size of 
the variable, or a -1 when the COPY has finished. Once a -1 has been returned, no 
other action is necessary, as COPY mode will have already terminated. Example:

  $dbh->do("COPY mytable TO STDOUT");
  my @data;
  my $x=0;
  1 while $dbh->pg_getcopydata($data[$x++]) >= 0;

There is also a variation of this method called B<pg_getcopydata_async>, which, 
as the name suggests, returns immediately. The only difference from the original 
method is that this version may return a 0, indicating that the row is not 
ready to be delivered yet. When this happens, the variable has not been changed, 
and you will need to call the method again until you get a non-zero result.
(Data is still always returned one data row at a time.)

=head3 B<pg_putcopydata>

Used to put data into a table after the server has been put into COPY IN mode 
by calling "COPY tablename FROM STDIN". The only argument is the data you want 
inserted. Issue a pg_putcopyend() when you have added all your rows.

The default delimiter is a tab character, but this can be changed in 
the COPY statement. Returns a 1 on successful input. Examples:

  ## Simple example:
  $dbh->do("COPY mytable FROM STDIN");
  $dbh->pg_putcopydata("123\tPepperoni\t3\n");
  $dbh->pg_putcopydata("314\tMushroom\t8\n");
  $dbh->pg_putcopydata("6\tAnchovies\t100\n");
  $dbh->pg_putcopyend();

  ## This example uses explicit columns and a custom delimiter
  $dbh->do("COPY mytable(flavor, slices) FROM STDIN WITH DELIMITER '~'");
  $dbh->pg_putcopydata("Pepperoni~123\n");
  $dbh->pg_putcopydata("Mushroom~314\n");
  $dbh->pg_putcopydata("Anchovies~6\n");
  $dbh->pg_putcopyend();

=head3 B<pg_putcopyend>

When you are finished with pg_putcopydata, call pg_putcopyend to let the server know 
that you are done, and it will return to a normal, non-COPY state. Returns a 1 on 
success. This method will fail if called when not in COPY IN mode.

=head2 Large Objects

DBD::Pg supports all largeobject functions provided by libpq via the
C<< $dbh->pg_lo* >> methods. Please note that access to a large object, even read-only 
large objects, must be put into a transaction.

=head2 Cursors

Although PostgreSQL supports cursors, they have not been used in the current
implementation. When DBD::Pg was created, cursors in PostgreSQL could only be
used inside a transaction block. Because only one transaction block at a time
is allowed, this would have implied the restriction not to use any nested
C<SELECT> statements. Therefore the L</execute> method fetches all data at
once into data structures located in the front-end application. This fact
must to be considered when selecting large amounts of data!

You can use cursors in your application, but you'll need to do a little
work. First you must declare your cursor. Now you can issue queries against
the cursor, then select against your queries. This typically results in a
double loop, like this:

  # WITH HOLD is not needed if AutoCommit is off
  $dbh->do("DECLARE csr CURSOR WITH HOLD FOR $sql");
  while (1) {
    my $sth = $dbh->prepare("fetch 1000 from csr");
    $sth->execute;
    last if 0 == $sth->rows;

    while (my $row = $sth->fetchrow_hashref) {
      # Do something with the data.
    }
  }
  $dbh->do("CLOSE csr");

=head2 Datatype bool

The current implementation of PostgreSQL returns 't' for true and 'f' for
false. From the Perl point of view, this is a rather unfortunate
choice. DBD::Pg therefore translates the result for the C<BOOL> data type in a
Perlish manner: 'f' becomes the number C<0> and 't' becomes the number C<1>. This way 
the application does not have to check the database-specific returned values for 
the data-type C<BOOL> because Perl treats C<0> as false and C<1> as true. You may 
set the L<pg_bool_tf|/pg_bool_tf_(boolean)> attribute to a true value to change the values back to 't' and
'f' if you wish.

Boolean values can be passed to PostgreSQL as TRUE, 't', 'true', 'y', 'yes' or
'1' for true and FALSE, 'f', 'false', 'n', 'no' or '0' for false.

=head2 Schema support

The PostgreSQL schema concept may differ from those of other databases. In a nutshell,
a schema is a named collection of objects within a single database. Please refer to the
PostgreSQL documentation for more details:

L<http://www.postgresql.org/docs/current/static/ddl-schemas.html>

DBD::Pg does not provide explicit support for PostgreSQL schemas.
However, schema functionality may be used without any restrictions by
explicitly addressing schema objects, e.g.

  my $res = $dbh->selectall_arrayref("SELECT * FROM my_schema.my_table");

or by manipulating the schema search path with C<SET search_path>, e.g.

  $dbh->do("SET search_path TO my_schema, public");

=head1 SEE ALSO

L<The B<DBI> module|DBI>

=head1 BUGS

To report a bug, or view the current list of bugs, please visit 
http://rt.cpan.org/Public/Dist/Display.html?Name=DBD-Pg

=head1 DEVELOPMENT

Patches can be submitted to rt.cpan.org. Detailed information on how to 
help out with this module can be found in the README.dev file. The latest 
development version can be obtained via: git clone git://github.com/bucardo/dbdpg.git

=head1 AUTHORS

DBI by Tim Bunce L<http://www.tim.bunce.name>

The original DBD-Pg was by Edmund Mergl (E.Mergl@bawue.de) and Jeffrey W. Baker
(jwbaker@acm.org). Major developers include David Wheeler <david@justatheory.com>, Jason
Stewart <jason@openinformatics.com>, Bruce Momjian <pgman@candle.pha.pa.us>, and 
Greg Sabino Mullane <greg@turnstep.com>, with help from many others: see the Changes 
file (L<http://search.cpan.org/dist/DBD-Pg/Changes>) for a complete list.

Parts of this package were originally copied from DBI and DBD-Oracle.

B<Mailing List>

The current maintainers may be reached through the 'dbd-pg' mailing list:
<dbd-pg@perl.org>. Subscribe by sending an email to dbd-pg-subscribe@perl.org. 
Visit the archives at http://grokbase.com/g/perl/dbd-pg

=head1 COPYRIGHT AND LICENSE

Copyright (C) 1994-2019, Greg Sabino Mullane

This module (DBD::Pg) is free software; you can redistribute it and/or modify it 
under the same terms as Perl 5.10.0. For more details, see the full text of the 
licenses in the directory LICENSES.

=cut
