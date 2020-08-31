#   Oracle.pm
#
#   Copyright (c) 1994-2005 Tim Bunce, Ireland
#   Copyright (c) 2006-2008 John Scoles (The Pythian Group), Canada
#
#   See COPYRIGHT section in the documentation below

require 5.006;

my $ORACLE_ENV  = ($^O eq 'VMS') ? 'ORA_ROOT' : 'ORACLE_HOME';

{
package DBD::Oracle;
    our $VERSION = '1.76'; # VERSION
# ABSTRACT: Oracle database driver for the DBI module

    use DBI ();
    use DynaLoader ();
    use Exporter ();
    use DBD::Oracle::Object();

    $DBD::Oracle::VERSION ||= '1.00';

    @ISA = qw(DynaLoader Exporter);
    %EXPORT_TAGS = (
      ora_types => [ qw(
            ORA_VARCHAR2 ORA_STRING ORA_NUMBER ORA_LONG ORA_ROWID ORA_DATE
            ORA_RAW ORA_LONGRAW ORA_CHAR ORA_CHARZ ORA_MLSLABEL ORA_XMLTYPE
            ORA_CLOB ORA_BLOB ORA_RSET ORA_VARCHAR2_TABLE ORA_NUMBER_TABLE
            SQLT_INT SQLT_FLT ORA_OCI SQLT_CHR SQLT_BIN
        ) ],
      ora_session_modes => [ qw( ORA_SYSDBA ORA_SYSOPER ORA_SYSASM ORA_SYSBACKUP ORA_SYSDG ORA_SYSKM) ],
      ora_fetch_orient  => [ qw( OCI_FETCH_NEXT OCI_FETCH_CURRENT OCI_FETCH_FIRST
                                 OCI_FETCH_LAST OCI_FETCH_PRIOR OCI_FETCH_ABSOLUTE
                                 OCI_FETCH_RELATIVE)],
      ora_exe_modes     => [ qw( OCI_STMT_SCROLLABLE_READONLY)],
      ora_fail_over     => [ qw( OCI_FO_END OCI_FO_ABORT OCI_FO_REAUTH OCI_FO_BEGIN
                                 OCI_FO_ERROR OCI_FO_NONE OCI_FO_SESSION OCI_FO_SELECT
                                 OCI_FO_TXNAL OCI_FO_RETRY)],
    );
    @EXPORT_OK = qw( OCI_FETCH_NEXT OCI_FETCH_CURRENT OCI_FETCH_FIRST OCI_FETCH_LAST OCI_FETCH_PRIOR
                     OCI_FETCH_ABSOLUTE OCI_FETCH_RELATIVE ORA_OCI SQLCS_IMPLICIT SQLCS_NCHAR
                     ora_env_var ora_cygwin_set_env );

    #unshift @EXPORT_OK, 'ora_cygwin_set_env' if $^O eq 'cygwin';
    Exporter::export_ok_tags(qw(ora_types ora_session_modes ora_fetch_orient ora_exe_modes ora_fail_over));

    require_version DBI 1.623;

    DBD::Oracle->bootstrap($DBD::Oracle::VERSION);

    $drh = undef;        # holds driver handle once initialized

    sub CLONE {
        $drh = undef;
    }

    sub driver {
        return $drh if $drh;

        my($class, $attr) = @_;
        my $oci = DBD::Oracle::ORA_OCI();

        $class .= '::dr';

        # not a 'my' since we use it above to prevent multiple drivers

        $drh = DBI::_new_drh($class, {
            'Name'        => 'Oracle',
            'Version'     => $VERSION,
            'Err'         => \my $err,
            'Errstr'      => \my $errstr,
            'Attribution' => "DBD::Oracle $VERSION using OCI$oci by Tim Bunce et. al.",
        });

        DBD::Oracle::dr::init_oci($drh) ;
        $drh->STORE('ShowErrorStatement', 1);

        DBD::Oracle::db->install_method($_) for qw/
            ora_lob_read
            ora_lob_write
            ora_lob_append
            ora_lob_trim
            ora_lob_length
            ora_lob_chunk_size
            ora_lob_is_init
            ora_nls_parameters
            ora_can_unicode
            ora_can_taf
            ora_db_startup
            ora_db_shutdown
        /;

        DBD::Oracle::st->install_method($_) for qw/
            ora_fetch_scroll
            ora_scroll_position
            ora_ping
            ora_stmt_type_name
            ora_stmt_type
        /;

        $drh;
    }


    END {
        # Used to silence 'Bad free() ...' warnings caused by bugs in Oracle's code
        # being detected by Perl's malloc.
        $ENV{PERL_BADFREE} = 0;
        #undef $Win32::TieRegistry::Registry if $Win32::TieRegistry::Registry;
    }

    sub AUTOLOAD {
            (my $constname = $AUTOLOAD) =~ s/.*:://;
            my $val = constant($constname);
            *$AUTOLOAD = sub { $val };
            goto &$AUTOLOAD;
    }

}


{   package                     # hide from PAUSE
    DBD::Oracle::dr;            # ====== DRIVER ======
    use strict;

    my %dbnames = ();        # holds list of known databases (oratab + tnsnames)

    sub load_dbnames {
        my ($drh) = @_;
        my $debug = $drh->debug;
        my $oracle_home = DBD::Oracle::ora_env_var($ORACLE_ENV);
        my $d;

        if (($^O eq 'MSWin32') or ($^O =~ /cygwin/i)) {
          # XXX experimental, will probably change
          $drh->trace_msg("Trying to fetch ORACLE_HOME and ORACLE_SID from the registry.\n")
               if $debug;
          my $sid = DBD::Oracle::ora_env_var("ORACLE_SID");
          $dbnames{$sid} = $oracle_home if $sid and $oracle_home;
          $drh->trace_msg("Found $sid \@ $oracle_home.\n") if $debug && $sid;
        }

        # get list of 'local' database SIDs from oratab
        foreach $d (qw(/etc /var/opt/oracle), DBD::Oracle::ora_env_var("TNS_ADMIN")) {
            next unless defined $d;
            next unless open(my $FH, '<', "$d/oratab");
            $drh->trace_msg("Loading $d/oratab\n") if $debug;
            my $ot;
            while (defined($ot = <$FH>)) {
               next unless $ot =~ m/^\s*(\w+)\s*:\s*(.*?)\s*:/;
               $dbnames{$1} = $2;       # store ORACLE_HOME value
               $drh->trace_msg("Found $1 \@ $2.\n") if $debug;
            }
            close $FH;
            last;
        }

        # get list of 'remote' database connection identifiers
        my @tns_admin = ( DBD::Oracle::ora_env_var("TNS_ADMIN"), '.' );
        push @tns_admin, map { join '/', $oracle_home, $_ }
                         'network/admin',        # OCI 7 and 8.1
                         'net80/admin',            # OCI 8.0
            if $oracle_home;
        push @tns_admin, '/var/opt/oracle', '/etc';

    TNS_ADMIN:
        foreach $d ( grep { $_ and -d $_ } @tns_admin  ) {
            open my $FH, '<', "$d/tnsnames.ora" or next TNS_ADMIN;

            $drh->trace_msg("Loading $d/tnsnames.ora\n") if $debug;
            local *_;
            while (<$FH>) {
               next unless m/^\s*([-\w\.]+)\s*=/;
               my $name = $1;
               $drh->trace_msg("Found $name. ".($dbnames{$name} ? "(oratab entry overridden)" : "")."\n")
                   if $debug;
               $dbnames{$name} = 0; # exists but false (to distinguish from oratab)
            }
            close $FH;
            last;
        }

        $dbnames{0} = 1;       # mark as loaded (even if empty)
    }

    sub data_sources {
        my $drh = shift;
        load_dbnames($drh) unless %dbnames;
        my @names = sort  keys %dbnames;
        my @sources = map { $_ ? ("dbi:Oracle:$_") : () } @names;
        return @sources;
    }


    sub connect {
        my ($drh, $dbname, $user, $auth, $attr)= @_;

    # Make 'sid=whatever' an alias for 'whatever'.
    # see  RT91775
    $dbname =~ s/^sid=([^;]+)$/$1/;

        if ($dbname =~ /;/) {
            my ($n,$v);
            $dbname =~ s/^\s+//;
            $dbname =~ s/\s+$//;
            my @dbname = map {
               ($n,$v) = split /\s*=\s*/, $_, -1;
               Carp::carp("DSN component '$_' is not in 'name=value' format")
                   unless defined $v && defined $n;
                (uc($n), $v)
            } split /\s*;\s*/, $dbname;
            my %dbname = ( PROTOCOL => 'tcp', @dbname );

            if ((exists $dbname{SERVER}) and ($dbname{SERVER} eq "POOLED")) {
               $attr->{ora_drcp}=1;
            }

            # extract main attributes for connect_data portion
            my @connect_data_attr = qw(SID INSTANCE_NAME SERVER SERVICE_NAME );
            my %connect_data = map { ($_ => delete $dbname{$_}) }
               grep { exists $dbname{$_} } @connect_data_attr;

            my $connect_data = join "", map { "($_=$connect_data{$_})" } keys %connect_data;
            return $drh->DBI::set_err(-1,
               "Can't connect using this syntax without specifying a HOST and one of @connect_data_attr")
               unless $dbname{HOST} and %connect_data;

            my @addrs = map { "($_=$dbname{$_})" } keys %dbname;
            my $addrs = join "", @addrs;
            if ($dbname{PORT}) {
               $addrs = "(ADDRESS=$addrs)";
            }
            else {
               $addrs = "(ADDRESS_LIST=(ADDRESS=$addrs(PORT=1526))"
                                  . "(ADDRESS=$addrs(PORT=1521)))";
            }
            $dbname = "(DESCRIPTION=$addrs(CONNECT_DATA=$connect_data))";
            $drh->trace_msg("connect using '$dbname'");
        }

        # If the application is asking for specific database
        # then we may have to mung the dbname

        $dbname = $1 if !$dbname && $user && $user =~ s/\@(.*)//s;

        $drh->trace_msg("$ORACLE_ENV environment variable not set\n")
               if !$ENV{$ORACLE_ENV} and $^O ne "MSWin32";

        # create a 'blank' dbh

        $user = '' if not defined $user;
        (my $user_only = $user) =~ s:/.*::;

        if (substr($dbname,-7,7) eq ':POOLED'){
           $dbname=substr($dbname,0,-7);
           $attr->{ora_drcp} = 1;
        }
        elsif ($ENV{ORA_DRCP}){
           $attr->{ora_drcp} = 1;
        }

        my ($dbh, $dbh_inner) = DBI::_new_dbh($drh, {
            'Name' => $dbname,
            'dbi_imp_data' => $attr->{dbi_imp_data},
            # these two are just for backwards compatibility
            'USER' => uc $user_only, 'CURRENT_USER' => uc $user_only,
            });

        # Call Oracle OCI logon func in Oracle.xs file
        # and populate internal handle data.


        if (exists $ENV{ORA_DRCP_CLASS}) {
           $attr->{ora_drcp_class} = $ENV{ORA_DRCP_CLASS}
        }
        if($attr->{ora_drcp_class}){
        # if using ora_drcp_class it cannot contain more than 1024 bytes
        # and cannot contain a *
           if (index($attr->{ora_drcp_class},'*') !=-1){
               Carp::croak("ora_drcp_class cannot contain a '*'!");
           }
           if (length($attr->{ora_drcp_class}) > 1024){
               Carp::croak("ora_drcp_class must be less than 1024 characters!");
           }
        }
        if (exists $ENV{ORA_DRCP_MIN}) {
           $attr->{ora_drcp_min} = $ENV{ORA_DRCP_MIN}
        }
        if (exists $ENV{ORA_DRCP_MAX}) {
           $attr->{ora_drcp_max} = $ENV{ORA_DRCP_MAX}
        }
        if (exists $ENV{ORA_DRCP_INCR}) {
           $attr->{ora_drcp_incr} = $ENV{ORA_DRCP_INCR}
        }
        if (exists $ENV{ORA_DRCP_RLB}) {
           $attr->{ora_drcp_rlb} = $ENV{ORA_DRCP_RLB}
        }

        if (exists $ENV{ORA_EVENTS}) {
           $attr->{ora_events} = $ENV{ORA_EVENTS};
        }

        if (exists $ENV{ORA_EVENTS}) {
           $attr->{ora_events} = $ENV{ORA_EVENTS};
        }

        {
           local @SIG{ @{ $attr->{ora_connect_with_default_signals} } }
          if $attr->{ora_connect_with_default_signals};
           DBD::Oracle::db::_login($dbh, $dbname, $user, $auth, $attr)
              or return undef;
        }

        unless (length $user_only) {
            # It may be we've already encountered a warning by this point,
            # such as "ORA-28002: the password will expire within %d days".
            # We'll cache it for reinstatement.
            my ($err, $errstr, $state) =
               ($dbh->err, $dbh->errstr, $dbh->state);
            $user_only = $dbh->selectrow_array(q{
               SELECT SYS_CONTEXT('userenv','session_user') FROM DUAL
            })||'';
            # Now we'll reinstate the earlier warning.  We're just
            # appending it, so in the extremeley unlikely case that the
            # selectrow_array we just issued also issued a warning, the
            # 2 warnings will appear out of order.
            $dbh->set_err($err, $errstr, $state) if defined $err;
            $dbh_inner->{Username} = $user_only;
            # these two are just for backwards compatibility
            $dbh_inner->{USER} = $dbh_inner->{CURRENT_USER} = uc $user_only;
        }
        if ($ENV{ORA_DBD_NCS_BUFFER}){
            $dbh->{'ora_ncs_buff_mtpl'}= $ENV{ORA_DBD_NCS_BUFFER};
        }
        $dbh;

    }

     sub private_attribute_info {
            return { ora_home_key=>undef};
    }

}


{   package                     # hide from PAUSE
    DBD::Oracle::db;            # ====== DATABASE ======
    use strict;
    use DBI qw(:sql_types);

    sub prepare {
        my($dbh, $statement, @attribs)= @_;

        # create a 'blank' sth

        my $sth = DBI::_new_sth($dbh, {
            'Statement' => $statement,
            });

        # Call Oracle OCI parse func in Oracle.xs file.
        # and populate internal handle data.

        DBD::Oracle::st::_prepare($sth, $statement, @attribs)
            or return undef;

        $sth;
    }

#Ah! I see you have the machine that goes PING!!
#Yes!! We leased it from the company that made it
#then the cost came out of the operating budget
#not the capital ...

    sub ping {
        my($dbh) = @_;
        local $@;
        my $ok = 0;
        eval {
            local $SIG{__DIE__};
            local $SIG{__WARN__};
            $ok=ora_ping($dbh);
        };
        return ($@) ? 0 : $ok;
    }


    sub get_info {
        my($dbh, $info_type) = @_;
        require DBD::Oracle::GetInfo;
        my $v = $DBD::Oracle::GetInfo::info{int($info_type)};
        $v = $v->($dbh) if ref $v eq 'CODE';
        return $v;
    }

    sub private_attribute_info { #this should only be for ones that have setters and getters
        return { ora_max_nested_cursors        => undef,
                 ora_array_chunk_size        => undef,
                 ora_ph_type               => undef,
                 ora_ph_csform               => undef,
                 ora_parse_error_offset => undef,
                 ora_dbh_share               => undef,
                 ora_envhp               => undef,
                 ora_svchp               => undef,
                 ora_errhp               => undef,
                 ora_init_mode               => undef,
                 ora_events               => undef,
                 ora_charset               => undef,
                 ora_ncharset               => undef,
                 ora_session_mode        => undef,
                 ora_verbose               => undef,
                 ora_oci_success_warn        => undef,
                 ora_objects               => undef,
                 ora_ncs_buff_mtpl        => undef,
                 ora_drcp               => undef,
                 ora_drcp_class               => undef,
                 ora_drcp_min               => undef,
                 ora_drcp_max               => undef,
                 ora_drcp_incr               => undef,
                 ora_drcp_rlb               => undef,
                 ora_oratab_orahome        => undef,
                 ora_module_name        => undef,
                 ora_driver_name        => undef,
                 ora_client_info        => undef,
                 ora_client_identifier        => undef,
                 ora_action               => undef,
                 ora_taf_function        => undef,
                 };
    }


    sub table_info {
        my($dbh, $CatVal, $SchVal, $TblVal, $TypVal) = @_;
        # XXX add knowledge of temp tables, etc
        # SQL/CLI (ISO/IEC JTC 1/SC 32 N 0595), 6.63 Tables
        if (ref $CatVal eq 'HASH') {
            ($CatVal, $SchVal, $TblVal, $TypVal) =
               @$CatVal{'TABLE_CAT','TABLE_SCHEM','TABLE_NAME','TABLE_TYPE'};
        }
        my @Where = ();
        my $SQL;
        if ( defined $CatVal && $CatVal eq '%' && (!defined $SchVal || $SchVal eq '') && (!defined $TblVal || $TblVal eq '')) { # Rule 19a
               $SQL = <<'SQL';
SELECT NULL TABLE_CAT
     , NULL TABLE_SCHEM
     , NULL TABLE_NAME
     , NULL TABLE_TYPE
     , NULL REMARKS
  FROM DUAL
SQL
        }
        elsif ( defined $SchVal && $SchVal eq '%' && (!defined $CatVal || $CatVal eq '') && (!defined $TblVal || $TblVal eq '')) { # Rule 19b
               $SQL = <<'SQL';
SELECT NULL TABLE_CAT
     , s    TABLE_SCHEM
     , NULL TABLE_NAME
     , NULL TABLE_TYPE
     , NULL REMARKS
  FROM
(
  SELECT USERNAME s FROM ALL_USERS
  UNION
  SELECT 'PUBLIC' s FROM DUAL
)
 ORDER BY TABLE_SCHEM
SQL
        }
        elsif ( defined $TypVal && $TypVal eq '%' && (!defined $CatVal || $CatVal eq '') && (!defined $SchVal || $SchVal eq '') && (!defined $TblVal || $TblVal eq '')) { # Rule 19c
               $SQL = <<'SQL';
SELECT NULL TABLE_CAT
     , NULL TABLE_SCHEM
     , NULL TABLE_NAME
     , t.tt TABLE_TYPE
     , NULL REMARKS
  FROM
(
  SELECT 'TABLE'    tt FROM DUAL
    UNION
  SELECT 'VIEW'     tt FROM DUAL
    UNION
  SELECT 'SYNONYM'  tt FROM DUAL
    UNION
  SELECT 'SEQUENCE' tt FROM DUAL
) t
 ORDER BY TABLE_TYPE
SQL
        }
        else {
               $SQL = <<'SQL';
SELECT *
  FROM
(
  SELECT /*+ CHOOSE */
       NULL         TABLE_CAT
     , t.OWNER      TABLE_SCHEM
     , t.TABLE_NAME TABLE_NAME
     , decode(t.OWNER
          , 'SYS'    , 'SYSTEM '
          , 'SYSTEM' , 'SYSTEM '
          , '' ) || t.TABLE_TYPE TABLE_TYPE
     , c.COMMENTS   REMARKS
  FROM ALL_TAB_COMMENTS c
     , ALL_CATALOG      t
 WHERE c.OWNER      (+) = t.OWNER
   AND c.TABLE_NAME (+) = t.TABLE_NAME
   AND c.TABLE_TYPE (+) = t.TABLE_TYPE
)
SQL
               if ( defined $SchVal ) {
                      push @Where, "TABLE_SCHEM LIKE '$SchVal' ESCAPE '\\'";
               }
               if ( defined $TblVal ) {
                      push @Where, "TABLE_NAME  LIKE '$TblVal' ESCAPE '\\'";
               }
               if ( defined $TypVal ) {
                      my $table_type_list;
                      $TypVal =~ s/^\s+//;
                      $TypVal =~ s/\s+$//;
                      my @ttype_list = split (/\s*,\s*/, $TypVal);
                      foreach my $table_type (@ttype_list) {
                             if ($table_type !~ /^'.*'$/) {
                                    $table_type = "'" . $table_type . "'";
                             }
                             $table_type_list = join(", ", @ttype_list);
                      }
                      push @Where, "TABLE_TYPE IN ($table_type_list)";
               }
               $SQL .= ' WHERE ' . join("\n   AND ", @Where ) . "\n" if @Where;
               $SQL .= " ORDER BY TABLE_TYPE, TABLE_SCHEM, TABLE_NAME\n";
        }
        my $sth = $dbh->prepare($SQL) or return undef;
        $sth->execute or return undef;
        $sth;
}


    sub primary_key_info {
        my($dbh, $catalog, $schema, $table) = @_;
        if (ref $catalog eq 'HASH') {
            ($schema, $table) = @$catalog{'TABLE_SCHEM','TABLE_NAME'};
            $catalog = undef;
        }
        my $SQL = <<'SQL';
SELECT *
  FROM
(
  SELECT /*+ CHOOSE */
         NULL              TABLE_CAT
       , c.OWNER           TABLE_SCHEM
       , c.TABLE_NAME      TABLE_NAME
       , c.COLUMN_NAME     COLUMN_NAME
       , c.POSITION        KEY_SEQ
       , c.CONSTRAINT_NAME PK_NAME
    FROM ALL_CONSTRAINTS   p
       , ALL_CONS_COLUMNS  c
   WHERE p.OWNER           = c.OWNER
     AND p.TABLE_NAME      = c.TABLE_NAME
     AND p.CONSTRAINT_NAME = c.CONSTRAINT_NAME
     AND p.CONSTRAINT_TYPE = 'P'
     AND p.STATUS = 'ENABLED'
)
 WHERE TABLE_SCHEM = ?
   AND TABLE_NAME  = ?
 ORDER BY TABLE_SCHEM, TABLE_NAME, KEY_SEQ
SQL
#warn "@_\n$Sql ($schema, $table)";
        my $sth = $dbh->prepare($SQL) or return undef;
        $sth->execute($schema, $table) or return undef;
        $sth;
}

    sub foreign_key_info {
        my $dbh  = shift;
        my $attr = ( ref $_[0] eq 'HASH') ? $_[0] : {
            'UK_TABLE_SCHEM' => $_[1],'UK_TABLE_NAME ' => $_[2]
           ,'FK_TABLE_SCHEM' => $_[4],'FK_TABLE_NAME ' => $_[5] };
        my $SQL = <<'SQL';  # XXX: DEFERABILITY
SELECT *
  FROM
(
  SELECT /*+ CHOOSE */
         to_char( NULL )    UK_TABLE_CAT
       , uk.OWNER           UK_TABLE_SCHEM
       , uk.TABLE_NAME      UK_TABLE_NAME
       , uc.COLUMN_NAME     UK_COLUMN_NAME
       , to_char( NULL )    FK_TABLE_CAT
       , fk.OWNER           FK_TABLE_SCHEM
       , fk.TABLE_NAME      FK_TABLE_NAME
       , fc.COLUMN_NAME     FK_COLUMN_NAME
       , uc.POSITION        ORDINAL_POSITION
       , 3                  UPDATE_RULE
       , decode( fk.DELETE_RULE, 'CASCADE', 0, 'RESTRICT', 1, 'SET NULL', 2, 'NO ACTION', 3, 'SET DEFAULT', 4 )
                            DELETE_RULE
       , fk.CONSTRAINT_NAME FK_NAME
       , uk.CONSTRAINT_NAME UK_NAME
       , to_char( NULL )    DEFERABILITY
       , decode( uk.CONSTRAINT_TYPE, 'P', 'PRIMARY', 'U', 'UNIQUE')
                            UNIQUE_OR_PRIMARY
    FROM ALL_CONSTRAINTS    uk
       , ALL_CONS_COLUMNS   uc
       , ALL_CONSTRAINTS    fk
       , ALL_CONS_COLUMNS   fc
   WHERE uk.OWNER            = uc.OWNER
     AND uk.CONSTRAINT_NAME  = uc.CONSTRAINT_NAME
     AND fk.OWNER            = fc.OWNER
     AND fk.CONSTRAINT_NAME  = fc.CONSTRAINT_NAME
     AND uk.CONSTRAINT_TYPE IN ('P','U')
     AND fk.CONSTRAINT_TYPE  = 'R'
     AND uk.CONSTRAINT_NAME  = fk.R_CONSTRAINT_NAME
     AND uk.OWNER            = fk.R_OWNER
     AND uc.POSITION         = fc.POSITION
     AND fk.STATUS           = 'ENABLED'
)
 WHERE 1              = 1
SQL
        my @BindVals = ();
        while ( my ( $k, $v ) = each %$attr ) {
            if ( $v ) {
               $SQL .= "   AND $k = ?\n";
               push @BindVals, $v;
            }
        }
        $SQL .= " ORDER BY UK_TABLE_SCHEM, UK_TABLE_NAME, FK_TABLE_SCHEM, FK_TABLE_NAME, ORDINAL_POSITION\n";
        my $sth = $dbh->prepare( $SQL ) or return undef;
        $sth->execute( @BindVals ) or return undef;
        $sth;
    }


    sub column_info {
        my $dbh  = shift;
        my $attr = ( ref $_[0] eq 'HASH') ? $_[0] : {
            'TABLE_SCHEM' => $_[1],'TABLE_NAME' => $_[2],'COLUMN_NAME' => $_[3] };
        my $ora_server_version = ora_server_version($dbh);
        my($typecase,$typecaseend, $choose) = ('','','/*+ CHOOSE */');
        if ($ora_server_version->[0] >= 8) {
            $typecase = <<'SQL';
CASE WHEN tc.DATA_TYPE LIKE 'TIMESTAMP% WITH% TIME ZONE' THEN 95
     WHEN tc.DATA_TYPE LIKE 'TIMESTAMP%'                 THEN 93
     WHEN tc.DATA_TYPE LIKE 'INTERVAL DAY% TO SECOND%'   THEN 110
     WHEN tc.DATA_TYPE LIKE 'INTERVAL YEAR% TO MONTH'    THEN 107
ELSE
SQL
            $typecaseend = 'END';
        } elsif ($ora_server_version->[0] >= 11) {
            # rt91217 CHOOSE hint deprecated
            $choose = '';
        }
        my $char_length = $ora_server_version->[0] < 9 ? 'DATA_LENGTH':'CHAR_LENGTH';
        my $SQL = <<"SQL";
SELECT *
  FROM
(
  SELECT $choose
         to_char( NULL )     TABLE_CAT
       , tc.OWNER            TABLE_SCHEM
       , tc.TABLE_NAME       TABLE_NAME
       , tc.COLUMN_NAME      COLUMN_NAME
       , $typecase decode( tc.DATA_TYPE
         , 'MLSLABEL' , -9106
         , 'ROWID'    , -9104
         , 'UROWID'   , -9104
         , 'BFILE'    ,    -4 -- 31?
         , 'LONG RAW' ,    -4
         , 'RAW'      ,    -3
         , 'LONG'     ,    -1
         , 'UNDEFINED',     0
         , 'CHAR'     ,     1
         , 'NCHAR'    ,     1
         , 'NUMBER'   ,     decode( tc.DATA_SCALE, NULL, 8, 3 )
         , 'FLOAT'    ,     8
         , 'VARCHAR2' ,    12
         , 'NVARCHAR2',    12
         , 'BLOB'     ,    30
         , 'CLOB'     ,    40
         , 'NCLOB'    ,    40
         , 'DATE'     ,    93
         , NULL
         ) $typecaseend      DATA_TYPE          -- ...
       , tc.DATA_TYPE        TYPE_NAME          -- std.?
       , decode( tc.DATA_TYPE
         , 'LONG RAW' , 2147483647
         , 'LONG'     , 2147483647
         , 'CLOB'     , 2147483647
         , 'NCLOB'    , 2147483647
         , 'BLOB'     , 2147483647
         , 'BFILE'    , 2147483647
         , 'NUMBER'   , decode( tc.DATA_SCALE
                        , NULL, 126
                        , nvl( tc.DATA_PRECISION, 38 )
                        )
         , 'FLOAT'    , tc.DATA_PRECISION
         , 'DATE'     , 19
         , 'VARCHAR2' , tc.$char_length
         , 'CHAR'     , tc.$char_length
         , 'NVARCHAR2', tc.$char_length
         , 'NCHAR'    , tc.$char_length
         , tc.DATA_LENGTH
         )                   COLUMN_SIZE
       , decode( tc.DATA_TYPE
         , 'LONG RAW' , 2147483647
         , 'LONG'     , 2147483647
         , 'CLOB'     , 2147483647
         , 'NCLOB'    , 2147483647
         , 'BLOB'     , 2147483647
         , 'BFILE'    , 2147483647
         , 'NUMBER'   , nvl( tc.DATA_PRECISION, 38 ) + 2
         , 'FLOAT'    ,  8 -- ?
         , 'DATE'     , 16
         , tc.DATA_LENGTH
         )                   BUFFER_LENGTH
       , decode( tc.DATA_TYPE
         , 'DATE'     ,  0
         , tc.DATA_SCALE
         )                   DECIMAL_DIGITS     -- ...
       , decode( tc.DATA_TYPE
         , 'FLOAT'    ,  2
         , 'NUMBER'   ,  decode( tc.DATA_SCALE, NULL, 2, 10 )
         , NULL
         )                   NUM_PREC_RADIX
       , decode( tc.NULLABLE
         , 'Y'        ,  1
         , 'N'        ,  0
         , NULL
         )                   NULLABLE
       , cc.COMMENTS         REMARKS
       , tc.DATA_DEFAULT     COLUMN_DEF         -- Column is LONG!
       , decode( tc.DATA_TYPE
         , 'MLSLABEL' , -9106
         , 'ROWID'    , -9104
         , 'UROWID'   , -9104
         , 'BFILE'    ,    -4 -- 31?
         , 'LONG RAW' ,    -4
         , 'RAW'      ,    -3
         , 'LONG'     ,    -1
         , 'UNDEFINED',     0
         , 'CHAR'     ,     1
         , 'NCHAR'    ,     1
         , 'NUMBER'   ,     decode( tc.DATA_SCALE, NULL, 8, 3 )
         , 'FLOAT'    ,     8
         , 'VARCHAR2' ,    12
         , 'NVARCHAR2',    12
         , 'BLOB'     ,    30
         , 'CLOB'     ,    40
         , 'NCLOB'    ,    40
         , 'DATE'     ,     9 -- not 93!
         , NULL
         )                   SQL_DATA_TYPE      -- ...
       , decode( tc.DATA_TYPE
         , 'DATE'     ,     3
         , NULL
         )                   SQL_DATETIME_SUB   -- ...
       , to_number( NULL )   CHAR_OCTET_LENGTH  -- TODO
       , tc.COLUMN_ID        ORDINAL_POSITION
       , decode( tc.NULLABLE
         , 'Y'        , 'YES'
         , 'N'        , 'NO'
         , NULL
         )                   IS_NULLABLE
    FROM ALL_TAB_COLUMNS  tc
       , ALL_COL_COMMENTS cc
   WHERE tc.OWNER         = cc.OWNER
     AND tc.TABLE_NAME    = cc.TABLE_NAME
     AND tc.COLUMN_NAME   = cc.COLUMN_NAME
)
 WHERE 1              = 1
SQL
        my @BindVals = ();
        while ( my ( $k, $v ) = each %$attr ) {
            if ( $v ) {
                $SQL .= "   AND $k LIKE ? ESCAPE '\\'\n";
                push @BindVals, $v;
            }
        }
        $SQL .= " ORDER BY TABLE_SCHEM, TABLE_NAME, ORDINAL_POSITION\n";


        # Since DATA_DEFAULT is a LONG, DEFAULT values longer than 80 chars will
        # throw an ORA-24345 by default; so we check if LongReadLen is set at
        # the default value, and if so, set it to something less likely to fail
        # in common usage.
        #
        # We do not set LongTruncOk however as that would make COLUMN_DEF
        # incorrect, in those (extreme!) cases it would be better if the user
        # sets LongReadLen herself.

        my $long_read_len = $dbh->FETCH('LongReadLen');

        my ($sth, $exc);

        {
            local $@;
            eval {
                $dbh->STORE(LongReadLen => 1024*1024) if $long_read_len == 80;
                $sth = $dbh->prepare( $SQL );
            };
            $exc = $@;
        }
        if ($exc) {
            $dbh->STORE(LongReadLen => 80) if $long_read_len == 80;
            die $exc;
        }

        $dbh->STORE(LongReadLen => 80) if $long_read_len == 80;

        return undef if not $sth;

        $sth->execute( @BindVals ) or return undef;
        $sth;
    }

    sub statistics_info {
        my($dbh, $catalog, $schema, $table, $unique_only, $quick) = @_;
        if (ref $catalog eq 'HASH') {
            ($schema, $table) = @$catalog{'TABLE_SCHEM','TABLE_NAME'};
            $catalog = undef;
        }
        my $choose = '/*+ CHOOSE */';
        my $ora_server_version = ora_server_version($dbh);
        if ($ora_server_version->[0] >= 11) {
            # rt91217 CHOOSE hint deprecated
            $choose = '';
        }
        my $SQL = <<"SQL";
SELECT *
  FROM
(
  SELECT $choose
         NULL              TABLE_CAT
       , t.OWNER           TABLE_SCHEM
       , t.TABLE_NAME      TABLE_NAME
       , to_number( NULL ) NON_UNIQUE
       , NULL              INDEX_QUALIFIER
       , NULL              INDEX_NAME
       ,'table'            TYPE
       , to_number( NULL ) ORDINAL_POSITION
       , NULL              COLUMN_NAME
       , NULL              ASC_OR_DESC
       , t.NUM_ROWS        CARDINALITY
       , t.BLOCKS          PAGES
       , NULL              FILTER_CONDITION
    FROM ALL_TABLES        t
   UNION
  SELECT NULL              TABLE_CAT
       , t.OWNER           TABLE_SCHEM
       , t.TABLE_NAME      TABLE_NAME
       , decode( t.UNIQUENESS,'UNIQUE', 0, 1 ) NON_UNIQUE
       , c.INDEX_OWNER     INDEX_QUALIFIER
       , c.INDEX_NAME      INDEX_NAME
       , decode( t.INDEX_TYPE,'NORMAL','btree','CLUSTER','clustered','other') TYPE
       , c.COLUMN_POSITION ORDINAL_POSITION
       , c.COLUMN_NAME     COLUMN_NAME
       , decode( c.DESCEND,'ASC','A','DESC','D') ASC_OR_DESC
       , t.DISTINCT_KEYS   CARDINALITY
       , t.LEAF_BLOCKS     PAGES
       , NULL              FILTER_CONDITION
    FROM ALL_INDEXES       t
       , ALL_IND_COLUMNS   c
   WHERE t.OWNER           = c.INDEX_OWNER
     AND t.INDEX_NAME      = c.INDEX_NAME
     AND t.TABLE_OWNER     = c.TABLE_OWNER
     AND t.TABLE_NAME      = c.TABLE_NAME
     AND t.UNIQUENESS   LIKE :3
)
 WHERE TABLE_SCHEM = :1
   AND TABLE_NAME  = :2
 ORDER BY NON_UNIQUE, TYPE, INDEX_QUALIFIER, INDEX_NAME, ORDINAL_POSITION
SQL
        my $sth = $dbh->prepare($SQL) or return undef;
        $sth->execute($schema, $table, $unique_only ?'UNIQUE':'%') or return undef;
        $sth;
    }

    sub type_info_all {
        my ($dbh) = @_;
        my $version = ( ora_server_version($dbh)->[0] < DBD::Oracle::ORA_OCI() )
                    ?   ora_server_version($dbh)->[0] : DBD::Oracle::ORA_OCI();
        my $vc2len = ( $version < 8 ) ? '2000' : '4000';

        my $type_info_all = [
            {
               TYPE_NAME          =>  0,
               DATA_TYPE          =>  1,
               COLUMN_SIZE        =>  2,
               LITERAL_PREFIX     =>  3,
               LITERAL_SUFFIX     =>  4,
               CREATE_PARAMS      =>  5,
               NULLABLE           =>  6,
               CASE_SENSITIVE     =>  7,
               SEARCHABLE         =>  8,
               UNSIGNED_ATTRIBUTE =>  9,
               FIXED_PREC_SCALE   => 10,
               AUTO_UNIQUE_VALUE  => 11,
               LOCAL_TYPE_NAME    => 12,
               MINIMUM_SCALE      => 13,
               MAXIMUM_SCALE      => 14,
               SQL_DATA_TYPE      => 15,
               SQL_DATETIME_SUB   => 16,
               NUM_PREC_RADIX     => 17,
               INTERVAL_PRECISION => 18,
            },
            [ 'LONG RAW',        SQL_LONGVARBINARY, 2147483647,"'",  "'",
               undef,            1,0,0,undef,0,undef,
               'LONG RAW',        undef,undef,SQL_LONGVARBINARY,undef,undef,undef, ],
            [ 'RAW',             SQL_VARBINARY,     2000,      "'",  "'",
               'max length',     1,0,3,undef,0,undef,
               'RAW',             undef,undef,SQL_VARBINARY,    undef,undef,undef, ],
            [ 'LONG',            SQL_LONGVARCHAR,   2147483647,"'",  "'",
               undef,            1,1,0,undef,0,undef,
               'LONG',            undef,undef,SQL_LONGVARCHAR,  undef,undef,undef, ],
            [ 'CHAR',            SQL_CHAR,          2000,      "'",  "'",
               'max length',     1,1,3,undef,0,0,
               'CHAR',            undef,undef,SQL_CHAR,         undef,undef,undef, ],
            [ 'DECIMAL',         SQL_DECIMAL,       38,        undef,undef,
               'precision,scale',1,0,3,0,    0,0,
               'DECIMAL',         0,    38,   SQL_DECIMAL,      undef,10,   undef, ],
            [ "DOUBLE PRECISION",SQL_DOUBLE,        15,        undef,undef,
               undef, 1,0,3,0,    0,0,
               "DOUBLE PRECISION",undef,undef,SQL_DOUBLE,       undef,10,   undef, ],
            [ "DATE",            SQL_TYPE_TIMESTAMP,19,        "'",  "'",
               undef,            1,0,3,undef,0,0,
               "DATE",            0,    0,    SQL_DATE,         3,    undef,undef, ],
            [ "VARCHAR2",        SQL_VARCHAR,       $vc2len,   "'",  "'",
               "max length",     1,1,3,undef,0,0,
               "VARCHAR2",        undef,undef,SQL_VARCHAR,      undef,undef,undef, ],
            [ "BLOB",            SQL_BLOB, 2147483647,"'",  "'",
                undef,            1,1,0,undef,0,undef,
               "BLOB",            undef,undef,SQL_LONGVARBINARY,undef,undef,undef, ],
            [ "BFILE",           -9114, 2147483647,"'",  "'",
               undef,            1,1,0,undef,0,undef,
               "BFILE",           undef,undef,SQL_LONGVARBINARY,undef,undef,undef, ],
            [ "CLOB",            SQL_CLOB,   2147483647,"'",  "'",
               undef,            1,1,0,undef,0,undef,
               "CLOB",            undef,undef,SQL_LONGVARCHAR,  undef,undef,undef, ],
               ["TIMESTAMP WITH TIME ZONE",       # type name
                      SQL_TYPE_TIMESTAMP_WITH_TIMEZONE,       # data type
                      40,              # column size
                      "TIMESTAMP'",       # literal prefix
                      "'",              # literal suffix
                      "precision",       # create params
                      1,              # nullable
                      0,              # case sensitive
                      3,              # searchable
                      undef,              # unsigned attribute
                      0,              # fixed prec scale
                      0,              # auto unique value
                      undef,              # local type name
                      0,              # minimum scale
                      6,              # maximum scale
                      SQL_TIMESTAMP,       # sql data type
                      5,              # sql datetime sub
                      undef,              # num prec radix
                      undef,              # interval precision
               ],
               [ "INTERVAL DAY TO SECOND",       # type name
                      SQL_INTERVAL_DAY_TO_SECOND,       # data type
                      22,                            # column size       '+00 11:12:10.222222200'
                      "INTERVAL'",       # literal prefix
                      "'",              # literal suffix
                      "precision",       # create params
                      1,              # nullable
                      0,              # case sensitive
                      3,              # searchable
                      undef,              # unsigned attribute
                      0,              # fixed prec scale
                      0,              # auto unique value
                      undef,              # local type name
                      0,              # minimum scale
                      9,              # maximum scale
                      SQL_INTERVAL,       # sql data type
                      10,              # sql datetime sub
                      undef,              # num prec radix
                      undef,              # interval precision
               ],
               [ "INTERVAL YEAR TO MONTH",       # type name
                      SQL_INTERVAL_YEAR_TO_MONTH,       # data type
                      13,              # column size        '+012345678-01'
                      "INTERVAL'",       # literal prefix
                      "'",              # literal suffix
                      "precision",       # create params
                      1,              # nullable
                      0,              # case sensitive
                      3,              # searchable
                      undef,              # unsigned attribute
                      0,              # fixed prec scale
                      0,              # auto unique value
                      undef,              # local type name
                      0,              # minimum scale
                      9,              # maximum scale
                      SQL_INTERVAL,       # sql data type
                      7,              # sql datetime sub
                      undef,              # num prec radix
                      undef,              # interval precision
               ]
          ];

        return $type_info_all;
    }

    sub plsql_errstr {
        # original version thanks to Bob Menteer
        my $sth = shift->prepare_cached(q{
            SELECT name, type, line, position, text
            FROM user_errors ORDER BY name, type, sequence
        }) or return undef;
        $sth->execute or return undef;
        my ( @msg, $oname, $otype, $name, $type, $line, $pos, $text );
        $oname = $otype = 0;
        while ( ( $name, $type, $line, $pos, $text ) = $sth->fetchrow_array ) {
            if ( $oname ne $name || $otype ne $type ) {
               push @msg, "Errors for $type $name:";
               $oname = $name;
               $otype = $type;
            }
            push @msg, "$line.$pos: $text";
        }
        return join( "\n", @msg );
    }

    #
    # note, dbms_output must be enabled prior to usage
    #
    sub dbms_output_enable {
        my ($dbh, $buffersize) = @_;
        $buffersize ||= 20000;       # use oracle 7.x default
        $dbh->do("begin dbms_output.enable(:1); end;", undef, $buffersize);
    }

    sub dbms_output_get {
        my $dbh = shift;
        my $sth = $dbh->prepare_cached("begin dbms_output.get_line(:l, :s); end;")
               or return;
        my ($line, $status, @lines);
        my $version = join ".", @{ ora_server_version($dbh) }[0..1];
        my $len =  32767;
        if ($version < 10.2){
            $len = 400;
        }
        # line can be greater that 255 (e.g. 7 byte date is expanded on output)
        $sth->bind_param_inout(':l', \$line, $len, { ora_type => 1 });
        $sth->bind_param_inout(':s', \$status, 20, { ora_type => 1 });
        if (!wantarray) {
            $sth->execute or return undef;
            return $line if $status eq '0';
            return undef;
        }
        push @lines, $line while($sth->execute && $status eq '0');
        return @lines;
    }

    sub dbms_output_put {
        my $dbh = shift;
        my $sth = $dbh->prepare_cached("begin dbms_output.put_line(:1); end;")
               or return;
        my $line;
        foreach $line (@_) {
            $sth->execute($line) or return;
        }
        return 1;
    }


    sub dbms_msgpipe_get {
        my $dbh = shift;
        my $sth = $dbh->prepare_cached(q{
            begin dbms_msgpipe.get_request(:returnpipe, :proc, :param); end;
        }) or return;
        my $msg = ['','',''];
        $sth->bind_param_inout(":returnpipe", \$msg->[0],   30);
        $sth->bind_param_inout(":proc",       \$msg->[1],   30);
        $sth->bind_param_inout(":param",      \$msg->[2], 4000);
        $sth->execute or return undef;
        return $msg;
    }

    sub dbms_msgpipe_ack {
        my $dbh = shift;
        my $msg = shift;
        my $sth = $dbh->prepare_cached(q{
            begin dbms_msgpipe.acknowledge(:returnpipe, :errormsg, :param); end;}) or return;
        $sth->bind_param_inout(':returnpipe', \$msg->[0],   30);
        $sth->bind_param_inout(':proc',       \$msg->[1],   30);
        $sth->bind_param_inout(':param',      \$msg->[2], 4000);
        $sth->execute or return undef;
        return 1;
    }

    sub ora_server_version {
        my $dbh = shift;
        return $dbh->{ora_server_version} if defined $dbh->{ora_server_version};
        my $banner = $dbh->selectrow_array(<<'SQL', undef, 'Oracle%', 'Personal Oracle%');
SELECT banner
  FROM v$version
  WHERE banner LIKE ? OR banner LIKE ?
SQL
        if (defined $banner) {
            my @version = $banner =~ /(?:^|\s)(\d+)\.(\d+)\.(\d+)\.(\d+)\.(\d+)(?:\s|$)/;
            $dbh->{ora_server_version} = \@version if @version;
        }

        # TODO looks like a bug that we don't return
        # $dbh->{ora_server_version} here
    }

    sub ora_nls_parameters {
        my $dbh = shift;
        my $refresh = shift;

        if ($refresh || !$dbh->{ora_nls_parameters}) {
            my $nls_parameters = $dbh->selectall_arrayref(q{
               SELECT parameter, value FROM v$nls_parameters
            }) or return;
            $dbh->{ora_nls_parameters} = { map { $_->[0] => $_->[1] } @$nls_parameters };
        }

        # return copy of params to protect against accidental editing
        my %nls = %{$dbh->{ora_nls_parameters}};
        return \%nls;
    }

    sub ora_can_unicode {
        my $dbh = shift;
        my $refresh = shift;
        # 0 = No Unicode support.
        # 1 = National character set is Unicode-based.
        # 2 = Database character set is Unicode-based.
        # 3 = Both character sets are Unicode-based.

        return $dbh->{ora_can_unicode}
            if defined $dbh->{ora_can_unicode} && !$refresh;

        my $nls = $dbh->ora_nls_parameters($refresh);

        $dbh->{ora_can_unicode}  = 0;
        $dbh->{ora_can_unicode} += 1 if $nls->{NLS_NCHAR_CHARACTERSET} =~ m/UTF/;
        $dbh->{ora_can_unicode} += 2 if $nls->{NLS_CHARACTERSET}       =~ m/UTF/;

        return $dbh->{ora_can_unicode};
    }

}   # end of package DBD::Oracle::db


{   package                     # hide from PAUSE
    DBD::Oracle::st;            # ====== STATEMENT ======


   sub bind_param_inout_array {
        my $sth = shift;
        my ($p_id, $value_array,$maxlen, $attr) = @_;
        return $sth->set_err($DBI::stderr, "Value for parameter $p_id must be an arrayref, not a ".ref($value_array))
           if defined $value_array and ref $value_array and ref $value_array ne 'ARRAY';

        return $sth->set_err($DBI::stderr, "Can't use named placeholder '$p_id' for non-driver supported bind_param_inout_array")
           unless DBI::looks_like_number($p_id); # because we rely on execute(@ary) here

        return $sth->set_err($DBI::stderr, "Placeholder '$p_id' is out of range")
           if $p_id <= 0; # can't easily/reliably test for too big

        # get/create arrayref to hold params
        my $hash_of_arrays = $sth->{ParamArrays} ||= { };

        $$hash_of_arrays{$p_id} = $value_array;
        return ora_bind_param_inout_array($sth, $p_id, $value_array,$maxlen, $attr);
        1;

    }


    sub execute_for_fetch {
       my ($sth, $fetch_tuple_sub, $tuple_status) = @_;
       my $row_count = 0;
       my $err_total = 0;
       my $tuple_count="0E0";
       my $tuple_batch_status;
       my $dbh = $sth->{Database};
       my $batch_size =($dbh->{'ora_array_chunk_size'}||= 1000);
       if(defined($tuple_status)) {
           @$tuple_status = ();
           $tuple_batch_status = [ ];
       }

       my $finished;
       while (1) {
           my @tuple_batch;
           for (my $i = 0; $i < $batch_size; $i++) {
               $finished = $fetch_tuple_sub->();
               push @tuple_batch, [@{$finished || last}];

           }
           last unless @tuple_batch;

           my $err_count = 0;
           my $res = ora_execute_array($sth,
                                           \@tuple_batch,
                                           scalar(@tuple_batch),
                                           $tuple_batch_status,
                                           $err_count );

           if (defined($res)) { #no error
                $row_count += $res;
           }
           else {
                $row_count = undef;
           }

           $err_total += $err_count;

           $tuple_count+=@tuple_batch;
           push @$tuple_status, @$tuple_batch_status
                if defined($tuple_status);

           last if !$finished;

       }
       #error check here
       return $sth->set_err($DBI::stderr, "executing $tuple_count generated $err_total errors")
                  if $err_total;

       return wantarray
                ? ($tuple_count, defined $row_count ? $row_count : undef)
                : $tuple_count;

    }

    sub private_attribute_info {
        return { map { $_ => undef } qw/
            ora_lengths
            ora_types
            ora_rowid
            ora_est_row_width
            ora_type
            ora_fail_over
        / };
   }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

DBD::Oracle - Oracle database driver for the DBI module

=head1 VERSION

version 1.76

=head1 SYNOPSIS

  use DBI;

  $dbh = DBI->connect("dbi:Oracle:$dbname", $user, $passwd);

  $dbh = DBI->connect("dbi:Oracle:host=$host;sid=$sid", $user, $passwd);

  # See the DBI module documentation for full details

  # for some advanced uses you may need Oracle type values:
  use DBD::Oracle qw(:ora_types);

=head1 DESCRIPTION

DBD::Oracle is a Perl module which works with the DBI module to provide
access to Oracle databases.

This documentation describes driver specific behaviour and restrictions. It is
not supposed to be used as the only reference for the user. In any case
consult the L<DBI> documentation first!

=head1 CONSTANTS

=over 4

=item :ora_session_modes

ORA_SYSDBA ORA_SYSOPER ORA_SYSASM ORA_SYSBACKUP ORA_SYSDG ORA_SYSKM

=item :ora_types

  ORA_VARCHAR2 ORA_STRING ORA_NUMBER ORA_LONG ORA_ROWID ORA_DATE ORA_RAW
  ORA_LONGRAW ORA_CHAR ORA_CHARZ ORA_MLSLABEL ORA_XMLTYPE ORA_CLOB ORA_BLOB
  ORA_RSET ORA_VARCHAR2_TABLE ORA_NUMBER_TABLE SQLT_INT SQLT_FLT ORA_OCI
  SQLT_CHR SQLT_BIN

=item SQLCS_IMPLICIT

=item SQLCS_NCHAR

SQLCS_IMPLICIT and SQLCS_NCHAR are I<character set form> values.
See notes about Unicode elsewhere in this document.

=item SQLT_INT

=item SQLT_FLT

These types are used only internally, and may be specified as internal
bind type for ORA_NUMBER_TABLE. See notes about ORA_NUMBER_TABLE elsewhere
in this document

=item ORA_OCI

Oracle doesn't provide a formal API for determining the exact version
number of the OCI client library used, so DBD::Oracle has to go digging
(and sometimes has to more or less guess).  The ORA_OCI constant
holds the result of that process.

In string context ORA_OCI returns the full "A.B.C.D" version string.

In numeric context ORA_OCI returns the major.minor version number
(8.1, 9.2, 10.0 etc).  But note that version numbers are not actually
floating point and so if Oracle ever makes a release that has a two
digit minor version, such as C<9.10> it will have a lower numeric
value than the preceding C<9.9> release. So use with care.

The contents and format of ORA_OCI are subject to change (it may,
for example, become a I<version object> in later releases).
I recommend that you avoid checking for exact values.

=item :ora_fetch_orient

  OCI_FETCH_CURRENT OCI_FETCH_NEXT OCI_FETCH_FIRST OCI_FETCH_LAST
  OCI_FETCH_PRIOR OCI_FETCH_ABSOLUTE OCI_FETCH_RELATIVE

These constants are used to set the orientation of a fetch on a scrollable cursor.

=item :ora_exe_modes

  OCI_STMT_SCROLLABLE_READONLY

=item :ora_fail_over

  OCI_FO_END OCI_FO_ABORT OCI_FO_REAUTH OCI_FO_BEGIN OCI_FO_ERROR
  OCI_FO_NONE OCI_FO_SESSION OCI_FO_SELECT OCI_FO_TXNAL OCI_FO_RETRY

=back

=head1 DBI CLASS METHODS

=head2 B<connect>

This method creates a database handle by connecting to a database, and is the DBI equivalent of the "new" method.
To open a connection to an Oracle database you need to specify a database connection string (URL), username and password.

The connection string is always of the form: "dbi:Oracle:<db identifier>"
There are several ways to identify a database:

=over

=item 1

If the database is local, specifying the SID or service name will be enough.

=item 2

If the database is defined in a TNSNAMES.ORA file, you can use the service name given in the file

=item 3

To connect without TNSNAMES.ORA file, you can use an EZCONNECT url, of the form:
//host[:port][/service_name]

=back

If port name is not specified, 1521 is the default. If service name is not specified, the hostname will be used as a service name.

The following examples show several ways a connection can be created:

  $dbh = DBI->connect('dbi:Oracle:DB','username','password');

  $dbh = DBI->connect('dbi:Oracle:DB','username/password','');

  $dbh = DBI->connect('dbi:Oracle:','username@DB','password');

  $dbh = DBI->connect('dbi:Oracle:host=foobar;sid=DB;port=1521', 'scott/tiger', '');

  $dbh = DBI->connect("dbi:Oracle://myhost:1522/ORCL",'username', 'password');

=head3 OS authentication

To connect to a local database with a user which has been set up to
authenticate via the OS ("ALTER USER username IDENTIFIED EXTERNALLY"):

  $dbh = DBI->connect('dbi:Oracle:','/','');

Note the lack of a connection name (use the ORACLE_SID environment
variable). If an explicit SID is used you will probably get an ORA-01004 error.

That only works for local databases. (Authentication to remote Oracle
databases using your Unix login name without a password is possible
but it is not secure and not recommended so not documented here.

=head3 Oracle Environment Variables

To use DBD::ORACLE to connect to an Oracle database, ORACLE_HOME environment variable should be set correctly.
In general, the value used should match the version of Oracle that was used to build DBD::Oracle.  If using dynamic linking then ORACLE_HOME should match the version of Oracle that will be used to load in the Oracle client libraries (via LD_LIBRARY_PATH, ldconfig, or similar on Unix).

Oracle can use two environment variables to specify default connections: ORACLE_SID and TWO_TASK.

To use them, specify either a local SID or service name, or a service name that is specified in the TNSNAMES.ORA file.

Note that if you have *both* local and remote databases, and you have ORACLE_SID *and* TWO_TASK set, and you don't specify a fully
qualified connect string on the command line, TWO_TASK takes precedence over ORACLE_SID (i.e. you get connected to remote system).

It is highly recommended not to rely on environment variables and to always explicitly specify the SID in the connection string. This can prevent serious mistakes such as dropping a schema in the wrong database, and generally makes debugging and troubleshooting easier.

Also remember that depending on the operating system you are using the differing "ORACLE" environment variables may be case sensitive, so if you are not connecting as you should double check the case of both the variable and its value.

=head3 Timezones

If the query is run through SQL*Net (mostly queries that are executed on remote servers), Oracle will return the time zone based on the setting of the UNIX environment variable "TZ" for the user who started the listener.

If the query is run locally, Oracle will return the time zone based on the "TZ" environment variable setting of the user running
the query.

With local queries, you can change the time zone for a particular user by simply changing the setting of "TZ". To check the current setting,
issue the UNIX "date" command.

=head3 Oracle DRCP

DBD::Oracle supports DRCP (Database Resident Connection Pool) so
if you have an 11.2 database and DRCP is enabled you can direct
all of your connections to it by adding ':POOLED' to the SID or
setting a connection attribute of ora_drcp, or set the SERVER=POOLED
when using a TNSENTRY style connection or even by setting an
environment variable ORA_DRCP.  All of which are demonstrated below;

  $dbh = DBI->connect('dbi:Oracle:DB:POOLED','username','password')

  $dbh = DBI->connect('dbi:Oracle:','username@DB:POOLED','password')

  $dbh = DBI->connect('dbi:Oracle:DB','username','password',{ora_drcp=>1})

  $dbh = DBI->connect('dbi:Oracle:DB','username','password',{ora_drcp=>1,
                                                             ora_drcp_class=>'my_app',
                                                             ora_drcp_min  =>10})

  $dbh = DBI->connect('dbi:Oracle:host=foobar;sid=ORCL;port=1521;SERVER=POOLED', 'scott/tiger', '')

  $dbh = DBI->connect('dbi:Oracle:', q{scott/tiger@(DESCRIPTION=
  (ADDRESS=(PROTOCOL=TCP)(HOST= foobar)(PORT=1521))
  (CONNECT_DATA=(SID=ORCL)(SERVER=POOLED)))}, "")

  if the ORA_DRCP environment variable is set then just this

  $dbh = DBI->connect('dbi:Oracle:DB','username','password')

You can find a white paper on setting up DRCP and its advantages at L<http://www.oracle.com/technetwork/articles/oracledrcp11g-1-133381.pdf>.

Please note that DRCP support in DBD::Oracle is relatively new so the
mechanics or its implementation are subject to change.

=head3 TAF (Transparent Application Failover)

Transparent Application Failover (TAF) is the feature in OCI that
allows for clients to automatically reconnect to an instance in the
event of a failure of the instance. The reconnect happens
automatically from within the OCI (Oracle Call Interface) library.
DBD::Oracle now supports a callback function that will fire when a TAF
event takes place. You may use the callback to inform the
user a failover is taking place or to setup the session again
once the failover has succeeded.

You will have to set up TAF on your instance before you can use this
callback.  You can test your instance to see if you can use TAF
callback with

  $dbh->ora_can_taf();

If you try to set up a callback without it being enabled DBD::Oracle
will croak.

NOTE: Currently, you must enable TAF during DBI's connect. However
once enabled you can change the TAF settings.

It is outside the scope of this document to go through all of the
possible TAF situations you might want to set up but here is a simple
example:

The TNS entry for the instance has had the following added to the
CONNECT_DATA section

   (FAILOVER_MODE=
               (TYPE=select)
               (METHOD=basic)
               (RETRIES=10)
               (DELAY=10))

You will also have to create your own perl function that will be
called from the client.  You can name it anything you want and it will
always be passed two parameters, the failover event value and the
failover type.  You can also set a sleep value in case of failover
error and the OCI client will sleep for the specified seconds before it
attempts another event.

  use DBD::Oracle(qw(:ora_fail_over));
  #import the ora fail over constants

  #set up TAF on the connection
  # NOTE since DBD::Oracle uses call_pv you may need to pass a full
  # name space as the function e.g., 'main::handle_taf'
  # NOTE from 1.49_00 ora_taf_function can accept a code ref as well
  #      as a sub name as it now uses call_sv
  my $dbh = DBI->connect('dbi:Oracle:XE', 'hr', 'hr',
                         {ora_taf_function => 'main::handle_taf'});

  #create the perl TAF event function

  sub handle_taf {
    # NOTE from 1.49_00 the $dbh handle was passed to your callback
    my ($fo_event,$fo_type, $dbh) = @_;
    if ($fo_event == OCI_FO_BEGIN){

      print " Instance Unavailable Please stand by!! \n";
      printf(" Your TAF type is %s \n",
                       (($fo_type==OCI_FO_NONE) ? "NONE"
                       :($fo_type==OCI_FO_SESSION) ? "SESSION"
                       :($fo_type==OCI_FO_SELECT) ? "SELECT"
                       : "UNKNOWN!"));
    }
    elsif ($fo_event == OCI_FO_ABORT){
       print " Failover aborted. Failover will not take place.\n";
    }
    elsif ($fo_event == OCI_FO_END){
       printf(" Failover ended ...Resuming your %s\n",(($fo_type==OCI_FO_NONE) ? "NONE"
                                                      :($fo_type==OCI_FO_SESSION) ? "SESSION"
                                                      :($fo_type==OCI_FO_SELECT) ? "SELECT"
                                                      : "UNKNOWN!"));
    }
    elsif ($fo_event == OCI_FO_REAUTH){
       print " Failed over user. Resuming services\n";
    }
    elsif ($fo_event == OCI_FO_ERROR){
       print " Failover error ...\n";
       sleep 5;                 # sleep before having another go
       return OCI_FO_RETRY;
    }
    else {
       printf(" Bad Failover Event: %d.\n",  $fo_event);

    }
    return 0;
  }

The TAF types are as follows

  OCI_FO_SESSION indicates the user has requested only session failover.
  OCI_FO_SELECT indicates the user has requested select failover.
  OCI_FO_NONE indicates the user has not requested a failover type.
  OCI_FO_TXNAL indicates the user has requested a transaction failover.

The TAF events are as follows

  OCI_FO_BEGIN indicates that failover has detected a lost connection and failover is starting.
  OCI_FO_END   indicates successful completion of failover.
  OCI_FO_ABORT indicates that failover was unsuccessful, and there is no option of retrying.
  OCI_FO_ERROR also indicates that failover was unsuccessful, but it gives the application the opportunity to handle the error and retry failover.
  OCI_FO_REAUTH indicates that you have multiple authentication handles and failover has occurred after the original authentication. It indicates that a user handle has been re-authenticated. To find out which, the application checks the OCI_ATTR_SESSION attribute of the service context handle (which is the first parameter).

=head3 Connect Attributes

=head4 ora_ncs_buff_mtpl

You can customize the size of the buffer when selecting LOBs with
the built-in AUTO Lob.  The default value is 4 which is probably
excessive for most situations but is needed for backward
compatibility.  If you not converting between a NCS on the DB and the
Client then you might want to set this to 1 to reduce memory usage.

This value can also be specified with the C<ORA_DBD_NCS_BUFFER>
environment variable in which case it sets the value at the connect
stage.

=head4 ora_drcp

For Oracle 11.2 or greater.

Set to I<1> to enable DRCP. Can also be set via the C<ORA_DRCP> environment variable.

=head4 ora_drcp_class

If you are using DRCP, you can set a CONNECTION_CLASS for your pools
as well.  As sessions from a DRCP cannot be shared by users, you can
use this setting to identify the same user across different
applications. OCI will ensure that sessions belonging to a 'class' are
not shared outside the class'.

The values for ora_drcp_class cannot contain a '*' and must be less
than 1024 characters.

This value can be also be specified with the C<ORA_DRCP_CLASS>
environment variable.

Note that a connection class must be specified in order to enable
inter-process sharing of server side sessions.

=head4 ora_drcp_min

This optional value specifies the minimum number of sessions that are
initially allocated for the application process.  New sessions are only
allocated after this value has been reached.

The default value is 0 and any value greater than or equal to 0 is valid.

For multi-process applications, it is recommended to leave the value at 0.
This ensures that each process is only occupying a server session while
the process is doing database work.

For multi-threaded applications, the value could be set to the number of
concurrent statements the application is planning or expecting to run.
Please note that DRCP has not been tested with multi-threading.

This value can also be specified with the C<ORA_DRCP_MIN> environment
variable.

=head4 ora_drcp_max

This optional value specifies the maximum number of sessions that can
be open at one time.  Once reached no more sessions can be opened
until one becomes free. The default value is 40 and any value above 0
is valid.  You should not set this value lower than ora_drcp_min as
that will just waste resources.

This value can also be specified with the C<ORA_DRCP_MAX> environment
variable.

=head4 ora_drcp_incr

This optional value specifies the next increment for sessions to be
started if the current number of sessions are less than
ora_drcp_max. The default value is 1 and any value above 0 is
valid as long as the value of ora_drcp_min + ora_drcp_incr is not
greater than ora_drcp_max.

This value can also be specified with the C<ORA_DRCP_INCR> environment
variable.

=head4 ora_drcp_rlb

This optional value controls whether run-time connection load balancing
is used for Oracle RAC. The default value is 0, which disables the feature.
Set the value to 1 to enable the feature.

This value can also be specified with the C<ORA_DRCP_RLB> environment
variable.

=head4 ora_taf

This attribute was removed in 1.49_00 as it was redundant. To
enable TAF simply set L</ora_taf_function>.

=head4 ora_taf_function

If your Oracle instance has been configured to use TAF events you can
enable the TAF callback by setting this option.

The name of the Perl subroutine (or a code ref from 1.49_00) that will
be called from OCI when a TAF event occurs. You must supply a perl
function to use the callback and it will always receive at least two
parameters; the failover event value and the failover type. From
1.49_00 the dbh is passed as the third argument. Below is an example
of a TAF function

  sub taf_event{
     # NOTE from 1.49_00 the $dbh handle is passed to the callback
     my ($event, $type, $dbh) = @_;

     print "My TAF event=$event\n";
     print "My TAF type=$type\n";
     return;
  }

Note if passing a sub name you will probably have to use the full name
space when setting the TAF function e.g., 'main::my_taf_function' and
not just 'my_taf_function'.

=head4 ora_taf_sleep

This attribute was removed in 1.49_00 as it was redundant. If you want
to sleep between retries simple add a sleep to your callback sub.

=head4 ora_session_mode

The ora_session_mode attribute can be used to connect with SYSDBA,
SYSOPER, ORA_SYSASM, ORA_SYSBACKUP, ORA_SYSKM and ORA_SYSDG authorization.
The ORA_SYSDBA, ORA_SYSOPER, ORA_SYSASM, ORA_SYSBACKUP, ORA_SYSKM
and ORA_SYSDG constants can be imported using

  use DBD::Oracle qw(:ora_session_modes);

This is one case where setting ORACLE_SID may be useful since
connecting as SYSDBA or SYSOPER via SQL*Net is frequently disabled
for security reasons.

Example:

  $dsn = "dbi:Oracle:";       # no dbname here
  $ENV{ORACLE_SID} = "orcl";  # set ORACLE_SID as needed
  delete $ENV{TWO_TASK};      # make sure TWO_TASK isn't set

  $dbh = DBI->connect($dsn, "", "", { ora_session_mode => ORA_SYSDBA });

It has been reported that this only works if C<$dsn> does not contain
a SID so that Oracle then uses the value of ORACLE_SID (not
TWO_TASK) environment variable to connect to a local instance. Also
the username and password should be empty, and the user executing the
script needs to be part of the dba group or osdba group.

Note that this does not work with DRCP.

=head4 ora_oratab_orahome

Passing a true value for the ora_oratab_orahome attribute will make
DBD::Oracle change C<$ENV{ORACLE_HOME}> to make the Oracle home directory
that specified in the C</etc/oratab> file I<if> the database to connect to
is specified as a SID that exists in the oratab file, and DBD::Oracle was
built to use the Oracle 7 OCI API (not Oracle 8+).

=head4 ora_module_name

After connecting to the database the value of this attribute is passed
to the SET_MODULE() function in the C<DBMS_APPLICATION_INFO> PL/SQL
package. This can be used to identify the application to the DBA for
monitoring and performance tuning purposes. For example:

  my $dbh = DBI->connect($dsn, $user, $passwd, { ora_module_name => $0 });

  $dbh->{ora_module_name} = $y;

The maximum size is 48 bytes.

NOTE: You will need an Oracle client 10.1 or later to use this.

=head4 ora_driver_name

For 11g and later you can now set the name of the driver layer using OCI.
Perl, Perl5, ApachePerl so on. Names starting with "ORA" are reserved. You
can enter up to 8 characters.  If none is enter then this will default to
DBDOxxxx where xxxx is the current version number. This value can be
retrieved on the server side using V$SESSION_CONNECT_INFO or
GV$SESSION_CONNECT_INFO

  my $dbh = DBI->connect($dsn, $user, $passwd, { ora_driver_name => 'ModPerl_1' });

  $dbh->{ora_driver_name} = $q;

=head4 ora_client_info

Allows you to add any value (up to 64 bytes) to your session and it can be
retrieved on the server side from the C<V$SESSION>a view.

  my $dbh = DBI->connect($dsn, $user, $passwd, { ora_client_info => 'Remote2' });

  $dbh->{ora_client_info} = "Remote2";

NOTE: You will need an Oracle client 10.1 or later to use this.

=head4 ora_client_identifier

Allows you to specify the user identifier in the session handle.

Most useful for web applications as it can pass in the session user
name which might be different to the connection user name. Can be up
to 64 bytes long but do not to include the password for security
reasons and the first character of the identifier should not be
':'. This value can be retrieved on the server side using C<V$SESSION>
view.

  my $dbh = DBI->connect($dsn, $user, $passwd, { ora_client_identifier => $some_web_user });

  $dbh->{ora_client_identifier} = $local_user;

=head4 ora_action

Allows you to specify any string up to 32 bytes which may be retrieved
on the server side using C<V$SESSION> view.

   my $dbh = DBI->connect($dsn, $user, $passwd, { ora_action => "Login"});

   $dbh->{ora_action} = "New Long Query 22";

NOTE: You will need an Oracle client 10.1 or later to use this.

=head4 ora_dbh_share

Requires at least Perl 5.8.0 compiled with ithreads.

Allows you to share
database connections between threads. The first connect will make the
connection, all following calls to connect with the same ora_dbh_share
attribute will use the same database connection. The value must be a
reference to a already shared scalar which is initialized to an empty
string.

  our $orashr : shared = '' ;

  $dbh = DBI->connect ($dsn, $user, $passwd, {ora_dbh_share => \$orashr}) ;

=head4 ora_events

Set this attribute to C<1> to enable Oracle Fast Application Notification
(FAN) in a new OCI environment. Can also be set via the C<ORA_EVENTS>
environment variable.

=head4 ora_envhp

The first time a connection is made a new OCI 'environment' is
created by DBD::Oracle and stored in the driver handle.
Subsequent connects reuse (share) that same OCI environment
by default.

The ora_envhp attribute can be used to disable the reuse of the OCI
environment from a previous connect. If the value is C<0> then
a new OCI environment is allocated and used for this connection.

The OCI environment holds information about the client side context,
such as the local NLS environment. By altering C<%ENV> and setting
ora_envhp to 0 you can create connections with different NLS
settings. This is most useful for testing.

Note that for DRCP, setting C<ora_envhp = 0> has no effect. Here,
a new session pool is created, using the current NLS environment,
for each new combination of dbname, uid/pwd, connection class,
and charset/ncharset.

=head4 ora_charset, ora_ncharset

For oracle versions >= 9.2 you can specify the client charset and
ncharset with the ora_charset and ora_ncharset attributes.  You
still need to pass C<ora_envhp = 0> for all but the first connect.

These attributes override the settings from environment variables.

  $dbh = DBI->connect ($dsn, $user, $passwd,
                       {ora_charset => 'AL32UTF8'});

=head4 ora_verbose

Use this value to enable DBD::Oracle only tracing.  Simply either set
the ora_verbose attribute on the connect() method to the trace level
you desire like this

  my $dbh = DBI->connect($dsn, "", "", {ora_verbose=>6});

or set it directly on the DB handle like this;

  $dbh->{ora_verbose} =6;

In both cases the DBD::Oracle trace level is set to 6, which is the highest
level tracing most of the calls to OCI.

NOTE: In future versions of DBD::Oracle ora_verbose will be changed so
that it is simply a switch to turn DBI's DBD tracing on or off.  A
true value will turn it on and a false value will turn it off.  DBI's
"DBD" tracing was not available when ora_verbose was created and
ora_verbose adds an additional test to every trace test.

=head4 ora_oci_success_warn

Use this value to print otherwise silent OCI warnings that may happen
when an execute or fetch returns "Success With Info" or when you want
to tune RowCaching and LOB Reads

  $dbh->{ora_oci_success_warn} = 1;

=head4 ora_objects

Use this value to enable extended embedded oracle objects mode. In extended:

=over 4

=item 1

Embedded objects are returned as <DBD::Oracle::Object> instance (including type-name etc.) instead of simple ARRAY.

=item 2

Determine object type for each instance. All object attributes are returned (not only super-type's attributes).

=back

  $dbh->{ora_objects} = 1;

=head4 ora_ph_type

The default placeholder datatype for the database session.
The C<TYPE> or L</ora_type> attributes to L<DBI/bind_param> and
L<DBI/bind_param_inout> override the datatype for individual placeholders.
The most frequent reason for using this attribute is to permit trailing spaces
in values passed by placeholders.

Constants for the values allowed for this attribute can be imported using

  use DBD::Oracle qw(:ora_types);

Only the following values are permitted for this attribute.

=over 4

=item ORA_VARCHAR2

Oracle clients using OCI 8 will strip trailing spaces and allow embedded \0 bytes.
Oracle clients using OCI 9.2 do not strip trailing spaces and allow embedded \0 bytes.
This is the normal default placeholder type.

=item ORA_STRING

Do not strip trailing spaces and end the string at the first \0.

=item ORA_CHAR

Do not strip trailing spaces and allow embedded \0.
Force 'blank-padded comparison semantics'.

For example:

  use DBD::Oracle qw(:ora_types);

  $SQL="select username from all_users where username = ?";
  #username is a char(8)
  $sth=$dbh->prepare($SQL)";
  $sth->bind_param(1,'bloggs',{ ora_type => ORA_CHAR});

Will pad bloggs out to 8 characters and return the username.

=back

=head4 ora_parse_error_offset

If the previous error was from a failed C<prepare> due to a syntax error,
this attribute gives the offset into the C<Statement> attribute where the
error was found.

=head4 ora_array_chunk_size

Due to OCI limitations, DBD::Oracle needs to buffer up rows of
bind values in its C<execute_for_fetch> implementation. This attribute
sets the number of rows to buffer at a time (default value is 1000).

The C<execute_for_fetch> function will collect (at most) this many
rows in an array, send them off to the DB for execution, then go back
to collect the next chunk of rows and so on. This attribute can be
used to limit or extend the number of rows processed at a time.

Note that this attribute also applies to C<execute_array>, since that
method is implemented using C<execute_for_fetch>.

=head4 ora_connect_with_default_signals

Sometimes the Oracle client seems to change some of the signal
handlers of the process during the connect phase.  For instance, some
users have observed Perl's default C<$SIG{INT}> handler being ignored
after connecting to an Oracle database.  If this causes problems in
your application, set this attribute to an array reference of signals
you would like to be localized during the connect process.  Once the
connect is complete, the signal handlers should be returned to their
previous state.

For example:

  $dbh = DBI->connect ($dsn, $user, $passwd,
                       {ora_connect_with_default_signals => [ 'INT' ] });

NOTE disabling the signal handlers the OCI library sets up may affect
functionality in the OCI library.

NOTE If you are using connect_cached then the above example will lead
to DBI thinking each connection is different as an anonymous array reference
is being used. To avoid this when using connect_cached you are advised
to use:

  my @ora_default_signals = (...);
  $dbh = DBI->connect($dsn, $user, $passwd,
      {ora_connect_with_default_signals => \@ora_default_signals});

In more recent Perl versions you could possibly make use of new state
variables.

=head2 B<connect_cached>

Implemented by DBI, no driver-specific impact.
Please note that connect_cached has not been tested with DRCP.

=head2 B<data_sources>

  @data_sources = DBI->data_sources('Oracle');
  @data_sources = $dbh->data_sources();

Returns a list of available databases. You will have to set either the 'ORACLE_HOME' or
'TNS_ADMIN' environment value to retrieve this list.  It will read these values from
TNSNAMES.ORA file entries.

=head1 METHODS COMMON TO ALL HANDLES

For all of the methods below, B<$h> can be either a database handle (B<$dbh>)
or a statement handle (B<$sth>). Note that I<$dbh> and I<$sth> can be replaced with
any variable name you choose: these are just the names most often used. Another
common variable used in this documentation is $I<rv>, which stands for "return value".

=head2 B<err>

  $rv = $h->err;

Returns the error code from the last method called.

=head2 B<errstr>

  $str = $h->errstr;

Returns the last error that was reported by Oracle. Starting with "ORA-00000" code followed by the error message.

=head2 B<state>

  $str = $h->state;

Oracle hasn't supported SQLSTATE since the early versions OCI. It will return empty when the command succeeds and
'S1000' (General Error) for all other errors.

While this method can be called as either C<< $sth->state >> or C<< $dbh->state >>, it
is usually clearer to always use C<< $dbh->state >>.

=head2 B<trace>

Implemented by DBI, no driver-specific impact.

=head2 B<trace_msg>

Implemented by DBI, no driver-specific impact.

=head2 B<parse_trace_flag> and B<parse_trace_flags>

Implemented by DBI, no driver-specific impact.

=head2 B<func>

DBD::Oracle uses the C<func> method to support a variety of functions.

=head2 B<Private database handle functions>

Some of these functions are called through the method func()
which is described in the DBI documentation. Any function that begins with ora_
can be called directly.

=head2 B<plsql_errstr>

This function returns a string which describes the errors
from the most recent PL/SQL function, procedure, package,
or package body compile in a format similar to the output
of the SQL*Plus command 'show errors'.

The function returns undef if the error string could not
be retrieved due to a database error.
Look in $dbh->errstr for the cause of the failure.

If there are no compile errors, an empty string is returned.

Example:

    # Show the errors if CREATE PROCEDURE fails
    $dbh->{RaiseError} = 0;
    if ( $dbh->do( q{
        CREATE OR REPLACE PROCEDURE perl_dbd_oracle_test as
        BEGIN
            PROCEDURE filltab( stuff OUT TAB ); asdf
        END; } ) ) {} # Statement succeeded
    }
    elsif ( 6550 != $dbh->err ) { die $dbh->errstr; } # Utter failure
    else {
        my $msg = $dbh->func( 'plsql_errstr' );
        die $dbh->errstr if ! defined $msg;
        die $msg if $msg;
    }

=head2 B<dbms_output_enable / dbms_output_put / dbms_output_get>

These functions use the PL/SQL DBMS_OUTPUT package to store and
retrieve text using the DBMS_OUTPUT buffer.  Text stored in this buffer
by dbms_output_put or any PL/SQL block can be retrieved by
dbms_output_get or any PL/SQL block connected to the same database
session.

Stored text is not available until after dbms_output_put or the PL/SQL
block that saved it completes its execution.  This means you B<CAN NOT>
use these functions to monitor long running PL/SQL procedures.

Example 1:

  # Enable DBMS_OUTPUT and set the buffer size
  $dbh->{RaiseError} = 1;
  $dbh->func( 1000000, 'dbms_output_enable' );

  # Put text in the buffer . . .
  $dbh->func( @text, 'dbms_output_put' );

  # . . . and retrieve it later
  @text = $dbh->func( 'dbms_output_get' );

Example 2:

  $dbh->{RaiseError} = 1;
  $sth = $dbh->prepare(q{
    DECLARE tmp VARCHAR2(50);
    BEGIN
      SELECT SYSDATE INTO tmp FROM DUAL;
      dbms_output.put_line('The date is '||tmp);
    END;
  });
  $sth->execute;

  # retrieve the string
  $date_string = $dbh->func( 'dbms_output_get' );

=head2 B<dbms_output_enable ( [ buffer_size ] )>

This function calls DBMS_OUTPUT.ENABLE to enable calls to package
DBMS_OUTPUT procedures GET, GET_LINE, PUT, and PUT_LINE.  Calls to
these procedures are ignored unless DBMS_OUTPUT.ENABLE is called
first.

The buffer_size is the maximum amount of text that can be saved in the
buffer and must be between 2000 and 1,000,000.  If buffer_size is not
given, the default is 20,000 bytes.

=head2 B<dbms_output_put ( [ @lines ] )>

This function calls DBMS_OUTPUT.PUT_LINE to add lines to the buffer.

If all lines were saved successfully the function returns 1.  Depending
on the context, an empty list or undef is returned for failure.

If any line causes buffer_size to be exceeded, a buffer overflow error
is raised and the function call fails.  Some of the text might be in
the buffer.

=head2 B<dbms_output_get>

This function calls DBMS_OUTPUT.GET_LINE to retrieve lines of text from
the buffer.

In an array context, all complete lines are removed from the buffer and
returned as a list.  If there are no complete lines, an empty list is
returned.

In a scalar context, the first complete line is removed from the buffer
and returned.  If there are no complete lines, undef is returned.

Any text in the buffer after a call to DBMS_OUTPUT.GET_LINE or
DBMS_OUTPUT.GET is discarded by the next call to DBMS_OUTPUT.PUT_LINE,
DBMS_OUTPUT.PUT, or DBMS_OUTPUT.NEW_LINE.

=head2 B<reauthenticate ( $username, $password )>

Starts a new session against the current database using the credentials
supplied. Note that this does not work with DRCP.

=head2 B<private_attribute_info>

  $hashref = $dbh->private_attribute_info();
  $hashref = $sth->private_attribute_info();

Returns a hash of all private attributes used by DBD::Oracle, for either
a database or a statement handle. Currently, all the hash values are undef.

=head1 ATTRIBUTES COMMON TO ALL HANDLES

=head2 B<InactiveDestroy> (boolean)

Implemented by DBI, no driver-specific impact.

=head2 B<RaiseError> (boolean, inherited)

Forces errors to always raise an exception. Although it defaults to off, it is recommended that this
be turned on, as the alternative is to check the return value of every method (prepare, execute, fetch, etc.)
manually, which is easy to forget to do.

=head2 B<PrintError> (boolean, inherited)

Forces database errors to also generate warnings, which can then be filtered with methods such as
locally redefining I<$SIG{__WARN__}> or using modules such as C<CGI::Carp>. This attribute is on
by default.

=head2 B<ShowErrorStatement> (boolean, inherited)

Appends information about the current statement to error messages. If placeholder information
is available, adds that as well. Defaults to true.

=head2 B<Warn> (boolean, inherited)

Enables warnings. This is on by default, and should only be turned off in a local block
for a short a time only when absolutely needed.

=head2 B<Executed> (boolean, read-only)

Indicates if a handle has been executed. For database handles, this value is true after the L</do> method has been called, or
when one of the child statement handles has issued an L</execute>. Issuing a L</commit> or L</rollback> always resets the
attribute to false for database handles. For statement handles, any call to L</execute> or its variants will flip the value to
true for the lifetime of the statement handle.

=head2 B<TraceLevel> (integer, inherited)

Sets the trace level, similar to the L</trace> method. See the sections on
L</trace> and L</parse_trace_flag> for more details.

=head2 B<Active> (boolean, read-only)

Indicates if a handle is active or not. For database handles, this indicates if the database has
been disconnected or not. For statement handles, it indicates if all the data has been fetched yet
or not. Use of this attribute is not encouraged.

=head2 B<Kids> (integer, read-only)

Returns the number of child processes created for each handle type. For a driver handle, indicates the number
of database handles created. For a database handle, indicates the number of statement handles created. For
statement handles, it always returns zero, because statement handles do not create kids.

=head2 B<ActiveKids> (integer, read-only)

Same as C<Kids>, but only returns those that are active.

=head2 B<CachedKids> (hash ref)

Returns a hashref of handles. If called on a database handle, returns all statement handles created by use of the
C<prepare_cached> method. If called on a driver handle, returns all database handles created by the L</connect_cached>
method.

=head2 B<ChildHandles> (array ref)

Implemented by DBI, no driver-specific impact.

=head2 B<PrintWarn> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head2 B<HandleError> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head2 B<HandleSetErr> (code ref, inherited)

Implemented by DBI, no driver-specific impact.

=head2 B<ErrCount> (unsigned integer)

Implemented by DBI, no driver-specific impact.

=head2 B<FetchHashKeyName> (string, inherited)

Implemented by DBI, no driver-specific impact.

=head2 B<ChopBlanks> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head2 B<Taint> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head2 B<TaintIn> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head2 B<TaintOut> (boolean, inherited)

Implemented by DBI, no driver-specific impact.

=head2 B<Profile> (inherited)

Implemented by DBI, no driver-specific impact.

=head2 B<Type> (scalar)

Returns C<dr> for a driver handle, C<db> for a database handle, and C<st> for a statement handle.
Should be rarely needed.

=head2 B<LongReadLen>

The maximum size of long or longraw columns to retrieve. If one of
these columns is longer than LongReadLen then either a data truncation
error will be raised (LongTrunkOk is false) or the column will be
silently truncated (LongTruncOk is true).

DBI currently defaults this to 80.

=head2 B<LongTruncOk>

Implemented by DBI, no driver-specific impact.

=head2 B<CompatMode>

Type: boolean, inherited

The CompatMode attribute is used by emulation layers (such as Oraperl) to enable compatible behaviour in the underlying driver (e.g., DBD::Oracle) for this handle. Not normally set by application code.

It also has the effect of disabling the 'quick FETCH' of attribute values from the handles attribute cache. So all attribute values are handled by the drivers own FETCH method. This makes them slightly slower but is useful for special-purpose drivers like DBD::Multiplex.

=head1 ORACLE-SPECIFIC DATABASE HANDLE METHODS

=head2 B<ora_can_unicode ( [ $refresh ] )>

Returns a number indicating whether either of the database character sets
is a Unicode encoding. Calls ora_nls_parameters() and passes the optional
$refresh parameter to it.

0 = Neither character set is a Unicode encoding.

1 = National character set is a Unicode encoding.

2 = Database character set is a Unicode encoding.

3 = Both character sets are Unicode encodings.

=head2 B<ora_can_taf>

Returns true if the current connection supports TAF events. False if otherwise.

=head2 B<ora_nls_parameters ( [ $refresh ] )>

Returns a hash reference containing the current NLS parameters, as given
by the v$nls_parameters view. The values fetched are cached between calls.
To cause the latest values to be fetched, pass a true value to the function.

=head1 ORACLE-SPECIFIC DATABASE FUNCTIONS

=head2 B<ora_server_version>

  $versions = $dbh->func('ora_server_version');

Returns an array reference of server version strings e.g.,

  [11,2,0,2,0]

=head1 DATABASE HANDLE METHODS

=head2 B<selectall_arrayref>

  $ary_ref = $dbh->selectall_arrayref($sql);
  $ary_ref = $dbh->selectall_arrayref($sql, \%attr);
  $ary_ref = $dbh->selectall_arrayref($sql, \%attr, @bind_values);

Returns a reference to an array containing the rows returned by preparing and executing the SQL string.
See the DBI documentation for full details.

=head2 B<selectall_hashref>

  $hash_ref = $dbh->selectall_hashref($sql, $key_field);

Returns a reference to a hash containing the rows returned by preparing and executing the SQL string.
See the DBI documentation for full details.

=head2 B<selectcol_arrayref>

  $ary_ref = $dbh->selectcol_arrayref($sql, \%attr, @bind_values);

Returns a reference to an array containing the first column
from each rows returned by preparing and executing the SQL string. It is possible to specify exactly
which columns to return. See the DBI documentation for full details.

=head2 B<prepare>

  $sth = $dbh->prepare($statement, \%attr);

Prepares a statement for later execution by the database engine and returns a reference to a statement handle object.

=head3 B<Prepare Attributes>

These attributes may be used in the C<\%attr> parameter of the
L<DBI/prepare> database handle method.

=over 4

=item ora_placeholders

Set to false to disable processing of placeholders. Used mainly for loading a
PL/SQL package that has been I<wrapped> with Oracle's C<wrap> utility.

=item ora_auto_lob

If true (the default), fetching retrieves the contents of the CLOB or
BLOB column in most circumstances.  If false, fetching retrieves the
Oracle "LOB Locator" of the CLOB or BLOB value.

See L</LOBS AND LONGS> for more details.

See also the LOB tests in 05dbi.t of Oracle::OCI for examples
of how to use LOB Locators.

=item ora_pers_lob

If true the L</Simple Fetch for CLOBs and BLOBs> method for the L</Data Interface for Persistent LOBs> will be
used for LOBs rather than the default method L</Data Interface for LOB Locators>.

=item ora_clbk_lob

If true the L</Piecewise Fetch with Callback> method for the L</Data
Interface for Persistent LOBs> will be used for LOBs.

=item ora_piece_lob

If true the L</Piecewise Fetch with Polling> method for the L</Data
Interface for Persistent LOBs> will be used for LOBs.

=item ora_piece_size

This is the max piece size for the L</Piecewise Fetch with Callback>
and L</Piecewise Fetch with Polling> methods, in chars for CLOBS, and
bytes for BLOBS.

=item ora_check_sql

If 1 (default), force SELECT statements to be described in prepare().
If 0, allow SELECT statements to defer describe until execute().

See L</Prepare Postponed Till Execute> for more information.

=item ora_exe_mode

This will set the execute mode of the current statement. Presently
only one mode is supported;

  OCI_STMT_SCROLLABLE_READONLY - make result set scrollable

See L</SCROLLABLE CURSORS> for more details.

=item ora_prefetch_rows

Sets the number of rows to be prefetched. If it is not set, then the
default value is 1.  See L</Row Prefetching> for more details.

=item ora_prefetch_memory

Sets the memory level for rows to be prefetched. The application then
fetches as many rows as will fit into that much memory.  See L</Row
Prefetching> for more details.

=item ora_row_cache_off

By default DBD::Oracle will use a row cache when fetching to cut down
the number of round trips to the server. If you do not want to use an
array fetch set this value to any value other than 0;

See L</Row Prefetching> for more details.

=back

=head3 B<Placeholders>

There are three types of placeholders that can be used in
DBD::Oracle.

The first is the "question mark" type, in which each placeholder is
represented by a single question mark character. This is the method
recommended by the DBI and is the most portable. Each question
mark is internally replaced by a "dollar sign number" in the order in
which they appear in the query (important when using L</bind_param>).

The second type of placeholder is "named parameters" in the format
":foo" which is the one Oracle prefers.

   $dbh->{RaiseError} = 1;        # save having to check each method call
   $sth = $dbh->prepare("SELECT name, age FROM people WHERE name LIKE :name");
   $sth->bind_param(':name', "John%");
   $sth->execute;
   DBI::dump_results($sth);

Note when calling bind_param with named parameters you must include
the leading colon. The advantage of this placeholder type is that you
can use the same placeholder more than once in the same SQL statement
but you only need to bind it once.

The last placeholder type is a variation of the two above where you
name each placeholder :N (where N is a number). Like the named
placeholders above you can use the same placeholder multiple times in
the SQL but when you call bind_param you only need to pass the N
(e.g., for :1 you use bind_param(1,...) and not bind_param(':1',...).

The different types of placeholders cannot be mixed within a statement, but you may
use different ones for each statement handle you have. This is confusing at best, so
stick to one style within your program.

=head2 B<prepare_cached>

  $sth = $dbh->prepare_cached($statement, \%attr);

Implemented by DBI, no driver-specific impact. This method is most useful
if the same query is used over and over as it will cut down round trips to the server.

=head2 B<do>

  $rv = $dbh->do($statement);
  $rv = $dbh->do($statement, \%attr);
  $rv = $dbh->do($statement, \%attr, @bind_values);

Prepare and execute a single statement. Returns the number of rows affected if the
query was successful, returns undef if an error occurred, and returns -1 if the
number of rows is unknown or not available. Note that this method will return B<0E0> instead
of 0 for 'no rows were affected', in order to always return a true value if no error occurred.

=head2 B<last_insert_id>

Oracle does not implement auto_increment of serial type columns it uses predefined
sequences where the id numbers are either selected before insert, at insert time with a trigger,
 or as part of the query.

Below is an example of you to use the latter with the SQL returning clause to get the ID number back
on insert with the bind_param_inout method.
.

  $dbh->do('CREATE SEQUENCE lii_seq START 1');
  $dbh->do(q{CREATE TABLE lii (
    foobar INTEGER NOT NULL UNIQUE,
    baz VARCHAR)});
  $SQL = "INSERT INTO lii (foobar,baz) VALUES (lii_seq.nextval,'XX') returning foobar into :p_new_id";";
  $sth = $dbh->prepare($SQL);
  my $p_new_id='-1';
  $sth->bind_param_inout(":p_new_id",\$p_new_id,38);
  $sth->execute();
  $db->commit();

=head2 B<commit>

  $rv = $dbh->commit;

Issues a COMMIT to the server, indicating that the current transaction is finished and that
all changes made will be visible to other processes. If AutoCommit is enabled, then
a warning is given and no COMMIT is issued. Returns true on success, false on error.

=head2 B<rollback>

  $rv = $dbh->rollback;

Issues a ROLLBACK to the server, which discards any changes made in the current transaction. If AutoCommit
is enabled, then a warning is given and no ROLLBACK is issued. Returns true on success, and
false on error.

=head2 B<begin_work>

This method turns on transactions until the next call to L</commit> or L</rollback>, if L</AutoCommit> is
currently enabled. If it is not enabled, calling begin_work will issue an error. Note that the
transaction will not actually begin until the first statement after begin_work is called.

=head2 B<disconnect>

  $rv = $dbh->disconnect;

Disconnects from the Oracle database. Any uncommitted changes will be rolled back upon disconnection. It's
good policy to always explicitly call commit or rollback at some point before disconnecting, rather than
relying on the default rollback behavior.

If the script exits before disconnect is called (or, more precisely, if the database handle is no longer
referenced by anything), then the database handle's DESTROY method will call the rollback() and disconnect()
methods automatically. It is best to explicitly disconnect rather than rely on this behavior.

=head2 B<ping>

  $rv = $dbh->ping;

This C<ping> method is used to check the validity of a database handle. The value returned is
either 0, indicating that the connection is no longer valid, or 1, indicating the connection is valid.
This function does 1 round trip to the Oracle Server.

=head2 B<get_info()>

 $value = $dbh->get_info($info_type);

DBD::Oracle supports C<get_info()>, but (currently) only a few info types.

=head2 B<table_info()>

DBD::Oracle supports attributes for C<table_info()>.

In Oracle, the concept of I<user> and I<schema> is (currently) the
same. Because database objects are owned by an user, the owner names
in the data dictionary views correspond to schema names.
Oracle does not support catalogues so TABLE_CAT is ignored as
selection criterion.

Search patterns are supported for TABLE_SCHEM and TABLE_NAME.

TABLE_TYPE may contain a comma-separated list of table types.
The following table types are supported:

  TABLE
  VIEW
  SYNONYM
  SEQUENCE

The result set is ordered by TABLE_TYPE, TABLE_SCHEM, TABLE_NAME.

The special enumerations of catalogues, schemas and table types are
supported. However, TABLE_CAT is always NULL.

An identifier is passed I<as is>, i.e. as the user provides or
Oracle returns it.
C<table_info()> performs a case-sensitive search. So, a selection
criterion should respect upper and lower case.
Normally, an identifier is case-insensitive. Oracle stores and
returns it in upper case. Sometimes, database objects are created
with quoted identifiers (for reserved words, mixed case, special
characters, ...). Such an identifier is case-sensitive (if not all
upper case). Oracle stores and returns it as given.
C<table_info()> has no special quote handling, neither adds nor
removes quotes.

=head2 B<primary_key_info()>

Oracle does not support catalogues so TABLE_CAT is ignored as
selection criterion.
The TABLE_CAT field of a fetched row is always NULL (undef).
See L</table_info()> for more detailed information.

If the primary key constraint was created without an identifier,
PK_NAME contains a system generated name with the form SYS_Cn.

The result set is ordered by TABLE_SCHEM, TABLE_NAME, KEY_SEQ.

An identifier is passed I<as is>, i.e. as the user provides or
Oracle returns it.
See L</table_info()> for more detailed information.

=head2 B<foreign_key_info()>

This method (currently) supports the extended behaviour of SQL/CLI, i.e. the
result set contains foreign keys that refer to primary B<and> alternate keys.
The field UNIQUE_OR_PRIMARY distinguishes these keys.

Oracle does not support catalogues, so C<$pk_catalog> and C<$fk_catalog> are
ignored as selection criteria (in the new style interface).
The UK_TABLE_CAT and FK_TABLE_CAT fields of a fetched row are always
NULL (undef).
See L</table_info()> for more detailed information.

If the primary or foreign key constraints were created without an identifier,
UK_NAME or FK_NAME contains a system generated name with the form SYS_Cn.

The UPDATE_RULE field is always 3 ('NO ACTION'), because Oracle (currently)
does not support other actions.

The DELETE_RULE field may contain wrong values. This is a known Bug (#1271663)
in Oracle's data dictionary views. Currently (as of 8.1.7), 'RESTRICT' and
'SET DEFAULT' are not supported, 'CASCADE' is mapped correctly and all other
actions (incl. 'SET NULL') appear as 'NO ACTION'.

The DEFERABILITY field is always NULL, because this columns is
not present in the ALL_CONSTRAINTS view of older Oracle releases.

The result set is ordered by UK_TABLE_SCHEM, UK_TABLE_NAME, FK_TABLE_SCHEM,
FK_TABLE_NAME, ORDINAL_POSITION.

An identifier is passed I<as is>, i.e. as the user provides or
Oracle returns it.
See L</table_info()> for more detailed information.

=head2 B<column_info()>

Oracle does not support catalogues so TABLE_CAT is ignored as
selection criterion.
The TABLE_CAT field of a fetched row is always NULL (undef).
See L</table_info()> for more detailed information.

The CHAR_OCTET_LENGTH field is (currently) always NULL (undef).

Don't rely on the values of the BUFFER_LENGTH field!
Especially the length of FLOATs may be wrong.

Datatype codes for non-standard types are subject to change.

Attention! The DATA_DEFAULT (COLUMN_DEF) column is of type LONG so you
may have to set LongReadLen on the connection handle before calling
column_info if you have a large default column. After DBD::Oracle 1.40
LongReadLen is set automatically to 1Mb when calling column_info and
reset aftwerwards.

The result set is ordered by TABLE_SCHEM, TABLE_NAME, ORDINAL_POSITION.

An identifier is passed I<as is>, i.e. as the user provides or
Oracle returns it.
See L</table_info()> for more detailed information.

It is possible with Oracle to make the names of the various DB objects (table,column,index etc)
case sensitive.

  alter table bloggind add ("Bla_BLA" NUMBER)

So in the example the exact case "Bla_BLA" must be used to get it info on the column. While this

 alter table bloggind add (Bla_BLA NUMBER)

any case can be used to get info on the column.

=head2 B<statistics_info()>

Oracle does not support catalogues so TABLE_CAT is ignored as
selection criterion.
The TABLE_CAT field of a fetched row is always NULL (undef).
See L</table_info()> for more detailed information.

The INDEX_QUALIFIER field of a fetched row is always NULL (undef),
for the same reason as for TABLE_CAT.

If an index was created without an identifier
(e.g. in the course of a PK creation),
INDEX_NAME contains a system generated name with the form SYS_.

COLUMN_NAME may contain a system generated name
(e.g. for function-based indexes).

For the TYPE column, a simple mapping is used:

  NORMAL   btree
  CLUSTER  clustered
  ...      other

The C<$quick> parameter is currently ignored.
The method uses the dictionary with the gathered statistics,
thus cannot ensure that the values for CARDINALITY and PAGES are current.

The result set is ordered by
NON_UNIQUE, TYPE, INDEX_QUALIFIER, INDEX_NAME, ORDINAL_POSITION.

An identifier is passed I<as is>, i.e. as the user provides or
Oracle returns it.
See L</table_info()> for more detailed information.

=head2 B<selectrow_array>

  @row_ary = $dbh->selectrow_array($sql);
  @row_ary = $dbh->selectrow_array($sql, \%attr);
  @row_ary = $dbh->selectrow_array($sql, \%attr, @bind_values);

Returns an array of row information after preparing and executing the provided SQL string. The rows are returned
by calling L</fetchrow_array>. The string can also be a statement handle generated by a previous prepare. Note that
only the first row of data is returned. If called in a scalar context, only the first column of the first row is
returned. Because this is not portable, it is not recommended that you use this method in that way.

=head2 B<selectrow_arrayref>

  $ary_ref = $dbh->selectrow_arrayref($statement);
  $ary_ref = $dbh->selectrow_arrayref($statement, \%attr);
  $ary_ref = $dbh->selectrow_arrayref($statement, \%attr, @bind_values);

Exactly the same as L</selectrow_array>, except that it returns a reference to an array, by internal use of
the L</fetchrow_arrayref> method.

=head2 B<selectrow_hashref>

  $hash_ref = $dbh->selectrow_hashref($sql);
  $hash_ref = $dbh->selectrow_hashref($sql, \%attr);
  $hash_ref = $dbh->selectrow_hashref($sql, \%attr, @bind_values);

Exactly the same as L</selectrow_array>, except that it returns a reference to an hash, by internal use of
the L</fetchrow_hashref> method.

=head2 B<clone>

  $other_dbh = $dbh->clone();

Creates a copy of the database handle by connecting with the same parameters as the original
handle, then trying to merge the attributes. See the DBI documentation for complete usage.

=head1 DATABASE HANDLE ATTRIBUTES

=head2 B<AutoCommit> (boolean)

Supported by DBD::Oracle as proposed by DBI.The default of AutoCommit is on, but this may change
in the future, so it is highly recommended that you explicitly set it when
calling L</connect>.

=head2 B<ReadOnly> (boolean)

  $dbh->{ReadOnly} = 1;

Specifies if the current database connection should be in read-only mode or not.

Please not that this method is not foolproof: there are still ways to update the
database. Consider this a safety net to catch applications that should not be
issuing commands such as INSERT, UPDATE, or DELETE.

This method method requires DBI version 1.55 or better.

=head2 B<Name> (string, read-only)

Returns the name of the current database. This is the same as the DSN, without the
"dbi:Oracle:" part.

=head2 B<Username> (string, read-only)

Returns the name of the user connected to the database.

=head2 B<Driver> (handle, read-only)

Holds the handle of the parent driver. The only recommended use for this is to find the name
of the driver using:

  $dbh->{Driver}->{Name}

=head2 B<RowCacheSize>

DBD::Oracle supports both Server pre-fetch and Client side row caching. By default both
are turned on to give optimum performance. Most of the time one can just let DBD::Oracle
figure out the best optimization.

=head3 B<Row Caching>

Row caching occurs on the client side and the object of it is to cut down the number of round
trips made to the server when fetching rows. At each fetch a set number of rows will be retrieved
from the server and stored locally. Further calls the server are made only when the end of the
local buffer(cache) is reached.

Rows up to the specified top level row
count C<RowCacheSize> are fetched if it occupies no more than the specified memory usage limit.
The default value is 0, which means that memory size is not included in computing the number of rows to prefetch. If
the C<RowCacheSize> value is set to a negative number then the positive value of RowCacheSize is used
to compute the number of rows to prefetch.

By default C<RowCacheSize> is automatically set. If you want to totally turn off prefetching set this to 1.

For any SQL statement that contains a LOB, Long or Object Type Row Caching will be turned off. However server side
caching still works.  If you are only selecting a LOB Locator then Row Caching will still work.

=head3 Row Prefetching

Row prefetching occurs on the server side and uses the DBI database handle attribute C<RowCacheSize> and or the
Prepare Attribute 'ora_prefetch_memory'. Tweaking these values may yield improved performance.

  $dbh->{RowCacheSize} = 100;
  $sth=$dbh->prepare($SQL,{ora_exe_mode=>OCI_STMT_SCROLLABLE_READONLY,ora_prefetch_memory=>10000});

In the above example 10 rows will be prefetched up to a maximum of 10000 bytes of data.  The Oracle Call Interface Programmer's Guide,
suggests a good row cache value for a scrollable cursor is about 20% of expected size of the record set.

The prefetch settings tell the DBD::Oracle to grab x rows (or x-bytes) when it needs to get new rows. This happens on the first
fetch that sets the current_positon to any value other than 0. In the above example if we do a OCI_FETCH_FIRST the first 10 rows are
loaded into the buffer and DBD::Oracle will not have to go back to the server for more rows. When record 11 is fetched DBD::Oracle
fetches and returns this row and the next 9 rows are loaded into the buffer. In this case if you fetch backwards from 10 to 1
no server round trips are made.

With large record sets it is best not to attempt to go to the last record as this may take some time, A large buffer size might even slow down
the fetch. If you must get the number of rows in a large record set you might try using an few large OCI_FETCH_ABSOLUTEs and then an OCI_FETCH_LAST,
this might save some time. So if you had a record set of 10000 rows and you set the buffer to 5000 and did a OCI_FETCH_LAST one would fetch the first 5000 rows into the buffer then the next 5000 rows.
If one requires only the first few rows there is no need to set a large prefetch value.

If the ora_prefetch_memory less than 1 or not present then memory size is not included in computing the
number of rows to prefetch otherwise the number of rows will be limited to memory size. Likewise if the RowCacheSize is less than 1 it
is not included in the computing of the prefetch rows.

=head1 ORACLE-SPECIFIC STATEMENT HANDLE METHODS

=head2 B<ora_stmt_type>

Returns the OCI Statement Type number for the SQL of a statement handle.

=head2 B<ora_stmt_type_name>

Returns the OCI Statement Type name for the SQL of a statement handle.

=head1 DBI STATEMENT HANDLE OBJECT METHODS

=head2 B<bind_param>

  $rv = $sth->bind_param($param_num, $bind_value);
  $rv = $sth->bind_param($param_num, $bind_value, $bind_type);
  $rv = $sth->bind_param($param_num, $bind_value, \%attr);

Allows the user to bind a value and/or a data type to a placeholder.

The value of C<$param_num> is a number if using the '?' or if using ":foo" style placeholders, the complete name
(e.g. ":foo") must be given.
The C<$bind_value> argument is fairly self-explanatory. A value of C<undef> will
bind a C<NULL> to the placeholder. Using C<undef> is useful when you want
to change just the type and will be overwriting the value later.
(Any value is actually usable, but C<undef> is easy and efficient).

The C<\%attr> hash is used to indicate the data type of the placeholder.
The default value is "varchar". If you need something else, you must
use one of the values provided by DBI or by DBD::Oracle. To use a SQL value,
modify your "use DBI" statement at the top of your script as follows:

  use DBI qw(:sql_types);

This will import some constants into your script. You can plug those
directly into the L</bind_param> call. Some common ones that you will
encounter are:

  SQL_INTEGER

To use Oracle SQL data types, import the list of values like this:

  use DBD::Oracle qw(:ora_types);

You can then set the data types by setting the value of the C<ora_type>
key in the hash passed to L</bind_param>.
The current list of Oracle data types exported is:

  ORA_VARCHAR2 ORA_STRING ORA_NUMBER ORA_LONG ORA_ROWID ORA_DATE ORA_RAW
  ORA_LONGRAW ORA_CHAR ORA_CHARZ ORA_MLSLABEL ORA_XMLTYPE ORA_CLOB ORA_BLOB
  ORA_RSET ORA_VARCHAR2_TABLE ORA_NUMBER_TABLE SQLT_INT SQLT_FLT ORA_OCI
  SQLT_CHR SQLT_BIN

Data types are "sticky," in that once a data type is set to a certain placeholder,
it will remain for that placeholder, unless it is explicitly set to something
else afterwards. If the statement has already been prepared, and you switch the
data type to something else, DBD::Oracle will re-prepare the statement for you before
doing the next execute.

Examples:

  use DBI qw(:sql_types);
  use DBD::Oracle qw(:ora_types);

  $SQL = "SELECT id FROM ptable WHERE size > ? AND title = ?";
  $sth = $dbh->prepare($SQL);

  ## Both arguments below are bound to placeholders as "varchar"
  $sth->execute(123, "Merk");

  ## Reset the datatype for the first placeholder to an integer
  $sth->bind_param(1, undef, SQL_INTEGER);

  ## The "undef" bound above is not used, since we supply params to execute
  $sth->execute(123, "Merk");

  ## Set the first placeholder's value and data type
  $sth->bind_param(1, 234, { pg_type => ORA_NUMBER });

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

These attributes may be used in the C<\%attr> parameter of the
L<DBI/bind_param> or L<DBI/bind_param_inout> statement handle methods.

=over 4

=item ora_type

Specify the placeholder's datatype using an Oracle datatype.
A fatal error is raised if C<ora_type> and the DBI C<TYPE> attribute
are used for the same placeholder.
Some of these types are not supported by the current version of
DBD::Oracle and will cause a fatal error if used.
Constants for the Oracle datatypes may be imported using

  use DBD::Oracle qw(:ora_types);

Potentially useful values when DBD::Oracle was built using OCI 7 and later:

  ORA_VARCHAR2, ORA_STRING, ORA_LONG, ORA_RAW, ORA_LONGRAW,
  ORA_CHAR, ORA_MLSLABEL, ORA_RSET

Additional values when DBD::Oracle was built using OCI 8 and later:

  ORA_CLOB, ORA_BLOB, ORA_XMLTYPE, ORA_VARCHAR2_TABLE, ORA_NUMBER_TABLE

Additional values when DBD::Oracle was built using OCI 9.2 and later:

  SQLT_CHR, SQLT_BIN

See L</Binding Cursors> for the correct way to use ORA_RSET.

See L</LOBS AND LONGS> for how to use ORA_CLOB and ORA_BLOB.

See L</SYS.DBMS_SQL datatypes> for ORA_VARCHAR2_TABLE, ORA_NUMBER_TABLE.

See L</Data Interface for Persistent LOBs> for the correct way to use SQLT_CHR and SQLT_BIN.

See L</OTHER DATA TYPES> for more information.

See also L<DBI/Placeholders and Bind Values>.

=item ora_csform

Specify the OCI_ATTR_CHARSET_FORM for the bind value. Valid values
are SQLCS_IMPLICIT (1) and SQLCS_NCHAR (2). Both those constants can
be imported from the DBD::Oracle module. Rarely needed.

=item ora_csid

Specify the I<integer> OCI_ATTR_CHARSET_ID for the bind value.
Character set names can't be used currently.

=item ora_maxdata_size

Specify the integer OCI_ATTR_MAXDATA_SIZE for the bind value.
May be needed if a character set conversion from client to server
causes the data to use more space and so fail with a truncation error.

=item ora_maxarray_numentries

Specify the maximum number of array entries to allocate. Used with
ORA_VARCHAR2_TABLE, ORA_NUMBER_TABLE. Define the maximum number of
array entries Oracle can pass back to you in OUT variable of type
TABLE OF ... .

=item ora_internal_type

Specify internal data representation. Currently is supported only for
ORA_NUMBER_TABLE.

=back

=head3 Optimizing Results

=head4 Prepare Postponed Till Execute

The DBD::Oracle module can avoid an explicit 'describe' operation
prior to the execution of the statement unless the application requests
information about the results (such as $sth->{NAME}). This reduces
communication with the server and increases performance (reducing the
number of PARSE_CALLS inside the server).

However, it also means that SQL errors are not detected until
C<execute()> (or $sth->{NAME} etc) is called instead of when
C<prepare()> is called. Note that if the describe is triggered by the
use of $sth->{NAME} or a similar attribute and the describe fails then
I<an exception is thrown> even if C<RaiseError> is false!

Set L</ora_check_sql> to 0 in prepare() to enable this behaviour.

=head2 B<bind_param_inout>

  $rv = $sth->bind_param_inout($param_num, \$scalar, 0);

DBD::Oracle fully supports bind_param_inout below are some uses for this method.

=head3 B<Returning A Value from an INSERT>

Oracle supports an extended SQL insert syntax which will return one
or more of the values inserted. This can be particularly useful for
single-pass insertion of values with re-used sequence values
(avoiding a separate "select seq.nextval from dual" step).

  $sth = $dbh->prepare(qq{
      INSERT INTO foo (id, bar)
      VALUES (foo_id_seq.nextval, :bar)
      RETURNING id INTO :id
  });
  $sth->bind_param(":bar", 42);
  $sth->bind_param_inout(":id", \my $new_id, 99);
  $sth->execute;
  print "The id of the new record is $new_id\n";

If you have many columns to bind you can use code like this:

  @params = (... column values for record to be inserted ...);
  $sth->bind_param($_, $params[$_-1]) for (1..@params);
  $sth->bind_param_inout(@params+1, \my $new_id, 99);
  $sth->execute;

If you have many rows to insert you can take advantage of Oracle's built in execute array feature
with code like this:

  my @in_values=('1',2,'3','4',5,'6',7,'8',9,'10');
  my @out_values;
  my @status;
  my $sth = $dbh->prepare(qq{
        INSERT INTO foo (id, bar)
        VALUES (foo_id_seq.nextval, ?)
        RETURNING id INTO ?
  });
  $sth->bind_param_array(1,\@in_values);
  $sth->bind_param_inout_array(2,\@out_values,0,{ora_type => ORA_VARCHAR2});
  $sth->execute_array({ArrayTupleStatus=>\@status}) or die "error inserting";
  foreach my $id (@out_values){
        print 'returned id='.$id.'\n';
  }

Which will return all the ids into @out_values.

=over

=item B<Note:>

=item This will only work for numbered (?) placeholders,

=item The third parameter of bind_param_inout_array, (0 in the example), "maxlen" is required by DBI but not used by DBD::Oracle

=item The "ora_type" attribute is not needed but only ORA_VARCHAR2 will work.

=back

=head3 Returning A Recordset

DBD::Oracle does not currently support binding a PL/SQL table (aka array)
as an IN OUT parameter to any Perl data structure.  You cannot therefore call
a PL/SQL function or procedure from DBI that uses a non-atomic datatype as
either a parameter, or a return value.  However, if you are using Oracle 9.0.1
or later, you can make use of table (or pipelined) functions.

For example, assume you have the existing PL/SQL Package :

  CREATE OR REPLACE PACKAGE Array_Example AS
    --
    TYPE tRec IS RECORD (
        Col1    NUMBER,
        Col2    VARCHAR2 (10),
        Col3    DATE) ;
    --
    TYPE taRec IS TABLE OF tRec INDEX BY BINARY_INTEGER ;
    --
    FUNCTION Array_Func RETURN taRec ;
    --
  END Array_Example ;

  CREATE OR REPLACE PACKAGE BODY Array_Example AS
  --
  FUNCTION Array_Func RETURN taRec AS
  --
    l_Ret       taRec ;
  --
  BEGIN
    FOR i IN 1 .. 5 LOOP
        l_Ret (i).Col1 := i ;
        l_Ret (i).Col2 := 'Row : ' || i ;
        l_Ret (i).Col3 := TRUNC (SYSDATE) + i ;
    END LOOP ;
    RETURN l_Ret ;
  END ;
  --
  END Array_Example ;
  /

Currently, there is no way to directly call the function
Array_Example.Array_Func from DBI.  However, by making the following relatively
painless additions, its not only possible, but extremely efficient.

First, you need to create database object types that correspond to the record
and table types in the package.  From the above example, these would be :

  CREATE OR REPLACE TYPE tArray_Example__taRec
  AS OBJECT (
      Col1    NUMBER,
      Col2    VARCHAR2 (10),
      Col3    DATE
  ) ;

  CREATE OR REPLACE TYPE taArray_Example__taRec
  AS TABLE OF tArray_Example__taRec ;

Now, assuming the existing function needs to remain unchanged (it is probably
being called from other PL/SQL code), we need to add a new function to the
package.  Here's the new package specification and body :

  CREATE OR REPLACE PACKAGE Array_Example AS
      --
      TYPE tRec IS RECORD (
          Col1    NUMBER,
          Col2    VARCHAR2 (10),
          Col3    DATE) ;
      --
      TYPE taRec IS TABLE OF tRec INDEX BY BINARY_INTEGER ;
      --
      FUNCTION Array_Func RETURN taRec ;
      FUNCTION Array_Func_DBI RETURN taArray_Example__taRec PIPELINED ;
      --
  END Array_Example ;

  CREATE OR REPLACE PACKAGE BODY Array_Example AS
  --
  FUNCTION Array_Func RETURN taRec AS
      l_Ret  taRec ;
  BEGIN
      FOR i IN 1 .. 5 LOOP
          l_Ret (i).Col1 := i ;
          l_Ret (i).Col2 := 'Row : ' || i ;
          l_Ret (i).Col3 := TRUNC (SYSDATE) + i ;
      END LOOP ;
      RETURN l_Ret ;
  END ;

  FUNCTION Array_Func_DBI RETURN taArray_Example__taRec PIPELINED AS
      l_Set  taRec ;
  BEGIN
      l_Set := Array_Func ;
      FOR i IN l_Set.FIRST .. l_Set.LAST LOOP
          PIPE ROW (
              tArray_Example__taRec (
                 l_Set (i).Col1,
                 l_Set (i).Col2,
                 l_Set (i).Col3
              )
          ) ;
      END LOOP ;
      RETURN ;
  END ;
  --
  END Array_Example ;

As you can see, the new function is very simple.  Now, it is a simple matter
of calling the function as a straight-forward SELECT from your DBI code.  From
the above example, the code would look something like this :

  my $sth = $dbh->prepare('SELECT * FROM TABLE(Array_Example.Array_Func_DBI)');
  $sth->execute;
  while ( my ($col1, $col2, $col3) = $sth->fetchrow_array {
    ...
  }

=head3 B<SYS.DBMS_SQL datatypes>

DBD::Oracle has built-in support for B<SYS.DBMS_SQL.VARCHAR2_TABLE>
and B<SYS.DBMS_SQL.NUMBER_TABLE> datatypes. The simple example is here:

    my $statement='
    DECLARE
            tbl       SYS.DBMS_SQL.VARCHAR2_TABLE;
    BEGIN
            tbl := :mytable;
            :cc := tbl.count();
            tbl(1) := \'def\';
            tbl(2) := \'ijk\';
            :mytable := tbl;
    END;
    ';

    my $sth=$dbh->prepare( $statement );

    my @arr=( "abc","efg","hij" );

    $sth->bind_param_inout(":mytable", \\@arr, 10, {
            ora_type => ORA_VARCHAR2_TABLE,
            ora_maxarray_numentries => 100
    } ) ;
    $sth->bind_param_inout(":cc", \$cc, 100  );
    $sth->execute();
    print        "Result: cc=",$cc,"\n",
            "\tarr=",Data::Dumper::Dumper(\@arr),"\n";

=over

=item B<Note:>

=item Take careful note that we use '\\@arr' here because  the 'bind_param_inout'
   will only take a reference to a scalar.

=back

=head3 B<ORA_VARCHAR2_TABLE>

SYS.DBMS_SQL.VARCHAR2_TABLE object is always bound to array reference.
( in bind_param() and bind_param_inout() ). When you bind array, you need
to specify full buffer size for OUT data. So, there are two parameters:
I<max_len> (specified as 3rd argument of bind_param_inout() ),
and I<ora_maxarray_numentries>. They define maximum array entry length and
maximum rows, that can be passed to Oracle and back to you. In this
example we send array with 1 element with length=3, but allocate space for 100
Oracle array entries with maximum length 10 of each. So, you can get no more
than 100 array entries with length <= 10.

If you set I<max_len> to zero, maximum array entry length is calculated
as maximum length of entry of array bound. If 0 < I<max_len> < length( $some_element ),
truncation occur.

If you set I<ora_maxarray_numentries> to zero, current (at bind time) bound
array length is used as maximum. If 0 < I<ora_maxarray_numentries> < scalar(@array),
not all array entries are bound.

=head3 B<ORA_NUMBER_TABLE>

SYS.DBMS_SQL.NUMBER_TABLE object handling is much alike ORA_VARCHAR2_TABLE.
The main difference is internal data representation. Currently 2 types of
bind is allowed : as C-integer, or as C-double type. To select one of them,
you may specify additional bind parameter I<ora_internal_type> as either
B<SQLT_INT> or B<SQLT_FLT> for C-integer and C-double types.
Integer size is architecture-specific and is usually 32 or 64 bit.
Double is standard IEEE 754 type.

I<ora_internal_type> defaults to double (SQLT_FLT).

I<max_len> is ignored for OCI_NUMBER_TABLE.

Currently, you cannot bind full native Oracle NUMBER(38). If you really need,
send request to dbi-dev list.

The usage example is here:

    $statement='
    DECLARE
            tbl     SYS.DBMS_SQL.NUMBER_TABLE;
    BEGIN
            tbl := :mytable;
            :cc := tbl(2);
            tbl(4) := -1;
            tbl(5) := -2;
            :mytable := tbl;
    END;
    ';

    $sth=$dbh->prepare( $statement );

    if( ! defined($sth) ){
            die "Prepare error: ",$dbh->errstr,"\n";
    }

    @arr=( 1,"2E0","3.5" );

    # note, that ora_internal_type defaults to SQLT_FLT for ORA_NUMBER_TABLE .
    if( not $sth->bind_param_inout(":mytable", \\@arr, 10, {
                    ora_type => ORA_NUMBER_TABLE,
                    ora_maxarray_numentries => (scalar(@arr)+2),
                    ora_internal_type => SQLT_FLT
              } ) ){
            die "bind :mytable error: ",$dbh->errstr,"\n";
    }
    $cc=undef;
    if( not $sth->bind_param_inout(":cc", \$cc, 100 ) ){
            die "bind :cc error: ",$dbh->errstr,"\n";
    }

    if( not $sth->execute() ){
            die "Execute failed: ",$dbh->errstr,"\n";
    }
    print   "Result: cc=",$cc,"\n",
            "\tarr=",Data::Dumper::Dumper(\@arr),"\n";

The result is like:

    Result: cc=2
            arr=$VAR1 = [
              '1',
              '2',
              '3.5',
              '-1',
              '-2'
            ];

If you change bind type to B<SQLT_INT>, like:

    ora_internal_type => SQLT_INT

you get:

    Result: cc=2
            arr=$VAR1 = [
              1,
              2,
              3,
              -1,
              -2
            ];

=head2 B<bind_param_inout_array>

DBD::Oracle supports this undocumented feature of DBI. See L</Returning A Value from an INSERT> for an example.

=head2 B<bind_param_array>

  $rv = $sth->bind_param_array($param_num, $array_ref_or_value)
  $rv = $sth->bind_param_array($param_num, $array_ref_or_value, $bind_type)
  $rv = $sth->bind_param_array($param_num, $array_ref_or_value, \%attr)

Binds an array of values to a placeholder, so that each is used in turn by a call
to the L</execute_array> method.

=head2 B<execute>

  $rv = $sth->execute(@bind_values);

Perform whatever processing is necessary to execute the prepared statement.

=head2 B<execute_array>

  $tuples = $sth->execute_array() or die $sth->errstr;
  $tuples = $sth->execute_array(\%attr) or die $sth->errstr;
  $tuples = $sth->execute_array(\%attr, @bind_values) or die $sth->errstr;

  ($tuples, $rows) = $sth->execute_array(\%attr) or die $sth->errstr;
  ($tuples, $rows) = $sth->execute_array(\%attr, @bind_values) or die $sth->errstr;

Execute a prepared statement once for each item in a passed-in hashref, or items that
were previously bound via the L</bind_param_array> method. See the DBI documentation
for more details.

DBD::Oracle takes full advantage of OCI's array interface so inserts and updates using this interface will run very
quickly.

=head2 B<execute_for_fetch>

  $tuples = $sth->execute_for_fetch($fetch_tuple_sub);
  $tuples = $sth->execute_for_fetch($fetch_tuple_sub, \@tuple_status);

  ($tuples, $rows) = $sth->execute_for_fetch($fetch_tuple_sub);
  ($tuples, $rows) = $sth->execute_for_fetch($fetch_tuple_sub, \@tuple_status);

Used internally by the L</execute_array> method, and rarely used directly. See the
DBI documentation for more details.

=head2 B<fetchrow_arrayref>

  $ary_ref = $sth->fetchrow_arrayref;

Fetches the next row of data from the statement handle, and returns a reference to an array
holding the column values. Any columns that are NULL are returned as undef within the array.

If there are no more rows or if an error occurs, this method returns undef. You should
check C<< $sth->err >> afterwards (or use the L</RaiseError> attribute) to discover if the undef returned
was due to an error.

Note that the same array reference is returned for each fetch, so don't store the reference and
then use it after a later fetch. Also, the elements of the array are also reused for each row,
so take care if you want to take a reference to an element. See also L</bind_columns>.

=head2 B<fetchrow_array>

  @ary = $sth->fetchrow_array;

Similar to the L</fetchrow_arrayref> method, but returns a list of column information rather than
a reference to a list. Do not use this in a scalar context.

=head2 B<fetchrow_hashref>

  $hash_ref = $sth->fetchrow_hashref;
  $hash_ref = $sth->fetchrow_hashref($name);

Fetches the next row of data and returns a hashref containing the name of the columns as the keys
and the data itself as the values. Any NULL value is returned as undef value.

If there are no more rows or if an error occurs, this method returns undef. You should
check C<< $sth->err >> afterwards (or use the L</RaiseError> attribute) to discover if the undef returned
was due to an error.

The optional C<$name> argument should be either C<NAME>, C<NAME_lc> or C<NAME_uc>, and indicates
what sort of transformation to make to the keys in the hash. By default Oracle uses upper case.

=head2 B<fetchall_arrayref>

  $tbl_ary_ref = $sth->fetchall_arrayref();
  $tbl_ary_ref = $sth->fetchall_arrayref( $slice );
  $tbl_ary_ref = $sth->fetchall_arrayref( $slice, $max_rows );

Returns a reference to an array of arrays that contains all the remaining rows to be fetched from the
statement handle. If there are no more rows, an empty arrayref will be returned. If an error occurs,
the data read in so far will be returned. Because of this, you should always check C<< $sth->err >> after
calling this method, unless L</RaiseError> has been enabled.

If C<$slice> is an array reference, fetchall_arrayref uses the L</fetchrow_arrayref> method to fetch each
row as an array ref. If the C<$slice> array is not empty then it is used as a slice to select individual
columns by perl array index number (starting at 0, unlike column and parameter numbers which start at 1).

With no parameters, or if $slice is undefined, fetchall_arrayref acts as if passed an empty array ref.

If C<$slice> is a hash reference, fetchall_arrayref uses L</fetchrow_hashref> to fetch each row as a hash reference.

See the DBI documentation for a complete discussion.

=head2 B<fetchall_hashref>

  $hash_ref = $sth->fetchall_hashref( $key_field );

Returns a hashref containing all rows to be fetched from the statement handle. See the DBI documentation for
a full discussion.

=head2 B<finish>

  $rv = $sth->finish;

Indicates to DBI that you are finished with the statement handle and are not going to use it again. Only needed
when you have not fetched all the possible rows.

=head2 B<rows>

  $rv = $sth->rows;

Returns the number of rows affected for updates, deletes and inserts and -1 for selects.

=head2 B<bind_col>

  $rv = $sth->bind_col($column_number, \$var_to_bind);
  $rv = $sth->bind_col($column_number, \$var_to_bind, \%attr );
  $rv = $sth->bind_col($column_number, \$var_to_bind, $bind_type );

Binds a Perl variable and/or some attributes to an output column of a SELECT statement.
Column numbers count up from 1. You do not need to bind output columns in order to fetch data.

NOTE: DBD::Oracle does not use the C<$bind_type> to determine how to
bind the column; it uses what Oracle says the data type is. You can
however set the StrictlyTyped/DiscardString attributes and these will
take effect as these attributes are applied after the column is
retrieved.

See the DBI documentation for a discussion of the optional parameters C<\%attr> and C<$bind_type>

=head2 B<bind_columns>

  $rv = $sth->bind_columns(@list_of_refs_to_vars_to_bind);

Calls the L</bind_col> method for each column in the SELECT statement, using the supplied list.

=head2 B<dump_results>

  $rows = $sth->dump_results($maxlen, $lsep, $fsep, $fh);

Fetches all the rows from the statement handle, calls C<DBI::neat_list> for each row, and
prints the results to C<$fh> (which defaults to F<STDOUT>). Rows are separated by C<$lsep> (which defaults
to a newline). Columns are separated by C<$fsep> (which defaults to a comma). The C<$maxlen> controls
how wide the output can be, and defaults to 35.

This method is designed as a handy utility for prototyping and testing queries. Since it uses
"neat_list" to format and edit the string for reading by humans, it is not recommended
for data transfer applications.

=head1 STATEMENT HANDLE ATTRIBUTES

=head2 B<NUM_OF_FIELDS> (integer, read-only)

Returns the number of columns returned by the current statement. A number will only be returned for
SELECT statements for INSERT,
UPDATE, and DELETE statements which contain a RETURNING clause.
This method returns undef if called before C<execute()>.

=head2 B<NUM_OF_PARAMS> (integer, read-only)

Returns the number of placeholders in the current statement.

=head2 B<NAME> (arrayref, read-only)

Returns an arrayref of column names for the current statement. This
method will only work for SELECT statements, for SHOW statements, and for
INSERT, UPDATE, and DELETE statements which contain a RETURNING clause.
This method returns undef if called before C<execute()>.

=head2 B<NAME_lc> (arrayref, read-only)

The same as the C<NAME> attribute, except that all column names are forced to lower case.

=head2 B<NAME_uc>  (arrayref, read-only)

The same as the C<NAME> attribute, except that all column names are forced to upper case.

=head2 B<NAME_hash> (hashref, read-only)

Similar to the C<NAME> attribute, but returns a hashref of column names instead of an arrayref. The names of the columns
are the keys of the hash, and the values represent the order in which the columns are returned, starting at 0.
This method returns undef if called before C<execute()>.

=head2 B<NAME_lc_hash> (hashref, read-only)

The same as the C<NAME_hash> attribute, except that all column names are forced to lower case.

=head2 B<NAME_uc_hash> (hashref, read-only)

The same as the C<NAME_hash> attribute, except that all column names are forced to lower case.

=head2 B<TYPE> (arrayref, read-only)

Returns an arrayref indicating the data type for each column in the statement.
This method returns undef if called before C<execute()>.

=head2 B<PRECISION> (arrayref, read-only)

Returns an arrayref of integer values for each column returned by the statement.
The number indicates the precision for C<NUMERIC> columns, the size in number of
characters for C<CHAR> and C<VARCHAR> columns, and for all other types of columns
it returns the number of I<bytes>.
This method returns undef if called before C<execute()>.

=head2 B<SCALE> (arrayref, read-only)

Returns an arrayref of integer values for each column returned by the statement. The number
indicates the scale of the that column. The only type that will return a value is C<NUMERIC>.
This method returns undef if called before C<execute()>.

=head2 B<NULLABLE> (arrayref, read-only)

Returns an arrayref of integer values for each column returned by the statement. The number
indicates if the column is nullable or not. 0 = not nullable, 1 = nullable, 2 = unknown.
This method returns undef if called before C<execute()>.

=head2 B<Database> (dbh, read-only)

Returns the database handle this statement handle was created from.

=head2 B<ParamValues> (hash ref, read-only)

Returns a reference to a hash containing the values currently bound to placeholders. If the "named parameters"
type of placeholders are being used (such as ":foo"), then the keys of the hash will be the names of the
placeholders (without the colon). If the "dollar sign numbers" type of placeholders are being used, the keys of the hash will
be the numbers, without the dollar signs. If the "question mark" type is used, integer numbers will be returned,
starting at one and increasing for every placeholder.

If this method is called before L</execute>, the literal values passed in are returned. If called after
L</execute>, then the quoted versions of the values are returned.

=head2 B<ParamTypes> (hash ref, read-only)

Returns a reference to a hash containing the type names currently bound to placeholders. The keys
are the same as returned by the ParamValues method. The values are hashrefs containing a single key value
pair, in which the key is either 'TYPE' if the type has a generic SQL equivalent, and 'pg_type' if the type can
only be expressed by a Postgres type. The value is the internal number corresponding to the type originally
passed in. (Placeholders that have not yet been bound will return undef as the value). This allows the output of
ParamTypes to be passed back to the L</bind_param> method.

=head2 B<Statement> (string, read-only)

Returns the statement string passed to the most recent "prepare" method called in this database handle, even if that method
failed. This is especially useful where "RaiseError" is enabled and the exception handler checks $@ and sees that a C<prepare>
method call failed.

=head2 B<RowsInCache>

Returns the number of un-fetched rows in the cache for selects.

=head1 SCROLLABLE CURSORS

Oracle supports the concept of a 'Scrollable Cursor' which is defined as a 'Result Set' where
the rows can be fetched either sequentially or non-sequentially. One can fetch rows forward,
backwards, from any given position or the n-th row from the current position in the result set.

Rows are numbered sequentially starting at one and client-side caching of the partial or entire result set
can improve performance by limiting round trips to the server.

Oracle does not support DML type operations with scrollable cursors so you are limited
to simple 'Select' operations only. As well you can not use this functionality with remote
mapped queries or if the LONG datatype is part of the select list.

However, LOBSs, CLOBSs, and BLOBs do work as do all the regular bind, and fetch methods.

Only use scrollable cursors if you really have a good reason to. They do use up considerable
more server and client resources and have poorer response times than non-scrolling cursors.

=head2 Enabling Scrollable Cursors

To enable this functionality you must first import the 'Fetch Orientation' and the 'Execution Mode' constants by using;

   use DBD::Oracle qw(:ora_fetch_orient :ora_exe_modes);

Next you will have to tell DBD::Oracle that you will be using scrolling by setting the ora_exe_mode attribute on the
statement handle to 'OCI_STMT_SCROLLABLE_READONLY' with the prepare method;

  $sth=$dbh->prepare($SQL,{ora_exe_mode=>OCI_STMT_SCROLLABLE_READONLY});

When the statement is executed you will then be able to use 'ora_fetch_scroll' method to get a row
or you can still use any of the other fetch methods but with a poorer response time than if you used a
non-scrolling cursor. As well scrollable cursors are compatible with any applicable bind methods.

=head2 Scrollable Cursor Methods

The following driver-specific methods are used with scrollable cursors.

=over

=item ora_scroll_position

  $position =  $sth->ora_scroll_position();

This method returns the current position (row number) attribute of the result set. Prior to the first fetch this value is 0. This is the only time
this value will be 0 after the first fetch the value will be set, so you can use this value to test if any rows have been fetched.
The minimum value will always be 1 after the first fetch. The maximum value will always be the total number of rows in the record set.

=item ora_fetch_scroll

  $ary_ref = $sth->ora_fetch_scroll($fetch_orient,$fetch_offset);

Works the same as C<fetchrow_arrayref>, excepts one passes in a 'Fetch Orientation' constant and a fetch_offset
value which will then determine the row that will be fetched. It returns the row as a list containing the field values.
Null fields are returned as I<undef> values in the list.

The valid orientation constant and fetch offset values combination are detailed below

  OCI_FETCH_CURRENT,  fetches the current row, the fetch offset value is ignored.
  OCI_FETCH_NEXT,     fetches the next row from the current position, the fetch offset value
                      is ignored.
  OCI_FETCH_FIRST,    fetches the first row, the fetch offset value is ignored.
  OCI_FETCH_LAST,     fetches the last row, the fetch offset value is ignored.
  OCI_FETCH_PRIOR,    fetches the previous row from the current position, the fetch offset
                      value is ignored.

  OCI_FETCH_ABSOLUTE, fetches the row that is specified by the fetch offset value.

  OCI_FETCH_ABSOLUTE, and a fetch offset value of 1 is equivalent to a OCI_FETCH_FIRST.
  OCI_FETCH_ABSOLUTE, and a fetch offset value of 0 is equivalent to a OCI_FETCH_CURRENT.

  OCI_FETCH_RELATIVE, fetches the row relative from the current position as specified by the
                      fetch offset value.

  OCI_FETCH_RELATIVE, and a fetch offset value of 0 is equivalent to a OCI_FETCH_CURRENT.
  OCI_FETCH_RELATIVE, and a fetch offset value of 1 is equivalent to a OCI_FETCH_NEXT.
  OCI_FETCH_RELATIVE, and a fetch offset value of -1 is equivalent to a OCI_FETCH_PRIOR.

The effect that a ora_fetch_scroll method call has on the current_positon attribute is detailed below.

  OCI_FETCH_CURRENT, has no effect on the current_positon attribute.
  OCI_FETCH_NEXT,    increments current_positon attribute by 1
  OCI_FETCH_NEXT,    when at the last row in the record set does not change current_positon
                     attribute, it is equivalent to a OCI_FETCH_CURRENT
  OCI_FETCH_FIRST,   sets the current_positon attribute to 1.
  OCI_FETCH_LAST,    sets the current_positon attribute to the total number of rows in the
                     record set.
  OCI_FETCH_PRIOR,   decrements current_positon attribute by 1.
  OCI_FETCH_PRIOR,   when at the first row in the record set does not change current_positon
                     attribute, it is equivalent to a OCI_FETCH_CURRENT.

  OCI_FETCH_ABSOLUTE, sets the current_positon attribute to the fetch offset value.
  OCI_FETCH_ABSOLUTE, and a fetch offset value that is less than 1 does not change
                      current_positon attribute, it is equivalent to a OCI_FETCH_CURRENT.
  OCI_FETCH_ABSOLUTE, and a fetch offset value that is greater than the number of records in
                      the record set, does not change current_positon attribute, it is
                      equivalent to a OCI_FETCH_CURRENT.
  OCI_FETCH_RELATIVE, sets the current_positon attribute to (current_positon attribute +
                      fetch offset value).
  OCI_FETCH_RELATIVE, and a fetch offset value that makes the current position less than 1,
                      does not change fetch offset value so it is equivalent to a OCI_FETCH_CURRENT.
  OCI_FETCH_RELATIVE, and a fetch offset value that makes it greater than the number of records
                      in the record set, does not change fetch offset value so it is equivalent
                      to a OCI_FETCH_CURRENT.

The effects of the differing orientation constants on the first fetch (current_postion attribute at 0) are as follows.

  OCI_FETCH_CURRENT, dose not fetch a row or change the current_positon attribute.
  OCI_FETCH_FIRST,   fetches row 1 and sets the current_positon attribute to 1.
  OCI_FETCH_LAST,    fetches the last row in the record set and sets the current_positon
                     attribute to the total number of rows in the record set.
  OCI_FETCH_NEXT,    equivalent to a OCI_FETCH_FIRST.
  OCI_FETCH_PRIOR,   equivalent to a OCI_FETCH_CURRENT.

  OCI_FETCH_ABSOLUTE, and a fetch offset value that is less than 1 is equivalent to a
                      OCI_FETCH_CURRENT.
  OCI_FETCH_ABSOLUTE, and a fetch offset value that is greater than the number of
                      records in the record set is equivalent to a OCI_FETCH_CURRENT.
  OCI_FETCH_RELATIVE, and a fetch offset value that is less than 1 is equivalent
                      to a OCI_FETCH_CURRENT.
  OCI_FETCH_RELATIVE, and a fetch offset value that makes it greater than the number
                      of records in the record set, is equivalent to a OCI_FETCH_CURRENT.

=back

=head2 Scrollable Cursor Usage

Given a simple code like this:

  use DBI;
  use DBD::Oracle qw(:ora_types :ora_fetch_orient :ora_exe_modes);
  my $dbh = DBI->connect($dsn, $dbuser, '');
  my $SQL = "select id,
                     first_name,
                     last_name
                from employee";
  my $sth=$dbh->prepare($SQL,{ora_exe_mode=>OCI_STMT_SCROLLABLE_READONLY});
  $sth->execute();
  my $value;

and one assumes that the number of rows returned from the query is 20, the code snippets below will illustrate the use of ora_fetch_scroll
method;

=over

=item Fetching the Last Row

  $value =  $sth->ora_fetch_scroll(OCI_FETCH_LAST,0);
  print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute to will be 20 after this snippet.  This is also a way to get the number of rows in the record set, however,
if the record set is large this could take some time.

=item Fetching the Current Row

  $value =  $sth->ora_fetch_scroll(OCI_FETCH_CURRENT,0);
  print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute will still be 20 after this snippet.

=item Fetching the First Row

  $value =  $sth->ora_fetch_scroll(OCI_FETCH_FIRST,0);
  print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute will be 1 after this snippet.

=item Fetching the Next Row

  for(my $i=0;$i<=3;$i++){
     $value =  $sth->ora_fetch_scroll(OCI_FETCH_NEXT,0);
     print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  }
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute will be 5 after this snippet.

=item Fetching the Prior Row

  for(my $i=0;$i<=3;$i++){
     $value =  $sth->ora_fetch_scroll(OCI_FETCH_PRIOR,0);
     print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  }
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute will be 1 after this snippet.

=item Fetching the 10th Row

  $value =  $sth->ora_fetch_scroll(OCI_FETCH_ABSOLUTE,10);
  print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute will be 10 after this snippet.

=item Fetching the 10th to 14th Row

  for(my $i=10;$i<15;$i++){
      $value =  $sth->ora_fetch_scroll(OCI_FETCH_ABSOLUTE,$i);
      print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  }
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute will be 14 after this snippet.

=item Fetching the 14th to 10th Row

  for(my $i=14;$i>9;$i--){
    $value =  $sth->ora_fetch_scroll(OCI_FETCH_ABSOLUTE,$i);
    print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  }
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute will be 10 after this snippet.

=item Fetching the 5th Row From the Present Position.

  $value =  $sth->ora_fetch_scroll(OCI_FETCH_RELATIVE,5);
  print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute will be 15 after this snippet.

=item Fetching the 9th Row Prior From the Present Position

  $value =  $sth->ora_fetch_scroll(OCI_FETCH_RELATIVE,-9);
  print "id=".$value->[0].", First Name=".$value->[1].", Last Name=".$value->[2]."\n";
  print "current scroll position=".$sth->ora_scroll_position()."\n";

The current_positon attribute will be 6 after this snippet.

=item Use Finish

  $sth->finish();

When using scrollable cursors it is required that you use the $sth->finish() method when you are done with the cursor as this type of
cursor has to be explicitly cancelled on the server. If you do not do this you may cause resource problems on your database.

=back

=head1 LOBS AND LONGS

The key to working with LOBs (CLOB, BLOBs) is to remember the value of an Oracle LOB column is not the content of the LOB. It's a
'LOB Locator' which, after being selected or inserted needs extra processing to read or write the content of the LOB. There are also legacy LONG types (LONG, LONG RAW)
which are presently deprecated by Oracle but are still in use.  These LONG types do not utilize a 'LOB Locator' and also are more limited in
functionality than CLOB or BLOB fields.

DBD::Oracle now offers three interfaces to LOB and LONG data,

=over

=item L</Data Interface for Persistent LOBs>

With this interface DBD::Oracle handles your data directly utilizing regular OCI calls, Oracle itself takes care of the LOB Locator operations in the case of
BLOBs and CLOBs treating them exactly as if they were the same as the legacy LONG or LONG RAW types.

=item L</Data Interface for LOB Locators>

With this interface DBD::Oracle handles your data utilizing LOB Locator OCI calls so it only works with CLOB and BLOB datatypes. With this interface DBD::Oracle takes care of the LOB Locator operations for you.

=item L</LOB Locator Method Interface>

This allows the user direct access to the LOB Locator methods, so you have to take care of the LOB Locator operations yourself.

=back

Generally speaking the interface that you will chose will be dependent on what end you are trying to achieve. All have their benefits and
drawbacks.

One point to remember when working with LOBs (CLOBs, BLOBs) is if your LOB column can be in one of three states;

=over

=item NULL

The table cell is created, but the cell holds no locator or value.
If your LOB field is in this state then there is no LOB Locator that DBD::Oracle can work so if your encounter a

  DBD::Oracle::db::ora_lob_read: locator is not of type OCILobLocatorPtr

error when working with a LOB.

You can correct this by using an SQL UPDATE statement to reset the LOB column to a non-NULL (or empty LOB) value with either EMPTY_BLOB or EMPTY_CLOB as in this example;

  UPDATE lob_example
     SET bindata=EMPTY_BLOB()
   WHERE bindata IS NULL.

=item Empty

A LOB instance with a locator exists in the cell, but it has no value. The length of the LOB is zero. In this case DBD::Oracle will return 'undef' for the field.

=item Populated

A LOB instance with a locator and a value exists in the cell. You actually get the LOB value.

=back

=head2 Data Interface for Persistent LOBs

This is the original interface for LONG and LONG RAW datatypes and from Oracle 9iR1 and later the OCI API was extended to work directly with the other LOB datatypes.
In other words you can treat all LOB type data (BLOB, CLOB) as if it was a LONG, LONG RAW, or VARCHAR2. So you can perform INSERT, UPDATE, fetch, bind, and define operations on LOBs using the same techniques
you would use on other datatypes that store character or binary data. In some cases there are fewer round trips to the server as no 'LOB Locators' are
used, normally one can get an entire LOB is a single round trip.

=head3 Simple Fetch for LONGs and LONG RAWs

As the name implies this is the simplest way to use this interface. DBD::Oracle just attempts to get your LONG datatypes as a single large piece.
There are no special settings, simply set the database handle's 'LongReadLen' attribute to a value that will be larger than the expected size of the LONG or LONG RAW.
If the size of the LONG or LONG RAW exceeds 'LongReadLen' DBD::Oracle will return an 'ORA-24345: A Truncation' error.  To stop this set the database handle's 'LongTruncOk' attribute to '1'.
The maximum value of 'LongReadLen' seems to be dependent on the physical memory limits of the box that Oracle is running on.  You have most likely reached this limit if you run into
an 'ORA-01062: unable to allocate memory for define buffer' error.  One solution is to set the size of 'LongReadLen' to a lower value.

For example give this table;

  CREATE TABLE test_long (
              id NUMBER,
            long1 long)

this code;

  $dbh->{LongReadLen} = 2*1024*1024; #2 meg
  $SQL='select p_id,long1 from test_long';
  $sth=$dbh->prepare($SQL);
  $sth->execute();
  while (my ( $p_id,$long )=$sth->fetchrow()){
    print "p_id=".$p_id."\n";
    print "long=".$long."\n";
  }

Will select out all of the long1 fields in the table as long as they are all under 2MB in length. A value in long1 longer than this will throw an error. Adding this line;

  $dbh->{LongTruncOk}=1;

before the execute will return all the long1 fields but they will be truncated at 2MBs.

=head3 Using ora_ncs_buff_mtpl

When getting CLOBs and NCLOBs in or out of Oracle, the Server will translate from the Server's NCharSet to the
Client's. If they happen to be the same or at least compatible then all of these actions are a 1 char to 1 char bases.
Thus if you set your LongReadLen buffer to 10_000_000 you will get up to 10_000_000 char.

However if the Server has to translate from one NCharSet to another it will use bytes for conversion. The buffer
value is set to 4 * LONG_READ_LEN which was very wasteful as you might only be asking for 10_000_000 bytes
but you were actually using 40_000_000 bytes of buffer under the hood.  You would still get 10_000_000 bytes
(maybe less characters though) but you are using allot more memory that you need.

You can now customize the size of the buffer by setting the 'ora_ncs_buff_mtpl' either on the connection or statement handle. You can
also set this as 'ORA_DBD_NCS_BUFFER' OS environment variable so you will have to go back and change all your code if you are getting into trouble.

The default value is still set to 4 for backward compatibility. You can lower this value and thus increase the amount of data you can retrieve. If the
ora_ncs_buff_mtpl is too small DBD::Oracle will throw and error telling you to increase this buffer by one.

If the error is not captured then you may get at some random point later on, usually at a finish() or disconnect() or even a fetch() this error;

  ORA-03127: no new operations allowed until the active operation ends

This is one of the more obscure ORA errors (have some fun and report it to Meta-Link they will scratch their heads for hours)

If you get this, simply increment the ora_ncs_buff_mtpl by one until it goes away.

This should greatly increase your ability to select very large CLOBs or NCLOBs, by freeing up a large block of memory.

You can tune this value by setting ora_oci_success_warn which will display the following

  OCILobRead field 2 of 3 SUCCESS: csform 1 (SQLCS_IMPLICIT), LOBlen 10240(characters), LongReadLen
  20(characters), BufLen 80(characters), Got 28(characters)

In the case above the query Got 28 characters (well really only 20 characters of 28 bytes) so we could use ora_ncs_buff_mtpl=>2 (20*2=40) thus saving 40bytes of memory.

=head3 Simple Fetch for CLOBs and BLOBs

To use this interface for the CLOB and BLOB datatypes set the 'ora_pers_lob' attribute of the statement handle to '1' with the prepare method, and
set the database handle's 'LongReadLen' attribute to a value that will be larger than the expected size of the LOB. If the size of the LOB exceeds
'LongReadLen' DBD::Oracle will return an 'ORA-24345: A Truncation' error.  To stop this set the database handle's 'LongTruncOk' attribute to '1'.
The maximum value of 'LongReadLen' seems to be dependent on the physical memory limits of the box that Oracle is running on in the same way that LONGs and LONG RAWs are.

For CLOBs and NCLOBs the limit is 64k chars if there is no truncation. This is an internal OCI limit--complain to them if you want it changed.  However if your CLOB is longer than this
and also larger than 'LongReadLen' then 'LongReadLen' chars are returned.

It seems with BLOBs you are not limited by the 64k.

For example give this table;

  CREATE TABLE test_lob (id NUMBER,
               clob1 CLOB,
               clob2 CLOB,
               blob1 BLOB,
               blob2 BLOB)

this code;

  $dbh->{LongReadLen} = 2*1024*1024; #2 meg
  $SQL='select p_id,lob_1,lob_2,blob_2 from test_lobs';
  $sth=$dbh->prepare($SQL,{ora_pers_lob=>1});
  $sth->execute();
  while (my ( $p_id,$log,$log2,$log3,$log4 )=$sth->fetchrow()){
    print "p_id=".$p_id."\n";
    print "clob1=".$clob1."\n";
    print "clob2=".$clob2."\n";
    print "blob1=".$blob2."\n";
    print "blob2=".$blob2."\n";
  }

Will select out all of the LOBs in the table as long as they are all under 2MB in length. Longer lobs will throw an error. Adding this line;

  $dbh->{LongTruncOk}=1;

before the execute will return all the lobs but they will be truncated at 2MBs.

=head3 Piecewise Fetch with Callback

With a piecewise callback fetch DBD::Oracle sets up a function that will 'callback' to the DB during the fetch and gets your LOB (LONG, LONG RAW, CLOB, BLOB) piece by piece.
To use this interface set the 'ora_clbk_lob' attribute of the statement handle to '1' with the prepare method. Next set the 'ora_piece_size' to the size of the piece that
you want to return on the callback. Finally set the database handle's 'LongReadLen' attribute to a value that will be larger than the expected
size of the LOB. Like the L</Simple Fetch for LONGs and LONG RAWs> and L</Simple Fetch for CLOBs and BLOBs> the if the size of the LOB exceeds 'LongReadLen' you can use the 'LongTruncOk' attribute to truncate the LOB
or set 'LongReadLen' to a higher value.  With this interface the value of 'ora_piece_size' seems to be constrained by the same memory limit as found on
the Simple Fetch interface. If you encounter an 'ORA-01062' error try setting the value of 'ora_piece_size' to a smaller value.   The value for 'LongReadLen' is
dependent on the version and settings of the Oracle DB you are using. In theory it ranges from 8GBs
in 9iR1 up to 128 terabytes with 11g but you will also be limited by the physical memory of your PERL instance.

Using the table from the last example this code;

  $dbh->{LongReadLen} = 20*1024*1024; #20 meg
  $SQL='select p_id,lob_1,lob_2,blob_2 from test_lobs';
  $sth=$dbh->prepare($SQL,{ora_clbk_lob=>1,ora_piece_size=>5*1024*1024});
  $sth->execute();
  while (my ( $p_id,$log,$log2,$log3,$log4 )=$sth->fetchrow()){
    print "p_id=".$p_id."\n";
    print "clob1=".$clob1."\n";
    print "clob2=".$clob2."\n";
    print "blob1=".$blob2."\n";
    print "blob2=".$blob2."\n";
  }

Will select out all of the LOBs in the table as long as they are all under 20MB in length. If the LOB is longer than 5MB (ora_piece_size) DBD::Oracle will fetch it in at least 2 pieces to a
maximum of 4 pieces (4*5MB=20MB). Like the Simple Fetch examples Lobs longer than 20MB will throw an error.

Using the table from the first example (LONG) this code;

  $dbh->{LongReadLen} = 20*1024*1024; #2 meg
  $SQL='select p_id,long1 from test_long';
  $sth=$dbh->prepare($SQL,{ora_clbk_lob=>1,ora_piece_size=>5*1024*1024});
  $sth->execute();
  while (my ( $p_id,$long )=$sth->fetchrow()){
    print "p_id=".$p_id."\n";
    print "long=".$long."\n";
  }

Will select all of the long1 fields from table as long as they are is under 20MB in length. If the long1 filed is longer than 5MB (ora_piece_size) DBD::Oracle will fetch it in at least 2 pieces to a
maximum of 4 pieces (4*5MB=20MB). Like the other examples long1 fields longer than 20MB will throw an error.

=head4 Piecewise Fetch with Polling

With a polling piecewise fetch DBD::Oracle iterates (Polls) over the LOB during the fetch getting your LOB (LONG, LONG RAW, CLOB, BLOB) piece by piece. To use this interface set the 'ora_piece_lob'
attribute of the statement handle to '1' with the prepare method. Next set the 'ora_piece_size' to the size of the piece that
you want to return on the callback. Finally set the database handle's 'LongReadLen' attribute to a value that will be larger than the expected
size of the LOB. Like the L</Piecewise Fetch with Callback> and Simple Fetches if the size of the LOB exceeds 'LongReadLen' you can use the 'LongTruncOk' attribute to truncate the LOB
or set 'LongReadLen' to a higher value.  With this interface the value of 'ora_piece_size' seems to be constrained by the same memory limit as found on
the L</Piecewise Fetch with Callback>.

Using the table from the example above this code;

  $dbh->{LongReadLen} = 20*1024*1024; #20 meg
  $SQL='select p_id,lob_1,lob_2,blob_2 from test_lobs';
  $sth=$dbh->prepare($SQL,{ora_piece_lob=>1,ora_piece_size=>5*1024*1024});
  $sth->execute();
  while (my ( $p_id,$log,$log2,$log3,$log4 )=$sth->fetchrow()){
    print "p_id=".$p_id."\n";
    print "clob1=".$clob1."\n";
    print "clob2=".$clob2."\n";
    print "blob1=".$blob2."\n";
    print "blob2=".$blob2."\n";
  }

Will select out all of the LOBs in the table as long as they are all under 20MB in length. If the LOB is longer than 5MB (ora_piece_size) DBD::Oracle will fetch it in at least 2 pieces to a
maximum of 4 pieces (4*5MB=20MB). Like the other fetch methods LOBs longer than 20MB will throw an error.

Finally with this code;

  $dbh->{LongReadLen} = 20*1024*1024; #2 meg
  $SQL='select p_id,long1 from test_long';
  $sth=$dbh->prepare($SQL,{ora_piece_lob=>1,ora_piece_size=>5*1024*1024});
  $sth->execute();
  while (my ( $p_id,$long )=$sth->fetchrow()){
    print "p_id=".$p_id."\n";
    print "long=".$long."\n";
  }

Will select all of the long1 fields from table as long as they are is under 20MB in length. If the long1 field is longer than 5MB (ora_piece_size) DBD::Oracle will fetch it in at least 2 pieces to a
maximum of 4 pieces (4*5MB=20MB). Like the other examples long1 fields longer than 20MB will throw an error.

=head3 Binding for Updates and Inserts for CLOBs and  BLOBs

To bind for updates and inserts all that is required to use this interface is to set the statement handle's prepare method
'ora_type' attribute to 'SQLT_CHR' in the case of CLOBs and NCLOBs or 'SQLT_BIN' in the case of BLOBs as in this example for an insert;

  my $in_clob = "<document>\n";
  $in_clob .= "  <value>$_</value>\n" for 1 .. 10_000;
  $in_clob .= "</document>\n";
  my $in_blob ="0101" for 1 .. 10_000;

  $SQL='insert into test_lob3@tpgtest (id,clob1,clob2, blob1,blob2) values(?,?,?,?,?)';
  $sth=$dbh->prepare($SQL );
  $sth->bind_param(1,3);
  $sth->bind_param(2,$in_clob,{ora_type=>SQLT_CHR});
  $sth->bind_param(3,$in_clob,{ora_type=>SQLT_CHR});
  $sth->bind_param(4,$in_blob,{ora_type=>SQLT_BIN});
  $sth->bind_param(5,$in_blob,{ora_type=>SQLT_BIN});
  $sth->execute();

So far the only limit reached with this form of insert is the LOBs must be under 2GB in size.

=head3 Support for Remote LOBs;

Starting with Oracle 10gR2 the interface for Persistent LOBs was expanded to support remote LOBs (access over a dblink). Given a database called 'lob_test' that has a 'LINK' defined like this;

  CREATE DATABASE LINK link_test CONNECT TO test_lobs IDENTIFIED BY tester USING 'lob_test';

to a remote database called 'test_lobs', the following code will work;

  $dbh = DBI->connect('dbi:Oracle:','test@lob_test','test');
  $dbh->{LongReadLen} = 2*1024*1024; #2 meg
  $SQL='select p_id,lob_1,lob_2,blob_2 from test_lobs@link_test';
  $sth=$dbh->prepare($SQL,{ora_pers_lob=>1});
  $sth->execute();
  while (my ( $p_id,$log,$log2,$log3,$log4 )=$sth->fetchrow()){
     print "p_id=".$p_id."\n";
     print "clob1=".$clob1."\n";
     print "clob2=".$clob2."\n";
     print "blob1=".$blob2."\n";
     print "blob2=".$blob2."\n";
  }

Below are the limitations of Remote LOBs;

=over

=item Queries involving more than one database are not supported;

so the following returns an error:

  SELECT t1.lobcol,
           a2.lobcol
    FROM t1,
         t2.lobcol@dbs2 a2 W
   WHERE LENGTH(t1.lobcol) = LENGTH(a2.lobcol);

as does:

     SELECT t1.lobcol
       FROM t1@dbs1
  UNION ALL
     SELECT t2.lobcol
       FROM t2@dbs2;

=item DDL commands are not supported;

so the following returns an error:

  CREATE VIEW v AS SELECT lob_col FROM tab@dbs;

=item Only binds and defines for data going into remote persistent LOBs are supported.

so that parameter passing in PL/SQL where CHAR data is bound or defined for remote LOBs is not allowed .

These statements all produce errors:

  SELECT foo() FROM table1@dbs2;

  SELECT foo()@dbs INTO char_val FROM DUAL;

  SELECT XMLType().getclobval FROM table1@dbs2;

=item If the remote object is a view such as

  CREATE VIEW v AS SELECT foo() FROM ...

the following would not work:

  SELECT * FROM v@dbs2;

=item Limited PL/SQL parameter passing

PL/SQL parameter passing is not allowed where the actual argument is a LOB type
and the remote argument is one of VARCHAR2, NVARCHAR2, CHAR, NCHAR, or RAW.

=item RETURNING INTO does not support implicit conversions between CHAR and CLOB.

so the following returns an error:

  SELECT t1.lobcol as test, a2.lobcol FROM t1, t2.lobcol@dbs2 a2 RETURNING test

=back

=head2 Locator Data Interface

=head3 Simple Usage

When fetching LOBs with this interface a 'LOB Locator' is created then used to get the lob with the LongReadLen and LongTruncOk attributes.
The value for 'LongReadLen' is dependent on the version and settings of the Oracle DB you are using. In theory it ranges from 8GBs
in 9iR1 up to 128 terabytes with 11g but you will also be limited by the physical memory of your PERL instance.

When inserting or updating LOBs some I<major> magic has to be performed
behind the scenes to make it transparent.  Basically the driver has to
insert a 'LOB Locator' and then refetch the newly inserted LOB
Locator before being able to write the data into it.  However, it works
well most of the time, and I've made it as fast as possible, just one
extra server-round-trip per insert or update after the first.  For the
time being, only single-row LOB updates are supported.

To insert or update a large LOB using a placeholder, DBD::Oracle has to
know in advance that it is a LOB type. So you need to say:

  $sth->bind_param($field_num, $lob_value, { ora_type => ORA_CLOB });

The ORA_CLOB and ORA_BLOB constants can be imported using

  use DBD::Oracle qw(:ora_types);

or use the corresponding integer values (112 and 113).

One further wrinkle: for inserts and updates of LOBs, DBD::Oracle has
to be able to tell which parameters relate to which table fields.
In all cases where it can possibly work it out for itself, it does,
however, if there are multiple LOB fields of the same type in the table
then you need to tell it which field each LOB param relates to:

  $sth->bind_param($idx, $value, { ora_type=>ORA_CLOB, ora_field=>'foo' });

There are some limitations inherent in the way DBD::Oracle makes typical
LOB operations simple by hiding the LOB Locator processing:

 - Can't read/write LOBs in chunks (except via DBMS_LOB.WRITEAPPEND in PL/SQL)
 - To INSERT a LOB, you need UPDATE privilege.

The alternative is to disable the automatic LOB Locator processing.
If L</ora_auto_lob> is 0 in prepare(), you can fetch the LOB Locators and
do all the work yourself using the ora_lob_*() methods.
See the L</Data Interface for LOB Locators> section below.

=head3 LOB support in PL/SQL

LOB Locators can be passed to PL/SQL calls by binding them to placeholders
with the proper C<ora_type>.  If L</ora_auto_lob> is true, output LOB
parameters will be automatically returned as strings.

If the Oracle driver has support for temporary LOBs (Oracle 9i and higher),
strings can be bound to input LOB placeholders and will be automatically
converted to LOBs.

Example:
     # Build a large XML document, bind it as a CLOB,
     # extract elements through PL/SQL and return as a CLOB

     # $dbh is a connected database handle
     # output will be large

     local $dbh->{LongReadLen} = 1_000_000;

     my $in_clob = "<document>\n";
     $in_clob .= "  <value>$_</value>\n" for 1 .. 10_000;
     $in_clob .= "</document>\n";

     my $out_clob;


     my $sth = $dbh->prepare(<<PLSQL_END);
     -- extract 'value' nodes
     DECLARE
       x XMLTYPE := XMLTYPE(:in);
     BEGIN
       :out := x.extract('/document/value').getClobVal();
     END;

     PLSQL_END

     # :in param will be converted to a temp lob
     # :out parameter will be returned as a string.

     $sth->bind_param( ':in', $in_clob, { ora_type => ORA_CLOB } );
     $sth->bind_param_inout( ':out', \$out_clob, 0, { ora_type => ORA_CLOB } );
     $sth->execute;

If you ever get an

  ORA-01691 unable to extend lob segment sss.ggg by nnn in tablespace ttt

error, while attempting to insert a LOB, this means the Oracle user has insufficient space for LOB you are trying to insert.
One solution it to use "alter database datafile 'sss.ggg' resize Mnnn" to increase the available memory for LOBs.

=head2 Persistent & Locator Interface Caveats

Now that one has the option of using the Persistent or the Locator interface for LOBs the questions arises
which one to use. For starters, if you want to access LOBs over a dblink you will have to use the Persistent
interface so that choice is simple.  The question of which one to use after that is a little more tricky.
It basically boils down to a choice between LOB size and speed.

The Callback and Polling piecewise fetches are very very slow
when compared to the Simple and the Locator fetches but they can handle very large blocks of data. Given a situation where a
large LOB is to be read the Locator fetch may time out while either of the piecewise fetches may not.

With the Simple fetch you are limited by physical memory of your server but it runs a little faster than the Locator, as there are fewer round trips
to the server. So if you have small LOBs and need to save a little bandwidth this is the one to use. It you are going after large LOBs then the Locator interface is the one to use.

If you need to update more than a single row of with LOB data then the Persistent interface can do it while the Locator can't.

If you encounter a situation where you have to access the legacy LOBs (LONG, LONG RAW) and the values are to large for you system then you can use
the Callback or Polling piecewise fetches to  get all of the data.

Not all of the Persistent interface has been implemented yet, the following are not supported;

  1) Piecewise, polling and callback binds for INSERT and UPDATE operations.
  2) Piecewise array binds for SELECT, INSERT and UPDATE operations.

Most of the time you should just use the L</Locator Data Interface> as this is in one that has the best combination of speed and size.

All this being said if you are doing some critical programming I would use the L</Data Interface for LOB Locators> as this gives you very
fine grain control of your LOBs, of course the code for this will be somewhat more involved.

=head2 Data Interface for LOB Locators

The following driver-specific methods let you manipulate "LOB Locators" directly.
To select a LOB locator directly set the if the C<ora_auto_lob>
attribute to false, or alternatively they can be returned via PL/SQL procedure calls.

(If using a DBI version earlier than 1.36 they must be called via the
func() method. Note that methods called via func() don't honour
RaiseError etc, and so it's important to check $dbh->err after each call.
It's recommended that you upgrade to DBI 1.38 or later.)

Note that LOB locators are only valid while the statement handle that
created them is valid.  When all references to the original statement
handle are lost, the handle is destroyed and the locators are freed.

=over 4

=item ora_lob_read

  $data = $dbh->ora_lob_read($lob_locator, $offset, $length);

Read a portion of the LOB. $offset starts at 1.
Uses the Oracle OCILobRead function.

NOTE: DBD::Oracle post 1.46 will return undef for any read lob if the
length specified in the ora_lob_read is 0. See RT 55028. This avoids
the potential problem with empty lobs (created with empty_clob) which
return a length of 0 from ora_lob_length and prior to 1.46 a call to
ora_lob_read with a 0 length would segfault.

=item ora_lob_write

  $rc = $dbh->ora_lob_write($lob_locator, $offset, $data);

Write/overwrite a portion of the LOB. $offset starts at 1.
Uses the Oracle OCILobWrite function.

=item ora_lob_append

  $rc = $dbh->ora_lob_append($lob_locator, $data);

Append $data to the LOB.  Uses the Oracle OCILobWriteAppend function.

=item ora_lob_trim

  $rc = $dbh->ora_lob_trim($lob_locator, $length);

Trims the length of the LOB to $length.
Uses the Oracle OCILobTrim function.

=item ora_lob_length

  $length = $dbh->ora_lob_length($lob_locator);

Returns the length of the LOB.
Uses the Oracle OCILobGetLength function.

=item ora_lob_is_init

  $is_init = $dbh->ora_lob_is_init($lob_locator);

Returns true(1) if the Lob Locator is initialized false(0) if it is not, or 'undef'
if there is an error.
Uses the Oracle OCILobLocatorIsInit function.

=item ora_lob_chunk_size

  $chunk_size = $dbh->ora_lob_chunk_size($lob_locator);

Returns the chunk size of the LOB.
Uses the Oracle OCILobGetChunkSize function.

For optimal performance, Oracle recommends reading from and
writing to a LOB in batches using a multiple of the LOB chunk size.
In Oracle 10g and before, when all defaults are in place, this
chunk size defaults to 8k (8192).

=back

=head3 LOB Locator Method Examples

I<Note:> Make sure you first read the note in the section above about
multi-byte character set issues with these methods.

The following examples demonstrate the usage of LOB Locators
to read, write, and append data, and to query the size of
large data.

The following examples assume a table containing two large
object columns, one binary and one character, with a primary
key column, defined as follows:

   CREATE TABLE lob_example (
      lob_id      INTEGER PRIMARY KEY,
      bindata     BLOB,
      chardata    CLOB
   )

It also assumes a sequence for use in generating unique
lob_id field values, defined as follows:

   CREATE SEQUENCE lob_example_seq

=head3 Example: Inserting a new row with large data

Unless enough memory is available to store and bind the
entire LOB data for insert all at once, the LOB columns must
be written interactively, piece by piece.  In the case of a new row,
this is performed by first inserting a row, with empty values in
the LOB columns, then modifying the row by writing the large data
interactively to the LOB columns using their LOB locators as handles.

The insert statement must create token values in the LOB
columns.  Here, we use the empty string for both the binary
and character large object columns 'bindata' and 'chardata'.

After the INSERT statement, a SELECT statement is used to
acquire LOB locators to the 'bindata' and 'chardata' fields
of the newly inserted row.  Because these LOB locators are
subsequently written, they must be acquired from a select
statement containing the clause 'FOR UPDATE' (LOB locators
are only valid within the transaction that fetched them, so
can't be used effectively if AutoCommit is enabled).

   my $lob_id = $dbh->selectrow_array( <<"   SQL" );
      SELECT lob_example_seq.nextval FROM DUAL
   SQL

   my $sth = $dbh->prepare( <<"   SQL" );
      INSERT INTO lob_example
      ( lob_id, bindata, chardata )
      VALUES ( ?, EMPTY_BLOB(),EMPTY_CLOB() )
   SQL
   $sth->execute( $lob_id );

   $sth = $dbh->prepare( <<"   SQL", { ora_auto_lob => 0 } );
      SELECT bindata, chardata
      FROM lob_example
      WHERE lob_id = ?
      FOR UPDATE
   SQL
   $sth->execute( $lob_id );
   my ( $bin_locator, $char_locator ) = $sth->fetchrow_array();
   $sth->finish();

   open BIN_FH, "/binary/data/source" or die;
   open CHAR_FH, "/character/data/source" or die;
   my $chunk_size = $dbh->ora_lob_chunk_size( $bin_locator );

   # BEGIN WRITING BIN_DATA COLUMN
   my $offset = 1;   # Offsets start at 1, not 0
   my $length = 0;
   my $buffer = '';
   while( $length = read( BIN_FH, $buffer, $chunk_size ) ) {
      $dbh->ora_lob_write( $bin_locator, $offset, $buffer );
      $offset += $length;
   }

   # BEGIN WRITING CHAR_DATA COLUMN
   $chunk_size = $dbh->ora_lob_chunk_size( $char_locator );
   $offset = 1;   # Offsets start at 1, not 0
   $length = 0;
   $buffer = '';
   while( $length = read( CHAR_FH, $buffer, $chunk_size ) ) {
      $dbh->ora_lob_write( $char_locator, $offset, $buffer );
      $offset += $length;
   }

In this example we demonstrate the use of ora_lob_write()
interactively to append data to the columns 'bin_data' and
'char_data'.  Had we used ora_lob_append(), we could have
saved ourselves the trouble of keeping track of the offset
into the lobs.  The snippet of code beneath the comment
'BEGIN WRITING BIN_DATA COLUMN' could look as follows:

   my $buffer = '';
   while ( read( BIN_FH, $buffer, $chunk_size ) ) {
      $dbh->ora_lob_append( $bin_locator, $buffer );
   }

The scalar variables $offset and $length are no longer
needed, because ora_lob_append() keeps track of the offset
for us.

=head3 Example: Updating an existing row with large data

In this example, we demonstrate a technique for overwriting
a portion of a blob field with new binary data.  The blob
data before and after the section overwritten remains
unchanged.  Hence, this technique could be used for updating
fixed length subfields embedded in a binary field.

   my $lob_id = 5;   # Arbitrary row identifier, for example

   $sth = $dbh->prepare( <<"   SQL", { ora_auto_lob => 0 } );
      SELECT bindata
      FROM lob_example
      WHERE lob_id = ?
      FOR UPDATE
   SQL
   $sth->execute( $lob_id );
   my ( $bin_locator ) = $sth->fetchrow_array();

   my $offset = 100234;
   my $data = "This string will overwrite a portion of the blob";
   $dbh->ora_lob_write( $bin_locator, $offset, $data );

After running this code, the row where lob_id = 5 will
contain, starting at position 100234 in the bin_data column,
the string "This string will overwrite a portion of the blob".

=head3 Example: Streaming character data from the database

In this example, we demonstrate a technique for streaming
data from the database to a file handle, in this case
STDOUT.  This allows more data to be read in and written out
than could be stored in memory at a given time.

   my $lob_id = 17;   # Arbitrary row identifier, for example

   $sth = $dbh->prepare( <<"   SQL", { ora_auto_lob => 0 } );
      SELECT chardata
      FROM lob_example
      WHERE lob_id = ?
   SQL
   $sth->execute( $lob_id );
   my ( $char_locator ) = $sth->fetchrow_array();

   my $chunk_size = 1034;   # Arbitrary chunk size, for example
   my $offset = 1;   # Offsets start at 1, not 0
   while(1) {
      my $data = $dbh->ora_lob_read( $char_locator, $offset, $chunk_size );
      last unless length $data;
      print STDOUT $data;
      $offset += $chunk_size;
   }

Notice that the select statement does not contain the phrase
"FOR UPDATE".  Because we are only reading from the LOB
Locator returned, and not modifying the LOB it refers to,
the select statement does not require the "FOR UPDATE"
clause.

A word of caution when using the data returned from an ora_lob_read in a conditional statement.
for example if the code below;

   while( my $data = $dbh->ora_lob_read( $char_locator, $offset, $chunk_size ) ) {
        print STDOUT $data;
        $offset += $chunk_size;
   }

was used with a chunk size of 4096 against a blob that requires more than 1 chunk to return
the data and the last chunk is one byte long and contains a zero (ASCII 48) you will miss this last byte
as $data will contain 0 which PERL will see as false and not print it out.

=head3 Example: Truncating existing large data

In this example, we truncate the data already present in a
large object column in the database.  Specifically, for each
row in the table, we truncate the 'bindata' value to half
its previous length.

After acquiring a LOB Locator for the column, we query its
length, then we trim the length by half.  Because we modify
the large objects with the call to ora_lob_trim(), we must
select the LOB locators 'FOR UPDATE'.

   my $sth = $dbh->prepare( <<"   SQL", { ora_auto_lob => 0 } );
      SELECT bindata
      FROM lob_example
      FOR UPATE
   SQL
   $sth->execute();
   while( my ( $bin_locator ) = $sth->fetchrow_array() ) {
      my $binlength = $dbh->ora_lob_length( $bin_locator );
      if( $binlength > 0 ) {
         $dbh->ora_lob_trim( $bin_locator, $binlength/2 );
      }
   }

=head1 SPACES AND PADDING

=head2 Trailing Spaces

Only the Oracle OCI 8 strips trailing spaces from VARCHAR placeholder
values and uses Nonpadded Comparison Semantics with the result.
This causes trouble if the spaces are needed for
comparison with a CHAR value or to prevent the value from
becoming '' which Oracle treats as NULL.
Look for Blank-padded Comparison Semantics and Nonpadded
Comparison Semantics in Oracle's SQL Reference or Server
SQL Reference for more details.

To preserve trailing spaces in placeholder values for Oracle clients that use OCI 8,
either change the default placeholder type with L</ora_ph_type> or the placeholder
type for a particular call to L<DBI/bind> or L<DBI/bind_param_inout>
with L</ora_type> or C<TYPE>.
Using L<ORA_CHAR> with L<ora_type> or C<SQL_CHAR> with C<TYPE>
allows the placeholder to be used with Padded Comparison Semantics
if the value it is being compared to is a CHAR, NCHAR, or literal.

Please remember that using spaces as a value or at the end of
a value makes visually distinguishing values with different
numbers of spaces difficult and should be avoided.

Oracle Clients that use OCI 9.2 do not strip trailing spaces.

=head2 Padded Char Fields

Oracle Clients after OCI 9.2 will automatically pad CHAR placeholder values to the size of the CHAR.
As the default placeholder type value in DBD::Oracle is ORA_VARCHAR2 to access this behaviour you will
have to change the default placeholder type with L</ora_ph_type> or placeholder
type for a particular call with L<DBI/bind> or L<DBI/bind_param_inout>
with L</ORA_CHAR>.

=head1 UNICODE

DBD::Oracle now supports Unicode UTF-8. There are, however, a number
of issues you should be aware of, so please read all this section
carefully.

In this section we'll discuss "Perl and Unicode", then "Oracle and
Unicode", and finally "DBD::Oracle and Unicode".

Information about Unicode in general can be found at:
L<http://www.unicode.org/>. It is well worth reading because there are
many misconceptions about Unicode and you may be holding some of them.

=head2 Perl and Unicode

Perl began implementing Unicode with version 5.6, but the implementation
did not mature until version 5.8 and later. If you plan to use Unicode
you are I<strongly> urged to use Perl 5.8.2 or later and to I<carefully> read
the Perl documentation on Unicode:

   perldoc perluniintro    # in Perl 5.8 or later
   perldoc perlunicode

And then read it again.

Perl's internal Unicode format is UTF-8
which corresponds to the Oracle character set called AL32UTF8.

=head2 Oracle and Unicode

Oracle supports many characters sets, including several different forms
of Unicode.  These include:

  AL16UTF16  =>  valid for NCHAR columns (CSID=2000)
  UTF8       =>  valid for NCHAR columns (CSID=871), deprecated
  AL32UTF8   =>  valid for NCHAR and CHAR columns (CSID=873)

When you create an Oracle database, you must specify the DATABASE
character set (used for DDL, DML and CHAR datatypes) and the NATIONAL
character set (used for NCHAR and NCLOB types).
The character sets used in your database can be found using:

  $hash_ref = $dbh->ora_nls_parameters()
  $database_charset = $hash_ref->{NLS_CHARACTERSET};
  $national_charset = $hash_ref->{NLS_NCHAR_CHARACTERSET};

The Oracle 9.2 and later default for the national character set is AL16UTF16.
The default for the database character set is often US7ASCII.
Although many experienced DBAs will consider an 8bit character set like
WE8ISO8859P1 or WE8MSWIN1252.  To use any character set with Oracle
other than US7ASCII, requires that the NLS_LANG environment variable be set.
See the L<"Oracle UTF8 is not UTF-8"> section below.

You are strongly urged to read the Oracle Internationalization documentation
specifically with respect the choices and trade offs for creating
a databases for use with international character sets.

Oracle uses the NLS_LANG environment variable to indicate what
character set is being used on the client.  When fetching data Oracle
will convert from whatever the database character set is to the client
character set specified by NLS_LANG. Similarly, when sending data to
the database Oracle will convert from the character set specified by
NLS_LANG to the database character set.

The NLS_NCHAR environment variable can be used to define a different
character set for 'national' (NCHAR) character types.

Both UTF8 and AL32UTF8 can be used in NLS_LANG and NLS_NCHAR.
For example:

   NLS_LANG=AMERICAN_AMERICA.UTF8
   NLS_LANG=AMERICAN_AMERICA.AL32UTF8
   NLS_NCHAR=UTF8
   NLS_NCHAR=AL32UTF8

=head2 Oracle UTF8 is not UTF-8

AL32UTF8 should be used in preference to UTF8 if it works for you,
which it should for Oracle 9.2 or later. If you're using an old
version of Oracle that doesn't support AL32UTF8 then you should
avoid using any Unicode characters that require surrogates, in other
words characters beyond the Unicode BMP (Basic Multilingual Plane).

That's because the character set that Oracle calls "UTF8" doesn't
conform to the UTF-8 standard in its handling of surrogate characters.
Technically the encoding that Oracle calls "UTF8" is known as "CESU-8".
Here are a couple of extracts from L<http://www.unicode.org/reports/tr26/>:

  CESU-8 is useful in 8-bit processing environments where binary
  collation with UTF-16 is required. It is designed and recommended
  for use only within products requiring this UTF-16 binary collation
  equivalence. It is not intended nor recommended for open interchange.

  As a very small percentage of characters in a typical data stream
  are expected to be supplementary characters, there is a strong
  possibility that CESU-8 data may be misinterpreted as UTF-8.
  Therefore, all use of CESU-8 outside closed implementations is
  strongly discouraged, such as the emittance of CESU-8 in output
  files, markup language or other open transmission forms.

Oracle uses this internally because it collates (sorts) in the same order
as UTF16, which is the basis of Oracle's internal collation definitions.

Rather than change UTF8 for clients Oracle chose to define a new character
set called "AL32UTF8" which does conform to the UTF-8 standard.
(The AL32UTF8 character set can't be used on the server because it
would break collation.)

Because of that, for the rest of this document we'll use "AL32UTF8".
If you're using an Oracle version below 9.2 you'll need to use "UTF8"
until you upgrade.

=head2 DBD::Oracle and Unicode

DBD::Oracle Unicode support has been implemented for Oracle versions 9
or greater, and Perl version 5.6 or greater (though we I<strongly>
suggest that you use Perl 5.8.2 or later).

You can check which Oracle version your DBD::Oracle was built with by
importing the C<ORA_OCI> constant from DBD::Oracle.

B<Fetching Data>

Any data returned from Oracle to DBD::Oracle in the AL32UTF8
character set will be marked as UTF-8 to ensure correct handling by Perl.

For Oracle to return data in the AL32UTF8 character set the
NLS_LANG or NLS_NCHAR environment variable I<must> be set as described
in the previous section.

When fetching NCHAR, NVARCHAR, or NCLOB data from Oracle, DBD::Oracle
will set the Perl UTF-8 flag on the returned data if either NLS_NCHAR
is AL32UTF8, or NLS_NCHAR is not set and NLS_LANG is AL32UTF8.

When fetching other character data from Oracle, DBD::Oracle
will set the Perl UTF-8 flag on the returned data if NLS_LANG is AL32UTF8.

B<Sending Data using Placeholders>

Data bound to a placeholder is assumed to be in the default client
character set (specified by NLS_LANG) except for a few special
cases. These are listed here with the highest precedence first:

If the C<ora_csid> attribute is given to bind_param() then that
is passed to Oracle and takes precedence.

If the value is a Perl Unicode string (UTF-8) then DBD::Oracle
ensures that Oracle uses the Unicode character set, regardless of
the NLS_LANG and NLS_NCHAR settings.

If the placeholder is for inserting an NCLOB then the client NLS_NCHAR
character set is used. (That's useful but inconsistent with the other behaviour
so may change. Best to be explicit by using the C<ora_csform>
attribute.)

If the C<ora_csform> attribute is given to bind_param() then that
determines if the value should be assumed to be in the default
(NLS_LANG) or NCHAR (NLS_NCHAR) client character set.

   use DBD::Oracle qw( SQLCS_IMPLICIT SQLCS_NCHAR );
   ...
   $sth->bind_param(1, $value, { ora_csform => SQLCS_NCHAR });

or

   $dbh->{ora_ph_csform} = SQLCS_NCHAR; # default for all future placeholders

Binding with bind_param_array and execute_array is also UTF-8 compatible in the same way.  If you attempt to
insert UTF-8 data into a non UTF-8 Oracle instance or with an non UTF-8 NCHAR or NVARCHAR the insert
will still happen but a error code of 0 will be returned with the following warning;

  DBD Oracle Warning: You have mixed utf8 and non-utf8 in an array bind in parameter#1. This may result in corrupt data.
  The Query charset id=1, name=US7ASCII

The warning will report the parameter number and the NCHAR setting that the query is running.

B<Sending Data using SQL>

Oracle assumes the SQL statement is in the default client character
set (as specified by NLS_LANG). So Unicode strings containing
non-ASCII characters should not be used unless the default client
character set is AL32UTF8.

=head2 DBD::Oracle and Other Character Sets and Encodings

The only multi-byte Oracle character set supported by DBD::Oracle is
"AL32UTF8" (and "UTF8"). Single-byte character sets should work well.

=head1 OBJECT & COLLECTION DATA TYPES

Oracle databases allow for the creation of object oriented like user-defined types.
There are two types of objects, Embedded--an object stored in a column of a regular table
and REF--an object that uses the REF retrieval mechanism.

DBD::Oracle supports only the 'selection' of embedded objects of the following types OBJECT, VARRAY
and TABLE in any combination. Support is seamless and recursive, meaning you
need only supply a simple SQL statement to get all the values in an embedded object.
You can either get the values as an array of scalars or they can be returned into a DBD::Oracle::Object.

Array example, given this type and table;

  CREATE OR REPLACE TYPE  "PHONE_NUMBERS" as varray(10) of varchar(30);

  CREATE TABLE  "CONTACT"
     (        "COMPANYNAME" VARCHAR2(40),
          "ADDRESS" VARCHAR2(100),
          "PHONE_NUMBERS"  "PHONE_NUMBERS"
   )

The code to access all the data in the table could be something like this;

   my $sth = $dbh->prepare('SELECT * FROM CONTACT');
   $sth->execute;
   while ( my ($company, $address, $phone) = $sth->fetchrow()) {
        print "Company: ".$company."\n";
        print "Address: ".$address."\n";
        print "Phone #: ";

        foreach my $items (@$phone){
           print $items.", ";
        }
        print "\n";
   }

Note that values in PHONE_NUMBERS are returned as an array reference '@$phone'.

As stated before DBD::Oracle will automatically drill into the embedded object and extract
all of the data as reference arrays of scalars. The example below has OBJECT type embedded in a TABLE type embedded in an
SQL TABLE;

   CREATE OR REPLACE TYPE GRADELIST AS TABLE OF NUMBER;

   CREATE OR REPLACE TYPE STUDENT AS OBJECT(
       NAME          VARCHAR2(60),
       SOME_GRADES   GRADELIST);

   CREATE OR REPLACE TYPE STUDENTS_T AS TABLE OF STUDENT;

   CREATE TABLE GROUPS(
       GRP_ID        NUMBER(4),
       GRP_NAME      VARCHAR2(10),
       STUDENTS      STUDENTS_T)
     NESTED TABLE STUDENTS STORE AS GROUP_STUDENTS_TAB
      (NESTED TABLE SOME_GRADES STORE AS GROUP_STUDENT_GRADES_TAB);

The following code will access all of the embedded data;

   $SQL='select grp_id,grp_name,students as my_students_test from groups';
   $sth=$dbh->prepare($SQL);
   $sth->execute();
   while (my ($grp_id,$grp_name,$students)=$sth->fetchrow()){
      print "Group ID#".$grp_id." Group Name =".$grp_name."\n";
      foreach my $student (@$students){
         print "Name:".$student->[0]."\n";
         print "Marks:";
         foreach my $grades (@$student->[1]){
            foreach my $marks (@$grades){
               print $marks.",";
            }
         }
         print "\n";
      }
      print "\n";
   }

Object example, given this object and table;

   CREATE OR REPLACE TYPE Person AS OBJECT (
     name    VARCHAR2(20),
     age     INTEGER)
   ) NOT FINAL;

   CREATE TYPE Employee UNDER Person (
     salary  NUMERIC(8,2)
   );

   CREATE TABLE people (id INTEGER, obj Person);

   INSERT INTO people VALUES (1, Person('Black', 25));
   INSERT INTO people VALUES (2, Employee('Smith', 44, 5000));

The following code will access the data;

   $dbh{'ora_objects'} =>1;

   $sth = $dbh->prepare("select * from people order by id");
   $sth->execute();

   # object are fetched as instance of DBD::Oracle::Object
   my ($id1, $obj1) = $sth->fetchrow();
   my ($id2, $obj2) = $sth->fetchrow();

   # get full type-name of object
   print $obj1->type_name."44\n";     # 'TEST.PERSON' is printed
   print $obj2->type_name."4\n";      # 'TEST.EMPLOYEE' is printed

   # get attribute NAME from object
   print $obj1->attr('NAME')."3\n";   # 'Black' is printed
   print $obj2->attr('NAME')."3\n";   # 'Smith' is printed

   # get all attributes as hash reference
   my $h1 = $obj1->attr;        # returns {'NAME' => 'Black', 'AGE' => 25}
   my $h2 = $obj2->attr;        # returns {'NAME' => 'Smith', 'AGE' => 44,
                                #          'SALARY' => 5000 }

   # get all attributes (names and values) as array
   my @a1 = $obj1->attributes;  # returns ('NAME', 'Black', 'AGE', 25)
   my @a2 = $obj2->attributes;  # returns ('NAME', 'Smith', 'AGE', 44,
                                #          'SALARY', 5000 )

So far DBD::Oracle has been tested on a table with 20 embedded Objects, Varrays and Tables
nested to 10 levels.

Any NULL values found in the embedded object will be returned as 'undef'.

=head1 OTHER DATA TYPES

DBD::Oracle does not I<explicitly> support most Oracle datatypes.
It simply asks Oracle to return them as strings and Oracle does so.
Mostly.  Similarly when binding placeholder values DBD::Oracle binds
them as strings and Oracle converts them to the appropriate type,
such as DATE, when used.

Some of these automatic conversions to and from strings use NLS
settings to control the formatting for output and the parsing for
input. The most common example is the DATE type. The default NLS
format for DATE might be DD-MON-YYYY and so when a DATE type is
fetched that's how Oracle will format the date. NLS settings also
control the default parsing of strings into DATE values. An error
will be generated if the contents of the string don't match the
NLS format. If you're dealing in dates which don't match the default
NLS format then you can either change the default NLS format or, more
commonly, use TO_CHAR(field, "format") and TO_DATE(?, "format")
to explicitly specify formats for converting to and from strings.

A slightly more subtle problem can occur with NUMBER types. The
default NLS settings might format numbers with a fullstop ("C<.>")
to separate thousands and a comma ("C<,>") as the decimal point.
Perl will generate warnings and use incorrect values when numbers,
returned and formatted as strings in this way by Oracle, are used
in a numeric context.  You could explicitly convert each numeric
value using the TO_CHAR(...) function but that gets tedious very
quickly. The best fix is to change the NLS settings. That can be
done for an individual connection by doing:

  $dbh->do("ALTER SESSION SET NLS_NUMERIC_CHARACTERS = '.,'");

There are some types, like BOOLEAN, that Oracle does not automatically
convert to or from strings (pity).  These need to be converted
explicitly using SQL or PL/SQL functions.

Examples:

  # DATE values
  my $sth0 = $dbh->prepare( <<SQL_END );
  SELECT username, TO_CHAR( created, ? )
      FROM all_users
      WHERE created >= TO_DATE( ?, ? )
   SQL_END
  $sth0->execute( 'YYYY-MM-DD HH24:MI:SS', "2003", 'YYYY' );

  # BOOLEAN values
  my $sth2 = $dbh->prepare( <<PLSQL_END );
  DECLARE
      b0 BOOLEAN;
      b1 BOOLEAN;
      o0 VARCHAR2(32);
      o1 VARCHAR2(32);

      FUNCTION to_bool( i VARCHAR2 ) RETURN BOOLEAN IS
      BEGIN
         IF    i IS NULL          THEN RETURN NULL;
         ELSIF i = 'F' OR i = '0' THEN RETURN FALSE;
         ELSE                          RETURN TRUE;
         END IF;
      END;
      FUNCTION from_bool( i BOOLEAN ) RETURN NUMBER IS
      BEGIN
         IF    i IS NULL THEN RETURN NULL;
         ELSIF i         THEN RETURN 1;
         ELSE                 RETURN 0;
         END IF;
      END;
  BEGIN
      -- Converting values to BOOLEAN
      b0 := to_bool( :i0 );
      b1 := to_bool( :i1 );

      -- Converting values from BOOLEAN
      :o0 := from_bool( b0 );
      :o1 := from_bool( b1 );
  END;
  PLSQL_END
  my ( $i0, $i1, $o0, $o1 ) = ( "", "Something else" );
  $sth2->bind_param( ":i0", $i0 );
  $sth2->bind_param( ":i1", $i1 );
  $sth2->bind_param_inout( ":o0", \$o0, 32 );
  $sth2->bind_param_inout( ":o1", \$o1, 32 );
  $sth2->execute();
  foreach ( $i0, $b0, $o0, $i1, $b1, $o1 ) {
      $_ = "(undef)" if ! defined $_;
  }
  print "$i0 to $o0, $i1 to $o1\n";
  # Result is : "'' to '(undef)', 'Something else' to '1'"

=head2 Support for Insert of XMLType (ORA_XMLTYPE)

Inserting large XML data sets into tables with XMLType fields is now supported by DBD::Oracle. The only special
requirement is the use of bind_param() with an attribute hash parameter that specifies ora_type as ORA_XMLTYPE. For
example with a table like this;

   create table books (book_id number, book_xml XMLType);

one can insert data using this code

   $SQL='insert into books values (1,:p_xml)';
   $xml= '<Books>
                 <Book id=1>
                        <Title>Programming the Perl DBI</Title>
                        <Subtitle>The Cheetah Book</Subtitle>
                        <Authors>
                               <Author>T. Bunce</Author>
                               <Author>Alligator Descartes</Author>
                        </Authors>

                </Book>
                <Book id=10000>...
            </Books>';
   my $sth =$dbh-> prepare($SQL);
   $sth-> bind_param("p_xml", $xml, { ora_type => ORA_XMLTYPE });
   $sth-> execute();

In the above case we will assume that $xml has 10000 Book nodes and is over 32k in size and is well formed XML.
This will also work for XML that is smaller than 32k as well. Attempting to insert malformed XML will cause an error.

=head2 Binding Cursors

Cursors can be returned from PL/SQL blocks, either from stored
functions (or procedures with OUT parameters) or
from direct C<OPEN> statements, as shown below:

  use DBI;
  use DBD::Oracle qw(:ora_types);
  my $dbh = DBI->connect(...);
  my $sth1 = $dbh->prepare(q{
      BEGIN OPEN :cursor FOR
          SELECT table_name, tablespace_name
          FROM user_tables WHERE tablespace_name = :space;
      END;
  });
  $sth1->bind_param(":space", "USERS");
  my $sth2;
  $sth1->bind_param_inout(":cursor", \$sth2, 0, { ora_type => ORA_RSET } );
  $sth1->execute;
  # $sth2 is now a valid DBI statement handle for the cursor
  while ( my @row = $sth2->fetchrow_array ) { ... }

The only special requirement is the use of C<bind_param_inout()> with an
attribute hash parameter that specifies C<ora_type> as C<ORA_RSET>.
If you don't do that you'll get an error from the C<execute()> like:
"ORA-06550: line X, column Y: PLS-00306: wrong number or types of
arguments in call to ...".

Here's an alternative form using a function that returns a cursor.
This example uses the pre-defined weak (or generic) REF CURSOR type
SYS_REFCURSOR. This is an Oracle 9 feature.

  # Create the function that returns a cursor
  $dbh->do(q{
      CREATE OR REPLACE FUNCTION sp_ListEmp RETURN SYS_REFCURSOR
      AS l_cursor SYS_REFCURSOR;
      BEGIN
          OPEN l_cursor FOR select ename, empno from emp
              ORDER BY ename;
          RETURN l_cursor;
      END;
  });

  # Use the function that returns a cursor
  my $sth1 = $dbh->prepare(q{BEGIN :cursor := sp_ListEmp; END;});
  my $sth2;
  $sth1->bind_param_inout(":cursor", \$sth2, 0, { ora_type => ORA_RSET } );
  $sth1->execute;
  # $sth2 is now a valid DBI statement handle for the cursor
  while ( my @row = $sth2->fetchrow_array ) { ... }

A cursor obtained from PL/SQL as above may be passed back to PL/SQL
by binding for input, as shown in this example, which explicitly
closes a cursor:

  my $sth3 = $dbh->prepare("BEGIN CLOSE :cursor; END;");
  $sth3->bind_param(":cursor", $sth2, { ora_type => ORA_RSET } );
  $sth3->execute;

It is not normally necessary to close a cursor explicitly in this
way. Oracle will close the cursor automatically at the first
client-server interaction after the cursor statement handle is
destroyed. An explicit close may be desirable if the reference to the
cursor handle from the PL/SQL statement handle delays the destruction
of the cursor handle for too long. This reference remains until the
PL/SQL handle is re-bound, re-executed or destroyed.

NOTE: From DBD::Oracle 1.57 functions or procedures returning
SYS_REFCURSORs which have not been opened (are still in the
initialised state) will return undef for the cursor statement handle
e.g., in the example above if the sp_ListEmp function simply returned l_cursor
instead of opening it. This means you can have a function/procedure
which can elect to open the cursor or not, Before this change if you called
a function/procedure which returned a SYS_REFCURSOR which was not opened
DBD::Oracle would error in the execute for a OCIAttrGet on the uninitialised
cursor.

See the C<curref.pl> script in the Oracle.ex directory in the DBD::Oracle
source distribution for a complete working example.

=head2 Fetching Nested Cursors

Oracle supports the use of select list expressions of type REF CURSOR.
These may be explicit cursor expressions - C<CURSOR(SELECT ...)>, or
calls to PL/SQL functions which return REF CURSOR values. The values
of these expressions are known as nested cursors.

The value returned to a Perl program when a nested cursor is fetched
is a statement handle. This statement handle is ready to be fetched from.
It should not (indeed, must not) be executed.

Oracle imposes a restriction on the order of fetching when nested
cursors are used. Suppose C<$sth1> is a handle for a select statement
involving nested cursors, and C<$sth2> is a nested cursor handle fetched
from C<$sth1>. C<$sth2> can only be fetched from while C<$sth1> is
still active, and the row containing C<$sth2> is still current in C<$sth1>.
Any attempt to fetch another row from C<$sth1> renders all nested cursor
handles previously fetched from C<$sth1> defunct.

Fetching from such a defunct handle results in an error with the message
C<ERROR nested cursor is defunct (parent row is no longer current)>.

This means that the C<fetchall...> or C<selectall...> methods are not useful
for queries returning nested cursors. By the time such a method returns,
all the nested cursor handles it has fetched will be defunct.

It is necessary to use an explicit fetch loop, and to do all the
fetching of nested cursors within the loop, as the following example
shows:

    use DBI;
    my $dbh = DBI->connect(...);
    my $sth = $dbh->prepare(q{
        SELECT dname, CURSOR(
            SELECT ename FROM emp
                WHERE emp.deptno = dept.deptno
                ORDER BY ename
        ) FROM dept ORDER BY dname
    });
    $sth->execute;
    while ( my ($dname, $nested) = $sth->fetchrow_array ) {
        print "$dname\n";
        while ( my ($ename) = $nested->fetchrow_array ) {
            print "        $ename\n";
        }
    }

The cursor returned by the function C<sp_ListEmp> defined in the
previous section can be fetched as a nested cursor as follows:

    my $sth = $dbh->prepare(q{SELECT sp_ListEmp FROM dual});
    $sth->execute;
    my ($nested) = $sth->fetchrow_array;
    while ( my @row = $nested->fetchrow_array ) { ... }

=head2 Pre-fetching Nested Cursors

By default, DBD::Oracle pre-fetches rows in order to reduce the number of
round trips to the server. For queries which do not involve nested cursors,
the number of pre-fetched rows is controlled by the DBI database handle
attribute C<RowCacheSize> (q.v.).

In Oracle, server side open cursors are a controlled resource, limited in
number, on a per session basis, to the value of the initialization
parameter C<OPEN_CURSORS>. Nested cursors count towards this limit.
Each nested cursor in the current row counts 1, as does
each nested cursor in a pre-fetched row. Defunct nested cursors do not count.

An Oracle specific database handle attribute, C<ora_max_nested_cursors>,
further controls pre-fetching for queries involving nested cursors. For
each statement handle, the total number of nested cursors in pre-fetched
rows is limited to the value of this parameter. The default value
is 0, which disables pre-fetching for queries involving nested cursors.

=head1 PL/SQL Examples

Most of these PL/SQL examples come from: Eric Bartley <bartley@cc.purdue.edu>.

   /*
    * PL/SQL to create package with stored procedures invoked by
    * Perl examples.  Execute using sqlplus.
    *
    * Use of "... OR REPLACE" prevents failure in the event that the
    * package already exists.
    */

    CREATE OR REPLACE PACKAGE plsql_example
    IS
      PROCEDURE proc_np;

      PROCEDURE proc_in (
          err_code IN NUMBER
      );

      PROCEDURE proc_in_inout (
          test_num IN NUMBER,
          is_odd IN OUT NUMBER
      );

      FUNCTION func_np
        RETURN VARCHAR2;

    END plsql_example;
  /

    CREATE OR REPLACE PACKAGE BODY plsql_example
    IS
      PROCEDURE proc_np
      IS
        whoami VARCHAR2(20) := NULL;
      BEGIN
        SELECT USER INTO whoami FROM DUAL;
      END;

      PROCEDURE proc_in (
        err_code IN NUMBER
      )
      IS
      BEGIN
        RAISE_APPLICATION_ERROR(err_code, 'This is a test.');
      END;

      PROCEDURE proc_in_inout (
        test_num IN NUMBER,
        is_odd IN OUT NUMBER
      )
      IS
      BEGIN
        is_odd := MOD(test_num, 2);
      END;

      FUNCTION func_np
        RETURN VARCHAR2
      IS
        ret_val VARCHAR2(20);
      BEGIN
        SELECT USER INTO ret_val FROM DUAL;
        RETURN ret_val;
      END;

    END plsql_example;
  /
  /* End PL/SQL for example package creation. */

  use DBI;

  my($db, $csr, $ret_val);

  $db = DBI->connect('dbi:Oracle:database','user','password')
        or die "Unable to connect: $DBI::errstr";

  # So we don't have to check every DBI call we set RaiseError.
  # See the DBI docs now if you're not familiar with RaiseError.
  $db->{RaiseError} = 1;

  # Example 1        Eric Bartley <bartley@cc.purdue.edu>
  #
  # Calling a PLSQL procedure that takes no parameters. This shows you the
  # basic's of what you need to execute a PLSQL procedure. Just wrap your
  # procedure call in a BEGIN END; block just like you'd do in SQL*Plus.
  #
  # p.s. If you've used SQL*Plus's exec command all it does is wrap the
  #      command in a BEGIN END; block for you.

  $csr = $db->prepare(q{
    BEGIN
      PLSQL_EXAMPLE.PROC_NP;
    END;
  });
  $csr->execute;


  # Example 2        Eric Bartley <bartley@cc.purdue.edu>
  #
  # Now we call a procedure that has 1 IN parameter. Here we use bind_param
  # to bind out parameter to the prepared statement just like you might
  # do for an INSERT, UPDATE, DELETE, or SELECT statement.
  #
  # I could have used positional placeholders (e.g. :1, :2, etc.) or
  # ODBC style placeholders (e.g. ?), but I prefer Oracle's named
  # placeholders (but few DBI drivers support them so they're not portable).

  my $err_code = -20001;

  $csr = $db->prepare(q{
          BEGIN
              PLSQL_EXAMPLE.PROC_IN(:err_code);
          END;
  });

  $csr->bind_param(":err_code", $err_code);

  # PROC_IN will RAISE_APPLICATION_ERROR which will cause the execute to 'fail'.
  # Because we set RaiseError, the DBI will croak (die) so we catch that with eval.
  eval {
    $csr->execute;
  };
  print 'After proc_in: $@=',"'$@', errstr=$DBI::errstr, ret_val=$ret_val\n";


  # Example 3        Eric Bartley <bartley@cc.purdue.edu>
  #
  # Building on the last example, I've added 1 IN OUT parameter. We still
  # use a placeholders in the call to prepare, the difference is that
  # we now call bind_param_inout to bind the value to the place holder.
  #
  # Note that the third parameter to bind_param_inout is the maximum size
  # of the variable. You normally make this slightly larger than necessary.
  # But note that the Perl variable will have that much memory assigned to
  # it even if the actual value returned is shorter.

  my $test_num = 5;
  my $is_odd;

  $csr = $db->prepare(q{
          BEGIN
              PLSQL_EXAMPLE.PROC_IN_INOUT(:test_num, :is_odd);
          END;
  });

  # The value of $test_num is _copied_ here
  $csr->bind_param(":test_num", $test_num);

  $csr->bind_param_inout(":is_odd", \$is_odd, 1);

  # The execute will automagically update the value of $is_odd
  $csr->execute;

  print "$test_num is ", ($is_odd) ? "odd - ok" : "even - error!", "\n";


  # Example 4        Eric Bartley <bartley@cc.purdue.edu>
  #
  # What about the return value of a PLSQL function? Well treat it the same
  # as you would a call to a function from SQL*Plus. We add a placeholder
  # for the return value and bind it with a call to bind_param_inout so
  # we can access its value after execute.

  my $whoami = "";

  $csr = $db->prepare(q{
          BEGIN
              :whoami := PLSQL_EXAMPLE.FUNC_NP;
          END;
  });

  $csr->bind_param_inout(":whoami", \$whoami, 20);
  $csr->execute;
  print "Your database user name is $whoami\n";

  $db->disconnect;

You can find more examples in the t/plsql.t file in the DBD::Oracle
source directory.

Oracle 9.2 appears to have a bug where a variable bound
with bind_param_inout() that isn't assigned to by the executed
PL/SQL block may contain garbage.
See L<http://www.mail-archive.com/dbi-users@perl.org/msg18835.html>

=head2 Avoid Using "SQL Call"

Avoid using the "SQL Call" statement with DBD:Oracle as you might find that
DBD::Oracle will not raise an exception in some case.  Specifically if you use
"SQL Call" to run a procedure all "No data found" exceptions will be quietly
ignored and returned as null. According to Oracle support this is part of the same
mechanism where;

  select (select * from dual where 0=1) from dual

returns a null value rather than an exception.

=head1 CONTRIBUTING

If you'd like DBD::Oracle to do something new or different the best way
to make that happen is to do it yourself and email to dbi-dev@perl.org a
patch of the source code (using 'diff' - see below) that shows the changes.

=head2 Speak before you patch

For anything non-trivial or possibly controversial it's a good idea
to discuss (on dbi-dev@perl.org) the changes you propose before
actually spending time working on them. Otherwise you run the risk
of them being rejected because they don't fit into some larger plans
you may not be aware of.

=head1 WHICH VERSION OF DBD::ORACLE IS FOR ME?

From version 1.25 onwards DBD::Oracle only support Oracle clients
9.2 or greater. Support for ProC connections was dropped in 1.29.

If you are still stuck with an older version of Oracle or its client you might want to look at the table below.

  +---------------------+-----------------------------------------------------+
  |                     |                   Oracle Version                    |
  +---------------------+----+-------------+---------+------+--------+--------+
  | DBD::Oracle Version | <8 | 8.0.3~8.0.6 | 8iR1~R2 | 8iR3 |   9i   | 9.2~11 |
  +---------------------+----+-------------+---------+------+--------+--------+
  |      0.1~16         | Y  |      Y      |    Y    |  Y   |    Y   |    Y   |
  +---------------------+----+-------------+---------+------+--------+--------+
  |      1.17           | Y  |      Y      |    Y    |  Y   |    Y   |    Y   |
  +---------------------+----+-------------+---------+------+--------+--------+
  |      1.18           | N  |      N      |    N    |  Y   |    Y   |    Y   |
  +---------------------+----+-------------+---------+------+--------+--------+
  |      1.19           | N  |      N      |    N    |  Y   |    Y   |    Y   |
  +---------------------+----+-------------+---------+------+--------+--------+
  |      1.20           | N  |      N      |    N    |  Y   |    Y   |    Y   |
  +---------------------+----+-------------+---------+------+--------+--------+
  |      1.21~1.24      | N  |      N      |    N    |  N   |    Y   |    Y   |
  +---------------------+----+-------------+---------+------+--------+--------+
  |      1.25+          | N  |      N      |    N    |  N   |    N   |    Y   |
  +---------------------+----+-------------+---------+------+--------+--------+

As there are dozens of different versions of Oracle's clients this
list does not include all of them, just the major released versions of
Oracle.

Note that one can still connect to any Oracle version with the older
DBD::Oracle versions the only problem you will have is that some of
the newer OCI and Oracle features available in later DBD::Oracle
releases will not be available to you.

So to make a short story a little longer:

=over

=item 1

If you are using Oracle 7 or early 8 DB and you can manage to get a 9 client and you can use
any DBD::Oracle version.

=item 2

If you have to use an Oracle 7 client then DBD::Oracle 1.17 should work

=item 3

Same thing for 8 up to R2, use 1.17, if you are lucky and have the right patch-set you might
go with 1.18.

=item 4

For 8iR3 you can use any of the DBD::Oracle versions up to 1.21. Again this depends on your
patch-set, If you run into trouble go with 1.19

=item 5

After 9.2 you can use any version you want.

=item 6

It seems that the 10g client can only connect to 9 and 11 DBs while the 9 can go back to 7
and even get to 10. I am not sure what the 11g client can connect to.

=back

=head1 SEE ALSO

=over

=item L<DBI>

http://search.cpan.org/~timb/DBD-Oracle/MANIFEST for all files in
the DBD::Oracle source distribution including the examples in the
Oracle.ex directory

=item DBD::Oracle Tutorial

http://www.pythian.com/blogs/wp-content/uploads/introduction-dbd-oracle.html

=item Oracle Instant Client

http://www.oracle.com/technology/tech/oci/instantclient/index.html

=item Oracle on Linux

http://www.ixora.com.au/

=item Free Oracle Tools and Links

ora_explain supplied and installed with DBD::Oracle.

http://www.orafaq.com/

http://vonnieda.org/oracletool/

=item Commercial Oracle Tools and Links

Assorted tools and references for general information.
No recommendation implied.

http://www.platinum.com

http://www.SoftTreeTech.com

Also PL/Vision from RevealNet and Steven Feuerstein, and
"Q" from Savant Corporation.

=back

=head1 AUTHORS

DBI by Tim Bunce L<http://www.tim.bunce.name>.

The original C<DBD::Oracle> was by Tim Bunce.
Maintained as of release 1.17 (February 2006) by John Scoles, then Yanick Champoux, under the
auspice of the Pythian Group (L<http://www.pythian.com>).

=head1 ACKNOWLEDGEMENTS

A great many people have helped with DBD::Oracle over the 17 years
between 1994 and 2011.  Far too many to name, but we thank them all.
Many are named in the Changes file.

=head1 COPYRIGHT

The DBD::Oracle module is Copyright (c) 1994-2006 Tim Bunce. Ireland.
The DBD::Oracle module is Copyright (c) 2006-2011 John Scoles (The Pythian Group). Canada.
The DBD::Oracle module is Copyright (c) 2011 John Scoles. Canada.

The DBD::Oracle module is free open source software; you can
redistribute it and/or modify it under the same terms as Perl 5.

=head1 AUTHORS

=over 4

=item *

Tim Bunce <timb@cpan.org>

=item *

John Scoles <byterock@cpan.org>

=item *

Yanick Champoux <yanick@cpan.org>

=item *

Martin J. Evans <mjevans@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018, 2014, 2013, 2012, 2011, 2010 by Tim Bunce.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
