=head1 NAME

DBD::ODBC::Changes - Log of significant changes to the DBD::ODBC

As of $LastChangedDate: 2012-04-07 14:53:28 +0100 (Sat, 07 Apr 2012) $

$Revision: 10667 $

=head2 Changes in DBD::ODBC 1.37 April 7 2012

  A full release of the 1.36 dev releases.

  Please note the changes in behaviour below.

=head2 Changes in DBD::ODBC 1.36_2 March 31 2012

  [BUG FIXES]

  * not strictly a bug fix, more a workaround. MS Access mdb driver
    reports 22 for the size of doubles but then returns an empty string
    for long doubles that still fit in 22 chrs. rt 69864.

  [CHANGE IN BEHAVIOUR]

  * The odbc_disable_array_operations has been replaced with
    odbc_array_operations and the default for array operations is off.
    Sorry, but I warned this was experimental. The
    ODBC_DISABLE_ARRAY_OPERATIONS env var remains.

  [DOCUMENTATION]

  * Rewrote parts of "Unicode implementation in DBD::ODBC" to describe
    UTF-16/UCS-2 internal implementation.

  * Changed ordering of some of the pod sections.

=head2 Changes in DBD::ODBC 1.36_1 March 21 2012

  [BUG FIXES]

  * Fixed 12blob.t skip count when driver does not have a big enough
    varchar column to run the test.

  * Work around problems hit during the test suite in DB2 driver which
    only allows rows of size up to the page size and varchars of 4K.

  * Fix bug in execute_for_fetch where it would ignore the parameters
    processed when creating the ArrayTupleStatus and hence could
    attempt to call SQLGetDiagRec on a parameter row which was not
    executed. See the logs in rt 75687 which show this although this
    is not a fix for this rt.

  * The code wasn't catching success with info from SQLExecute in
    execute_for_fetch.

  * Add support for drivers (DB2 in this case) which return
    SQL_PARAM_DIAG_UNAVAILABLE in bound parameter status array when
    running execute_for_fetch. Any driver returning doing
    SQL_PARC_NO_BATCH from SQLGetInfo(SQL_PARAM_ARRAY_ROW_COUNTS)
    might do this.

  * Fix test code for execute_for_fetch to a) handle drivers doing
    SQL_PARC_NO_BATCH and b) add "not null" to primary key fields for
    drivers like DB2 which require it.

  [CHANGE IN BEHAVIOUR]

  * In execute_for_fetch set the parameter status array to all 9999
    (which is invalid) so we can see if the ODBC driver actually sets
    them and we can warn if they don't.

  * For freeTDS default odbc_disable_array_operations to 1 as no
    version of the freeTDS driver can be found that works. I was
    requested to do this by the dbix-class guys. I may revert this
    later if freeTDS is fixed.

  * as above for MS Access. It is a shame I cannot find any way of
    finding out if a driver is capable of array operations.

  [ENHANCEMENTS]

  * execute_for_fetch code now checks the
    ODBC_DISABLE_ARRAY_OPERATIONS environment variable which can
    be set to 1 or 0 to override the internal default.

  [DOCUMENTATION]

  * Fixed ColAttributes example in pod which used a $dbh instead of a
    $sth.

  * Fixed DescribeCol example in pod which used a $dbh instead of a
    $sth.

  * new FAQ on SQLRowCount, freeTDS and execute_for_fetch

  * Fix typo shown in rt 75860.

  [OTHER]

  * Reduced usage of D_imp_xxx to avoid calls to dbih_getcom2.  See
    thread on dbi-dev at
    http://www.mail-archive.com/dbi-dev@perl.org/msg06675.html

  * Changed the 70execute_array.t test to run it twice, once using
    DBI's methods and once using the native one in DBD::ODBC.

  * Made the 2 unicode tests work with DB2 ODBC driver.

=head2 Changes in DBD::ODBC 1.35 March 6 2012

  Full release of the 1.34 development releases

=head2 Changes in DBD::ODBC 1.34_7 March 2 2012

  [BUG FIXES]

  * Fixed more compiler errors highlighed by a smoker using MS Visual
    C where some code come before a variable definition.

=head2 Changes in DBD::ODBC 1.34_6 February 27 2012

  [BUG FIXES]

  * Fixed some compiler warnings and a compile error highlighed by a
    smoker using MS Visual C where some code come before a variable
    definition.

=head2 Changes in DBD::ODBC 1.34_5 February 17 2012

  [BUG FIXES]

  * The 40UnicodeRoundTrip tests counts could be 1 off in some cases.

  * Fix for t/03batt.t which could fail a test if the data source had
    no table - Kenichi Ishigaki

  * If a driver misbehaves during global destruction e.g. SQLFreeStmt
    fails but no error is available DBD::ODBC issues an error saying
    an error occurred but no error diagnostics could be found. This is
    pointless and irritating during global destruction. This stems
    from a change in 1.28. Thanks to Peter Rabbitson for reporting
    and suggested fix.

  [CHANGE IN BEHAVIOUR]

  * Prior to this release if you called selectall_* methods with a
    non-select statement DBD::ODBC would raise an error saying "no
    select statement currently executing".  See RT 68720. After
    discussions on dbi-dev the concensus seems to be that issuing a
    warning in this case is better so that is what I've done. As a
    result t/rt_68720.t has been removed and
    t/85_selectall_non_select.t has been added.

  [DOCUMENTATION]

  * odbc_getdiagfield was incorrectly named odbc_getdiagrec in the pod

  * add DBI version required for StrictlyTyped and DiscardString to
    pod

  * Added new FAQ on why a transaction may be committed when
    AutoCommit is turned off.

  [OTHER]

  * Make examples\odbc_diag.pl more tolerant of drivers which do not
    handle diagnostic calls properly.

  * Make t/40UnicodeRoundTrip.t work with SQLite - Kenichi Ishigaki

  * Make t/odbc_describe_parameter.t work with SQLite - Kenichi
    Ishigaki

  * Add 80_odbc_diags.t based on the same file in examples

=head2 Changes in DBD::ODBC 1.34_4 February 5 2012

  [BUG FIXES]

  * When odbc_getdiag* methods were added they installed themselves
    into DBI but did not set IMP_KEEP_ERR so calling them cleared
    DBI's errors.

=head2 Changes in DBD::ODBC 1.34_3 February 3 2012

  [BUG FIXES]

  * Linking against unixODBC was working by accident on most UNIX
    machines and depended on the order of the files in /usr/lib (or
    wherever) and what files there were (e.g. an archive or a shared
    object). Same applied to iODBC but it was more broken especially
    on machines where libiodbc.so.N.N existed but there was no
    libiodbc.so which could lead to no adding the shared object at
    all. I doubt anyone really noticed this but I did eventually on
    Ubuntu where libiodbc.so.N.N existed but libiodbc.so did not.

  [ENHANCEMENTS]

  * Added experimental odbc_getdiagrec and odbc_getdiagrec methods,
    examples/odbc_diag.pl and examples/params_in_error.pl.

  [DOCUMENTATION]

  * New FAQ entries.

=head2 Changes in DBD::ODBC 1.34_2 January 25 2012

  [BUG FIXES]

  * Fixed rt73734 - debian moved where unixODBC libs are stored.

  * Fixed memory leak of the parameter status array introduced in
    previous release when execute_for_fetch used. When the statement
    handle is destroyed the parameter status array was not freed.

  [ENHANCEMENTS]

  * Added environment variable PERL_DBD_ODBC_PREFER_UNIXODBC as a
    synonym for -x from Rafael Kitover (Caelum).

  [DOCUMENTATION]

  * Add a deviation from DBI spec for type_info_all.

  [OTHER]

  * Added example execute_for_fetch.pl
  
=head2 Changes in DBD::ODBC 1.34_1 December 11 2011

  [ENHANCEMENTS]

  * Added experimental execute_for_fetch support and associated
    attributes odbc_batch_size and odbc_disable_array_operations.

=head2 Changes in DBD::ODBC 1.33 December 1 2011

  This is simply the official release of the 1.32 development
  releases.

=head2 Changes in DBD::ODBC 1.32_5 November 24 2011

  [ENHANCEMENTS]

  * Enable multiple active statement support in 70execute_array.t for
    drivers we recognise which support MAS.

  * Change column_info to support Unicode catalog/schema/table/column
    names.

=head2 Changes in DBD::ODBC 1.32_4 November 22 2011

  [BUG FIXES]

  * remove debugging printf which output "HERE" in some rare cases.
    rt 72534 - thanks John Deighan for spotting this.

  * The test 70execute_array.t could fail due to warning being output
    if the driver does not support Multiple Active Statements.

  [ENHANCEMENTS]

  * Use SQLGetTypeInfoW on unicode builds.

=head2 Changes in DBD::ODBC 1.32_3 November 15 2011

  [BUG FIXES]

  * Fix bug in utf16_copy which was not adding a trailing NUL but I'm
    not sure this affected anyone until I changed table_info this
    release.

  [ENHANCEMENTS]

  * DBD::ODBC now allows unicode catalog/schema/table parameters to be
    passed to table_info. Of course they will only reliably work with
    a supporting Unicode ODBC driver.

=head2 Changes in DBD::ODBC 1.32_2 October 22 2011

  [ENHANCEMENTS]

  * Added new odbc_driver_complete attribute allowing the ODBC Driver
    Manager and ODBC Driver to throw dialogues for incomplete
    connection strings or expired passwords etc.

  [OTHER]

  * added more examples

  [DOCUMENTATION]

  * new FAQ entries

  * added note saying you cannot pass unicode schema/table/column
    names to metadata calls like table_info/column_info currently.

=head2 Changes in DBD::ODBC 1.32_1 June 24 2011

  [BUG FIXES]

  * I omitted rt_68720.t from the 1.31 distribution which leads
    to a warning as it is mentioned in the MANIFEST.
 
  [OTHER]

  * Changed line endings in README.af and README.unicode to be unix
    line endings and native eol-style in subversion.

  * Minor changes to Makefile.PL to save the opensuse guys patching.

  * Added unicode_sql.pl and unicode_params.pl examples

=head2 Changes in DBD::ODBC 1.31 June 21, 2011

  [BUG FIXES]

  Recently introduced test sql_type_cast.t cannot work with DBI less
  than 1.611.

  Minor change to Makefile.PL to avoid use of unitialised warning on
  $ENV{LD_LIBRARY_PATH} in warning when it is not set.

=head2 Changes in DBD::ODBC 1.30_7 June 15, 2011

  [BUG FIXES]

  Some time ago (I don't know when) a few internal errors generated by
  DBD::ODBC got ignored. There are about 5 of them but I seriously
  doubt anyone would hit any other than the data truncated error
  (which is reported by the ODBC driver anyway) and "no select
  statement currently executing". You can see rt_68720.t in the t
  directory for an example of the latter.

  [ENHANCEMENTS]

  An extra argument has been added to the sub associated with
  odbc_err_handler.  The arguments passed to the odbc_err_handler are
  now state (string), error (string), native error code (number) and
  the status returned from the last ODBC API. The status will be
  SQL_ERROR (-1) for errors or SQL_SUCCESS_WITH_INFO (1) for
  informational messages.

=head2 Changes in DBD::ODBC 1.30_6 June 4, 2011

  [BUG FIXES]

  * When DBD::ODBC calls SQLExecDirect (the do method) it was not
    reporting informational diagnostics (SQL_SUCCESS_WITH_INFO) and
    not calling the error handler.

    Arguably, this is a change in behaviour but one I've struggled to
    resolve since in all other cases of warnings DBD::ODBC's error
    handler is called. However, DBI does not call its error handler
    for warnings so was DBD::ODBC wrong to call it's error in the
    first place for warnings? I decided it was better to leave this as
    it is but add checking of SQLExecDirect/do. Apart from anything
    else if DBD::ODBC does not call its error handler for
    informational diagnostics there is no way to retrieve print
    statements etc from procedures.

  * The odbc_describe_parameter.t test could fail with some versions
    of MS SQL Server ODBC Driver. It was down to when
    SQLDescribeParameter is disabled, the column_size passed to
    SQLBindParameter is 0.

  * pod example of odbc_err_handler incorrectly used \$err_handler
    instead of \&err_handler.

=head2 Changes in DBD::ODBC 1.30_5 May 24, 2011

  [BUG FIXES]

  * The change in behavior detailed in 1.30_1 for wide character
    binding was still not working properly (see
    http://rt.cpan.org/Ticket/Display.html?id=67994). It was working
    for SQL_CHAR described columns but not SQL_VARCHAR.

=head2 Changes in DBD::ODBC 1.30_4 May 18, 2011

  [BUG FIXES]

  * Fix issue described in
    http://www.nntp.perl.org/group/perl.dbi.dev/2011/05/msg6567.html.
    If you prepare a statement, disconnect and then try and execute the
    statement you get an error but it does not tell what is wrong.

  [ENHANCEMENTS]

  * Added support for StrictlyTyped and DiscardString to the bind_col
    method.

  [OTHER]

  * Minor changes to META.yml for mailing list, dynamic_config,
    homepage and keywords.

  * The pod was missing = before the heads on a couple of sections in
    "Private DBD::ODBC Functions"

  * TreatAsLob was incorrectly documented as BindAsLob.

=head2 Changes in DBD::ODBC 1.30_3 May 17, 2011

  [BUG FIXES]

  * Made the new odbc_describe_parameters work and added test case.

=head2 Changes in DBD::ODBC 1.30_2 May 16, 2011

  [ENHANCEMENTS]

  * Added the new odbc_describe_parameters attribute.

=head2 Changes in DBD::ODBC 1.30_1 May 12, 2011

  [BUG FIXES]

  * Fixed some compiler warnings shown with -Wall including some
    printf formats that had extra/missing arguments.

  * Fixed t/70execute_array.t which was missing an "order by" in the
    check_data sub which could cause failures for drivers not
    returning the rows in the order they were inserted.

  * Minor fix to Makefile.PL to avoid issuing variable used in void
    context.

  [CHANGE IN BEHAVIOUR]

  * DBD::ODBC used to quietly rollback any transactions when
    disconnect was called and AutoCommit was off. This can mask a
    problem and leads to different behaviour when disconnect is called
    vs not calling disconnect (where you get a warning). This release
    issues a warning if you call disconnect and a transaction is in
    progress then it is rolled back.

  * DBD::ODBC used to bind char/varchar/longvarchar columns as SQL_CHAR
    meaning that in the unicode build of DBD::ODBC the bound column
    data would be returned 8bit in whatever character-set (codepage) the
    data was in, in the database. This was inconvenient and arguably a
    mistake. Columns like nchar/nvarchar etc were bound as SQL_WCHAR and
    returned as Unicode. This release changes the behaviour in a unicode
    build of DBD::ODBC to bind all char columns as SQL_WCHAR. This may
    inconvenience a few people who expected 8bit chars back, knew the
    char set and decoded them (sorry). See odbc_old_unicode to return
    to old behaviour.

  [ENHANCEMENTS]

  * added -w option to Makefile.PL to add "-Wall" to CCFLAGS and
    -fno-strict-aliasing so I can find warnings.

  * Cope with broken ODBC drivers that describe a parameter as SQL
    type 0.

  [OTHER]

  * Add "disconnect and transactions" to pod describing what DBD::ODBC
    does if you call disconnect with an outstanding transaction.

  * Reorganised FAQ for bound parameter issues and added a lot on
    bound parameter problems.

  * Added new FAQ entry for Firebird

  * Removed some unused variables and added some missing function
    prototypes

=head2 Changes in DBD::ODBC 1.29 March 8, 2011

  * An official release of the 1.28 development releases.

  [NOTE]

  * The 1.28 development releases made a change which causes a generic
    error to be reported when an ODBC call fails but an error message
    is not retrieved from the ODBC Driver. It appears this has caught
    out a few ODBC drivers - notably freeTDS and Firebird. You now may
    see errors that were missed before e.g., DBIx::Class's tests for
    Firebird now errors test 21 "outer txn rolled back" (and others)
    because SQLRowCount returns an error after "ROLLBACK TO SAVEPOINT
    savepoint_0"; before this error was missed.

=head2 Changes in DBD::ODBC 1.28_5 March 6, 2011

  [BUG FIXES]

  * rt_59621.t had wrong skip count

  * Fixed missing SQL_MAX_TABLE_NAME_LEN definition from test.

  * Fixed problem with some drivers which batch "insert;select" where
    SQLMoreResults is not required and an extra describe is done.

  * Fixed "select 1" in 02simple.t for Firebird ODBC Driver.

  * disconnect call added to 70execute_array.t was in the wrong place.

  * In non-unicode mode we bind strings as SQL_CHAR but the driver may
    have described them as SQL_WCHAR and we were not doing ChopBlanks
    processing in that case.

  [REQUIREMENTS]

  * Now needs Test::Simple 0.90.

  [OTHER]

  * Added dml_counts.pl example

  * worked around a problem in freeTDS in the 20SqlServer.t test
    provided by Ralph Doncaster.

  * Changed test rt_62033.t to try and make it work with freeTDS - I
    failed. It now skips attempts to fetch when the insert fails.

  * Worked around problem in Firebird ODBC driver which reports
    timestamps have a display size of 24 characters but then can
    return 29 which could lead to data truncation errors.
    See http://tracker.firebirdsql.org/browse/ODBC-112

  * Worked around problem in Firebird ODBC driver which reports
    VARCHARs have a maximum length of 32765 but in fact it is 4000.
    See http://tracker.firebirdsql.org/browse/ODBC-111

  * Improvements in tracing to take account of DBI's neatsvpv lops 5
    characters off maxsize on a string.

=head2 Changes in DBD::ODBC  1.28_4 February 24, 2011

  [BUG FIXES]

  * Fixed compilation problems with DBIf_TRACE_TXN

  * Added missing disconnect call to 70execute_array.t

=head2 Changes in DBD::ODBC  1.28_3 February 22, 2011

  [BUG FIXES]

  * Fixed MANIFEST in 1.28_2 which named 2 files incorrectly.

  * Fixed use of PREREQ_PRINT in versions of ExtUtils::MakeMaker which
    don't support it.

  * Fixed a check on LD_LIBRARY_PATH for unixODBC which could report
    you've not set LD_LIBRARY_PATH correctly.

  [ENHANCEMENTS]

  * Added Perl and ExtUtils::MakeMaker version output to build process.

  * Added support for DBI's new trace flags ENC, CON, TXN and
    DBD. From DBI 1.617 you should be able to use: DBI_TRACE=DBD to
    ONLY get DBD::ODBC tracing without DBI tracing ENC and CON DBI
    flags are synonymous with DBD::ODBC's odbcconnection and
    odbcunicode trace flags which you can still use for now.

  [OTHER]

  * From now on I'm changing the way the Changes file is written as
    per article at
    http://blog.urth.org/2011/01/changes-file-how-and-how-not-to.html

  * Some broken drivers (freeTDS in this case) can return SQL_ERROR
    from an ODBC API function and then SQLError does not return
    error details. In this case set a generic error saying an error
    occurred but we could not retrieve it.

  * Added FAQ entry on MS Access and text columns limited to 255 chrs.

  * Added 70execute_array.t test.

=head2 Changes in DBD::ODBC  1.28_2 January 24, 2011

  Added -x argument to Makefile.PL saying you prefer unixODBC over
  iODBC driver managers as a) we need to look for iODBC first on some
  platforms to detect iODBC and b) some platforms (Debian/Ubuntu)
  people have both binary packages installed but only unixODBC dev
  package.

  Patch from Rafael Kitover (Caelum) for better Cygwin handling.

  Minor change to data sources test to cope with someone having no
  data sources and using a DSN-less connection for testing.

  Fixed MARS test when a DSN-less connection used for testing - thanks
  to Rafael Kitover (Caelum) for spotting this.

  pod patch for "CPAN Testers Reporting" to point at cpan testers wiki
  from Rafael Kitover (Caelum).

  Fixed some broken links in the FAQ.

  Add a multiple active statement document link to random links and
  the FAQ entry.

  A call to SQLNumResultCols was not checked to test it succeeded.
  Not seen anyone run into this as yet.

=head2 Changes in DBD::ODBC  1.28_1 December 29, 2010

  Rewrote documentation on odbc_SQL_ROWSET_SIZE and added loads of
  notes so I don't have to go through a huge irc conversation with
  ribasushi again.

  Workaround bug in DBI (prior to 1.616) which mistakenly issues a
  FETCH on any attribute passed to the connect method and sometimes
  driver managers error on SQL_ROWSET_SIZE in SQLGetConnectAttr.

  ChopBlanks was not working on UCS-2 encoded data written into bound
  columns and was also sometimes reading off the end of the bound
  array.

  Minor FAQ changes:
    Added an additional way to read MS Access dbs from Unix
    Clarified versions for MARS_Connection
    updates to cancel_big_fetch.pl

  Updated TO_DO with more stuff to do

  Improved tracing output

  Tidied up some of the examples

=head2 Changes in DBD::ODBC  1.27 December 29, 2010

  Official release of the 1.26 development releases.

=head2 Changes in DBD::ODBC  1.26_4 December 14, 2010

  Fixed bug highlighted by investigation into rt 62033 where the
  active flag was sometimes not set on the statement after
  SQLMoreResults indicates a result-set exists.

  Fix rt 63550 reported by John Corcoran where if a do method call
  fails the SQL C<Statement> is not available in an error handler as
  we never created a DBI statement in the first place. Added a note
  to "do" deviations pod.

  Minor fix to head at wrong level in the pod.

  Fix a possible snprintf buffer overflow in GetTypeInfo when
  the type is specified and it is negative.

=head2 Changes in DBD::ODBC  1.26_3 November 18, 2010

  Fixed rt 63108. The change to column binding in 1.24_2 was not
  complete and causes bound columns to be rebound on each execute
  wasting time and leaking memory. Found, diagnosed and proposed fix
  by Steve Bentley.

=head2 Changes in DBD::ODBC  1.26_2 November 9, 2010

  Fixed bug found by frew where an snprintf can overflow when binding
  a lot of parameters.

=head2 Changes in DBD::ODBC  1.26_1 October 24, 2010

  There are over 200 block changes in the source code for this release
  from the previous one (see below). If you are using DBD::ODBC in
  production you should not upgrade without testing this release first
  as it introduces a lot of internal changes. DBD::ODBC has now gone
  entirely ODBC 3 and relies on an ODBC Driver Manager to map calls
  to ODBC 2.0 drivers (why are you still using ODBC 2.0 drivers?).
  From now on DBD::ODBC needs to be linked with an ODBC Driver Manager
  and I recommend unixODBC on UNIX and the MS ODBC Driver Manager
  on Windows. There are really good reasons for this but mainly it
  is because an ODBC Driver Manager will map ODBC 2.0 calls to an
  ODBC 3.0 driver and vice versa and handle UNICODE transparently.

  Bumped Test::Simple requirement up to 0.90 so we can use done_testing
  and note.

  Bump Perl requirement to 5.8 as per DBI.

  Workaround a bug in mdbtools which fails to set the out connection
  string in a SQLDriverConnect call. This can lead to:

  Fixed panic: sv_setpvn called with negative strlen at
  blib/lib/DBD/ODBC.pm line 107.

  Added rt_61370.t for rt 61370.

  Removed last remaining sprintf calls and replaced with snprintf.

  Changed the point at which DBD::ODBC switches from VARCHAR to
  LONGVARCHAR or WVARCHAR to WLONGVARCHAR when SQLDesribeParam fails. It
  was 4000 and is now 2000 for unicode builds.  Works around a daft issue
  in the MS SQL Server driver which will not allow 'x' x 2001 converted to
  wide characters to be inserted into a varchar(8000).

  Minor change to Makefile.PL to print out found libs for iODBC and
  unixODBC.

  Added some FAQs for problems with iODBC and a recent bug in DBI.

  Added FAQ on my_snprintf problem.

  Fixed some pod errors in FAQ document.

  Fixed warning on 64 bit Strawberry Perl when compiling dbdimp.c for
  cast from ptr to UDWORD.

  Moved to_do items from Changes to TO_DO.

  Reformatted this file to save Jens work.

  Changed calls to SQLTransact (ODBC 2.0) to SQLEndTran (ODBC 3.0).
  There really shouldn't be any ODBC 2.0 drivers still around but even
  if there are, the ODBC driver manager should do the mapping for us.

  Changed calls to SQLGetConnectOption/SQLSetConnectOption to
  to SQLGetConnectAttr/SQLSetConnectAttr for ODBC 3.0.

  Changed calls to SQLColAttributes (ODBC 2.0) to SQLColAttribute
  (ODBC 3.0).

  Bumped requirement on DBI to 1.609 because that is the first version
  to contain a dbipport.h which defined my_snprintf - see
  https://rt.cpan.org/Public/Bug/Display.html?id=62346 and
  http://www.nntp.perl.org/group/perl.dbi.dev/2010/10/msg6335.html

  Various small changes to dbdimp.c which should have no effect other
  than to make it leaner:

    Removed all dTHR occurrences from dbdimp.c as it is a NOOP since 5.8
     and we need 5.8 at least.
    Removed dbd_param_err as it was not used
    Removed odbc_get_query_timeout as it was never compiled
    Removed eod field from statement as it was never used
    Removed a load of commented out code
    Replaced some SvPV calls with SvPV_nolen when we didn't used the
      length
    Removed some silly code from dbd_db_execdirect which attempted to
      return -3 - don't think it could ever get run.
    Minor tracing output tidy up
    Removed dbd_caution as it was no used
    Localised more variable declarations

=head2 Changes in DBD::ODBC  1.25 September 22, 2010

  Official release of 1.25 combining all the changes in the 1.24_x
  development releases.

=head2 Changes in DBD::ODBC  1.24_6 September 16, 2010

  rt 61370 - default XML type parameters in SQL Server to SQL_WCHAR so
  they accept unicode strings.

=head2 Changes in DBD::ODBC  1.24_5 September 15, 2010

  Fixed missing SvSETMAGIC on a bound scalar which was causing length() to
  return the wrong result - see http://www.perlmonks.org/?node_id=860211
  and a big thank you to Perl Monks and in particular ikegami.

  Changed bind_col so it actually pays attention to the TYPE attribute as
  you could not override the bind type of a bound column before.

=head2 Changes in DBD::ODBC  1.24_4 September 8, 2010

  Left a sv_undef in - thanks smoke testers for finding that.

  Change sprintf to snprintf for safety.

=head2 Changes in DBD::ODBC  1.24_3 September 6, 2010

  Added note from Robert Freimuth for obtaining the last insert ID in MS
  Access.

  Changed all &sv_yes/&sv_no occurrances in XS to PL_sv_yes/PL_sv_no as
  the originals have now gone from blead Perl.

  Minor change to fix missing newline in trace output.

  Added a FAQ entry for how "use regional settings" in MS SQL Server
  breaks things.

=head2 Changes in DBD::ODBC  1.24_2 July 23, 2010

  Fix rt57957 reported by Marc Prewitt. DBD::ODBC was not ignoring named
  placeholders and ? inside comments. Comments are deemed as text between
  "/*" and "*/" (if not in a literal) and line comments begin with "--".

  Added a FAQ on procedures not completing in MS SQL Server. Thanks to
  Peter Rabbitson for hitting this problem and reminding me I'd seen it a
  couple of times before.

  Added a FAQ on equality comparisons with ntext columns.

  Added pod for last_insert_id which is not currently supported.

  Fix bug where if SQLMoreResults was called and failed it was not
  reported.

  Removed some unused fields from the fbh structure which should save a
  little memory for each column in a result-set.

  Started adding support for DBI's DiscardString and StrictlyTyped but not
  complete yet so don't use them yet.

  Added experimental odbc_lob_read method - see pod. Thanks to tye and
  ikegami on perlmonks for pointing out some problems with my initial
  implementation.

  Moved the binding of columns to the first call to fetch instead of after
  execute is called as it prevents bind_col overrriding the type used for
  binding and I needed it to support odbc_lob_read. This may have
  undesired affects so any testing of this release would be appreciated.

  Added bind_col method so DBD::ODBC can support attributes on a bind_col
  call.

  Removed support for DBI's blob_read - it was totally flawed and did not
  work at all. May replace in the future.

  Added support for MS SQL Server XML type (SQL type -152). See rt 59621.

  Added note on do method implementation in DBD::ODBC and how some may
  consider it to deviate from the DBI specification.

=head2 Changes in DBD::ODBC  1.24_1 May 27, 2010

  Corrected pod and private attributes for the odbc_SQL_DRIVER_ODBC_VER
  attribute which was documented as SQL_DRIVER_ODBC_VER.

  Added FAQ on pauses on statement destruction when all the result-set has
  not been retrieved (mostly freeTDS and MS SQL Server ODBC Driver).

  Fixed bug introduced in 1.24 where if you are using MS SQL Server, and
  you are preparing, binding placeholders and re-executing multiple times
  you may get a "Invalid character value for cast specification"
  error. Thanks to anonymous for spotting this and producing a standalone
  example of the problem that made it so much easier to find.

=head2 Changes in DBD::ODBC  1.24 May 14, 2010

  Minor change in Makefile.PL to only use NO_META if ExtUtils::MakeMaker
  is at least at version 6.10. Reported by Chunmei Wu.

  Minor change to test rt_50852 which had wrong skip count.

=head2 Changes in DBD::ODBC  1.23_5 May 6, 2010

  Added advice from Jan Dubois (ActiveState) on building DBD::ODBC for
  ActivePerl (see README.windows).

  rt56692. Fix spelling mistake in DBD::ODBC pod - thanks to Ansgar
  Burchardt.

  Added a 7th way to help documentation - become a tester.

  Hopefully fixed problems building on windows 32 bit platforms that have
  old sql header files not mentioning SQLLEN/SQLULEN.

=head2 Changes in DBD::ODBC  1.23_4 April 13, 2010

  Added more FAQs.

  Small optimization to remove calls to SQLError when tracing is not
  turned on. This was a bug. We only need to call SQLError when SQLExecute
  succeeds if there is an error handler or if tracing is enabled. The test
  was for tracing disabled!

  Large experimental change primarily affecting MS SQL Server users but it
  does impact on other drivers too. Firstly, for MS SQL Server users we no
  longer SQLFreeStmt(SQL_RESET_PARAMS) and rebind bound parameters as it
  is causing the MS SQL Server ODBC driver to re-prepare the SQL.
  Secondly (for all drivers) we no longer call SQLBindParameter again IF
  all the arguments to it are the same as the previous call. If you find
  something not working you better let me know as this is such a speed up
  I'm going to go with this unless anyone complains.

  Minor change to avoid a double call to SQLGetInfo for SQL_DBMS_NAME
  immediately after connection.

  Small change for rt 55736 (reported by Matthew Kidd) to not assume a
  parameter is varXXX(max) if SQLDescribeParam failed in the Microsoft
  Native Client driver.

=head2 Changes in DBD::ODBC  1.23_3 March 24, 2010

  Minor changes to Makefile.PL and dbdimp.c to remove some compiler
  warnings.

  Fix some calls to SQLMoreResults which were not passing informational
  messages on to DBI's set_err. As you could not see all the informational
  messages from procedures, only the first.

  Fix minor issue in 02simple test which printed the Perl subversion
  before the version.

  Changes to 20SqlServer.t to fix a few typos and make table names
  consistent wrt to case - (as someone had turned on case-sensitivity in
  SQL Server) Similar changes in rt_38977.t and rt_50852.t

=head2 Changes in DBD::ODBC  1.23_2 January 26, 2010

  Fixed bug in Makefile.PL which could fail to find unixODBC/iODBC header
  files but not report it as a problem. Thanks to Thomas J. Dillman and
  his smoker for finding this.

  Fixed some compiler warnings in dbdimp.c output by latest gcc wrt to
  format specifiers in calls to PerlIO_printf.

  Added the odbc_force_bind_type attribute to help sort out problems with
  ODBC Drivers which support SQLDescribeParam but describe the parameters
  incorrectly (see rt 50852). Test case also added as rt_50852.t.

=head2 Changes in DBD::ODBC  1.23_1 October 21, 2009

  makefile.PL changes:
    some formatting changes to output warn if unixodbc headers are not
      found that the unixodbc-dev package is not installed use $arext
      instead of "a"
    pattern match for pulling libodbc.* changed
    warn if DBI_DSN etc not defined
    change odbc_config output for stderr to /dev/null
    missing / on /usr/local wheb finding find_dm_hdr_files()

  New FAQ entries from Oystein Torget for bind parameter bugs in SQL
  Server.

  rt_46597.rt - update on wrong table

  Copied dbivport.h from the latest DBI distribution into DBD::ODBC.

  Added if_you_are_taking_over_this_code.txt.

  Add latest Devel::PPPort ppport.h to DBD::ODBC and followed all
  recommendations for changes to dbdimp.c.

  Added change to Makefile.PL provided by Shawn Zong to make
  Windows/Cygwin work again.

  Minor change to Makefile.PL to output env vars to help in debugging
  peoples build failures.

  Added odbc_utf8_on attribute to dbh and sth handles to mark all strings
  coming from the database as utf8.  This is for Aster (based on
  PostgreSQL) which returns all strings as UTF-8 encoded unicode.  Thanks
  to Noel Burton-Krahn.

=head2 Changes in DBD::ODBC 1.23 September 11, 2009

  Only a readme change and version bumped to 1.23. This is a full release
  of all the 1.22_x development releases.

=head2 Changes in DBD::ODBC 1.22_3 August 19, 2009

  Fix skip count in rt_38977.t and typo in ok call.

  Workaround a bug in unixODBC 2.2.11 which can write off the end of the
  string buffer passed to SQLColAttributes.

  Fix skip count in rt_null_nvarchar.t test for non SQL Server drivers.

  Fix test in 02simple.t which reported a fail if you have no ODBC
  datasources.

  In 99_yaml.t pick up the yaml spec version from the meta file instead of
  specifying it.

  Change calls to SQLPrepare which passed in the string length of the SQL
  to use SQL_NTS because a) they are null terminated and more importantly
  b) unixODBC contains a bug in versions up to 2.2.16 which can overwrite
  the stack by 1 byte if the string length is specified and not built with
  iconv support and converting the SQL from ASCII to Unicode.

  Fixed bug in ping method reported by Lee Anne Lester where it dies if
  used after the connection is closed.

  A great deal of changes to Makefile.PL to improve the automatic
  detection and configuration for ODBC driver managers - especially on
  64bit platforms. See rt47650 from Marten Lehmann which started it all
  off.

  Add changes from Chris Clark for detecting IngresCLI.

  Fix for rt 48304. If you are using a Microsoft SQL Server database and
  nvarchar(max) you could not insert values between 4001 and 8000
  (inclusive) in size. A test was added to the existing rt_38977.t test.
  Thanks to Michael Thomas for spotting this.

  Added FAQ on UTF-8 encoding and IBM iSeries ODBC driver.

  Add support for not passing usernames and passwords in call to connect.
  Previously DBD::ODBC would set an unspecified username/password to '' in
  ODBC.pm before calling one of the login_xxx functions.  This allows the
  driver to pull the username/password from elsewhere e.g., like the
  odbc.ini file.

=head2 Changes in DBD::ODBC 1.22_1 June 16, 2009

  Applied a slightly modified version of patch from Jens Rehsack to
  improve support for finding the iODBC driver manager.

  A UNICODE enabled DBD::ODBC (the default on Windows) did not handle
  UNICODE usernames and passwords in the connect call properly.

  Updated "Attribution" in ODBC.pm.

  Unicode support is no longer experimental hence warning and prompt
  removed from the Makefile.PL.

  old_ping method removed.

  Fixed bug in 02simple.t test which is supposed to check you have at
  least one data source defined. Unfortunately, it was checking you had
  more than 1 data source defined.

  rt_null_varchar had wrong skip count meaning non-sql-server drivers or
  sql server drivers too old skipped 2 tests more than were planned.

=head2 Changes in DBD::ODBC 1.22 June 10, 2009

  Fixed bug which led to "Use of uninitialized value in subroutine entry"
  warnings when writing a NULL into a NVARCHAR with a unicode-enabled
  DBD::ODBC. Thanks to Jirka Novak and Pavel Richter who found, reported
  and patched a fix.

   Fixed serious bug in unicode_helper.c for utf16_len which I'm ashamed
   to say was using an unsigned short to return the length. This meant you
   could never have UTF16 strings of more than ~64K without risking
   serious problems. The DBD::ODBC test code actually got a

     *** glibc detected *** /usr/bin/perl: double free or corruption
    (out): 0x406dd008 ***

  If you use a UNICODE enabled DBD::ODBC (the default on Windows) and
  unicode strings larger than 64K you should definitely upgrade now.

=head2 Changes in DBD::ODBC 1.21_1 June 2, 2009

  Fixed bug referred to in rt 46597 reported by taioba and identified by
  Tim Bunce. In Calls to bind_param for a given statement handle if you
  specify a SQL type to bind as, this should be "sticky" for that
  parameter.  That means if you do:

    $sth->bind_param(1, $param, DBI::SQL_LONGVARCHAR)

  and follow it up with execute calls that also specify the parameter:

   $sth->execute("a param");

  then the parameter should stick with the SQL_LONGVARCHAR type and not
  revert to the default parameter type. The DBI docs (from 1.609) make it
  clear the parameter type is sticky for the duration of the statement but
  some DBDs allow the parameter to be rebound with a different type -
  DBD::ODBC is one of those drivers.

=head2 Changes in DBD::ODBC 1.21 April 27, 2009

  Change 02simple test to output Perl, DBI and DBD::ODBC versions.

  Fixed bug where if ODBC driver supports SQLDescribeParam and it succeeds
  for a parameterised query but you override the parameter type, DBD::ODBC
  was still using the size returned by SQLDescribeParam. Thanks to Brian
  Becker for finding, diagnosing and fixing this issue.

  Added FAQ entry about SQL Server and calling procedures with named
  parameters out of order.

  Added test_results.txt containing some supplied make test results.

=head2 Changes in DBD::ODBC 1.20 April 20, 2009

  Fix bug in handling of SQL_WLONGVARCHAR when not built with unicode
  support.  The column was not identified as a long column and hence the
  size of the column was not restricted to LongReadLen. Can cause
  DBD::ODBC to attempt to allocate a huge amount of memory.

  Minor changes to Makefile.PL to help diagnose how it decided which
  driver manager to use and where it was found.

  Offer suggestion to debian-based systems when some of unixODBC is found
  (the bin part) but the development part is missing.

  In 20SqlServer.t attempt to drop any procedures we created if they still
  exist at the end of the test. Reported by Michael Higgins.

  In 12blob.t separate code to delete test table into sub and call at
  being and end, handle failures from prepare there were two ENDs.

  In ODBCTEST.pm when no acceptable test column type is found output all
  the found types and BAIL_OUT the entire test.

  Skip rt_39841.t unless actually using the SQL Server ODBC driver or
  native client.

  Handle drivers which return 0 for SQL_MAX_COLUMN_NAME_LEN.

  Double the buffer size used for column names if built with unicode.

=head2 Changes in DBD::ODBC 1.19 April 2, 2009

  Some minor diagnostic output during tests when running against freeTDS
  to show we know of issues in freeTDS.

  Fixed issue in 20SqlServer.t where the connection string got set with
  two consecutive semi-colons. Most drivers don't mind this but freeTDS
  ignores everything after that point in the connection string.

  Quieten some delete table output during tests.

  Handle connect failures in 20SqlServer.t in the multiple active
  statement tests.

  In 02simple.t cope with ODBC drivers or databases that do not need a
  username or password (MS Access).

  In 20SqlServer.t fix skip count and an erroneous assignment for
  driver_name.

  Change some if tests to Test::More->is tests in 02simple.t.

  Fix "invalid precision" error during tests with the new ACEODBC.DLL MS
  Access driver. Same workaround applied for the old MS Access driver
  (ODBCJT32.DLL) some time ago.

  Fix out of memory error during tests against the new MS Access driver
  (ACEODBC.DLL). The problem appears to be that the new Access driver
  reports ridiculously large parameter sizes for "select ?" queries and
  there are some of these in the unicode round trip test.

  Fixed minor typo in Makefile.PL - diagnostic message mentioned "ODBC
  HOME" instead of ODBCHOME.

  12blob.t test somehow got lost from MANIFEST - replaced. Also changed
  algorithm to get a long char type column as some MS Access drivers only
  show SQL_WLONGVARCHAR type in unicode.

  Added diagnostic output to 02simple.t to show the state of
  odbc_has_unicode.

=head2 Changes in DBD::ODBC 1.18_4 March 13, 2009

  A mistake in the MANIFEST lead to the rt_43384.t test being omitted.

  Brian Becker reported the tables PERL_DBD_39897 and PERL_DBD_TEST are
  left behind after testing. I've fixed the former but not the latter yet.

  Yet another variation on the changes for rt 43384. If the parameter is
  bound specifically as SQL_VARCHAR, you got invalid precision
  error. Thanks to Øystein Torget for finding this and helping me verify
  the fix.

  If you attempt to insert large amounts of data into MS Access (which
  does not have SQLDescribeParam) you can get an invalid precision error
  which can be worked around by setting the bind type to
  SQL_LONGVARCHAR. This version does that for you.

  08bind2.t had a wrong skip count.

  12blob.t had strict commented out and GetTypeInfo was not quoted. Also
  introduced a skip if the execute fails as it just leads to more obvious
  failures.

  In dbdimp.c/rebind_ph there was a specific workaround for SQL Server
  which was not done after testing if we are using SQL Server - this was
  breaking tests to MS Access.

=head2 Changes in DBD::ODBC 1.18_2 March 9, 2009

  Added yet another workaround for the SQL Server Native Client driver
  version 2007.100.1600.22 and 2005.90.1399.00 (driver version 09.00.1399)
  which leads to HY104, "Invalid precision value" in the rt_39841.t test.

=head2 Changes in DBD::ODBC 1.18_1 March 6, 2009

  Fixed bug reported by Toni Salomäki leading to a describe failed error
  when calling procedures with no results. Test cases added to
  20SqlServer.t.

  Fixed bug rt 43384 reported by Øystein Torget where you cannot insert
  more than 127 characters into a Microsoft Access text(255) column when
  DBD::ODBC is built in unicode mode.

=head2 Changes in DBD::ODBC 1.18 January 16, 2009

  Major release of all the 1.17 development releases below.

=head2 Changes in DBD::ODBC 1.17_3 December 19, 2008

  Reinstated the answer in the FAQ for "Why do I get invalid value for
  cast specification" which had got lost - thanks to EvanCarroll in
  rt41663.

  rt 41713. Applied patch from JHF Remmelzwaal to handle ODBC drivers
  which do not support SQL_COLUMN_DISPLAY_SIZE and SQL_COLUMN_LENGTH
  attributes in the SQLColAttributes calls after SQLTables and
  SQLColumns. Specifically, the driver he was using was the "Infor
  Integration ODBC driver".

  Added notes from JHF Remmelzwaal on resolving some problems he came
  across building DBD::ODBC on Windows with Visual Studio 6.0 and SDK Feb
  2003.

  New odbc_column_display_size attribute for when drivers does not return
  a display size.

  Loads of tracing changes to make it easier for me to debug problems.

  Fixed bug in tracing of dbd_execute when parameter is char but undef
  which was leading to an access violation on Windows when tracing
  enabled.

  Minor changes to diagnostic output in some rt tests.

  One of the rt tests was not skipping the correct number of tests if the
  driver was not SQL Server.

=head2 Changes in DBD::ODBC 1.17_2 November 17, 2008

  Changed ParamTypes attribute to be as specification i.e.,

    {
      parameter_1 => {TYPE => sql_type}
      parameter_2 => {TYPE => sql_type}
      ...
    }

  and changed the tests in 07bind.t to reflect this.

  A few minor perlcritic changes to ODBC.pm.

  Added 99_yaml.t test to check META.yml.

  Added patch from Spicy Jack to workaround problems with Strawberry Perl
  setting INC on the command line when running Makefile.PL.

=head2 Changes in DBD::ODBC 1.17_1 October 10, 2008

  Missing newline from end of META.yml upsets cpan

  Add code to Makefile.PL to spot command line containing INC, outline
  problem and resolution and not generate Makefile to avoid cpan-testers
  failures.

  Loads of pod formatting changes including a section in the wrong place

  New kwalitee test

  Fix rt 39841. Bug in SQL Server ODBC driver which describes parameters
  by rearranging your SQL to do a select on the columns matching the
  parameters.  Sometimes it gets this wrong and ends up describing the
  wrong column. This can lead to a varchar(10) being described with a
  column-size less than 10 and hence you get data truncation on execute.

  Added a test case for rt 39841.

  Fix rt 39897. 1.17 added support for varchar(max) in SQL Server but it
  broke support for SQL_VARCHAR columns in that they had LongReadLen and
  LongTruncOk applied to them. This means that in 1.16 you could retrieve
  a SQL_VARCHAR column without worrying about how long it was but in 1.17
  if the same column was greater than 80 characters then you would get a
  truncated error. The only way the around this was to set LongTruncOk or
  LongReadLen.

  Added a test case for rt 39897.

=head2 Changes in DBD::ODBC 1.17 September 22, 2008

  In the absence of any bug reports since 1.16_4 this is the official 1.17
  release. See below for changes since 1.16.

  Minor pod changes.

  Added support for ParamTypes (see DBI spec) and notes in DBD::ODBC pod.

=head2 Changes in DBD::ODBC 1.16_4 September 12, 2008

  Small change to Makefile.PL to work around problem in darwin 8 with
  iODBC which leads to "Symbol not found: _SQLGetPrivateProfileString"
  errors.

  Added new [n]varXXX(max) column type tests to 20SqlServer.t.

  Fixed support for SQL_WCHAR and SQL_WVARCHAR support in non-unicode
  build. These types had ended up only being included for unicode builds.

  More changes to ODBC pod to 1) encourage people to use CPAN::Reporter,
  2) rework contributing section, 3) mention DBIx::Log4perl 4) add a BUGS
  section 5) add a "ODBC Support in ODBC Drivers" section etc.

  Changed default fallback parameter bind type to SQL_WVARCHAR for unicode
  builds. This affects ODBC drivers which don't have
  SQLDescribeParam. Problem reported by Vasili Galka with MS Access when
  reading unicode data from one table and inserting it into another table.
  The read data was unicode but it was inserted as SQL_CHARs because
  SQLDescribeParam does not exist in MS Access so we fallback to either a
  default bind type (which was SQL_VARCHAR) or whatever was specified in
  the bind_param call.

  Fixed bug in 20SqlServer.t when DBI_DSN is defined including "DSN=".

=head2 Changes in DBD::ODBC 1.16_3 September 3, 2008

  Changed Makefile.PL to add "-framework CoreFoundation" to linker line on
  OSX/darwin.

  Disallow building with iODBC if it is a unicode build.

  More tracing for odbcconnect flag.

  Fix bug in out connection string handling that attempted to use an out
  connection string when SQLDriverConnect[W] fails.

  Fixed yet more test count problems due to Test::NoWarnings not being
  installed.

  Skip private_attribute_info tests if DBI < 1.54

  About a 30% rewrite of bound parameter code which started with an
  attempt to support the new VARBINARY(MAX) and VARCHAR(MAX) columns in
  SQL Server when the parameter length is > 400K in size (see elsewhere in
  this Changelog). This is a seriously big change to DBD::ODBC to attempt
  to be a bit more clever in its handling of drivers which either do not
  support SQLDescribeParam or do support SQLDescribeParam but cannot
  describe all parameters e.g., MS SQL Server ODBC driver cannot describe
  "select ?, LEN(?)". If you specify the bound parameter type in your
  calls to bind_param and run them to an ODBC driver which supports
  SQLDescribeParam you may want to check carefully and probably remove the
  parameter type from the bind_param method call.

  Added rt_38977.t test to test suite to test varchar(max) and
  varbinary(max) columns in SQL Server.

  Moved most of README.unicode to ODBC.pm pod.

  Added workaround for problem with the Microsoft SQL Server driver when
  attempting to insert more than 400K into a varbinary(max) or
  varchar(max) column. Thanks to Julian Lishev for finding the problem and
  identifying 2 possible solutions.

=head2 Changes in DBD::ODBC 1.16_2 September 2, 2008

  Removed szDummyBuffer field from imp_fbh_st and code in dbd_describe
  which clears it. It was never used so this was a waste of time.

  Changed the remaining calls to SQLAllocEnv, SQLAllocConnect and
  SQLAllocStmt and their respective free calls to the ODBC 3.0
  SQLAllocHandle and SQLFreeHandle equivalents.

  Rewrote ColAttributes code to understand string and numeric attributes
  rather than trying to guess by what the driver returns. If you see any
  change in behaviour in ColAttributes calls you'll have to let me know as
  there were a number of undocumented workarounds for drivers.

  Unicode build of DBD::ODBC now supports:
    column names
      The retrieval of unicode column names
    SQL strings
      Unicode in prepare strings (but not unicode parameter names) e.g.,
        select unicode_column from unicode_table
      is fine but
        select * from table where column = :unicode_param_name
      is not so stick to ascii parameter names if you use named
      parameters.
      Unicode SQL strings passed to the do method are supported.
      SQL strings passed to DBD::ODBC when the odbc_exec_direct attribute
        is set will not be passed as unicode strings - this is a
        limitation of the odbc_exec_direct attribute.
    connection strings
      True unicode connection string support will require a new version of
      DBI (post 1.607).
      Note that even though unicode connection strings are not
      supported currently DBD::ODBC has had to be changed to call
      SQLDriverConnectW/SQLConnectW to indicate to the driver manager it's
      intention to use some of the ODBC wide APIs. This only affects
      DBD::ODBC when built for unicode.
    odbcunicode trace flag
      There is a new odbcunicode trace flag to enable unicode-specific
      tracing.

  Skipped 40Unicode.t test if the ODBC driver is Oracle's ODBC as I cannot
  make it work.

  Changes internally to use sv_utf8_decode (where defined) instead of
  setting utf8 flag.

  Fix problems in the test when Test::NoWarnings is not installed.

  Removed some unused variables that were leading to compiler warnings.

  Changed a lot of tracing to use new odbcconnection flag

  Changed to use dbd_db_login6_sv if DBI supports it.

  Commented out a diag in 20SqlServer.t that was leading to confusion.

  Added diag to 20SqlServer.t in mars test to explain why it may fail.

  Various pod changes for clarification and to note odbc_err_handler is
  deprecated.

  Removed odbcdev trace flag - it was not really used.

  New odbc_out_connect_string attribute for connections which returns the
  ODBC driver out connection string.

=head2 Changes in DBD::ODBC 1.16_1 August 28, 2008

  Fixed bug in odbc_cancel which was checking the ACTIVE flag was on
  before calling SQLCancel. Non-select statements can also be cancelled so
  this was wrong. Thanks to Dean Arnold for spotting.

  Minor changes to test 01base to attempt to catch install_driver failing,
  report it as a fail and skip other tests.

  Fixed bug reported by James K. Lowden with asynchronous mode and
  SQLParamData where the code was not expecting SQL_STILL_EXECUTING and
  did not handle it

  Added odbc_putdata_start attribute to determine when to start using
  SQLPutData on lobs.

  Fixed bug in lob inserts where decimal_digits was being set to the size
  of the bound lob unnecessarily.

  Minor change to connect/login code to delete driver-specific attributes
  passed to connect so they do not have to be processed again when DBI
  calls STORE with them.

  New 12blob.t test.

  A lot of code tidy up but not expecting any real benefit or detriment
  when using DBD::ODBC.

  Fixed retrieving [n]varchar(max) columns which were only returning 1
  byte - thanks to Fumiaki Yoshimatsu and perl monks for finding it.  See
  http://markmail.org/message/fiym5r7q22oqlzsf#query:Fumiaki Yoshimatsu
  odbc+page:1+mid:fiym5r7q22oqlzsf+state:results

  Various minor changes to get the CPANTS kwalitee score up.
    fixed pod issues in FAQ.pm
    moved mytest dir to examples
    added generated_by and requires perl version to META.yml
    added pod and pod-coverage tests
    removed executable flag from Makefile.PL
    added use warnings to some modules and tests
    fixed pod errors in ODBC.pm
    added AUTHOR and LICENSE section to ODBC.pm
    added Test::NoWarnings to tests

  Added support for setting the new(ish) DBI ReadOnly attribute on a
  connection. See notes in pod.

  Changes to test suite to work around problems in Oracle's instant client
  11r1 ODBC driver for Linux (SQLColAttributes problems - see 02simple.t).

  New tests in 30Oracle.t for oracle procedures.

=head2 Changes in DBD::ODBC 1.16 May 13, 2008

=head3 Test Changes

  Small change to the last test in 10handler.t to cope with the prepare
  failing instead of the execute failing - spotted by Andrei Kovalevski
  with the ODBCng Postgres driver.

  Changed the 20SqlServer.t test to specifically disable MARS for the test
  to check multiple active statements and added a new test to check that
  when MARS_Connection is enabled multiple active statements are allowed.

  Changed the 09multi.t test to use ; as a SQL statement seperator instead
  of a newline.

  A few minor "use of unitialised" fixes in tests when a test fails.

  In 02simple.t Output DBMS_NAME/VER, DRIVER_NAME/VER as useful debugging
  aid when cpan testers report a fail.

  2 new tests for odbc_query_timeout added to 03dbatt.t.

  Changed 02simple.t test which did not work for Oracle due to a "select
  1" in the test. Test changed to do "select 1 from dual" for Oracle.

  New tests for numbered and named placeholders.

=head3 Documentation Changes

  Added references to DBD::ODBC ohloh listing and markmail archives.

  Added Tracing sections.

  Added "Deviations from the DBI specification" section.

  Moved the FAQ entries from ODBC.pm to new FAQ document. You can view the
  FAQ with perldoc DBD::ODBC::FAQ.

  Added provisional README.windows document.

  Rewrote pod for odbc_query_timeout.

  Added a README.osx.

=head3 Internal Changes

  More tracing in dbdimp.c for named parameters.

  #ifdeffed out odbc_get_primary_keys in dbdimp.c as it is no longer used.
  $h->func($catalog, $schema, $table, 'GetPrimaryKeys') ends up in
  dbdimp.c/dbd_st_primary_keys now.

  Reformatted dbdimp.c to avoid going over 80 columns.

  Tracing changed. Levels reviewed and changed in many cases avoiding
  levels 1 and 2 which are reserved for DBI. Now using DBIc_TRACE macro
  internally.  Also tracing SQL when 'SQL' flag set.

=head3 Build Changes

  Changes to Makefile.PL to fix a newly introduced bug with 'tr', remove
  easysoft OOB detection and to try and use odbc_config and odbcinst if we
  find them to aid automatic configuration. This latter change also adds
  "odbc_config --cflags" to the CC line when building DBD::ODBC.

  Avoid warning when testing ExtUtils::MakeMaker version and it is a test
  release with an underscore in the version.

=head3 Functionality Changes

  Added support for parse_trace_flag and parse_trace_flags methods and
  defined a DBD::ODBC private flag 'odbcdev' as a test case.

  Add support for the 'SQL' trace type. Added private trace type odbcdev
  as an experimental start.

  Change odbc_query_timeout attribute handling so if it is set to 0 after
  having set it to a non-zero value the default of no time out is
  restored.

  Added support for DBI's statistics_info method.

=head3 Bug Fixes

  Fix bug in support for named placeholders leading to error "Can't rebind
  placeholder" when there is more than one named placeholder.

  Guard against scripts attempting to use a named placeholder more than
  once in a single SQL statement.

  If you called some methods after disconnecting (e.g., prepare/do and any
  of the DBD::ODBC specific methods via "func") then no error was
  generated.

  Fixed issue with use of true/false as fields names in structure on MAC
  OS X 10.5 (Leopard) thanks to Hayden Stainsby.

  Remove tracing of bound wide characters as it relies on null-terminated
  strings that don't exist.

  Fix issue causing a problem with repeatedly executing a stored procedure
  which returns no result-set. SQLMoreResults was only called on the first
  execute and some drivers (SQL Server) insist a procedure is not finished
  until SQLMoreResults returns SQL_NO_DATA.

=head2 Changes in DBD::ODBC 1.15 January 29, 2008

  1.15 final release.

  Fixed bug reported by Toni Salomaki where DBD::ODBC may call
  SQLNumResultCols after SQLMoreResults returns SQL_NO_DATA. It led to the
  error:
    Describe failed during DBI::st=HASH(0x19c2048)->FETCH(NUM_OF_FIELDS,0)
  when NUM_OF_FIELDS was referenced in the Perl script.

  Updated odbc_exec_direct documentation to describe its requirement when
  creating temporary objects in SQL Server.

  Added FAQ on SQL Server temporary tables.

  Fixed bug in dbdimp.c which was using SQL_WCHAR without testing it was
  defined - thanks Jergen Dutch.

  Fixed use of "our" in UCHelp.pm which fails on older Perls.

  Minor changes to 02simple.t and 03dbatt.t to fix diagnostics output and
  help debug DBD which does not handle long data properly.

  Further changes to Makefile.PL to avoid change in behaviour of
  ExtUtils::MakeMaker wrt order of execution of PREREQ_PM and CONFIGURE.
  Now if DBI::DBD is not installed we just warn and exit 0 to avoid a
  cpan-testers failure.

=head2 Changes in DBD::ODBC 1.15_2 November 14, 2007

  Fix bug in DBD::ODBC's private function data_sources which was returning
  data sources prefixed with "DBI:ODBC" instead of "dbi:ODBC".

  If you don't have at least DBI 1.21 it is now a fatal error instead of
  just a warning.

  DBI->connect changed so informational diagnostics like "Changed database
  context to 'master'" from SQL Server are available in
  errstr/state. These don't cause DBI->connect to die but you can test
  $h->err eq "" after connect and obtain the informational diagnostics
  from errstr/state if you want them.

  Fixed problem in 41Unicode.t where utf8 was used before testing we had a
  recent enough Perl - thank you cpan testers.

  Changed "our" back to "my" in Makefile.PL - thank you cpan testers.

  Removed all calls to DBIh_EVENT2 in dbdimp.c as it is no longer used
  (see posts on dbi-dev).

  Changed text output when a driver manager is not found to stop referring
  to iodbcsrc which is no longer included with DBD::ODBC.

  Changed Makefile.PL to attempt to find unixODBC if -o or ODBCHOME not
  specified.

  Updated META.yml based on new 1.2 spec.

  Changed Makefile.PL so if an ODBC driver manager is not found then we
  issue warning and exit cleanly but not generating a Makefile. This
  should stop cpan-testers from flagging a fail because they haven't got
  an ODBC driver manager.

  Changed Makefile.PL so it no longer "use"s DBI/DBI::DBD because this
  makes cpan-testers log a fail if DBI is not installed. Changed to put
  the DBI::DBD use in the CONFIGURE sub so PREREQ_PM will filter out
  machines where DBI is not installed.

  Fix a possible typo, used once in 10handler.t.

=head2 Changes in DBD::ODBC 1.15_1 November 6, 2007

  Minor changes to 20SqlServer.t test for SQL Server 2008 (Katmai).
  Timestamps now return an extra 4 digits of precision (all 0000) and the
  driver reported in dbcc messages has a '.' in the version which was not
  handled.

  New FAQ entry and test code for "Datetime field overflow" problem in
  Oracle.

  Changed all ODBC code to use new SQLLEN/SQLULEN types where Microsoft's
  headers indicate, principally so DBD::ODBC builds and works on
  win64. NOTE: you will need an ODBC Driver Manager on UNIX which knows
  SQLLEN/SQLULEN types. The unixODBC driver manager uses SQLLEN/SQLULEN in
  versions from at least 2.2.12. Thanks to Nelson Oliveira for finding,
  patching and testing this and then fixing problems with bound parameters
  on 64 bit Windows.

  Added private_attribute_info method DBI introduced (see DBI docs) and
  test cases to 02simple.t.

  Fairly major changes to dbd_describe in dbdimp.c to reduce ODBC calls by
  1 SQLDescribeCol call per column when describing result sets. Instead of
  calculating the amount of memory required to hold every column name we
  work on the basis that (num_columns + 1) * SQL_MAX_COLUMN_NAME_LEN can
  hold all column names. However, to avoid using a large amount of memory
  unnecessarily if an ODBC driver supports massive column name lengths the
  maximum size per column is restricted to 256.

  Changed to avoid using explicit use of DBIc_ERRXXX in favour of newish
  (ok, DBD::ODBC is a bit behind the times in this respect)
  DBIh_SET_ERR_CHAR.  This involved a reworking or the error handling and
  although all test cases still pass I cannot guarantee it has no other
  effects - please let me know if you spot differences in error messages.

  Fixed bug in 20SqlServer test for multiple results that was passing but
  for the wrong reason (namely, that the odbc_err_handler was being called
  when it should not have been).

  Fixed bug in odbc_err_handler that prevented it from being reset so you
  don't have an error handler. Looks like the problem was in
  dbd_db_STORE_attrib where "if(valuesv == &PL_sv_undef)" was used to
  detect undef and I think it should be "if (!SvOK(valuesv))".

  Improvements to odbc_err_handler documentation.

  Added 10handler.t test cases.

  More tests in 02simple.t to check NUM_OF_FIELDS and NAMES_uc.

  Bit of a tidy up:

  Removed some unused variable declarations from dbdimp.c.

  Lots of changes to DBD::ODBC tracing, particularly in dbd_describe, and
  dbd_error2 and login6.

  Removed a lot of tracing code in comments or #if 0 as it never gets
  built.

  Changed dual tests on SQL_SUCCESS and SQL_SUCCESS_WITH_INFO to use
  SQL_SUCCEEDED.

=head2 Changes in DBD::ODBC 1.14 July 17, 2007

  Fix bug reported where ping crashes after disconnect thanks to Steffen
  Goeldner.

  Fix bug in dbd_bind_ph which leads to the error Can't change param 1
  maxlen (51->50) after first bind in the 20SqlServer test. This is caused
  by svGROW in Perl 5.8.8 being changed to possibly grow by more than you
  asked (e.g. up to the next longword boundary).

  Fix problem with binding undef as an output parameter. Reported by
  Stephen More with IBM's ODBC driver for iSeries.

  Removed comment delimiters in comments in dbdimp.h leading to warnings.

  Removed some unused variable declarations leading to warnings.

  Removed PerlIO_flush calls as it is believed they are not required.

  Add logging for whether SQLDescribeParam is supported.

  Fixed use of unitialised variable in dbd_bind_ph where an undef is bound
  and tracing is enabled.

  Fixed issue with TRACESTATUS change in 20SqlServer.t tests 28, 31, 32
  and 33 leading to those tests failing when testing with SQL Server 2005
  or Express.

  Many compiler warnings fixed - especially for incompatible types.

  Add provisional Unicode support - thanks to Alexander Foken. This change
  is very experimental (especially on UNIX). Please see ODBC.pm
  documentation. Also see README.unicode and README.af. New database
  attribute odbc_has_unicode to test if DBD::ODBC was built with UNICODE
  support. New tests for Unicode. New requirement for Perl 5.8.1 if
  Unicode support required. New -[no]u argument to Makefile.PL. New
  warning in Makefile.PL if Unicode support built for UNIX.

  Fix use of unitialised var in Makefile.PL.

  Fix use of scalar with no effect on Makefile.PL

  Added warning to Makefile.PL about building/running with LANG using
  UTF8.

  Added warning to Makefile.PL about using thread-safe ODBC drivers.

  Updated MANIFEST to include more test code from mytest and remove
  MANIFEST.SKIP etc.

  Removed calls to get ODBC errors when SQLMoreResults returns
  SQL_NO_DATA.  These are a waste of time since SQL_NO_DATA is expected
  and there is no error diagnostic to retrieve.

  Changes to test 17 of 02simple.t which got "not ok 17 - Col count
  matches correct col count" errors with some Postgres ODBC
  drivers. Caused by test expecting column names to come back
  uppercase. Fixes by uppercasing returned column names.

  Changes to tests in 03batt.t which correctly expects an ODBC 3 driver to
  return the column names in SQLTables result-set as per ODBC 3.0
  spec. Postgres which reports itself as an ODBC 3.0 driver seems to
  return the ODBC 2 defined column names. Changed tests to catch ODBC 2.0
  names are pass test put issue warning.

  For postgres skip test (with warning) checking $sth->{NAME} returns
  empty listafter execute on update

  Updated FAQ, added a few more questions etc.

  DBD::ODBC requires at least 5.6.0 of Perl.

  Many updates to pod documentation.

  Removed some dead HTTP links in the pod I could not find equivalents for
  - let me know if you have working replacements for ones removed

  Add some HTTP links to useful tutorials on DBD::ODBC

=head2 Changes in DBD::ODBC 1.13 November 8, 2004

  Fix inconsistency/bug with odbc_exec_direct vs. odbc_execdirect
  settings.  Now made consistent with odbc_exec_direct.  For now, will
  still look for odbc_execdirect in prepare, but not as DBH attribute as a
  backup (which is what it was doing), but that support will be dropped at
  some time in the future.  Please use odbc_exec_direct from now on...

  Fix handling of print statements for SQL Server thanks to Martin Evans!
  Thanks for all your work on this!

  Due to bug in SQL Server, you must use odbc_exec_direct.  See
  t/20SqlServer.t for example.  You will need to call
  $sth->{odbc_more_results} to clear out any trailing messages.

  Change tests to use Test::More.  Whew, that's much nicer!

  Fix Oracle integral/numeric output params so that warning not printed
  about value not being numeric (even though it is!)

=head2 Changes in DBD::ODBC 1.12 October 26, 2004

  Fix bug with odbc_execdirect attributed thanks to Martin Evans

  Fix bug(s) with odbc_query_timeout and tested with SQL*Server.  Oracle
  tests failed with setting timeout.  Probably not handled by Oracle's
  ODBC driver

=head2 Changes in DBD::ODBC 1.11 October 11, 2004

  Added odbc_timeout, but untested

=head2 Changes in DBD::ODBC 1.10 September 8, 2004

  Fixed bug in Makefile.PL.

  Added pod.t test, taken from DBI.

  Fixed various small POD issues, discovered during the pod test.

  Fixed bug in bind_param_inout

=head2 Changes in DBD::ODBC 1.09 March 10, 2004

  Duh.  I forgot to add new dbivport.h to MANIFEST and SVN before
  submitting.  Fixed.

=head2 Changes in DBD::ODBC 1.08 March 6, 2004

  Added check in Makefile.PL to detect if the environment variable LANG is
  Set.  If so, prints a warning about potential makefile generation
  issues.

  Change to use dbivport.h per new DBI spec.

  Add ability to set the cursor type during the connect.  This may allow
  some servers which do not support multiple concurrent statements to
  permit them -- tested with SQL Server.  Thanks to Martin Busik!  See
  odbc_cursortype information in the ODBC POD.

=head2 Changes in DBD::ODBC 1.07 February 19, 2004

  Added to Subversion version control hosted by perl.org.  Thanks Robert!
  See ODBC.pm POD for more information.

  Added contributing section to ODBC.pm POD -- see more details there!

  Added parameter to odbc_errhandler for the NativeError -- thanks to
  Martin Busik.

  Fix for Makefile.PL not having tab in front of $(NOOP) (Finally).

  Fix for SQLForeignKeys thanks to Kevin Shepherd.

=head2 Changes in DBD::ODBC 1.06 June 19, 2003

  Fixed test in t/02simple.t to skip if the DSN defined by the user has
  DSN= in it.

  Added tests for wrong DSN, ensuring the DBI::errstr is appropriately
  set.

  Fixed small issue in Makefile.PL for Unix systems thanks to H.Merijn
  Brand.

  Update to NOT copy user id and password to connect string if UID or PWD
  parameter in connect string.

  Updated Makefile.PL for dmake, per patch by Steffen Goldner.  Thanks
  Steffen!

=head2 Changes in DBD::ODBC 1.05 March 14, 2003

  Cleaned up Makefile.PL and added Informix support thanks to Jonathan
  Leffler (see README.informix)

  Added nicer error message when attempting to do anything while the
  database is disconnected.

  Fixed fetchrow_hashref('NAME_uc | NAME_lc') with odbc_more_results.

  Added exporter to allow perl -MDBD::ODBC=9999 command line to determine
  version

  Fixed for building with DBI 1.33 and greater

  Removed all C++ style comments

  Ensured files are in Unix format, with the exception of the README type
  information and Makefile.PL

=head2 Changes in DBD::ODBC 1.04 January 24, 2003

  It seems that case insensitive string comparison with a limit causes
  problems for multiple platforms.  strncmpi, strncasecmp, _strcmpin are
  all functions hit and it seems to be a hit-or-miss.  Hence, I rewrote it
  to upper case the string then do strncmp, which should be safe...sheesh.
  A simple thing turned into a headache...

=head2 Changes in DBD::ODBC 1.03 January 17, 2003

  Add automatic detection of DRIVER= or DSN= to add user id and password
  to connect string.

=head2 Changes in DBD::ODBC 1.02 January 6, 2003

  Fix to call finish() automatically if execute is re-called in a loop
  (and test in t/02simple.t to ensure it's fixed)

  Augmented error message when longs are truncated to help users determine
  where to look for help.

  Fixes for build under Win32 with Perl5.8.

=head2 Changes in DBD::ODBC 1.01 December 9, 2002

  Forgot to fix require DBI 1.201 in ODBC.pm to work for perl 5.8.  Fixed

=head2 Changes in DBD::ODBC 1.00 December 8, 2002

  (Please see all changes since version 0.43)

  Updated Makefile.PL to handle SQL_Wxxx types correctly with unixODBC and
  linking directly with EasySoft OOB.  Note that I could not find where
  iODBC defines SQL_WLONG_VARCHAR, so I'm not sure it's fixed on all other
  platforms.  Should not have been a problem under Win32...

  Found that the fix in _18 was only enabled if debug enabled and it broke
  something else.  removed the fix.

  Updated Makefile.PL to use DBI version 1.21 instead of 1.201 to
  facilitate builds under latest development versions of Perl.

  Updated code to use the *greater* of the column display size and the
  column length for allocating column buffers.  This *should* workaround a
  problem with DBD::ODBC and the Universe database.

  Added code thanks to Michael Riber to handle SQLExecDirect instead of
  SQLPrepare.  There are two ways to get this:
	$dbh->prepare($sql, { odbc_execdirect => 1});
    and
	$dbh->{odbc_execdirect} = 1;

  When $dbh->prepare() is called with the attribute "ExecDirect" set to a
  non-zero value dbd_st_prepare do NOT call SQLPrepare, but set the sth
  flag odbc_exec_direct to 1.

  Fixed numeric value binding when binding non-integral values.  Now lets
  the driver or the database handle the conversion.

  Fixed makefile.pl generation of makefile to force the ODBC directory
  first in the include list to help those installing ODBC driver managers
  on systems which already have ODBC drivers in their standard include
  path.

=head2 Changes in DBD::ODBC 0.45_18 September 26, 2002

  Updated MANIFEST to include more of the mytest/* files (examples, tests)

  Fixed problem when attempting to get NUM_OF_FIELDS after execute returns
  no rows/columns.

=head2 Changes in DBD::ODBC 0.45_17 August 26, 2002

  More fixes for multiple result sets.  Needed to clear the DBIc_FIELDS_AV
  when re-executing the multiple-result set stored procedure/query.

=head2 Changes in DBD::ODBC 0.45_16 August 26, 2002

  Updated to fix output parameters with multiple result sets.  The output
  parameters are not set until the last result set has been retrieved.

=head2 Changes in DBD::ODBC 0.45_15 August 20, 2002

  Updated for new DBIc_STATE macros (all debug, as it turned out) to be
  thread safer in the long run

  Updated for the new DBIc_LOGFP macros

  Added CLONE method

  Fix for SQL Server where multiple result sets being returned from a
  stored proc, where one of the result sets was empty (insert/update).

  Added new attribute odbc_force_rebind, which forces DBD::ODBC to check
  recheck for new result sets every execute.  This is only really
  necessary if you have a stored procedure which returns different result
  sets with each execute, given the same "prepare".  Many times this will
  be automatically set by DBD::ODBC, however, if there is only one result
  set in the stored proc, but it can differ with each call, then DBD::ODBC
  will not know to set it.

  Updated the DBD::ODBC POD documentation to document DBD::ODBC private
  attributes and usage.

=head2 Changes in DBD::ODBC 0.45_14 August 13, 2002

  Added support to handle (better) DBI begin_work().

  Fix for binding undef parameters on SQL Server.

  Fix bug when connecting twice in the same script.  Trying to set the
  environment ODBC version twice against the same henv caused an error.

=head2 Changes in DBD::ODBC 0.45_13 August 9, 2002

  Workaround problem with iODBC where SQLAllocHandleStd is not present in
  iODBC.

  Made Changes file accessible via perldoc DBD::ODBC::Changes.  In the
  near future the change log will be removed from here and put in changes
  to tidy up a bit.

=head2 Changes in DBD::ODBC 0.45_12 August 9, 2002

  Fixed global destruction access violation (which was seemingly random).

=head2 Changes in DBD::ODBC 0.45_11 August 8, 2002

  Updated manifest to include more samples.

  Working on checking for leaks on Linux, where I might get more
  information about the process memory.

  Working on fixing problems with MS SQL Server binding parameters.  It
  seems that SQLServer gets "confused" if you bind a NULL first.  In
  "older" (SQLServer 2000 initial release) versions of the driver, it
  would truncate char fields.  In "newer" versions of the SQL Server
  driver, it seems to only truncate dates (actually, round them to the
  nearest minute).  If you have problems in the SQL Server tests, please
  upgrade your driver to the latest version on Microsoft's website (MDAC
  2.7 or above) http://www.microsoft.com/data

=head2 Changes in DBD::ODBC 0.45_10 July 30, 2002

  Added database specific tests to ensure things are working.  Some of the
  tests may not work for all people or may not be desirable.  I have tried
  to keep them as safe as possible, but if they don't work, please let me
  know.

  Added support for the internal function GetFunctions to handle ODBC 3's
  SQL_API_ODBC3_ALL_FUNCTIONS.  Would have caused a memory overwrite on
  the stack if it was called.

=head2 Changes in DBD::ODBC 0.45_9 July 30, 2002

  Fixed bug in procedure handling for SQLServer.  Was not re-describing
  the result sets if the SQLMoreResults in the execute needs to be called.

=head2 Changes in DBD::ODBC 0.45_8 July 25, 2002

  Fixed bug in tracing code when binding an undef parameter which did not
  happen to have a valid buffer with tracing level >= 2

  Fixed bug when binding undef after a valid data bind on a timestamp.
  The Scale value was being calculated based upon the string that had been
  bound prior to the bind of the undef and if that had a sub-second value,
  then the scale would be set to the wrong value...I.e.
    bind_param(1, '2000-05-17 00:01:00.250', SQL_TYPE_TIMESTAMP) then
    execute
    bind_param(1, undef, SQL_TYPE_TIMESTAMP) then

  Fixed SQL Server issue when binding a null and the length was set to 0
  instead of 1

=head2 Changes in DBD::ODBC 0.45_7 July 25, 2002

  Adding support for array binding, but not finished.

  Fixed bug where SqlServer Stored procedures which perform INSERT would
  not correctly return a result set.  Thanks to Joe Tebelskis for finding
  it and Martin Evans for supplying a fix.

  Fixed bug where binding the empty string would cuase a problem.  Fixed
  and added test in t/07bind.t.

=head2 Changes in DBD::ODBC 0.45_6 July 24, 2002

  Added support for new DBI ParamValues feature.

=head2 Changes in DBD::ODBC 0.45_5 July 23, 2002

  Added odbc_err_handler and odbc_async_exec thanks to patches by David
  L. Good.  See example in mytest/testerrhandler.pl

  Here's the notes about it:

    I've implemented two separate functions.  The first is an "error
    handler" similar to that in DBD::Sybase.  The error handler can be
    used to intercept error and status messages from the server.  It is
    the only way (at least currently) that you can retrieve non-error
    status messages when execution is successful.

    To use the error handler, set the "odbc_err_handler" attribute on your
    database handle to a reference to a subroutine that will act as the
    error handler.  This subroutine will be passed two args, the SQLSTATE
    and the error message.  If the subroutine returns 0, the error message
    will be otherwise ignored.  If it returns non-zero, the error message
    will be processed normally.

    The second function implemented is asynchronous execution.  It's only
    useful for retrieving server messages with an error handler during an
    execute() that takes a long time (such as a DBCC on a large database)
    ODBC doesn't have the concept of a callback routine like Sybase's
    DBlib/CTlib does, so asynchronous execution is needed to be able to
    get the server messages before the SQL statement is done processing.

   To use asynchronous execution, set the "odbc_async_exec" attribute on
   your database handle to 1.  Not all ODBC drivers support asynchronous
   execution.  To see if yours does, set odbc_async_exec to 1 and then
   check it's value.  If the value is 1, your ODBC driver can do
   asynchronous execution.  If the value is 0, your ODBC driver cannot.

=head2 Changes in DBD::ODBC 0.45_4 July 22, 2002

  More fixes for DB2 tests and timestamp handling.

=head2 Changes in DBD::ODBC 0.45_3 July 22, 2002

  Changes to internal timestamp type handling and test structure to ensure
  tests work for all platforms.  DB2 was giving me fits due to bad
  assumptions.  Thanks to Martin Evans (again) for help in identifying the
  problems and helping research solutions.  This includes the
  scale/precision values to correctly store full timestamps.

=head2 Changes in DBD::ODBC 0.45_2 July 19, 2002

  Moving API usage to ODBC 3.0 specifications.  With lots of help from
  Martin Evans (again!).  Thanks Martin!!!!!

=head2 Changes in DBD::ODBC 0.44 July 18, 2002

  .44 was never officially released.

  Fix for do() and execute to handle DB2 correctly.  Patch/discovery
  thanks to Martin Evans.

  Partly moving towards defaulting to ODBC 3.x standards.

=head2 Changes in DBD::ODBC 0.43 July 18, 2002

  Fix for FoxPro (and potentially other) Drivers!!!!!

  Add support for DBI column_info

  Fix for binding undef value which comes from dereferencing hash

  Fix to make all bound columns word (int) aligned in the buffer.

=head2 Changes in DBD::ODBC 0.42 July 8, 2002

  Added patches to the tests to support ActiveState's automated build
  process.

  Fix ping() to try SQLTables for a test, instead of a strange query.

=head2 Changes in DBD::ODBC 0.41 April 15, 2002

  Fixed problem where SQLDescribeParam would fail (probably bug in ODBC
  driver).  Now reverts to SQL_VARCHAR if that happens, instead of failing
  the query.

  Fixed error report when using Oracle's driver.  There is a known
  problem.  Left the error on the test, but added warning indicating it's
  a known problem.

=head2 Changes in DBD::ODBC 0.40 April 12, 2002

  Most significant change is the change in the default binding type which
  forces DBD::ODBC to attempt to determine the bind type if one is not
  passed.  I decided to make this the default behaviour to make things as
  simple as possible.

  Fixed connection code put in 0.39 to work correctly.

  Two minor patches for building, one for Cygwin one if both iODBC and
  unixODBC libraries are installed.  Probably need better command line on
  this, but if someone has the problem, please let me know (and hopefully
  send a patch with it).

=head2 Changes in DBD::ODBC 0.39 March 12, 2002

  See mytest/longbin.pl for demonstration of inserting and retrieving long
  binary files to/from the db.  Uses MD5 algorithm to verify data.  Please
  do some similar test(s) with your database before using it in
  production.  The various bind types are different for each database!

  Finally removed distribution of old iODBC.  See www.iodbc.org or
  www.unixodbc.org for newer/better versions of the ODBC driver manager
  for Unix (and others?).

  Added ability to force ODBC environment version.

  Fix to SQLColAttributes.

  Changes to connect sequence to provide better error messages for those
  using DSN-less connections.

=head2 Changes in DBD::ODBC 0.38 February 12, 2002

  Fixed do function (again) thanks to work by Martin Evans.

=head2 Changes in DBD::ODBC 0.37 February 10, 2002

  Patches for get_info where return type is string.  Patches thanks to
  Steffen Goldner.  Thanks Steffen!

  Patched get_info to NOT attempt to get data for SQL_DRIVER_HSTMT and
  SQL_DRIVER_HDESC as they expect data in and have limited value (IMHO).

  Further fixed build for ODBC 2.x drivers.  The new SQLExecDirect code
  had SQLAllocHandle which is a 3.x function, not a 2.x function.  Sigh.
  I should have caught that the first time.  Signed, the Mad-and-
  not-thorough-enough-patcher.

  Additionally, a random core dump occurred in the tests, based upon the
  new SQLExecDirect code.  This has been fixed.

=head2 Changes in DBD::ODBC 0.36 February 10, 2002

  Fixed build for ODBC 2.x drivers.  The new SQLExecDirect code had
  SQLFreeHandle which is a 3.x function, not a 2.x function.

=head2 Changes in DBD::ODBC 0.35 February 9, 2002

  Fixed (finally) multiple result sets with differing numbers of columns.
  The final fix was to call SQLFreeStmt(SQL_UNBIND) before repreparing the
  statement for the next query.

  Added more to the multi-statement tests to ensure the data retrieved was
  what was expected.

  Now, DBD::ODBC overrides DBI's do to call SQLExecDirect for simple
  statements (those without parameters).  Please advise if you run into
  problems.  Hopefully, this will provide some small speed improvement for
  simple "do" statements.  You can also call $dbh->func($stmt,
  ExecDirect).  I'm not sure this has great value unless you need to
  ensure SQLExecDirect is being called.  Patches thanks to Merijn Broeren.
  Thanks Merijn!

=head2 Changes in DBD::ODBC 0.34 February 7, 2002

  Further revamped tests to attempt to determine if SQLDescribeParam will
  work to handle the binding types.  The t/08bind.t attempts to determine
  if SQLDescribeParam is supported.  note that Oracle's ODBC driver under
  NT doesn't work correctly when binding dates using the ODBC date
  formatting {d } or {ts }.  So, test #3 will fail in t/08bind.t

  New support for primary_key_info thanks to patches by Martin Evans.

  New support for catalog, schema, table and table_type in table_info
  thanks to Martin Evans.  Thanks Martin for your work and your continuing
  testing, suggestions and general support!

  Support for upcoming dbi get_info.

=head2 Changes in DBD::ODBC 0.33_3 February 4, 2002

  Revamped tests to include tests for multiple result sets.  The tests are
  ODBC driver platform specific and will be skipped for drivers which do
  not support multiple result sets.

=head2 Changes in DBD::ODBC 0.33_2 February 4, 2002

  Finally tested new binding techniques with SQL Server 2000, but there is
  a nice little bug in their MDAC and ODBC drivers according to the
  knowledge base article # Q273813, titled

     "FIX: "Incorrect Syntax near the Keyword 'by' "
     Error Message with Column Names of "C", "CA" or "CAS" (Q273813)

  DBD::ODBC now does not name any of the columns A, B, C, or D they are
  now COL_A, COL_B, COL_C, COL_D.

   *** NOTE: *** I AM STRONGLY CONSIDERING MAKING THE NEW BINDING the
   default for future versions.  I do not believe it will break much
   existing code (if any) as anyone binding to non VARCHAR (without the
   ODBC driver doing a good conversion from the VARCHAR) will have a
   problem.  It may be subtle, however, since much code will work, but
   say, binding dates may not with some drivers.

   Please comment soon...

=head2 Changes in DBD::ODBC 0.33_1 February 4, 2002

  *** WARNING: ***

   Changes to the binding code to allow the use of SQLDescribeParam to
   determine if the type of column being bound.  This is experimental and
   activated by setting

    $dbh->{odbc_default_bind_type} = 0; # before creating the query...

  Currently the default value of odbc_default_bind_type = SQL_VARCHAR
  which mimicks the current behaviour.  If you set odbc_default_bind_type
  to 0, then SQLDescribeParam will be called to determine the columen
  type.  Not ALL databases handle this correctly.  For example, Oracle
  returns SQL_VARCHAR for all types and attempts to convert to the correct
  type for us.  However, if you use the ODBC escaped date/time format such
  as: {ts '1998-05-13 00:01:00'} then Oracle complains.  If you bind this
  with a SQL_TIMESTAMP type, however, Oracle's ODBC driver will parse the
  time/date correctly.  Use at your own risk!

  Fix to dbdimp.c to allow quoted identifiers to begin/end with either "
  or '.  The following will not be treated as if they have a bind
  placeholder:

     "isEstimated?"
     '01-JAN-1987 00:00:00'
     'Does anyone insert a ?'

=head2 Changes in DBD::ODBC 0.32 January 22, 2002

  More SAP patches to Makfile.PL to eliminate the call to Data Sources

  A patch to the test (for SAP and potentially others), to allow fallback
  to SQL_TYPE_DATE in the tests

=head2 Changes in DBD::ODBC 0.31 January 18, 2002

  Added SAP patches to build directly against SAP driver instead of driver
  manager thanks to Flemming Frandsen (thanks!)

  Added support to fix ping for Oracle8.  May break other databases, so
  please report this as soon as possible.  The downside is that we need to
  actually execute the dummy query.

=head2 Changes in DBD::ODBC 0.30 January 8, 2002

  Added ping patch for Solid courtesy of Marko Asplund

  Updated disconnect to rollback if autocommit is not on.  This should
  silence some errors when disconnecting.

  Updated SQL_ROWSET_SIZE attribute.  Needed to force it to
  odbc_SQL_ROWSET_SIZE to obey the DBI rules.

  Added odbc_SQL_DRIVER_ODBC_VER, which obtains the version of the Driver
  upon connect.  This internal capture of the version is a read-only
  attribute and is used during array binding of parameters.

  Added odbc_ignore_named_placeholders attribute to facilicate creating
  triggers within SAPDB and Oracle, to name two. The syntax in these DBs
  is to allow use of :old and :new to access column values before and
  after updates.  Example:

   $dbh->{odbc_ignore_named_placeholders} = 1;
            # set it for all future statements
            # ignores :foo, :new, etc, but not :1 or ?
   $dbh->do("create or replace etc :new.D = sysdate etc");

=head2 Changes in DBD::ODBC 0.29 August 22, 2001

  Cygwin patches from Neil Lunn (untested by me).  Thanks Neil!

  SQL_ROWSET_SIZE attribute patch from Andrew Brown

    There are only 2 additional lines allowing for the setting of
    SQL_ROWSET_SIZE as db handle option.

   The purpose to my madness is simple. SqlServer (7 anyway) by default
   supports only one select statement at once (using std ODBC cursors).
   According to the SqlServer documentation you can alter the default
   setting of three values to force the use of server cursors - in which
   case multiple selects are possible.

   The code change allows for:
     $dbh->{SQL_ROWSET_SIZE} = 2;    # Any value > 1

   For this very purpose.

   The setting of SQL_ROWSET_SIZE only affects the extended fetch command
   as far as I can work out and thus setting this option shouldn't affect
   DBD::ODBC operations directly in any way.

   Andrew

  VMS and other patches from Martin Evans (thanks!)

    [1] a fix for Makefile.PL to build DBD::ODBC on OpenVMS.
    [2] fix trace message coredumping after SQLDriverConnect
    [3] fix call to SQLCancel which fails to pass the statement handle
    properly.
    [4] consume diagnostics after SQLDriverConnect/SQLConnect call or they
    remain until the next error occurs and it then looks confusing (this
    is due to ODBC spec for SQLError). e.g. test 02simple returns a data
    truncated error only now instead of all the informational diags that
    are left from the connect call, like the "database changed", "language
    changed" messages you get from MS SQL Server.

  Replaced C++ style comments with C style to support more platforms more
  easily.

  Fixed bug which use the single quote (') instead of a double quote (")
   for "literal" column names.  This helped when having a colon (:) in the
   column name.

  Fixed bug which would cause DBD::ODBC to core-dump (crash) if DBI
  tracing level was greater than 3.

  Fixed problem where ODBC.pm would have "use of uninitialized variable"
  if calling DBI's type_info.

  Fixed problem where ODBC.xs *may* have an overrun when calling
  SQLDataSources.

  Fixed problem with DBI 1.14, where fprintf was being called instead of
  PerlIO_printf for debug information

  Fixed problem building with unixODBC per patch from Nick Gorham

  Added ability to bind_param_inout() via patches from Jeremy Cooper.
  Haven't figured out a good, non-db specific way to test.  My current
  test platform attempts to determine the connected database type via ugly
  hacks and will test, if it thinks it can.  Feel free to patch and send
  me something...Also, my current Oracle ODBC driver fails miserably and
  dies.

  Updated t/02simple.t to not print an error, when there is not one.

=head2 Changes in DBD::ODBC 0.28 March 23, 2000

  Added support for SQLSpecialColumns thanks to patch provided by Martin
  J. Evans

  Fixed bug introduced in 0.26 which was introduced of SQLMoreResults was
  not supported by the driver.

=head2 Changes in DBD::ODBC 0.27 March 8, 2000

  Examined patch for ping method to repair problem reported by Chris
  Bezil.  Thanks Chris!

  Added simple test for ping method working which should identify this in
  the future.

=head2 Changes in DBD::ODBC 0.26 March 5, 2000

  Put in patch for returning only positive rowcounts from dbd_st_execute.
  The original patch was submitted by Jon Smirl and put back in by David
  Good.  Reasoning seems sound, so I put it back in.  However, any
  databases that return negative rowcounts for specific reasons, will no
  longer do so.

  Put in David Good's patch for multiple result sets.  Thanks David!  See
  mytest\moreresults.pl for an example of usage.

  Added readme.txt in iodbcsrc explaining an issue there with iODBC 2.50.3
  and C<data_sources>.

  Put in rudimentary cancel support via SQLCancel.  Call $sth->cencel to
  utilize.  However, it is largely untested by me, as I do not have a good
  sample for this yet.  It may come in handy with threaded perl, someday
  or it may work in a signal handler.

=head2 Changes in DBD::ODBC 0.25 March 4, 2000

  Added conditional compilation for SQL_WVARCHAR and SQL_WLONGVARCHAR.  If
  they are not defined by your driver manager, they will not be compiled
  in to the code.  If you would like to support these types on some
  platforms, you may be able to #define SQL_WVARCHAR (-9) #define
  SQL_WLONGVARCHAR (-10)

  Added more long tests with binding in t\09bind.t.  Note use of
  bind_param!

=head2 Changes in DBD::ODBC 0.24 February 24, 2000

  Fixed Test #13 in 02simple.t.  Would fail, improperly, if there was only
  one data source defined.

  Fixed (hopefully) SQL Server 7 and ntext type "Out of Memory!" errors
  via patch from Thomas Lowery.  Thanks Thomas!

  Added more support for Solid to handle the fact that it does not support
  data_sources nor SQLDriverConnect.  Patch supplied by Samuli Karkkainen
  [skarkkai@woods.iki.fi].  Thanks!  It's untested by me, however.

  Added some information from Adam Curtin about a bug in iodbc 2.50.3's
  data_sources.  See iodbcsrc\readme.txt.

  Added information in this pod from Stephen Arehart regarding DSNLess
  connections.

  Added fix for sp_prepare/sp_execute bug reported by Paul G. Weiss.

  Added some code for handling a hint on disconnect where the user gets an
  error for not committing.

=head2 Changes in DBD::ODBC 0.22 September 8, 1999

  Fixed for threaded perl builds.  Note that this was tested only on
  Win32, with no threads in use and using DBI 1.13.  Note, for
  ActiveState/PERL_OBJECT builds, DBI 1.13_01 is required as of 9/8/99.
  If you are using ActiveState's perl, this can be installed by using PPM.

=head2 Changes in DBD::ODBC 0.21

Thanks to all who provided patches!

  Added ability to connect to an ODBC source without prior creation of
  DSN. See mytest/contest.pl for example with MS Access.  (Also note that
  you will need documentation for your ODBC driver -- which, sadly, can be
  difficult to find).

  Fixed case sensitivity in tests.

  Hopefully fixed test #4 in t/09bind.t.  Updated it to insert the date
  column and updated it to find the right type of the column.  However, it
  doesn't seem to work on my Linux test machine, using the OpenLink
  drivers with MS-SQL Server (6.5).  It complains about binding the date
  time.  The same test works under Win32 with SQL Server 6.5, Oracle 8.0.3
  and MS Access 97 ODBC drivers.  Hmmph.

  Fixed some binary type issues (patches from Jon Smirl)

  Added SQLStatistics, SQLForeignKeys, SQLPrimaryKeys (patches from Jon
  Smirl) Thanks (again), Jon, for providing the build_results function to
  help reduce duplicate code!

  Worked on LongTruncOk for Openlink drivers.

  Note: those trying to bind variables need to remember that you should
  use the following syntax:

	use DBI;
	...
	$sth->bind_param(1, $str, DBI::SQL_LONGVARCHAR);

  Added support for unixodbc (per Nick Gorham)

  Added support for OpenLinks udbc (per Patrick van Kleef)

  Added Support for esodbc (per Martin Evans)

  Added Support for Easysoft (per Bob Kline)

  Changed table_info to produce a list of views, too.

  Fixed bug in SQLColumns call.

  Fixed blob handling via patches from Jochen Wiedmann.

  Added data_sources capability via snarfing code from DBD::Adabas (Jochen
  Wiedmann)

=head2 Changes in DBD::ODBC 0.20 August 14, 1998

  SQLColAttributes fixes for SQL Server and MySQL. Fixed tables method by
  renaming to new table_info method. Added new tyoe_info_all method.
  Improved Makefile.PL support for Adabase.

=head2 Changes in DBD::ODBC 0.19

  Added iODBC source code to distribution.Fall-back to using iODBC header
  files in some cases.

=head2 Changes in DBD::ODBC 0.18

  Enhancements to build process. Better handling of errors in error
  handling code.

=head2 Changes in DBD::ODBC 0.17

  This release is mostly due to the good work of Jeff Urlwin.  My eternal
  thanks to you Jeff.

  Fixed "SQLNumResultCols err" on joins and 'order by' with some drivers
  (see Microsoft Knowledge Base article #Q124899).  Thanks to Paul
  O'Fallon for that one.

  Added more (probably incomplete) support for unix ODBC in Makefile.PL

  Increased default SQL_COLUMN_DISPLAY_SIZE and SQL_COLUMN_LENGTH to 2000
  for drivers that don't provide a way to query them dynamically. Was 100!

  When fetch reaches the end-of-data it automatically frees the internal
  ODBC statement handle and marks the DBI statement handle as inactive
  (thus an explicit 'finish' is *not* required).

  Also:

  LongTruncOk for Oracle ODBC (where fbh->datalen < 0)

  Added tracing into SQLBindParameter (help diagnose oracle odbc bug)

  Fixed/worked around bug/result from Latest Oracle ODBC driver where in
  SQLColAttribute cbInfoValue was changed to 0 to indicate fDesc had a value

  Added work around for compiling w/ActiveState PRK (PERL_OBJECT)

  Updated tests to include date insert and type

  Added more "backup" SQL_xxx types for tests

  Updated bind test to test binding select

   NOTE: bind insert fails on Paradox driver (don't know why)

  Added support for: (see notes below)

   SQLGetInfo       via $dbh->func(xxx, GetInfo)
   SQLGetTypeInfo   via $dbh->func(xxx, GetTypeInfo)
   SQLDescribeCol   via $sth->func(colno, DescribeCol)
   SQLColAttributes via $sth->func(xxx, colno, ColAttributes)
   SQLGetFunctions  via $dbh->func(xxx, GetFunctions)
   SQLColumns       via $dbh->func(catalog, schema, table, column, 'columns')

  Fixed $DBI::err to reflect the real ODBC error code which is a 5 char
  code, not necessarily numeric.

  Fixed fetches when LongTruncOk == 1.

  Updated tests to pass more often (hopefully 100% <G>)

  Updated tests to test long reading, inserting and the LongTruncOk attribute.

  Updated tests to be less driver specific.

  They now rely upon SQLGetTypeInfo I<heavily> in order to create the
  tables.  The test use this function to "ask" the driver for the name of
  the SQL type to correctly create long, varchar, etc types.  For example,
  in Oracle the SQL_VARCHAR type is VARCHAR2, while MS Access uses TEXT
  for the SQL Name.  Again, in Oracle the SQL_LONGVARCHAR is LONG, while
  in Access it's MEMO.  The tests currently handle this correctly (at
  least with Access and Oracle, MS SQL server will be tested also).

=cut
