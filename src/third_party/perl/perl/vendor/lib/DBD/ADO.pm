{
  package DBD::ADO;

  use strict;
  use DBI();
  use Win32::OLE();
  use vars qw($VERSION $drh);

  $VERSION = '2.99';

  $drh = undef;


  sub driver {
    return $drh if $drh;
    my ( $class, $attr ) = @_;
    $drh = DBI::_new_drh( $class . '::dr', {
      Name        => 'ADO'
    , Version     => $VERSION
    , Attribution => 'DBD ADO for Win32 by Tim Bunce, Phlip, Thomas Lowery and Steffen Goeldner'
    } );
    if ( $DBI::VERSION >= 1.37 ) {
      DBD::ADO::db->install_method('ado_open_schema');
    }
    $drh->STORE('LongReadLen', 2147483647 );
    return $drh;
  }


  sub CLONE {
    undef $drh;
  }


  sub errors {
    my $h = shift;
    my $Cxn = $h->{ado_conn};
    my $MaxErrors = $h->{ado_max_errors} || 50;
    my @Err = ();

    my $lastError = Win32::OLE->LastError;
    if ( $lastError ) {
      $h->{ado_errcum} = $h->{ado_err} = 0+$lastError;
      push @Err,"\n  Last error : $h->{ado_err}\n\n$lastError";
    }
    else {
      $h->{ado_errcum} = $h->{ado_err} = 0;
    }
    $h->{ado_state} = '';

    if ( ref $Cxn ) {
      my $Errors = $Cxn->Errors;
      if ( $Errors ) {
        my $Count = $Errors->Count;
        for ( my $i = 1; $i <= $Count; $i++ ) {
          if ( $i > $MaxErrors ) {
            push @Err,"\n    ... (too many errors: $Count)";
            $i = $Count;
          }
          my $err = $Errors->Item( $i - 1 );
          push @Err,'';
          push @Err, sprintf "%19s : %s", $_, $err->$_ ||'' for qw(
            Description HelpContext HelpFile NativeError Number Source SQLState);
          push @Err,'    ';
          $h->{ado_errcum} |= $err->Number;
          $h->{ado_state}   = $err->SQLState ||'';
        }
        $Errors->Clear;
      }
    }
    join "\n", @Err;
  }


  sub Failed {
    my $h   = shift;

    my $lastError = DBD::ADO::errors( $h ) or return 0;

    my ( $package, $filename, $line ) = caller;
    my $s = shift()
          . "\n"
          . "\n  Package    : $package"
          . "\n  Filename   : $filename"
          . "\n  Line       : $line"
          ;
    $h->{ado_err} = 0 unless $h->{ado_errcum} & 1 << 31;  # oledberr.h
    my $state = $h->{ado_state} if length $h->{ado_state} == 5;
    $h->set_err( $h->{ado_err}, $s . $lastError, $state );
    return 1;
  }

}

{ package DBD::ADO::dr; # ====== DRIVER ======

  use strict;
  use DBI();
  use Win32::OLE();

  $DBD::ADO::dr::imp_data_size = 0;


  sub data_sources {
    my ( $drh, $attr ) = @_;
    my @list = ();
    $drh->{ado_data_sources} ||= eval { require Local::DBD::ADO::DSN } || [];
    $drh->trace_msg("    !! $@", 7 ) if $@;
    for my $h ( @{$drh->{ado_data_sources}} ) {
      my @a = map "$_=$h->{$_}", sort keys %$h;
      push @list,'dbi:ADO:' . join(';', @a );
    }
    return @list;
  }


  sub connect {
    my ( $drh, $dsn, $user, $auth, $attr ) = @_;

    local $Win32::OLE::Warn = 0;

    my $conn = Win32::OLE->new('ADODB.Connection');
    return if DBD::ADO::Failed( $drh,"Can't create 'ADODB.Connection'");

    if ( exists $attr->{ado_ConnectionTimeout} ) {
      $conn->{ConnectionTimeout} = $attr->{ado_ConnectionTimeout};
      return if DBD::ADO::Failed( $drh,"Can't set ConnectionTimeout");
    }
    if ( exists $attr->{ado_Mode} ) {
      $conn->{Mode} = $attr->{ado_Mode};
      return if DBD::ADO::Failed( $drh,"Can't set Mode");
    }

    my ( $outer, $dbh ) = DBI::_new_dbh( $drh, { Name => $dsn } );

    $dbh->{AutoCommit}     = 1;  # Initially, ADO is in auto-commit mode

    $dbh->{ado_conn}       = $conn;
    $dbh->{ado_max_errors} = 50;
    $dbh->{ado_ti_ver}     = 2;  # TypeInfo version

		# ODBC rule: NULL is not the same as an empty password ...
		$auth = '' unless defined $auth;

		my @dsn;
		for my $s ( split /;/, $dsn ) {
			my ( $k, $v ) = split /=/, $s, 2;
			if ( defined $conn->{$k} ) {
				$conn->{$k} = $v;
				next;
			}
			push @dsn, $s;
		}
		my $ConnectionString = join ';', @dsn;
		$drh->trace_msg("    -- ConnectionString: $ConnectionString\n", 5 );

		$conn->Open( $ConnectionString, $user, $auth );
		return if DBD::ADO::Failed( $drh,"Can't Open Connection '$dsn'");

		# Determine transaction support
		eval {
			$dbh->{ado_txn_capable} = $conn->{Properties}{'Transaction DDL'}{Value};
		};
		if ( $@ ) {
			$dbh->{ado_txn_capable} = 0;
			my $lastError = DBD::ADO::errors( $dbh );
			$drh->trace_msg("    !! Can't determine transaction support: $lastError\n", 5 );
		}
		$drh->trace_msg("    -- Transaction support: $dbh->{ado_txn_capable}\n", 5 );

    $dbh->STORE('Warn'  , 0 );
    $dbh->STORE('Active', 1 );

    return $outer;
  }

} # ====== DRIVER ======

{ package DBD::ADO::db; # ====== DATABASE ======

  use strict;
  use DBI();
  use Win32::OLE();
  use Win32::OLE::Variant();
  use DBD::ADO::TypeInfo();
  use DBD::ADO::Const();
  use Carp();

  $DBD::ADO::db::imp_data_size = 0;

  my $Enums = DBD::ADO::Const->Enums;

  my $ado_schematables = [
    qw( TABLE_CAT TABLE_SCHEM TABLE_NAME TABLE_TYPE REMARKS
      TABLE_GUID TABLE_PROPID DATE_CREATED DATE_MODIFIED
  ) ];
  my $ado_dbi_schematables = [
    qw( TABLE_CAT TABLE_SCHEM TABLE_NAME TABLE_TYPE REMARKS )
  ];
  my $sch_dbi_to_ado = {
    TABLE_CAT     => 'TABLE_CATALOG'
  , TABLE_SCHEM   => 'TABLE_SCHEMA'
  , TABLE_NAME    => 'TABLE_NAME'
  , TABLE_TYPE    => 'TABLE_TYPE'
  , REMARKS       => 'DESCRIPTION'
  , TABLE_GUID    => 'TABLE_GUID'
  , TABLE_PROPID  => 'TABLE_PROPID'
  , DATE_CREATED  => 'DATE_CREATED'
  , DATE_MODIFIED => 'DATE_MODIFIED'
  };


  sub ping {
    my ( $dbh ) = @_;
    my $conn = $dbh->{ado_conn};

    defined $conn && $conn->State & $Enums->{ObjectStateEnum}{adStateOpen};
  }


  sub disconnect {
    my ( $dbh ) = @_;
    my $conn = $dbh->{ado_conn};

    if ( defined $conn ) {
      local $Win32::OLE::Warn = 0;
      my $State = $conn->State || 0;
      $dbh->trace_msg("    -- State: $State\n", 5 );
      if ( $State & $Enums->{ObjectStateEnum}{adStateOpen} ) {
        # Change the connection attribute so Commit/Rollback
        # does not start another transaction.
        $conn->{Attributes} = 0;
        my $lastError = DBD::ADO::errors( $dbh );
        return $dbh->set_err( -925,"Can't set CommitRetaining: $lastError") if $lastError && $lastError !~ m/-2147168242/;
        $dbh->trace_msg('    -- Modified ADO Connection Attributes: ' . $conn->{Attributes} . "\n", 5 );

        $dbh->rollback if !$dbh->{AutoCommit} && $dbh->{ado_txn_capable};

        $conn->Close;
      }
      $dbh->{ado_conn} = undef;
    }
    $dbh->SUPER::STORE('Active', 0 );
    return 1;
  }


	sub commit {
		my ( $dbh ) = @_;
		my $conn = $dbh->{ado_conn};

		return Carp::carp "Commit ineffective when AutoCommit is on\n"
			if $dbh->{AutoCommit} && $dbh->FETCH('Warn');
		return Carp::carp 'Transactions are not supported'
			unless $dbh->{ado_txn_capable};
    if ( $dbh->FETCH('BegunWork') ) {
      $dbh->{AutoCommit} = 1;
      $dbh->SUPER::STORE('BegunWork', 0 );
      $conn->{Attributes} = 0;
      return if DBD::ADO::Failed( $dbh,"Can't set CommitRetaining");
    }
		if ( defined $conn && $conn->State & $Enums->{ObjectStateEnum}{adStateOpen} ) {
			$conn->CommitTrans;
			return if DBD::ADO::Failed( $dbh,"Can't Commit transaction");
		}
    return 1;
	}


	sub rollback {
		my ( $dbh ) = @_;
		my $conn = $dbh->{ado_conn};

		return Carp::carp "Rollback ineffective when AutoCommit is on\n"
			if $dbh->{AutoCommit} && $dbh->FETCH('Warn');
		return Carp::carp 'Transactions are not supported'
			unless $dbh->{ado_txn_capable};
    if ( $dbh->FETCH('BegunWork') ) {
      $dbh->{AutoCommit} = 1;
      $dbh->SUPER::STORE('BegunWork', 0 );
      $conn->{Attributes} = 0;
      return if DBD::ADO::Failed( $dbh,"Can't set CommitRetaining");
    }
		if ( defined $conn && $conn->State & $Enums->{ObjectStateEnum}{adStateOpen} ) {
			$conn->RollbackTrans;
			return if DBD::ADO::Failed( $dbh,"Can't Rollback transaction");
		}
    return 1;
	}


	# The create parm methods builds a usable type statement for constructing
	# tables.
	# XXX This method may not stay ...
	sub create_parm {
		my ( $dbh, $type ) = @_;

		my $field = undef;

		if ( $type ) {
    	$field = $type->{TYPE_NAME};
			if ( defined $type->{CREATE_PARAMS} ) {
				$field .= '(' . $type->{COLUMN_SIZE} . ')'
					if $type->{CREATE_PARAMS} =~ /LENGTH/i;
				$field .= '(' . $type->{COLUMN_SIZE} . ', 0)'
					if $type->{CREATE_PARAMS} =~ /PRECISION,SCALE/i;
			}
		}
		return $field;
	}


	sub prepare {
		my ( $dbh, $statement, $attr ) = @_;
		my $conn = $dbh->{ado_conn};

		my $comm = Win32::OLE->new('ADODB.Command');
		return if DBD::ADO::Failed( $dbh,"Can't create 'ADODB.Command'");

		$comm->{ActiveConnection} = $conn;
		return if DBD::ADO::Failed( $dbh,"Can't set ActiveConnection");

		$comm->{CommandText} = $statement;
		return if DBD::ADO::Failed( $dbh,"Can't set CommandText");

		my $ct = $attr->{CommandType} ? $attr->{CommandType} : 'adCmdText';
		$comm->{CommandType} = $Enums->{CommandTypeEnum}{$ct};
		return if DBD::ADO::Failed( $dbh,"Can't set CommandType");

		$comm->{CommandTimeout} = defined $attr->{ado_commandtimeout}
      ? $attr->{ado_commandtimeout} : $conn->{CommandTimeout};
		return if DBD::ADO::Failed( $dbh,"Can't set CommandTimeout");

		my ( $outer, $sth ) = DBI::_new_sth( $dbh, { Statement => $statement } );

		$sth->{ado_cachesize}     = $dbh->{ado_cachesize};
		$sth->{ado_comm}          = $comm;
		$sth->{ado_conn}          = $conn;
		$sth->{ado_cursortype}    = $dbh->{ado_cursortype} || $attr->{CursorType};
		$sth->{ado_fields}        = undef;
		$sth->{ado_max_errors}    = $dbh->{ado_max_errors};
		$sth->{ado_refresh}       = 1;
		$sth->{ado_rownum}        = -1;
		$sth->{ado_rows}          = -1;
		$sth->{ado_rowset}        = undef;
		$sth->{ado_type}          = undef;
		$sth->{ado_usecmd}        = undef;
		$sth->{ado_users}         = undef;
		$sth->{ado_executeoption} = 0;

		# Set overrides for and attributes.
		for my $key ( grep { /^ado_/ } keys %$attr ) {
      next if $key eq 'ado_commandtimeout';
			$sth->trace_msg("    -- Attribute: $key => $attr->{$key}\n", 5 );
			if ( exists $sth->{$key} ) {
				$sth->{$key} = $attr->{$key};
			}
			else {
				warn "Unknown attribute $key\n";
			}
		}

    my $Cnt;
    if ( $sth->{ado_refresh} == 1 ) {
      # Refresh() is - among other things - useful to detect syntax errors.
      # The eval block is used because Refresh() may not be supported (but
      # no such case is known).
      # Buggy drivers, e.g. FoxPro, may leave the Parameters collection
      # empty, without returning an error. Then _refresh() is deferred until
      # bind_param() is called.
      eval {
        local $Win32::OLE::Warn = 0;
        $comm->Parameters->Refresh;
        $Cnt = $comm->Parameters->Count;
      };
      my $lastError = DBD::ADO::errors( $dbh );
      if ( $lastError ) {
        $dbh->trace_msg("    !! Refresh error: $lastError\n", 5 );
        $sth->{ado_refresh} = 2;
      }
    }
    if ( $sth->{ado_refresh} == 2 ) {
      $Cnt = DBD::ADO::st::_refresh( $sth );
    }
	# LRB
	if ( $sth->{ado_executeoption} && $sth->{ado_executeoption} == $Enums->{ExecuteOptionEnum}{adExecuteStream}) {
		my $sResponseStream = Win32::OLE->new('ADODB.Stream');
		return if DBD::ADO::Failed($dbh, "Can't create 'ADODB.Stream'");
		$sResponseStream->Open();
		return if DBD::ADO::Failed($dbh, "Can't open 'ADODB.Stream'");
		my $vObj = Win32::OLE::Variant->new(Win32::OLE::Variant::VT_VARIANT()|Win32::OLE::Variant::VT_BYREF(), $sResponseStream);
		return if DBD::ADO::Failed($dbh, "Can't create Variant for 'ADODB.Stream'");
		$comm->{Properties}{'Output Stream'}{Value} = $vObj;
		$sth->{ado_responsestream} = $sResponseStream;
	}
    if ( $Cnt ) {
      # Describe the Parameters:
      for my $p ( Win32::OLE::in( $comm->Parameters ) ) {
        my @p = map "$_ => $p->{$_}", qw(Name Type Direction Size);
        $dbh->trace_msg("    -- Parameter: @p\n", 5 );
      }
      $sth->STORE('NUM_OF_PARAMS', $Cnt );
    }
    $comm->{Prepared} = 1;
    return if DBD::ADO::Failed( $dbh,"Can't set Prepared");

    return $outer;
  }


	# Creates a Statement handle from a row set.
	sub _rs_sth_prepare {
		my ( $dbh, $rs, $sth1 ) = @_;

		$dbh->trace_msg("    -> _rs_sth_prepare: Create statement handle from RecordSet\n", 3 );

		my $conn = $dbh->{ado_conn};
		my @Fields = Win32::OLE::in( $rs->Fields );

		my ( $outer, $sth ) = $sth1
                        ? ( undef, $sth1 )
                        : DBI::_new_sth( $dbh, { Statement => $rs->Source } );

		$sth->{ado_comm}       = $conn;  # XXX
		$sth->{ado_conn}       = $conn;
		$sth->{ado_fields}     = \@Fields;
		$sth->{ado_max_errors} = $dbh->{ado_max_errors};
		$sth->{ado_refresh}    = 0;
		$sth->{ado_rownum}     = 0;
		$sth->{ado_rows}       = -1;
		$sth->{ado_rowset}     = $rs;
		$sth->{ado_type}       = [ map { $_->Type } @Fields ];

		$sth->{NAME}           = [ map { $_->Name } @Fields ];
		$sth->{TYPE}           = [ map { scalar DBD::ADO::TypeInfo::ado2dbi( $_->Type ) } @Fields ];
		$sth->{PRECISION}      = [ map { $_->Precision } @Fields ];
		$sth->{SCALE}          = [ map { $_->NumericScale } @Fields ];
		$sth->{NULLABLE}       = [ map { $_->Attributes & $Enums->{FieldAttributeEnum}{adFldMayBeNull} ? 1 : 0 } @Fields ];

		$sth->STORE('NUM_OF_FIELDS', scalar @Fields );
		$sth->STORE('Active', 1 );

		$dbh->trace_msg("    <- _rs_sth_prepare: Create statement handle from RecordSet\n", 3 );
		return $outer;
	}


	sub get_info {
		my ( $dbh, $info_type ) = @_;
		$info_type = int $info_type;
		require DBD::ADO::GetInfo;
		return $dbh->{ado_conn}->Properties->{$DBD::ADO::GetInfo::odbc2ado{$info_type}}{Value}
			if exists $DBD::ADO::GetInfo::odbc2ado{$info_type};
		my $v = $DBD::ADO::GetInfo::info{$info_type};
		if ( ref $v eq 'CODE') {
			my $get_info_cache = $dbh->{dbd_get_info_cache} ||= {};
			return $get_info_cache->{$info_type} if exists $get_info_cache->{$info_type};
			$v = $v->( $dbh );
			return $$v if ref $v eq 'SCALAR';  # don't cache!
			$get_info_cache->{$info_type} = $v;
		}
		return $v;
	}


	sub ado_schema_dbinfo_literal {
		my ( $dbh, $literal_name ) = @_;
		my $cache = $dbh->{ado_schema_dbinfo_literal_cache};
		unless ( defined $cache ) {
			$dbh->trace_msg("    -- ado_schema_dbinfo_literal: filling cache\n", 5 );
			$cache = $dbh->{ado_schema_dbinfo_literal_cache} = {};
			my $sth = $dbh->func('adSchemaDBInfoLiterals','OpenSchema');
			while ( my $row = $sth->fetch ) {
				$cache->{$row->[0]} = [ @$row ];
			}
		}
		my $row = $cache->{$literal_name};
		return $row->[1] unless wantarray;  # literal value
		return @$row;
	}


	sub table_info {
		my ( $dbh, $attr ) = @_;
		$attr = {
		  TABLE_CAT   => $_[1]
		, TABLE_SCHEM => $_[2]
		, TABLE_NAME  => $_[3]
		, TABLE_TYPE  => $_[4]
		, ref $_[5] eq 'HASH' ? %{$_[5]} : ()
		} unless ref $attr eq 'HASH';
		my @Rows;
		my $conn = $dbh->{ado_conn};

    $attr->{ado_columns}      = $attr->{ADO_Columns}  if exists $attr->{ADO_Columns}  && !exists $attr->{ado_columns};
    $attr->{ado_filter}       = $attr->{Filter}       if exists $attr->{Filter}       && !exists $attr->{ado_filter};
    $attr->{ado_trim_catalog} = $attr->{Trim_Catalog} if exists $attr->{Trim_Catalog} && !exists $attr->{ado_trim_catalog};

		my $field_names = $attr->{ado_columns}
			?  $ado_schematables : $ado_dbi_schematables;
		my $rs;

		#
		# If the value of $catalog is '%' and $schema and $table name are empty
		# strings, the result set contains a list of catalog names.
		#
		if ( (defined $attr->{TABLE_CAT}   && $attr->{TABLE_CAT}   eq '%')
			&& (defined $attr->{TABLE_SCHEM} && $attr->{TABLE_SCHEM} eq '' )
			&& (defined $attr->{TABLE_NAME}  && $attr->{TABLE_NAME}  eq '' ) ) { # Rule 19a
			# This is the easy way to determine catalog support.
			eval {
				local $Win32::OLE::Warn = 0;
				$rs = $conn->OpenSchema( $Enums->{SchemaEnum}{adSchemaCatalogs} );
				my $lastError = DBD::ADO::errors( $dbh );
				$lastError = undef if $lastError =~ m/0x80020007/;
				die $lastError if $lastError;
			};
			$dbh->trace_msg("    !! Eval of adSchemaCatalogs died: $@\n", 5 ) if $@;
			$dbh->trace_msg("    -- Rule 19a\n", 5 );
			if ( $rs ) {
				$dbh->trace_msg("    -- Rule 19a, record set defined\n", 5 );
				while ( !$rs->{EOF} ) {
					push @Rows, [ $rs->Fields(0)->{Value}, undef, undef, undef, undef ];
					$rs->MoveNext;
				}
			}
			else {
				# The provider does not support the adSchemaCatalogs.  Let's attempt
				# to still return a list of catalogs.
				$dbh->trace_msg("    -- Rule 19a, record set undefined\n", 5 );
				my $sth = $dbh->table_info( { ado_trim_catalog => 1 } );
				if ( $sth ) {
          my $ref = {};
          my $Undef = 0;  # for 'undef' hash keys (which mutate to '')
          while ( my $Row = $sth->fetch ) {
            defined $Row->[0] ? $ref->{$Row->[0]} = 1 : $Undef = 1;
          }
          push @Rows, [ undef, undef, undef, undef, undef ] if $Undef;
          push @Rows, [    $_, undef, undef, undef, undef ] for sort keys %$ref;
				}
				else {
					push @Rows, [ undef, undef, undef, undef, undef ];
				}
			}
		}
		#
		# If the value of $schema is '%' and $catalog and $table are empty
		# strings, the result set contains a list of schema names.
		#
		elsif ( (defined $attr->{TABLE_CAT}   && $attr->{TABLE_CAT}   eq '' )
				 && (defined $attr->{TABLE_SCHEM} && $attr->{TABLE_SCHEM} eq '%')
				 && (defined $attr->{TABLE_NAME}  && $attr->{TABLE_NAME}  eq '' ) ) { # Rule 19b
			eval {
				local $Win32::OLE::Warn = 0;
				$rs = $conn->OpenSchema( $Enums->{SchemaEnum}{adSchemaSchemata} );
				my $lastError = DBD::ADO::errors( $dbh );
				$lastError = undef if $lastError =~ m/0x80020007/;
				die $lastError if $lastError;
			};
			$dbh->trace_msg("    !! Eval of adSchemaSchemata died: $@\n", 5 ) if $@;
			$dbh->trace_msg("    -- Rule 19b\n", 5 );
			if ( $rs ) {
				$dbh->trace_msg("    -- Rule 19b, record set defined\n", 5 );
				while ( !$rs->{EOF} ) {
					push @Rows, [ $rs->Fields(0)->{Value}, $rs->Fields(1)->{Value}, undef, undef, undef ];
					$rs->MoveNext;
				}
			}
			else {
				# The provider does not support the adSchemaSchemata.  Let's attempt
				# to still return a list of schemas.
				$dbh->trace_msg("    -- Rule 19b, record set undefined\n", 5 );
				my $sth = $dbh->table_info( { ado_trim_catalog => 1 } );
				if ( $sth ) {
          my $ref = {};
          my $Undef = 0;  # for 'undef' hash keys (which mutate to '')
          while ( my $Row = $sth->fetch ) {
            defined $Row->[0] ? $ref->{$Row->[0]} = 1 : $Undef = 1;
          }
          push @Rows, [ undef, undef, undef, undef, undef ] if $Undef;
          push @Rows, [ undef,    $_, undef, undef, undef ] for sort keys %$ref;
				}
				else {
					push @Rows, [ undef, undef, undef, undef, undef ];
				}
			}
		}
		#
		# If the value of $type is '%' and $catalog, $schema, and $table are all
		# empty strings, the result set contains a list of table types.
		#
		elsif ( (defined $attr->{TABLE_CAT}   && $attr->{TABLE_CAT}   eq '' )
				 && (defined $attr->{TABLE_SCHEM} && $attr->{TABLE_SCHEM} eq '' )
				 && (defined $attr->{TABLE_NAME}  && $attr->{TABLE_NAME}  eq '' )
				 && (defined $attr->{TABLE_TYPE}  && $attr->{TABLE_TYPE}  eq '%')
				 ) { # Rule 19c
			$dbh->trace_msg("    -- Rule 19c\n", 5 );
			my @TableTypes = ('ALIAS','TABLE','SYNONYM','SYSTEM TABLE','VIEW','GLOBAL TEMPORARY','LOCAL TEMPORARY','SYSTEM VIEW'); # XXX
			for ( sort @TableTypes ) {
				push @Rows, [ undef, undef, undef, $_, undef ];
			}
		}
		else {
			my @Criteria;
			for ( my $i = 0; $i < @$ado_dbi_schematables; $i++ ) {
				my $field = $ado_dbi_schematables->[$i];
				$Criteria[$i] = $attr->{$field} if exists $attr->{$field};
			}

			eval {
				local $Win32::OLE::Warn = 0;
				$rs = $conn->OpenSchema( $Enums->{SchemaEnum}{adSchemaTables}, @Criteria ? \@Criteria : undef );
				my $lastError = DBD::ADO::errors( $dbh );
				$lastError = undef if $lastError =~ m/0x80020007/;
				die $lastError if $lastError;
			};
			$dbh->trace_msg("    !! Eval of adSchemaTables died: $@\n", 5 ) if $@;

			if ( $rs ) {
				$rs->{Filter} = $attr->{ado_filter} if exists $attr->{ado_filter};

				while ( !$rs->{EOF} ) {
					my @row = map { $rs->Fields( $_ )->{Value} }
						map { $sch_dbi_to_ado->{$_} } @$field_names;
					# Jan Dubois jand@activestate.com addition to handle changes
					# in Win32::OLE return of Variant types of data.
					for ( @row ) {
						$_ = $_->As( Win32::OLE::Variant::VT_BSTR() )
							if defined $_ && UNIVERSAL::isa( $_,'Win32::OLE::Variant');
					}
					if ( $attr->{ado_trim_catalog} ) {
						$row[0] =~ s/^(.*\\)// if defined $row[0];  # removes leading
						$row[0] =~ s/(\..*)$// if defined $row[0];  # removes file extension
					}
					push @Rows, \@row;
					$rs->MoveNext;
				}
			}
			else {
				push @Rows, [ undef, undef, undef, undef, undef ];
			}
		}

		$rs->Close if $rs;
		$rs = undef;

		DBI->connect('dbi:Sponge:','','', { RaiseError => 1 } )->prepare(
			'adSchemaTables', { rows => \@Rows
			, NAME => $field_names
		} );
	}


	sub column_info {
		my ( $dbh, @Criteria ) = @_;
		my $QueryType = 'adSchemaColumns';
		my @Rows;
		my $conn = $dbh->{ado_conn};
		my $tmpCursorLocation = $conn->{CursorLocation};
		$conn->{CursorLocation} = $Enums->{CursorLocationEnum}{adUseClient};

		my $rs = $conn->OpenSchema( $Enums->{SchemaEnum}{$QueryType}, @Criteria ? \@Criteria : undef );
		return if DBD::ADO::Failed( $dbh,"Can't OpenSchema ($QueryType)");

		$rs->{Sort} = 'TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, ORDINAL_POSITION';
		return if DBD::ADO::Failed( $dbh,"Can't set Sort");

		while ( !$rs->{EOF} ) {
			my $AdoType    = $rs->{DATA_TYPE   }{Value};
			my $ColFlags   = $rs->{COLUMN_FLAGS}{Value};
			my $IsLong     = $ColFlags & $Enums->{FieldAttributeEnum}{adFldLong } ? 1 : 0;
			my $IsFixed    = $ColFlags & $Enums->{FieldAttributeEnum}{adFldFixed} ? 1 : 0;
			my @SqlType    = DBD::ADO::TypeInfo::ado2dbi( $AdoType, $IsFixed, $IsLong );
			my $IsNullable = $rs->{IS_NULLABLE}{Value} ? 'YES' : 'NO';
			my $ColSize    = $rs->{NUMERIC_PRECISION       }{Value}
			              || $rs->{CHARACTER_MAXIMUM_LENGTH}{Value}
			              || 0;  # Default value to stop warnings ???
			my $TypeName;
			my $ado_tis    = DBD::ADO::TypeInfo::Find3( $dbh, $AdoType, $IsFixed, $IsLong );
			$dbh->trace_msg('    ** ' . $rs->{COLUMN_NAME}{Value} . "($ColSize): $AdoType, $IsFixed, $IsLong\n", 6 );
			# find the first type which has a large enough COLUMN_SIZE:
			for my $ti ( sort { $a->{COLUMN_SIZE} <=> $b->{COLUMN_SIZE} } @$ado_tis ) {
				$dbh->trace_msg("      ** => $ti->{TYPE_NAME}($ti->{COLUMN_SIZE})\n", 7 );
				if ( $ti->{COLUMN_SIZE} >= $ColSize ) {
					$TypeName = $ti->{TYPE_NAME};
					last;
				}
			}
			# unless $TypeName: Standard SQL type name???

			my $Fields =
			[
			  $rs->{TABLE_CATALOG         }{Value} #  0 TABLE_CAT
			, $rs->{TABLE_SCHEMA          }{Value} #  1 TABLE_SCHEM
			, $rs->{TABLE_NAME            }{Value} #  2 TABLE_NAME
			, $rs->{COLUMN_NAME           }{Value} #  3 COLUMN_NAME
			, $SqlType[0]                          #  4 DATA_TYPE !!!
			, $TypeName                            #  5 TYPE_NAME !!!
			, $ColSize                             #  6 COLUMN_SIZE !!! MAX for *LONG*
			, $rs->{CHARACTER_OCTET_LENGTH}{Value} #  7 BUFFER_LENGTH !!! MAX for *LONG*, ... (e.g. num)
			, $rs->{NUMERIC_SCALE         }{Value} #  8 DECIMAL_DIGITS ???
			, undef                                #  9 NUM_PREC_RADIX !!!
			, $rs->{IS_NULLABLE           }{Value} # 10 NULLABLE !!!
			, $rs->{DESCRIPTION           }{Value} # 11 REMARKS
			, $rs->{COLUMN_DEFAULT        }{Value} # 12 COLUMN_DEF
			, $SqlType[1]                          # 13 SQL_DATA_TYPE !!!
			, $SqlType[2]                          # 14 SQL_DATETIME_SUB !!!
			, $rs->{CHARACTER_OCTET_LENGTH}{Value} # 15 CHAR_OCTET_LENGTH !!! MAX for *LONG*
			, $rs->{ORDINAL_POSITION      }{Value} # 16 ORDINAL_POSITION
			, $IsNullable                          # 17 IS_NULLABLE !!!
			];
			push @Rows, $Fields;
			$rs->MoveNext;
		}

		$rs->Close; undef $rs;
		$conn->{CursorLocation} = $tmpCursorLocation;

		DBI->connect('dbi:Sponge:','','', { RaiseError => 1 } )->prepare(
			$QueryType, { rows => \@Rows
			, NAME => [ qw( TABLE_CAT TABLE_SCHEM TABLE_NAME COLUMN_NAME DATA_TYPE TYPE_NAME COLUMN_SIZE BUFFER_LENGTH DECIMAL_DIGITS NUM_PREC_RADIX NULLABLE REMARKS COLUMN_DEF SQL_DATA_TYPE SQL_DATETIME_SUB CHAR_OCTET_LENGTH ORDINAL_POSITION IS_NULLABLE ) ]
			, TYPE => [            12,         12,        12,         12,        5,       12,          4,            4,             5,             5,       5,     12,        12,            5,               5,                4,               4,         12   ]
		} );
	}


	sub primary_key_info {
		my ( $dbh, @Criteria ) = @_;
		my $QueryType = 'adSchemaPrimaryKeys';
		my @Rows;
		my $conn = $dbh->{ado_conn};
		my $tmpCursorLocation = $conn->{CursorLocation};
		$conn->{CursorLocation} = $Enums->{CursorLocationEnum}{adUseClient};

		my $rs = $conn->OpenSchema( $Enums->{SchemaEnum}{$QueryType}, @Criteria ? \@Criteria : undef );
		return if DBD::ADO::Failed( $dbh,"Can't OpenSchema ($QueryType)");

		$rs->{Sort} = 'TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, ORDINAL';
		return if DBD::ADO::Failed( $dbh,"Can't set Sort");

		while ( !$rs->{EOF} ) {
			my @Fields = (map { $_->{Value} } Win32::OLE::in( $rs->Fields ) ) [ 0,1,2,3,6,7 ];
			push @Rows, \@Fields;
			$rs->MoveNext;
		}

		$rs->Close; undef $rs;
		$conn->{CursorLocation} = $tmpCursorLocation;

		DBI->connect('dbi:Sponge:','','', { RaiseError => 1 } )->prepare(
			$QueryType, { rows => \@Rows
			, NAME => [ qw( TABLE_CAT TABLE_SCHEM TABLE_NAME COLUMN_NAME KEY_SEQ PK_NAME ) ]
			, TYPE => [            12,         12,        12,         12,      5,     12   ]
		} );
	}


	sub foreign_key_info {
		my ( $dbh, @Criteria ) = @_;
		my $QueryType = 'adSchemaForeignKeys';
		my $RefActions = {
		 'CASCADE'     => 0
		,'RESTRICT'    => 1
		,'SET NULL'    => 2
		,'NO ACTION'   => 3
		,'SET DEFAULT' => 4
		};
		my @Rows;
		my $conn = $dbh->{ado_conn};
		my $tmpCursorLocation = $conn->{CursorLocation};
		$conn->{CursorLocation} = $Enums->{CursorLocationEnum}{adUseClient};

		my $rs = $conn->OpenSchema( $Enums->{SchemaEnum}{$QueryType}, @Criteria ? \@Criteria : undef );
		return if DBD::ADO::Failed( $dbh,"Can't OpenSchema ($QueryType)");

		$rs->{Sort} = 'PK_TABLE_CATALOG, PK_TABLE_SCHEMA, PK_TABLE_NAME, FK_TABLE_CATALOG, FK_TABLE_SCHEMA, FK_TABLE_NAME';
		return if DBD::ADO::Failed( $dbh,"Can't set Sort");

		while ( !$rs->{EOF} ) {
			my @Fields = (map { $_->{Value} } Win32::OLE::in( $rs->Fields ) ) [ 0..3,6..9,12..14,16,15,17 ];
			$Fields[ 9]  = $RefActions->{$Fields[ 9]};
			$Fields[10]  = $RefActions->{$Fields[10]};
			$Fields[13] += 4 if $Fields[13];
			push @Rows, \@Fields;
			$rs->MoveNext;
		}

		$rs->Close; undef $rs;
		$conn->{CursorLocation} = $tmpCursorLocation;

		DBI->connect('dbi:Sponge:','','', { RaiseError => 1 } )->prepare(
			$QueryType, { rows => \@Rows
			, NAME => [ qw( PKTABLE_CAT PKTABLE_SCHEM PKTABLE_NAME PKCOLUMN_NAME FKTABLE_CAT FKTABLE_SCHEM FKTABLE_NAME FKCOLUMN_NAME KEY_SEQ UPDATE_RULE DELETE_RULE FK_NAME PK_NAME DEFERRABILITY ) ]
			, TYPE => [              12,           12,          12,           12,         12,           12,          12,           12,      5,          5,          5,     12,     12,            5   ]
		} );
	}


	sub statistics_info {
		my ( $dbh, $catalog, $schema, $table, $unique_only, $quick ) = @_;
		my $QueryType = 'adSchemaIndexes';
		my $IndexType = {
		  #    'table'
		  1 => 'btree'
		, 2 => 'hashed'
		, 3 => 'content'
		, 4 => 'other'
		  #    'clustered'
		};
		my $Collation = {
		  1 => 'A'
		, 2 => 'D'
		};
		my @Rows;
		my $conn = $dbh->{ado_conn};

		my $rs = $conn->OpenSchema( $Enums->{SchemaEnum}{$QueryType}, [ $catalog, $schema, undef, undef, $table ] );
		return if DBD::ADO::Failed( $dbh,"Can't OpenSchema ($QueryType)");

		while ( !$rs->{EOF} ) {
			my @Fields = (map { $_->{Value} } Win32::OLE::in( $rs->Fields ) ) [ 0..2,7,4..5,9,16,17,20..23,8 ];
			$Fields[ 3]  = $Fields[ 3] ? 0 : 1;
			$Fields[ 6]  = pop @Fields ? 'clustered' : defined $Fields[ 6] ? $IndexType->{$Fields[ 6]} : '';
			$Fields[ 9]  = $Collation->{$Fields[ 9]};
			$rs->MoveNext;
			next if $unique_only && $Fields[ 3];
			push @Rows, \@Fields;
		}
		$rs->Close;
		@Rows = sort {
		     $a->[3]       <=>   $b->[3]
		||   $a->[6]       cmp   $b->[6]
		|| ( $a->[4] ||'') cmp ( $b->[4] ||'')
		||   $a->[5]       cmp   $b->[5]
		||   $a->[7]       <=>   $b->[7]
		} @Rows;
		{
		my $QueryType = 'adSchemaStatistics';
		my $rs = $conn->OpenSchema( $Enums->{SchemaEnum}{$QueryType}, [ $catalog, $schema, $table ] );
		return if DBD::ADO::Failed( $dbh,"Can't OpenSchema ($QueryType)");

		while ( !$rs->{EOF} ) {
			my @Fields = ( undef ) x 13;
			@Fields[ 6, 0..2, 10] = ('table', map { $_->{Value} } Win32::OLE::in( $rs->Fields ) );
			unshift @Rows, \@Fields;
			$rs->MoveNext;
		}
		$rs->Close;
		}

		DBI->connect('dbi:Sponge:','','', { RaiseError => 1 } )->prepare(
			$QueryType, { rows => \@Rows
			, NAME => [ qw( TABLE_CAT TABLE_SCHEM TABLE_NAME NON_UNIQUE INDEX_QUALIFIER INDEX_NAME TYPE ORDINAL_POSITION COLUMN_NAME ASC_OR_DESC CARDINALITY PAGES FILTER_CONDITION ) ]
			, TYPE => [            12,         12,        12,         5,             12,        12,  12,               5,         12,          1,          4,    4,              12   ]
		} );
	}


  sub type_info_all {
    my ( $dbh ) = @_;
    return $dbh->{ado_ti_ver} == 2
    ? &DBD::ADO::TypeInfo::type_info_all_2
    : &DBD::ADO::TypeInfo::type_info_all_1;
  }


  sub ado_open_schema {
    my ( $dbh, $QueryType, @Criteria ) = @_;

    return $dbh->set_err( -910,"OpenSchema called with unknown parameter: $QueryType")
      unless exists $Enums->{SchemaEnum}{$QueryType};

    my $conn = $dbh->{ado_conn};
    my $rs   = $conn->OpenSchema( $Enums->{SchemaEnum}{$QueryType}, @Criteria ? \@Criteria : undef );
    return if DBD::ADO::Failed( $dbh,"Can't OpenSchema ($QueryType)");

    return _rs_sth_prepare( $dbh, $rs );
  }

  *OpenSchema = \&ado_open_schema;


  sub FETCH {
    my ( $dbh, $key ) = @_;

    if ( $key eq 'RowCacheSize') {
      return $dbh->{ado_cachesize};
    }
    elsif ( $key =~ /^ado_/) {
      return $dbh->{ado_conn}{CommandTimeout} if $key eq 'ado_commandtimeout';
      return $dbh->{$key} if exists $dbh->{$key};
      my $value;
      eval {
        $key =~ s/^ado_//;
        local $Win32::OLE::Warn = 0;
        my $conn = $dbh->{ado_conn};
        $value = $conn->{$key};
        my $lastError = DBD::ADO::errors( $dbh );
        $lastError = undef if $lastError =~ m/0x80020007/;
        die $lastError if $lastError;
      };
      return $value unless $@;
    }
    return $dbh->SUPER::FETCH( $key );
  }


	sub STORE {
		my ( $dbh, $key, $value ) = @_;

		if ( $key eq 'Warn') {
			$Win32::OLE::Warn = $value;
			return $dbh->SUPER::STORE( $key, $value );
		}
		elsif ( $key eq 'RowCacheSize') {
			return $dbh->{ado_cachesize} = $value;
		}
		elsif ( $key eq 'AutoCommit') {
			if ( $dbh->{ado_txn_capable} ) {
				return $dbh->{AutoCommit} = _auto_commit( $dbh, $value );
			}
			else {
				return $value if $value;
				Carp::croak("Can't disable AutoCommit: Provider does not support transactions.");
			}
		}
    elsif ( $key eq 'ado_commandtimeout') {
      $dbh->{ado_conn}{CommandTimeout} = $value;
      return if DBD::ADO::Failed( $dbh,"Can't set $key: $value");
      return 1;
    }
		elsif ( $key =~ /^ado_/) {
			return $dbh->{$key} = $value;
		}
		elsif ( $key !~ /PrintError|RaiseError/) {
			eval {
				local $Win32::OLE::Warn = 0;
				my $conn = $dbh->{ado_conn};
				$conn->{$key} = $value;
				my $lastError = DBD::ADO::errors( $dbh );
				die $lastError if $lastError;
			};
			Carp::carp $@ if $@ && $dbh->FETCH('Warn');
			return $value unless $@;
		}
		return $dbh->SUPER::STORE( $key, $value );
	}


  sub _auto_commit {
    my ( $dbh, $value ) = @_;

    my $cv = $dbh->FETCH('AutoCommit') || 0;

    if ( !$cv && $value ) {  # Current off, turn on
      my $conn = $dbh->{ado_conn};
      $conn->{Attributes} = 0;
      return if DBD::ADO::Failed( $dbh,"Can't set CommitRetaining");
      $dbh->commit;
      return 1;
    }
    elsif ( $cv && !$value ) {
      my $conn = $dbh->{ado_conn};
      $conn->{Attributes} = $Enums->{XactAttributeEnum}{adXactCommitRetaining}
                          | $Enums->{XactAttributeEnum}{adXactAbortRetaining};
      return if DBD::ADO::Failed( $dbh,"Can't set CommitRetaining");
      $conn->BeginTrans;
      return if DBD::ADO::Failed( $dbh,"Can't Begin transaction");
      return 0;
    }
    return $cv;  # Didn't change the value.
  }


  sub do {
    my $dbh = shift;
    my $sql = shift;

    return $dbh->SUPER::do( $sql, @_ ) if @_;

    my $Rows = Win32::OLE::Variant->new( $DBD::ADO::Const::VT_I4_BYREF, 0 );
    $dbh->{ado_conn}->Execute( $sql, $Rows, 129 );  # adCmdText | adExecuteNoRecords
    return if DBD::ADO::Failed( $dbh,"Can't Execute '$sql'");
    return $Rows->Value || '0E0';
  }


  sub DESTROY {
    my ( $dbh ) = @_;

    my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
    local $SIG{__WARN__} = sub {
      $warn_handler->(@_) unless $_[0] =~ /Not a Win32::OLE object/
    };

    $dbh->disconnect if $dbh->FETCH('Active');
    return;
  }

} # ====== DATABASE ======

{ package DBD::ADO::st; # ====== STATEMENT ======

  use strict;
  use Win32::OLE();
  use Win32::OLE::Variant();
  use DBD::ADO::TypeInfo();
  use DBD::ADO::Const();

  $DBD::ADO::st::imp_data_size = 0;

  my $Enums = DBD::ADO::Const->Enums;


  sub blob_read {
    my ( $sth, $n, $offset, $size, $attr ) = @_;
    my $Field = $sth->{ado_fields}[$n];
    my $Chunk;
    if ( $Field->Attributes & $Enums->{FieldAttributeEnum}{adFldLong} ) {
      $Chunk = $Field->GetChunk( $size );
    }
    else {
      $Chunk = substr $Field->Value, $offset, $size;
    }
    return defined $Chunk ? $Chunk : '';
  }


  sub _params  # Determine the number of parameters, if Refresh fails.
  {
    my $sql = shift;
    use Text::ParseWords;
    local $^W = 0;
    $sql =~ s/\n/ /;
    my $rtn = join(' ', grep { m/\?/ }
      grep { ! m/^['"].*\?/ } &quotewords('\s+', 1, $sql ) );
    my $cnt = ( $rtn =~ tr /?//) || 0;
    return $cnt;
  }


  sub _refresh {
    my ( $sth ) = @_;
    $sth->trace_msg("    -> _refresh\n", 3 );
    my $conn = $sth->{ado_conn};
    my $comm = $sth->{ado_comm};

    my $Cnt = _params( $sth->FETCH('Statement') );

    for ( 0 .. $Cnt - 1 ) {
      my $Parameter = $comm->CreateParameter("$_"
      , $Enums->{DataTypeEnum}{adVarChar}
      , $Enums->{ParameterDirectionEnum}{adParamInput}
      , 1
      ,'');
      return if DBD::ADO::Failed( $sth,"Can't CreateParameter");

      $comm->Parameters->Append( $Parameter );
      return if DBD::ADO::Failed( $sth,"Can't Append Parameter");
    }
    $sth->STORE('NUM_OF_PARAMS', $Cnt );
    $sth->trace_msg("    <- _refresh\n", 3 );
    return $Cnt;
  }


  sub bind_param {
    # my ( $sth, $n, $value, $attr ) = @_;
    # return _bind_param( $sth, $n, $value, $attr, FALSE, 0 )
    return _bind_param( @_[0..3], 0, 0 );
  }

  sub bind_param_inout {
    # my ( $sth, $n, $vref, $maxlen, $attr ) = @_;
    # return _bind_param( $sth, $n, $vref, $attr, TRUE, $maxlen )
    return _bind_param( @_[0..2, 4], 1, $_[3] );
  }

  sub _bind_param {
    my ( $sth, $n, $value, $attr, $is_bind_by_ref, $maxlen ) = @_;

    my $conn = $sth->{ado_conn};
    my $comm = $sth->{ado_comm};
    my $is_stored_procedure = $comm->{CommandType} == $Enums->{CommandTypeEnum}{adCmdStoredProc};

    $attr = {} unless defined $attr;
    $attr = { TYPE => $attr } unless ref $attr;

    my $param_cnt = $sth->FETCH('NUM_OF_PARAMS') || _refresh( $sth );
    --$param_cnt if $is_stored_procedure;

    return $sth->set_err( -915,"Bind Parameter $n outside current range of $param_cnt.") if $n > $param_cnt || $n < 1;

    if ( $is_bind_by_ref && defined $value ) {
      return $sth->set_err( -930,"Bind target for OUT parameter $n must be a scalar reference.")
        unless ref $value eq 'SCALAR';
      if ( $sth->{TraceLevel} >= 5 ) {
        $sth->trace_msg("    -- discard old binding for $n", 5 )
          if exists $sth->{ado_ParamRefs}{$n};
        $sth->trace_msg("    -- bind param $n by reference to '$$value'; maxlen=$maxlen; attr={"
            . join(", ", map "$_ => $attr->{$_}", keys %$attr ) . "}\n", 5 );
      }
      $sth->{ado_ParamRefs}{$n} = $value;
      $sth->{ado_ParamRefAttrs}{$n} = $attr;
    }
    else {  # delete even if not ref; might need to clobber an old ref.
      $sth->trace_msg("    -- discard old binding for $n", 5 )
        if exists $sth->{ado_ParamRefs}{$n};
      delete $sth->{ado_ParamRefs}{$n};
      delete $sth->{ado_ParamRefAttrs}{$n};
    }

    # support adCmdStoredProc command format, where param 0 is @RETURN_VALUE
    my $i = $comm->Parameters->Item( $n - ( $is_stored_procedure ? 0 : 1 ) );

    if ( exists $attr->{ado_type} ) {
      $i->{Type} = $attr->{ado_type};
    }
    elsif ( exists $attr->{TYPE} ) {
      $i->{Type} = $DBD::ADO::TypeInfo::dbi2ado->{$attr->{TYPE}};
    }

    if ( $is_bind_by_ref ) {
      $attr->{ado_maxlen} = $maxlen;
    }
    else {
      # factored-out to support delayed binding
      _assign_param( $sth, $n, $value, $attr, $i );
    }
    return 1;
  }

  sub _assign_param {
    my ( $sth, $n, $value, $attr, $i ) = @_;

    $i = $sth->{ado_comm}->Parameters->Item( int( $i ) -
      ( $sth->{ado_comm}{CommandType} == $Enums->{CommandTypeEnum}{adCmdStoredProc} ? 0 : 1) )
      unless defined $i;
    $attr = {} unless defined $attr;

    $sth->{ParamValues}{$n} = $value;

    if ( defined $value ) {
      if ( defined $attr->{ado_size} ) {
        $i->{Size} = $attr->{ado_size};
      }
      elsif ( defined $attr->{ado_maxlen} && $attr->{ado_maxlen} > length $value ) {
        $i->{Size} = $attr->{ado_maxlen};
      }
      else {
        $i->{Size} = length $value || 1;
      }
      if ( $i->{Type} == $Enums->{DataTypeEnum}{adVarBinary}
        || $i->{Type} == $Enums->{DataTypeEnum}{adLongVarBinary}
         ) {
        my $pic = Win32::OLE::Variant->new( Win32::OLE::Variant::VT_UI1() | Win32::OLE::Variant::VT_ARRAY(), $i->{Size} );
        return $sth->set_err( -935, "Failed to create a Variant array of size $i->{Size}.")
          unless defined $pic;
        $pic->Put( $value );
        $i->{Value} = $pic;
        $sth->trace_msg("    -- Binary: $i->{Type} $i->{Size}\n", 5 );
      }
      else {
        $i->{Value} = $value;
        $sth->trace_msg("    -- Type : $i->{Type} $i->{Size}\n", 5 );
      }
    }
    else {
      $i->{Value} = Win32::OLE::Variant->new( Win32::OLE::Variant::VT_NULL() );
    }
  }

  sub _retrieve_out_params {
    my ( $sth ) = @_;
    my $comm = $sth->{ado_comm};
    my $is_stored_procedure = $comm->{CommandType} == $Enums->{CommandTypeEnum}{adCmdStoredProc};
    while ( my ( $n, $vref ) = each %{$sth->{ado_ParamRefs}} ) {
      my $value = $comm->Parameters->Item( $n - ( $is_stored_procedure ? 0 : 1 ) )->{Value};
      # XXX perhaps should translate Variant null representation, here, first?
      $sth->{ParamValues}{$n} = $$vref = $value;
      $sth->trace_msg("    -- _retrieve_out_params : param => $n  value => '$value'\n", 5 );
    }
    if ($is_stored_procedure) {
      $sth->{ado_returnvalue} = $comm->Parameters->Item( 0 )->{Value};
      $sth->trace_msg("    -- _retrieve_out_params : param => RETURN_VALUE  value => '$sth->{ado_returnvalue}'\n", 5 );
    }
  }

	sub execute {
		my ( $sth, @bind_values ) = @_;
		my $conn = $sth->{ado_conn};
		my $comm = $sth->{ado_comm};
		my $sql  = $sth->FETCH('Statement');
		my $rows = Win32::OLE::Variant->new( $DBD::ADO::Const::VT_I4_BYREF, 0 );
		my $rs;

    $sth->finish if $sth->{Active};

    $sth->bind_param( $_, $bind_values[$_-1] ) or return for 1 .. @bind_values;

    ## delayed binding of by-ref input[/output] parameters
    unless (@bind_values) {
      while ( my ( $n, $vref ) = each %{$sth->{ado_ParamRefs}} ) {
        my $i = $comm->Parameters->Item( $n - ($comm->{CommandType} == $Enums->{CommandTypeEnum}{adCmdStoredProc} ? 0 : 1) );
        if ( $i->{Direction} & $Enums->{ParameterDirectionEnum}{adParamInput} ) {
          # probably don't need the ternary; creation of ado_maxlen should
          # guarantee that this will always exist
          my $attr = defined $sth->{ado_ParamRefAttrs}{$n} ? $sth->{ado_ParamRefAttrs}{$n} : undef;
          _assign_param( $sth, $n, $$vref, $attr, $i );
        }
      }
    }
		# At this point a Command is ready to Execute. To allow for different
		# type of cursors, we need to create a Recordset object.
		# However, a Recordset Open does not return affected rows. So we need to
		# determine if a Recordset Open is needed, or a Command Execute.
		my $UseRecordSet = !defined $sth->{ado_usecmd} &&
			(  defined $sth->{ado_cursortype}
			|| defined $sth->{ado_users}
			);
		my $UseResponseStream = $sth->{ado_executeoption} &&
			( $sth->{ado_executeoption} == $Enums->{ExecuteOptionEnum}{adExecuteStream} );

		if ( $UseResponseStream ) {
			$sth->trace_msg("    -- Execute: Using Response Stream\n", 5 );
			$comm->Execute( { 'Options' => $sth->{ado_executeoption} } );
			return if DBD::ADO::Failed( $sth,"Can't Execute Command '$sql'");
      _retrieve_out_params( $sth );
			return $sth->{ado_responsestream}->ReadText();
		}
		elsif ( $UseRecordSet ) {
			$rs = Win32::OLE->new('ADODB.RecordSet');
			return if DBD::ADO::Failed( $sth,"Can't create 'ADODB.RecordSet'");

			my $CursorType = $sth->{ado_cursortype} || 'adOpenForwardOnly';
			$sth->trace_msg("    -- Open Recordset using CursorType '$CursorType'\n", 5 );
			$rs->Open( $comm, undef, $Enums->{CursorTypeEnum}{$CursorType} );
			return if DBD::ADO::Failed( $sth,"Can't Open Recordset for '$sql'");
      _retrieve_out_params( $sth );
			$sth->trace_msg("    -- CursorType: $rs->{CursorType}\n", 5 );
		}
		else {
			$rs = $comm->Execute( $rows );
			return if DBD::ADO::Failed( $sth,"Can't Execute Command '$sql'");
      _retrieve_out_params( $sth );
		}
    $rows = $rows->Value;  # to make a DBD::Proxy client w/o Win32::OLE happy
    my @Fields;
    # some providers close the rs, e.g. after DROP TABLE
    if ( defined $rs && $rs->State ) {
		  @Fields = Win32::OLE::in( $rs->Fields );
		  return if DBD::ADO::Failed( $sth,"Can't enumerate Fields");
    }
    $sth->{ado_fields} = \@Fields;
		my $num_of_fields = @Fields;

		if ( $num_of_fields == 0 ) {  # assume non-select statement
			$sth->trace_msg("    -- no fields (non-select statement?)\n", 5 );
			# Clean up the record set that isn't used.
			if ( defined $rs && (ref $rs) =~ /Win32::OLE/) {
				$rs->Close if $rs && $rs->State & $Enums->{ObjectStateEnum}{adStateOpen};
			}
			$rs = undef;
			$sth->{ado_rows} = $rows;
			return $rows || '0E0';
		}

    if ( defined $sth->{ado_cachesize} && $sth->{ado_cachesize} > 0 ) {
      $sth->trace_msg("    -- changing CacheSize $rs->{CacheSize} => $sth->{ado_cachesize}\n", 5 );
      $rs->{CacheSize} = $sth->{ado_cachesize};
      my $lastError = DBD::ADO::errors( $sth );
      $sth->set_err( 0, $lastError ) if $lastError;
    }

    my $Attributes;
       $Attributes     |= $_->Attributes for @Fields;
		$sth->{ado_has_lob} = $Attributes & $Enums->{FieldAttributeEnum}{adFldLong} ? 1 : 0;
		$sth->{ado_rowset}  = $rs;
		$sth->{ado_rownum}  = 0;
		$sth->{ado_rows}    = $rows;  # $rs->RecordCount
		$sth->{ado_type}    = [ map { $_->Type } @Fields ];

		$sth->{NAME}        = [ map { $_->Name } @Fields ];
		$sth->{TYPE}        = [ map { scalar DBD::ADO::TypeInfo::ado2dbi( $_->Type ) } @Fields ];
		$sth->{PRECISION}   = [ map { $_->Precision } @Fields ];
		$sth->{SCALE}       = [ map { $_->NumericScale } @Fields ];
		$sth->{NULLABLE}    = [ map { $_->Attributes & $Enums->{FieldAttributeEnum}{adFldMayBeNull} ? 1 : 0 } @Fields ];

		$sth->STORE('Statement'    , $rs->Source );
		$sth->STORE('NUM_OF_FIELDS', $num_of_fields );
		$sth->STORE('Active'       , 1 );

		# We need to return a true value for a successful select
		# -1 means total row count unavailable
		return $rows || '0E0';  # seems more reliable than $rs->RecordCount
  }


  sub more_results {
    my ( $sth ) = @_;

    my $rs = $sth->{ado_rowset}->NextRecordset;
    return if DBD::ADO::Failed( $sth,"Can't NextRecordset");

    return undef unless $rs;

    delete $sth->{NUM_OF_FIELDS};
    DBD::ADO::db::_rs_sth_prepare( $sth, $rs, $sth );

    return 1;
  }


  sub rows {
    my ( $sth ) = @_;

    return unless defined $sth;
    my $rows = $sth->{ado_rows};
    return defined $rows ? $rows : -1;
  }


  sub fetch {
    my ( $sth ) = @_;
    my $rs = $sth->{ado_rowset};

    return $sth->set_err( -900,'Statement handle not marked as Active.') unless $sth->FETCH('Active');
    return $sth->set_err( -905,'Recordset undefined, execute statement not called?') unless $rs;

    if ( $sth->{ado_rownum}++ > 0 ) {
      $rs->MoveNext;
      return if DBD::ADO::Failed( $sth,"Can't MoveNext");
    }
    $sth->STORE('Active', 0 ), return if $rs->{EOF};

    my @row;
    if ( $sth->{ado_has_lob} && $sth->FETCH('LongReadLen') < 2147483647 ) {
      my $LongReadLen = $sth->FETCH('LongReadLen');
      my $LongTruncOk = $sth->FETCH('LongTruncOk');
      for ( Win32::OLE::in( $rs->Fields ) ) {
        if ( $_->Attributes & $Enums->{FieldAttributeEnum}{adFldLong} ) {
          if ( $LongReadLen == 0 ) {
            push @row, undef;
          }
          else {
            my $ActualSize = $_->{ActualSize};
            return if DBD::ADO::Failed( $sth,"Can't get ActualSize");
            $sth->trace_msg("    -- ActualSize: $ActualSize, LongReadLen: $LongReadLen\n", 7 );
            return $sth->set_err( -920,"LONG value truncated: $ActualSize > $LongReadLen")
              if !$LongTruncOk && $ActualSize > $LongReadLen;
            push @row, $_->GetChunk( $LongReadLen );
            return if DBD::ADO::Failed( $sth,"Can't GetChunk");
          }
        }
        else {
          push @row, $_->Value;
        }
      }
    }
    else {
      @row = map { $_->Value } Win32::OLE::in( $rs->Fields );
    }
    # Jan Dubois jand@activestate.com addition to handle changes
    # in Win32::OLE return of Variant types of data.
    for ( @row ) {
      $_ = $_->As( Win32::OLE::Variant::VT_BSTR() )
        if UNIVERSAL::isa( $_,'Win32::OLE::Variant');
    }
    map { s/\s+$// } @row if $sth->FETCH('ChopBlanks');

    $sth->{ado_rows} = $sth->{ado_rownum};
    return $sth->_set_fbav( \@row );
  }

  *fetchrow_arrayref = \&fetch;


  sub finish {
    my ( $sth ) = @_;

    my $rs = $sth->{ado_rowset};
    $rs->Close if $rs && $rs->State;
    $sth->{ado_rowset} = undef;

    $sth->SUPER::finish;
    return 1;
  }


  sub FETCH {
    my ( $sth, $key ) = @_;

    return $sth->{ado_comm}{CommandTimeout} if $key eq 'ado_commandtimeout';
    return $sth->{$key} if exists $sth->{$key};
    return $sth->SUPER::FETCH( $key );
  }


  sub STORE {
    my ( $sth, $key, $value ) = @_;

    if ( $key eq 'ado_commandtimeout') {
      $sth->{ado_comm}{CommandTimeout} = $value;
      return if DBD::ADO::Failed( $sth,"Can't set $key: $value");
      return 1;
    }
    return $sth->{$key} = $value if exists $sth->{$key};
    return $sth->SUPER::STORE( $key, $value );
  }


  sub DESTROY {
    my ( $sth ) = @_;

    # not $sth->finish to avoid '!! ERROR: ... CLEARED by call to finish method'
    #   e.g. in $dbh->do
    finish( $sth );

    return;
  }

}

1;

=head1 NAME

DBD::ADO - A DBI driver for Microsoft ADO (Active Data Objects)


=head1 SYNOPSIS

  use DBI();

  my $dbh = DBI->connect("dbi:ADO:$dsn", $usr, $pwd, $att ) or die $DBI::errstr;


=head1 DESCRIPTION

The DBD::ADO module supports ADO access on a Win32 machine.
DBD::ADO is written to support the standard DBI interface to
data sources.


=head1 PREREQUISITES

It is recommended that you use recent versions of the following prerequisites:

=over

=item DBI

  http://search.cpan.org/~timb/DBI/

=item Win32::OLE

  http://search.cpan.org/~jdb/Win32-OLE/

=item ADO

  http://msdn.microsoft.com/data/

=back


=head1 DBI Methods

=head2 connect

Use the DBI connect method to establish a database connection:

  my $dbh = DBI->connect("dbi:ADO:$dsn", $usr, $pwd, $att ) or die $DBI::errstr;

where

  $dsn - is an ADO ConnectionString
  $usr - is a user name
  $pwd - is a password
  $att - is a hash reference with additional attributes

Typical connection attributes are

  RaiseError => 1
  PrintError => 0
  AutoCommit => 0

See the DBI module documentation for full details.

Driver specific connection attributes are:

  ado_ConnectionTimeout
  ado_Mode

These attributes initialize the corresponding ADO Connection properties
before opening the connection.

An ADO ConnectionString usually contains either a 'Provider' or a
'File Name' argument. If you omit these arguments, Provider defaults
to MSDASQL (Microsoft OLE DB Provider for ODBC). Therefore you can
pass an ODBC connection string (with DSN or DSN-less) as valid ADO
connection string.
If you use the OLE DB Provider for ODBC, it may be better to omit this
additional layer and use DBD::ODBC with the ODBC driver.

In addition the following attributes may be set in the connection string:

  Attributes
  CommandTimeout
  ConnectionString
  ConnectionTimeout
  CursorLocation
  DefaultDatabase
  IsolationLevel
  Mode

B<Warning:> This feature is supported for backward compatibility.
It's saver to use the driver specific attributes described above.
Anyway, the application is responsible for passing the correct
values when setting any of these attributes.

See the ADO documentation for more information on connection strings.

ADO ConnectionString examples:

  test
  File Name=test.udl
  Provider=Microsoft.Jet.OLEDB.4.0;Data Source=C:\data\test.mdb
  Provider=VFPOLEDB;Data Source=C:\data\test.dbc
  Provider=MSDAORA

For more examples, see e.g.:

  http://www.able-consulting.com/tech.htm


=head2 data_sources

Because ADO doesn't provide a data source repository, DBD::ADO uses it's
own. It tries to load Local::DBD::ADO::DSN and expects an array of hashes
describing the data sources. See ex/Local/DBD/ADO/DSN.pm for an example.

B<Warning:> This is experimental and may change.

B<Warning:> Check for the unlikly case that a file Local/DBD/ADO/DSN.pm
exists in your module search path which causes unwanted side effects when
loaded.


=head2 prepare

The B<prepare> methods allows attributes (see DBI):

  $sth = $dbh->prepare( $statement )          or die $dbh->errstr;
  $sth = $dbh->prepare( $statement, \%attr )  or die $dbh->errstr;

DBD::ADO's prepare() supports setting the CursorType, e.g.:

  $sth = $dbh->prepare( $sql, { ado_cursortype => 'adOpenForwardOnly' } ) ...
  # the CursorType attribute is deprecated:
  $sth = $dbh->prepare( $sql, { CursorType     => 'adOpenForwardOnly' } ) ...

Possible cursortypes are:

  adOpenForwardOnly (default)
  adOpenKeyset
  adOpenDynamic
  adOpenStatic

It may be necessary to prepare the statement using cursortype 'adOpenStatic'
when using a statement handle within a statement handle:

  while( my $table = $sth1->fetchrow_hashref ) {
    ...
    my $col = $sth2->fetchrow_hashref;
    ...
  }

Changing the CursorType is a solution to the following problem:

  Can't execute statement 'select * from authors':
  Lasterror : -2147467259
  OLE exception from "Microsoft OLE DB Provider for SQL Server":

  Cannot create new connection because in manual or distributed transaction
  mode.

  Win32::OLE(0.1403) error 0x80004005: "Unspecified error"
      in METHOD/PROPERTYGET "Open"

          Description : Cannot create new connection because in manual or distributed transaction mode.
          HelpContext : 0
          HelpFile    :
          NativeError : 0
          Number      : -2147467259
          Source      : Microsoft OLE DB Provider for SQL Server
          SQLState    :


=head2 bind_param

Normally, the datatypes of placeholders are known after the statement is
prepared. In this case, you don't need to provide any type information:

  $sth->bind_param( 1, $value );

Sometimes, you need to specify a type for the parameter, e.g.:

  $sth->bind_param( 1, $value, SQL_NUMERIC );

As a last resort, you can provide an ADO-specific type, e.g.:

  $sth->bind_param( 1, $value, { ado_type => 6 } );  # adCurrency

If no type is given (neither by the provider nor by you), the datatype
defaults to SQL_VARCHAR (adVarChar).


=head2 bind_param_inout

This can be utilized (with IN parameters) to support simple
call-by-reference, allowing for lazy parameter binding.

  $sth->bind_param_inout( 1, \$value, 1024 );

The contents of $value will not be dereferenced until the call to
C<$sth-E<gt>execute();> is made.  To use IN/OUT parameter types with stored
procedures, remember that you will need to specify the appropriate command
type when preparing the statement, e.g.:

  $sth = $dbh->prepare('sproc_name', { CommandType => 'adCmdStoredProc' } );

After execution, all call-by-reference parameters will be updated with the
parameter values reported by the ADO command object.

For stored procedures, the parameter at index C<0> is treated as the
return value of the procedure.  After execution, it is copied to the
C<$sth-E<gt>{ado_returnvalue}> attribute.


=head2 Type info

There exists two implementations of type_info_all(). Which version is
used depends on the ado_ti_ver database handle attribute:

=over

=item C<$dbh-E<gt>{ado_ti_ver} = 1>

The first implementations tries to find for various DBI types a set of
ADO types supported by the provider. The algorithm is highly sophisticated.
It tends to generate more duplicate type codes and names.

=item C<$dbh-E<gt>{ado_ti_ver} = 2> (default)

The second implementations is quite straightforward. It uses the set
which the provider returns and tries to map various ADO codes to
DBI/ODBC codes. The mapping is similar to the one used in column_info().
Duplicate type codes and names tend to occur less often.
The rows are ordered by DATA_TYPE, but not necessarily by 'how closely
each type maps to the corresponding ODBC SQL data type'. This second
sort criterion is difficult to achieve.

=back


=head2 table_info

  $sth = $dbh->table_info( $catalog, $schema, $table, $type, \%attr );
  $sth = $dbh->table_info( \%attr );  # deprecated

  $sth = $dbh->table_info( undef, undef, undef,'VIEW', {
    ado_columns      => 1,
    ado_trim_catalog => 0,
    ado_filter       => q{TABLE_NAME LIKE 'C%'},
  } );

Returns an active statement handle that can be used to fetch
information about tables and views that exist in the database.
By default the handle contains the columns described in the DBI documentation:

  TABLE_CAT, TABLE_SCHEM, TABLE_NAME, TABLE_TYPE, REMARKS

DBD::ADO allows selection criteria to be specified in the attributes hash
for backward compatibility.
In this way the record set can be restricted, for example, to only include
tables of type 'VIEW':

  %attr = ( TABLE_TYPE => 'VIEW')  # deprecated

You can add criteria for any of the following columns:

  TABLE_CAT, TABLE_SCHEM, TABLE_NAME, TABLE_TYPE

=over

=item B<ado_filter> (deprecated: Filter)

DBD::ADO also allows the recordset to be filtered on a criteria string:
a string made up of one or more individual clauses concatenated with AND
or OR operators.

  %attr = ( ado_filter => q{TABLE_TYPE LIKE 'SYSTEM%'} )

The criteria string is made up of clauses in the form FieldName-Operator-Value.
This is more flexible than using column criteria in that the filter allows a
number of operators:

  <, >, <=, >=, <>, =, or LIKE

The Fieldname must be one of the ADO 'TABLES Rowset' column names:

  TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, DESCRIPTION,
  TABLE_GUID, TABLE_PROPID, DATE_CREATED, DATE_MODIFIED

Value is the value with which you will compare the field values
(for example,'Smith', #8/24/95#, 12.345, or $50.00).
Use single quotes with strings and pound signs (#) with dates.
For numbers, you can use decimal points, dollar signs, and scientific notation.
If Operator is LIKE, Value can use wildcards.
Only the asterisk (*) and percent sign (%) wild cards are allowed,
and they must be the last character in the string. Value cannot be null.

=item B<ado_columns> (deprecated: ADO_Columns)

Additional ADO-only fields will be included if the C<ado_columns> attribute
is set to true:

  %attr = ( ado_columns => 1 );

=item B<ado_trim_catalog> (deprecated: Trim_Catalog)

Some ADO providers include path info in the TABLE_CAT column.
This information will be trimmed if the C<ado_trim_catalog> attribute is
set to true:

  %attr = ( ado_trim_catalog => 1 );

=back


=head2 tables

  @names = $dbh->tables(  $catalog, $schema, $table, $type, \%attr );

Returns a list of table and view names.
Accepts any of the attributes described in the L</table_info> method:

  @names = $dbh->tables( undef, undef, undef,'VIEW');


=head2 statistics_info

The row for TYPE 'table' is always the first row. Its PAGES field will be
NULL (C<undef>).

The C<$quick> argument is ignored.


=head1 ADO-specific methods

=head2 ado_open_schema

  $sth = $dbh->ado_open_schema( $QueryType, @Criteria ) or die ...;

This method can be used to obtain database schema information from the
provider.
It returns a valid statement handle upon success.

C<$QueryType> may be any valid ADO SchemaEnum name such as

  adSchemaTables
  adSchemaIndexes
  adSchemaProviderTypes

C<@Criteria> (optional) is a list of query constraints depending on each
C<$QueryType>.

Example:

  my $sth = $dbh->ado_open_schema('adSchemaCheckConstraints','Catalog1');

B<Note:> With DBI version 1.36 and earlier, the func() method has to be used
to call private methods implemented by the driver:

  $h->func( @func_arguments, $func_name ) or die ...;

where C<$func_name> is 'ado_open_schema'.
You can use 'OpenSchema' for backward compatibility.

Example:

  my $sth = $dbh->func('adSchemaCheckConstraints','Catalog1','OpenSchema');

See ex/OpenSchema.pl for a working example.


=head1 Error handling

An ADO provider may return a collection of more than one error. After
stringification , DBD::ADO concatenates these error messages to set the
errstr value of the handle.
However, the err value is set to the LastError known to Win32::OLE. Usually,
this is the native OLE DB error code. These codes contain the following
severity codes (see oledberr.h from the MDAC SDK):

  00 - Success
  01 - Informational
  10 - Warning
  11 - Error

The err value is set to 0 if all error codes belong to the Success or
Informational category, which doesn't trigger the normal DBI error
handling mechanisms.

The standard SQLSTATE is seldom supported by ADO providers and cannot be
relied on.

The db/st handle attribute 'ado_max_errors' limits the number of errors
extracted from the errors collection. To avoid time-consuming processing
of huge error collections, it defaults to 50.


=head1 ADO specific attributes


=head2 ado_commandtimeout (all handles, decimal, inherited)

This attribute indicates the time (in seconds) after which a command is
canceled.
Typically, cancellation may occur during a long running C<execute> method.
Not all ADO providers support this functionality.
Whereas ADO's Command object doesn't inherit the Connection's CommandTimeout
setting, DBD::ADO's statement handle is initialized with the ado_commandtimeout
attribute of its associated database handle.


=head1 LOB support

Since version 2.92, DBD::ADO supports the DBI handle attributes C<LongReadLen>
and C<LongTruncOk>.
C<LongReadLen> defaults to C<2147483647> (for backwards compatibility).

For multibyte strings, it's unspecified whether C<LongReadLen> means I<bytes>
or I<characters>. It's passed through to ADO's C<GetChunk> method and
C<ActualSize> property.

B<Caveat:> Unexpected truncation errors may occur if the ADO provider (e.g.
Microsoft.Jet.OLEDB.4.0) uses I<bytes> for C<ActualSize> but I<characters>
for C<GetChunk>.

B<Caveat:> The ADO provider may have problems if the long field isn't the last
one in the list of selected columns, e.g.:

  Description : Catastrophic failure
  HelpContext : 5000000
     HelpFile :
  NativeError :
       Number : -2147418113
       Source : Microsoft JET Database Engine
     SQLState :

The (experimental) C<blob_read> method ignores the C<$offset> argument for
long fields (ADO maintains a I<current offset> internally). To ensure that
C<blob_read> reads from the beginning, C<LongReadLen> must be set to C<0>.


=head1 CAVEATS

=head2 Character set

Proper Unicode support depends on all components involved in your
application: the DBMS, the ADO provider, Perl and some perl modules.

In short: Perl 5.8 and Win32::OLE 0.16 (or later) are strongly
recommended and Win32::OLE has to be prepared to use the correct
codepage:

  Win32::OLE->Option( CP => Win32::OLE::CP_UTF8 );

More detailed notes can be found at

  http://purl.net/stefan_ram/pub/perl_unicode_en

=head2 ADO providers

=over

=item SQLOLEDB may truncate inserted strings

It seems that the size of the first inserted string is sticky.
Inserted strings longer than the first one are truncated.

As a workaround, the C<ado_size> attribute for C<bind_param> was
introduced in version 2.95:

  $sth->bind_param( $p_num, $bind_value, { ado_size => $size } );

=item MSDAORA may have problems with client-side cursors

MSDAORA may throw an error, return an empty result set or loop forever
when C<CursorLocation> is set to C<adUseClient>.
This setting is used in catalog methods for sorting and filtering.

=back


=head1 AUTHORS

Tim Bunce and Phlip. With many thanks to Jan Dubois and Jochen Wiedmann
for additions, debuggery and general help.
Special thanks to Thomas Lowery, who maintained this module 2001-2003.
Current maintainer is Steffen Goeldner.


=head1 SUPPORT

This software is supported via the dbi-users mailing list.
For more information and to keep informed about progress you can join the
mailing list by sending a message to dbi-users-help@perl.org

Please post details of any problems (or changes you needed to make) to
dbi-users@perl.org and CC them to me (sgoeldner@cpan.org).


=head1 COPYRIGHT

  Copyright (c) 1998, Tim Bunce
  Copyright (c) 1999, Tim Bunce, Phlip, Thomas Lowery
  Copyright (c) 2000, Tim Bunce, Thomas Lowery
  Copyright (c) 2001, Tim Bunce, Thomas Lowery, Steffen Goeldner
  Copyright (c) 2002, Thomas Lowery, Steffen Goeldner
  Copyright (c) 2003, Thomas Lowery, Steffen Goeldner
  Copyright (c) 2004-2011 Steffen Goeldner

  All rights reserved.

  You may distribute under the terms of either the GNU General Public
  License or the Artistic License, as specified in the Perl README file.


=head1 SEE ALSO

=head2 Books

  ADO Reference book:  ADO 2.0 Programmer's Reference
  David Sussman and Alex Homer
  Wrox
  ISBN 1-861001-83-5

  ADO: ActiveX Data Objects
  Jason T. Roff
  O'Reilly
  ISBN 1-56592-415-0
  http://www.oreilly.com/catalog/ado/index.html

If there's anything better please let me know.

=head2 Perl modules

L<DBI>, L<DBD::ODBC>, L<Win32::OLE>

=cut
