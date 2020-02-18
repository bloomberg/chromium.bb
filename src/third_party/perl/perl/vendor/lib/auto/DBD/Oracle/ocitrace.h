#ifndef DBD_OCI_TRACEON

/* OCI functions "wrapped" to produce tracefile dumps (may be handy when giving
	diagnostic info to Oracle Support, or just learning about OCI)
	Macros are named "_log" as a mnemonic that they log to the tracefile if needed
	Macros named "_log_stat" return status in last parameter.
*/

#define DBD_OCI_TRACEON(h) (DBIc_DBISTATE(h)->debug >= 6 || dbd_verbose >= 6)
#define DBD_OCI_TRACEFP(h) (DBIc_LOGPIO(h))
#define OciTp		("\tOCI")		/* OCI Trace Prefix */
#define OciTstr(s)	((s) ? (text*)(s) : (text*)"<NULL>")
#define ul_t(v)		((unsigned long)(v))
#define pul_t(v)	((unsigned long *)(v))
#define sl_t(v)		((signed long)(v))
#define psl_t(v)	((signed long *)(v))

/* XXX TO DO

	1.	Add parenthesis around all macro args. (or do item 4 below case-by-case)
	DMG: Partly done, sort of. At least the types all match the doc'd casts, anyway.

	2.	#define a set of OciTxxx macros for different types of parameters
	that would allow
	a: casting to be hidden
	b: casting easily changed on different platforms (64bit etc)
	c: mapping of some type values to strings,
	d: return pointed-to value instead of pointer where applicable

	How to output arguments that are handles to opaque entities (OCIEnv*, etc)?
	Output of pointer address is a quick n' dirty way of identifying
	any number of handles that may be allocated.... yuck...
	It sure would be nice to give something more mnemonic! (and meaningful!)
	XXX Turn pointers into variable names by adding a prefix letter and, where
	appropriate an &, thus: "...,&p%ld,...",
	If done well the log will read like a compilable program.
*/

#define OCIServerRelease_log_stat(impdbh,sc,errhp,b,bl,ht,ver,stat) \
	stat =OCIServerRelease(sc,errhp,b,bl,ht,ver);\
	(DBD_OCI_TRACEON(impdbh))                   \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),         \
				 "%sOCIServerRelease(%p)=%s\n",\
				 OciTp, sc,oci_status_name(stat)),stat \
	: stat

#define OCISessionRelease_log_stat(impdbh,svchp,errhp,tag,tagl,mode,stat)	\
	stat =OCISessionRelease(svchp, errhp, tag, tagl, mode);\
	(DBD_OCI_TRACEON(impdbh))                                       \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),                             \
						 "%sOCISessionRelease(svchp=%p,tag=\"%s\",mode=%u)=%s\n",\
						 OciTp, svchp,tag,mode,oci_status_name(stat)),stat	\
	: stat

#define OCISessionPoolDestroy_log_stat(impdbh, ph, errhp,stat )  \
	stat =OCISessionPoolDestroy(ph, errhp,OCI_DEFAULT);\
	(DBD_OCI_TRACEON(impdbh))                         \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),               \
					 "%sOCISessionPoolDestroy(ph=%p)=%s\n",\
					 OciTp, ph,oci_status_name(stat)),stat \
	: stat
#define OCISessionGet_log_stat(impdbh,envhp,errhp,sh,ah,pn,pnl,tag,tagl,rettag,rettagl,found,stat) \
	stat =OCISessionGet(envhp, errhp, sh, ah,pn,pnl,tag,tagl,rettag,rettagl,found, OCI_SESSGET_SPOOL);\
	(DBD_OCI_TRACEON(impdbh))                                          \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),                           \
					 "%sOCISessionGet(envhp=%p,sh=%p,ah=%p,pn=%p,pnl=%d,tag=\"%s\",found=%d)=%s\n",\
					 OciTp, envhp,sh,ah,pn,pnl,tag,*found,oci_status_name(stat)),stat \
	: stat

#define OCISessionPoolCreate_log_stat(impdbh,envhp,errhp,ph,pn,pnl,dbn,dbl,sn,sm,si,un,unl,pw,pwl,mode,stat) \
    stat =OCISessionPoolCreate(envhp,errhp,ph,pn,pnl,dbn,dbl,sn,sm,si,un,unl,pw,pwl,mode);\
    (DBD_OCI_TRACEON(impdbh))                                          \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),                           \
					 "%sOCISessionPoolCreate(envhp=%p,ph=%p,pn=%p,pnl=%p,min=%d,max=%d,incr=%d, un=%s,unl=%lu,pw=%s,pwl=%lu,mode=%u)=%s\n",\
					 OciTp, envhp,ph,pn,pnl,sn,sm,si,un,(unsigned long)unl,pw,(unsigned long)pwl,mode,oci_status_name(stat)),stat \
	: stat

#if defined(ORA_OCI_102)
#define OCIPing_log_stat(impdbh,sc,errhp,stat)  \
	stat =OCIPing(sc,errhp,OCI_DEFAULT);\
	(DBD_OCI_TRACEON(impdbh))                  \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),   \
				 "%sOCIPing(%p)=%s\n",\
				 OciTp, sc,oci_status_name(stat)),stat \
	: stat
#endif

#define OCIServerVersion_log_stat(impdbh,sc,errhp,b,bl,ht,stat) \
	stat =OCIServerVersion(sc,errhp,b,bl,ht);\
	(DBD_OCI_TRACEON(impdbh))               \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),               \
				 "%sOCIServerVersion_log_stat(%p,%s)=%s\n",\
				 OciTp, sc,b,oci_status_name(stat)),stat \
	: stat

#define OCIStmtGetPieceInfo_log_stat(impsth,stmhp,errhp,hdlptr,hdltyp,in_out,iter,idx,piece,stat) \
	stat =OCIStmtGetPieceInfo(stmhp,errhp,hdlptr,hdltyp,in_out,iter,idx,piece);\
	(DBD_OCI_TRACEON(impsth))                                          \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impsth),                           \
				"%sOCIStmtGetPieceInfo_log_stat(%p,%p,%u)=%s\n",\
				OciTp, (void*)errhp,fbh,*piece,oci_status_name(stat)),stat \
	: stat


#define OCIStmtSetPieceInfo_log_stat(impsth,ptr,errhp,buf,blen,p,indp,rc,stat) \
	stat =OCIStmtSetPieceInfo(ptr,OCI_HTYPE_DEFINE,errhp, buf, blen, p,indp,rc);\
	(DBD_OCI_TRACEON(impsth))                                          \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impsth),                           \
				 "%sOCIStmtSetPieceInfo_log_stat(%p,%p,%d,%p)=%s\n",\
				 OciTp, (void*)errhp,fbh,piece,blen,oci_status_name(stat)),stat \
	: stat


#define OCIDefineDynamic_log_stat(impsth,defnp,errhp,fbh,stat)          \
	stat =OCIDefineDynamic(defnp,errhp,fbh,(OCICallbackDefine) presist_lob_fetch_cbk );\
	(DBD_OCI_TRACEON(impsth))                                          \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impsth),                           \
				 "%sOCIDefineDynamic_log_stat(%p,%p,%p)=%s\n",\
				 OciTp, (void*)defnp, (void*)errhp,fbh,oci_status_name(stat)),stat \
	: stat

#define OCIXMLTypeCreateFromSrc_log_stat(impdbh,svchp,errhp,duration,src_type,src_ptr,ind,xml,stat) \
	stat =OCIXMLTypeCreateFromSrc (svchp,errhp,duration,(ub1)src_type,(dvoid *)src_ptr,(sb4)ind, xml);\
	(DBD_OCI_TRACEON(impdbh))                                          \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),                           \
				 "%sOCIXMLTypeCreateFromSrc_log_stat(%p,%p,%d,%d,%p,%d,%p)=%s\n",\
                    OciTp,	(void*)svchp,(void*)errhp, duration, src_type, src_ptr, ind, xml, oci_status_name(stat)),stat \
	: stat

#define OCILobFileIsOpen_log_stat(impdbh,envhp,errhp,loc,is_open,stat)  \
    stat = OCILobFileIsOpen(envhp,errhp,loc,is_open);\
    (DBD_OCI_TRACEON(impdbh))                       \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),                           \
                 "%sOCILobFileIsOpen_log_stat(%p,%p,%p,%p,%d)=%s\n",\
                            OciTp, (void*)envhp, (void*)errhp, loc, is_open, *is_open,oci_status_name(stat)),stat : stat

#define OCILobLocatorIsInit_log_stat(impdbh,envhp,errhp,loc,is_initp,stat) \
	stat =OCILobLocatorIsInit (envhp,errhp,loc,is_initp );\
	(DBD_OCI_TRACEON(impdbh))                            \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh),                           \
				 "%sOCILobLocatorIsInit_log_stat(%p,%p,%p,%d)=%s\n",\
				 OciTp, (void*)envhp, (void*)errhp,loc,*is_initp,oci_status_name(stat)),stat \
	: stat

#define OCIObjectPin_log_stat(impsth,envhp,errhp,or,ot,stat)            \
	stat = OCIObjectPin(envhp,errhp,or,(OCIComplexObject *)0,OCI_PIN_LATEST,OCI_DURATION_TRANS,OCI_LOCK_NONE,ot);\
	(DBD_OCI_TRACEON(impsth))                                          \
    ?	PerlIO_printf(DBD_OCI_TRACEFP(impsth),                         \
				 "%sObjectPin_log_stat(%p,%p,%p,%p)=%s\n",\
				 OciTp, (void*)envhp, (void*)errhp,or,ot,oci_status_name(stat)),stat \
	: stat

/*
#define OCICollGetElem_log_stat(envhp,errhp,v,i,ex,e,ne,stat)\
	stat = OCICollGetElem(envhp,errhp, v,i,ex,e,ne);\
	(DBD_OCI_TRACEON) \
			?	PerlIO_printf(DBD_OCI_TRACEFP,\
				 "%sOCICollGetElem_log_stat(%p,%p,%d,%d,%d,%d,%d)=%s\n",\
				 OciTp, (void*)envhp, (void*)errhp,v,i,ex,e,ne,oci_status_name(stat)),stat \
	: stat
*/
/*
#define OCITableFirst_log_stat(envhp,errhp,v,i,stat)\
	stat = OCITableFirst(envhp,errhp,v,i);\
	(DBD_OCI_TRACEON) \
			?	PerlIO_printf(DBD_OCI_TRACEFP,\
				 "%sOCITableFirst_log_stat(%p,%p,%d,%d)=%s\n",\
				 OciTp, (void*)envhp, (void*)errhp,v,i,oci_status_name(stat)),stat \
	: stat
*/
#define OCIObjectGetAttr_log_stat(impsth,envhp,errhp,instance,nullstruct,tdo,names,lengths,namecount,indexes,indexcount,attrnullstatus,attrnullstruct,attrvalue, attrtdo, stat) \
	stat = OCIObjectGetAttr(envhp,errhp,instance,nullstruct,tdo,names,lengths,namecount,indexes,indexcount,attrnullstatus,attrnullstruct,attrvalue,attrtdo); \
	(DBD_OCI_TRACEON(impsth))                                          \
    ?	PerlIO_printf(DBD_OCI_TRACEFP(impsth),                         \
				 "%sOCIObjectGetAttr_log_stat(%p,%p,%p,%p,%p,%p,%p,%d,%p,%d,%p,%p,%p,%p)=%s\n",\
				 OciTp, (void*)envhp,(void*)errhp,instance,nullstruct,tdo,names,lengths,namecount,indexes,indexcount,attrnullstatus,attrnullstruct,attrvalue,attrtdo,oci_status_name(stat)),stat \
	: stat



#define OCIIntervalToText_log_stat(impsth,envhp,errhp,di,sb,ln,sl,stat) \
	stat = OCIIntervalToText(envhp,errhp, *(OCIInterval**)di,3,3,sb,ln,sl);\
	(DBD_OCI_TRACEON(impsth))                                          \
    ?	PerlIO_printf(DBD_OCI_TRACEFP(impsth),                         \
				"%sOCIIntervalToText(%p,%p,%p,%s)=%s\n",\
				OciTp, (void*)errhp, di,sl,sb,oci_status_name(stat)),stat \
	: stat

#define OCIDateTimeToText_log_stat(impsth,envhp,errhp,d,sl,sb,stat)     \
	stat = OCIDateTimeToText(envhp,errhp, *(OCIDateTime**)d,(CONST text*) 0,(ub1) 0,6, (CONST text*) 0, (ub4) 0,(ub4 *)sl,sb );\
	(DBD_OCI_TRACEON(impsth))                                          \
    ?	PerlIO_printf(DBD_OCI_TRACEFP(impsth),                         \
				 "%sOCIDateTimeToText(%p,%p,%p,%s)=%s\n",\
				 OciTp, (void*)errhp, d,sl,sb,oci_status_name(stat)),stat \
	: stat


#define OCIDateToText_log_stat(impsth,errhp,d,sl,sb,stat)               \
	stat = OCIDateToText(errhp, (CONST OCIDate *) d,(CONST text*) 0,(ub1) 0, (CONST text*) 0, (ub4) 0,(ub4 *)sl,sb );\
	(DBD_OCI_TRACEON(impsth))                                          \
    ?	PerlIO_printf(DBD_OCI_TRACEFP(impsth),                         \
				 "%sDateToText_log_stat(%p,%p,%p,%s)=%s\n",\
				 OciTp, (void*)errhp, d,sl,sb,oci_status_name(stat)),stat \
	: stat


#define OCIIterDelete_log_stat(impsth,envhp,errhp,itr,stat) \
	stat = OCIIterDelete(envhp,errhp,itr );\
	(DBD_OCI_TRACEON(impsth))                  \
    ?	PerlIO_printf(DBD_OCI_TRACEFP(impsth),             \
				 "%sOCIIterDelete_log_stat(%p,%p,%p)=%s\n",\
				 OciTp, (void*)envhp, (void*)errhp,itr,oci_status_name(stat)),stat \
	: stat


#define OCIIterCreate_log_stat(impsth,envhp,errhp,coll,itr,stat)    \
	stat = OCIIterCreate(envhp,errhp,coll,itr);\
	(DBD_OCI_TRACEON(impsth))                 \
    ?	PerlIO_printf(DBD_OCI_TRACEFP(impsth),             \
				 "%sIterCreate_log_stat(%p,%p,%p)=%s\n",\
				 OciTp, (void*)envhp, (void*)errhp,(void*)coll,oci_status_name(stat)),stat \
	: stat
/*
#define OCICollSize_log_stat(envhp,errhp,coll,coll_siz,stat)\
	stat = OCICollSize(envhp,errhp,(CONST OCIColl *)coll,coll_siz);\
	(DBD_OCI_TRACEON) \
			?	PerlIO_printf(DBD_OCI_TRACEFP,\
				 "%sOCICollSize_log_stat(%p,%p,%d)=%s\n",\
				 OciTp, (void*)envhp, (void*)errhp,oci_status_name(stat)),stat \
	: stat
*/
#define OCIDefineObject_log_stat(impsth,defnp,errhp,tdo,eo_buff,eo_ind,stat) \
	stat = OCIDefineObject(defnp,errhp,tdo,eo_buff,0,eo_ind, 0);\
	(DBD_OCI_TRACEON(impsth))                                  \
    ?	PerlIO_printf(DBD_OCI_TRACEFP(impsth),                 \
				 "%sOCIDefineObject(%p,%p,%p)=%s\n",\
			 OciTp, (void*)defnp, (void*)errhp, (void*)tdo,oci_status_name(stat)),stat \
	: stat

#define OCITypeByName_log(impsth,envhp,errhp,svchp,sn,snl,tn,tnl,vn,vnl,duration,option,tdo,stat) \
	stat = OCITypeByName(envhp,errhp,svchp,sn,snl,tn,tnl,vn,vnl,duration,option,tdo); \
	(DBD_OCI_TRACEON(impsth)) \
    ? PerlIO_printf(DBD_OCI_TRACEFP(impsth),                      \
                    "%sTypeByName(%p,%p,%p,%s,%d,%s,%d,\"\",0,%d,%d,%p)=%s\n", \
                    OciTp, (void*)envhp, (void*)errhp, (void*)svchp, sn,snl,tn,tnl,duration,option,tdo,oci_status_name(stat)), stat \
        :stat

#define OCITypeByRef_log_stat(impsth,envhp,errhp,ref,tdo,stat)          \
	stat = OCITypeByRef(envhp,errhp,ref,OCI_DURATION_TRANS,OCI_TYPEGET_ALL,tdo);\
	(DBD_OCI_TRACEON(impsth))                                          \
    ?	PerlIO_printf(DBD_OCI_TRACEFP(impsth),                         \
			 "%sTypeByRef(%p,%p,%p)=%s\n",\
			 OciTp, (void*)envhp, (void*)errhp, (void*)ref,oci_status_name(stat)),stat \
	: stat

/* added by lab */
#define OCILobCharSetId_log_stat(impxxh, envhp, errhp, locp, csidp, stat ) \
	stat = OCILobCharSetId( envhp, errhp, locp, csidp ); \
	(DBD_OCI_TRACEON(impxxh))                           \
	?	PerlIO_printf(DBD_OCI_TRACEFP(impxxh),          \
		 "%sLobCharSetId(%p,%p,%p,%d)=%s\n",\
		 OciTp, (void*)envhp, (void*)errhp, (void*)locp, *csidp, oci_status_name(stat)),stat \
	: stat

/* added by lab */
#define OCILobCharSetForm_log_stat(impxxh, envhp, errhp, locp, formp, stat ) \
	stat = OCILobCharSetForm( envhp, errhp, locp, formp ); \
	(DBD_OCI_TRACEON(impxxh))                             \
	?	PerlIO_printf(DBD_OCI_TRACEFP(impxxh),            \
		 "%sLobCharSetForm(%p,%p,%p,%d)=%s\n",\
		 OciTp, (void*)envhp, (void*)errhp, (void*)locp, *formp, oci_status_name(stat)),stat \
	: stat

/* added by lab */
#define OCINlsEnvironmentVariableGet_log_stat(impdbh, valp, size, item, charset, rsizep ,stat ) \
	stat = OCINlsEnvironmentVariableGet(	valp, size, item, charset, rsizep ); \
	(DBD_OCI_TRACEON(impdbh))                                          \
	?	PerlIO_printf(DBD_OCI_TRACEFP(impdbh),                         \
		 "%sNlsEnvironmentVariableGet(%d,%lu,%d,%d,%lu)=%s\n",\
		 OciTp, *valp, (unsigned long)size, item, charset, (unsigned long)*rsizep, oci_status_name(stat)),stat \
	: stat

/* added by lab */
#define OCIEnvNlsCreate_log_stat(impdbh, envp, mode, ctxp, f1, f2, f3, sz, usremepp ,chset, nchset ,stat ) \
	stat = OCIEnvNlsCreate(envp, mode, ctxp, f1, f2, f3, sz, usremepp ,chset, nchset ); \
	(DBD_OCI_TRACEON(impdbh))                                          \
	?	PerlIO_printf(DBD_OCI_TRACEFP(impdbh),                         \
		 "%sEnvNlsEnvCreate(%p,%s,%d,%d,%p,%p,%p,%d,%p,%d,%d)=%s\n", \
		 OciTp, (void*)envp, oci_mode(mode),mode, ctxp, (void*)f1, (void*)f2, (void*)f3, sz, (void*)usremepp ,chset, nchset, oci_status_name(stat)),stat \
	: stat


#define OCIAttrGet_log_stat(impxxh, th,ht,ah,sp,at,eh,stat)  \
	stat = OCIAttrGet(th,ht,ah,sp,at,eh);				\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sAttrGet(%p,%s,%p,%p,%s,%p)=%s\n",			\
		OciTp, (void*)th,oci_hdtype_name(ht),(void*)ah,pul_t(sp),oci_attr_name(at),(void*)eh,\
		oci_status_name(stat)),stat : stat

#define OCIAttrGet_d_log_stat(impsth, th,ht,ah,sp,at,eh,stat)    \
	stat = OCIAttrGet(th,ht,ah,sp,at,eh);				\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sAttrGet(%p,%s,%p,%p,%s,%p)=%s\n",			\
		OciTp, (void*)th,oci_hdtype_name(ht),(void*)ah,pul_t(sp),oci_dtype_attr_name(at),(void*)eh,\
		oci_status_name(stat)),stat : stat

 #define OCIAttrGet_parmap(imp_sth,dh, ht, p1, l, stat)				\
        OCIAttrGet_log_stat(imp_sth, dh, ht,                                \
		(void*)(p1), (l), OCI_ATTR_PARAM, imp_sth->errhp, stat)


#define OCIAttrGet_parmdp(imp_sth, parmdp, p1, l, a, stat)				\
	OCIAttrGet_d_log_stat(imp_sth, parmdp, OCI_DTYPE_PARAM,              \
		(void*)(p1), (l), (a), imp_sth->errhp, stat)

#define OCIAttrGet_stmhp_stat(imp_sth, p1, l, a, stat)					\
	OCIAttrGet_log_stat(imp_sth, imp_sth->stmhp, OCI_HTYPE_STMT,         \
		(void*)(p1), (l), (a), imp_sth->errhp, stat)

#define OCIAttrGet_stmhp_stat2(imp_sth, stmhp, p1, l, a, stat)              \
	OCIAttrGet_log_stat(imp_sth, stmhp, OCI_HTYPE_STMT,         \
		(void*)(p1), (l), (a), imp_sth->errhp, stat)

#define OCIAttrSet_log_stat(impxxh,th,ht,ah,s1,a,eh,stat)   \
	stat=OCIAttrSet(th,ht,ah,s1,a,eh);				\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sAttrSet(%p,%s, %p,%lu,Attr=%s,%p)=%s\n",			\
		OciTp, (void*)th,oci_hdtype_name(ht),(void *)ah,ul_t(s1),oci_attr_name(a),(void*)eh,	\
		oci_status_name(stat)),stat : stat

#define OCIBindByName_log_stat(impsth,sh,bp,eh,p1,pl,v,vs,dt,in,al,rc,mx,cu,md,stat) \
	stat=OCIBindByName(sh,bp,eh,p1,pl,v,vs,dt,in,al,rc,mx,cu,md);	\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sBindByName(%p,%p,%p,\"%s\",placeh_len=%ld,value_p=%p,value_sz=%ld," \
		"dty=%u,indp=%p,alenp=%p,rcodep=%p,maxarr_len=%lu,curelep=%p (*=%d),mode=%s,%lu)=%s\n",\
 		OciTp, (void*)sh,(void*)bp,(void*)eh,p1,sl_t(pl),(void*)(v),	\
		sl_t(vs),(ub2)(dt),(void*)(in),(ub2*)(al),(ub2*)(rc),		\
		ul_t((mx)),pul_t((cu)),(cu ? *(int*)cu : 0 ) ,oci_bind_options(md),ul_t((md)),				\
		oci_status_name(stat)),stat : stat

#define	OCIBindArrayOfStruct_log_stat(impsth,bp,ep,sd,si,sl,sr,stat)	\
	stat=OCIBindArrayOfStruct(bp,ep,sd,si,sl,sr);		\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sOCIBindArrayOfStruct(%p,%p,%u,%u,%u,%u)=%s\n",	\
		OciTp,(void*)bp,(void*)ep,sd,si,sl,sr,		\
		oci_status_name(stat)),stat : stat

#define OCIBindDynamic_log(impsth,bh,eh,icx,cbi,ocx,cbo,stat)   \
	stat=OCIBindDynamic(bh,eh,icx,cbi,ocx,cbo);			\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sBindDynamic(%p,%p,%p,%p,%p,%p)=%s\n",			\
		OciTp, (void*)bh,(void*)eh,(void*)icx,(void*)cbi,		\
		(void*)ocx,(void*)cbo,					\
		oci_status_name(stat)),stat : stat

#define OCIDefineByPos_log_stat(impsth,sh,dp,eh,p1,vp,vs,dt,ip,rp,cp,m,stat) \
	stat=OCIDefineByPos(sh,dp,eh,p1,vp,vs,dt,ip,rp,cp,m);		\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sDefineByPos(%p,%p,%p,%lu,%p,%ld,%u,%p,%p,%p,mode=%s,%lu)=%s\n",	\
		OciTp, (void*)sh,(void*)dp,(void*)eh,ul_t((p1)),(void*)(vp),	\
		sl_t(vs),(ub2)dt,(void*)(ip),(ub2*)(rp),(ub2*)(cp),oci_define_options(m),ul_t(m),	\
		oci_status_name(stat)),stat : stat

#define OCIDescribeAny_log_stat(impsth,sh,eh,op,ol,opt,il,ot,dh,stat)   \
	stat=OCIDescribeAny(sh,eh,op,ol,opt,il,ot,dh);			\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sDescribeAny(%p,%p,%p,%lu,%u,%u,%u,%p)=%s\n",	 		\
		OciTp, (void*)sh,(void*)eh,(void*)op,ul_t(ol),		\
		(ub1)opt,(ub1)il,(ub1)ot,(void*)dh,				\
		oci_status_name(stat)),stat : stat

#define OCIDescriptorAlloc_ok(impxxh,envhp, p1, t)              \
	if (DBD_OCI_TRACEON(impxxh)) PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sDescriptorAlloc(%p,%p,%s,0,0)\n",					\
		OciTp,(void*)envhp,(void*)(p1),oci_hdtype_name(t));			\
	if (OCIDescriptorAlloc((envhp), (void**)(p1), (t), 0, 0)==OCI_SUCCESS);	\
	else croak("OCIDescriptorAlloc (type %d) failed",t)

#define OCIDescriptorFree_log(impxxh,d,t)                       \
	if (DBD_OCI_TRACEON(impxxh)) PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sDescriptorFree(%p,%s)\n", OciTp, (void*)d,oci_hdtype_name(t));	\
	OCIDescriptorFree(d,t)

#define OCIEnvInit_log_stat(impdbh,ev,md,xm,um,stat)    \
	stat=OCIEnvInit(ev,md,xm,um);					\
	(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sEnvInit(%p,%lu,%lu,%p)=%s\n",				\
		OciTp, (void*)ev,ul_t(md),ul_t(xm),(void*)um,			\
		oci_status_name(stat)),stat : stat

#define OCIErrorGet_log_stat(impxxh, hp,rn,ss,ep,bp,bs,t, stat)  \
	((stat = OCIErrorGet(hp,rn,ss,ep,bp,bs,t)),			\
     ((DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sErrorGet(%p,%lu,\"%s\",%p,\"%s\",%lu,%lu)=%s\n",		\
		OciTp, (void*)hp,ul_t(rn),OciTstr(ss),psl_t(ep),		\
		bp,ul_t(bs),ul_t(t), oci_status_name(stat)),stat : stat))

#define OCIHandleAlloc_log_stat(impxxh,ph,hp,t,xs,ump,stat) \
	stat=OCIHandleAlloc(ph,hp,t,xs,ump);				\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sHandleAlloc(%p,%p,%s,%lu,%p)=%s\n",			\
		OciTp, (void*)ph,(void*)hp,oci_hdtype_name(t),ul_t(xs),(void*)ump,	\
		oci_status_name(stat)),stat : stat

#define OCIHandleAlloc_ok(impxxh,envhp, p1, t, stat)                \
	OCIHandleAlloc_log_stat(impxxh,(envhp),(void**)(p1),(t),0,0, stat);	\
	if (stat==OCI_SUCCESS) ;					\
	else croak("OCIHandleAlloc(%s) failed",oci_hdtype_name(t))

#define OCIHandleFree_log_stat(impxxh,hp,t,stat)    \
	stat=OCIHandleFree(	(hp), (t));				\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sHandleFree(%p,%s)=%s\n",OciTp,(void*)hp,oci_hdtype_name(t),		\
		oci_status_name(stat)),stat : stat

#define OCILobGetLength_log_stat(impxxh,sh,eh,lh,l,stat)    \
	stat=OCILobGetLength(sh,eh,lh,l);				\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sLobGetLength(%p,%p,%p,%p)=%s\n",				\
		OciTp, (void*)sh,(void*)eh,(void*)lh,pul_t(l),		\
		oci_status_name(stat)),stat : stat


#define OCILobGetChunkSize_log_stat(impdbh,sh,eh,lh,cs,stat)    \
	stat=OCILobGetChunkSize(sh,eh,lh,cs);				\
	(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sLobGetChunkSize(%p,%p,%p,%p)=%s\n",				\
		OciTp, (void*)sh,(void*)eh,(void*)lh,pul_t(cs),		\
		oci_status_name(stat)),stat : stat


#define OCILobFileOpen_log_stat(impxxh,sv,eh,lh,mode,stat)  \
	stat=OCILobFileOpen(sv,eh,lh,mode);				\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sLobFileOpen(%p,%p,%p,%u)=%s\n",				\
		OciTp, (void*)sv,(void*)eh,(void*)lh,(ub1)mode,		\
		oci_status_name(stat)),stat : stat

#define OCILobFileClose_log_stat(impsth,sv,eh,lh,stat)  \
	stat=OCILobFileClose(sv,eh,lh);					\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sLobFileClose(%p,%p,%p)=%s\n",				\
		OciTp, (void*)sv,(void*)eh,(void*)lh,				\
		oci_status_name(stat)),stat : stat
/*Added by JPS for Jeffrey.Klein*/

#define OCILobCreateTemporary_log_stat(impdbh,sv,eh,lh,csi,csf,lt,ca,dur,stat) \
	stat=OCILobCreateTemporary(sv,eh,lh,csi,csf,lt,ca,dur);					\
	(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sLobCreateTemporary(%p,%p,%p,%lu,%lu,%lu,%lu,%lu)=%s\n",				\
		OciTp, (void*)sv,(void*)eh,(void*)lh,				\
			ul_t(csi),ul_t(csf),ul_t(lt),ul_t(ca),ul_t(dur), \
		oci_status_name(stat)),stat : stat
/*end add*/

#define OCILobFreeTemporary_log_stat(impxxh,sv,eh,lh,stat)  \
	stat=OCILobFreeTemporary(sv,eh,lh);					\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sLobFreeTemporary(%p,%p,%p)=%s\n",				\
		OciTp, (void*)sv,(void*)eh,(void*)lh,				\
		oci_status_name(stat)),stat : stat

#define OCILobIsTemporary_log_stat(impsth,ev,eh,lh,istemp,stat) \
	stat=OCILobIsTemporary(ev,eh,lh,istemp);					\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sLobIsTemporary(%p,%p,%p,%p)=%s\n",				\
		OciTp, (void*)ev,(void*)eh,(void*)lh,(void*)istemp,		\
		oci_status_name(stat)),stat : stat

/*Added by JPS for Jeffrey.Klein */

#define OCILobLocatorAssign_log_stat(impdbh,sv,eh,src,dest,stat)    \
		stat=OCILobLocatorAssign(sv,eh,src,dest); \
		(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sLobLocatorAssign(%p,%p,%p,%p)=%s\n", \
		OciTp,(void*)sv,(void*)eh,(void*)src,(void*)dest, \
		oci_status_name(stat)),stat : stat

/*end add*/

#define OCILobRead_log_stat(impxxh,sv,eh,lh,am,of,bp,bl,cx,cb,csi,csf,stat) \
	stat=OCILobRead(sv,eh,lh,am,of,bp,bl,cx,cb,csi,csf);		\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sLobRead(%p,%p,%p,%p,%lu,%p,%lu,%p,%p,%u,%u)=%s\n",		\
		OciTp, (void*)sv,(void*)eh,(void*)lh,pul_t(am),ul_t(of),	\
		(void*)bp,ul_t(bl),(void*)cx,(void*)cb,(ub2)csi,(ub1)csf,	\
		oci_status_name(stat)),stat : stat

#define OCILobTrim_log_stat(impxxh,sv,eh,lh,l,stat) \
	stat=OCILobTrim(sv,eh,lh,l);					\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sLobTrim(%p,%p,%p,%lu)=%s\n",				\
		OciTp, (void*)sv,(void*)eh,(void*)lh,ul_t(l),			\
		oci_status_name(stat)),stat : stat

#define OCILobWrite_log_stat(impxxh,sv,eh,lh,am,of,bp,bl,p1,cx,cb,csi,csf,stat) \
	stat=OCILobWrite(sv,eh,lh,am,of,bp,bl,p1,cx,cb,csi,csf);		\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sLobWrite(%p,%p,%p,%p,%lu,%p,%lu,%u,%p,%p,%u,%u)=%s\n",	\
		OciTp, (void*)sv,(void*)eh,(void*)lh,pul_t(am),ul_t(of),	\
		(void*)bp,ul_t(bl),(ub1)p1,					\
		(void*)cx,(void*)cb,(ub2)csi,(ub1)csf,			\
		oci_status_name(stat)),stat : stat

#define OCILobWriteAppend_log_stat(impxxh,sv,eh,lh,am,bp,bl,p1,cx,cb,csi,csf,stat) \
	stat=OCILobWriteAppend(sv,eh,lh,am,bp,bl,p1,cx,cb,csi,csf);		\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sLobWriteAppend(%p,%p,%p,%p,%p,%lu,%u,%p,%p,%u,%u)=%s\n",	\
		OciTp, (void*)sv,(void*)eh,(void*)lh,pul_t(am),	\
		(void*)bp,ul_t(bl),(ub1)p1,					\
		(void*)cx,(void*)cb,(ub2)csi,(ub1)csf,			\
		oci_status_name(stat)),stat : stat

#define OCIParamGet_log_stat(impsth,hp,ht,eh,pp,ps,stat)    \
	stat=OCIParamGet(hp,ht,eh,pp,ps);				\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sParamGet(%p,%lu,%p,%p,%lu,%s)=%s\n",				\
		OciTp, (void*)hp,ul_t((ht)),(void*)eh,(void*)pp,ul_t(ps),	\
		oci_hdtype_name(ht),oci_status_name(stat)),stat : stat

#define OCIServerAttach_log_stat(imp_dbh, dbname,md,stat)				 \
	stat=OCIServerAttach( imp_dbh->srvhp, imp_dbh->errhp,		\
		(text*)dbname, (sb4)strlen(dbname), md);				\
	(DBD_OCI_TRACEON(imp_dbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(imp_dbh), \
		"%sServerAttach(%p, %p, \"%s\", %lu, mode=%s,%lu)=%s\n",			\
		OciTp, (void*)imp_dbh->srvhp,(void*)imp_dbh->errhp, dbname,	\
		ul_t(strlen(dbname)), oci_mode(md),ul_t(md),oci_status_name(stat)),stat : stat

#define OCIStmtExecute_log_stat(impsth,sv,st,eh,i,ro,si,so,md,stat) \
	stat=OCIStmtExecute(sv,st,eh,i,ro,si,so,md);			\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sStmtExecute(%p,%p,%p,%lu,%lu,%p,%p,mode=%s,%lu)=%s\n",		\
		OciTp, (void*)sv,(void*)st,(void*)eh,ul_t((i)),		\
		ul_t((ro)),(void*)(si),(void*)(so),oci_exe_mode(md),ul_t((md)),		\
		oci_status_name(stat)),stat : stat

#define OCIStmtFetch_log_stat(impsth,sh,eh,nr,or,os,stat)       \
    stat=OCIStmtFetch2(sh,eh,nr,or,os,OCI_DEFAULT);                     \
    (DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
			"%sStmtFetch(%p,%p,%lu,%u,%d)=%s\n",					\
			OciTp, (void*)sh,(void*)eh,ul_t(nr),(ub2)or,(ub2)os, \
			oci_status_name(stat)),stat : stat

#define OCIStmtPrepare_log_stat(impsth,sh,eh,s1,sl,l,m,stat)    \
	stat=OCIStmtPrepare(sh,eh,s1,sl,l,m);				\
	(DBD_OCI_TRACEON(impsth)) ? PerlIO_printf(DBD_OCI_TRACEFP(impsth), \
		"%sStmtPrepare(%p,%p,'%s',%lu,%lu,%lu)=%s\n",			\
		OciTp, (void*)sh,(void*)eh,s1,ul_t(sl),ul_t(l),ul_t(m),	\
		oci_status_name(stat)),stat : stat

#define OCIServerDetach_log_stat(impdbh,sh,eh,md,stat)  \
	stat=OCIServerDetach(sh,eh,md);					\
	(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sServerDetach(%p,%p,mode=%s,%lu)=%s\n",				\
		OciTp, (void*)sh,(void*)eh,oci_mode(md),ul_t(md),				\
		oci_status_name(stat)),stat : stat

#define OCISessionBegin_log_stat(impdbh,sh,eh,uh,cr,md,stat)    \
	stat=OCISessionBegin(sh,eh,uh,cr,md);				\
	(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sSessionBegin(%p,%p,%p,%lu,mode=%s %lu)=%s\n",			\
		OciTp, (void*)sh,(void*)eh,(void*)uh,ul_t(cr),oci_mode(md),ul_t(md),	\
		oci_status_name(stat)),stat : stat

#define OCISessionEnd_log_stat(impdbh,sh,eh,ah,md,stat) \
	stat=OCISessionEnd(sh,eh,ah,md);				\
	(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sSessionEnd(%p,%p,%p,mode=%s %lu)=%s\n",				\
		OciTp, (void*)sh,(void*)eh,(void*)ah,oci_mode(md),ul_t(md),		\
		oci_status_name(stat)),stat : stat

#define OCITransCommit_log_stat(impxxh,sh,eh,md,stat)   \
	stat=OCITransCommit(sh,eh,md);					\
	(DBD_OCI_TRACEON(impxxh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impxxh), \
		"%sTransCommit(%p,%p,%lu)=%s\n",				\
		OciTp, (void*)sh,(void*)eh,ul_t(md),				\
		oci_status_name(stat)),stat : stat

#define OCITransRollback_log_stat(impdbh,sh,eh,md,stat) \
	stat=OCITransRollback(sh,eh,md);				\
	(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sTransRollback(%p,%p,mode=%s %lu)=%s\n",				\
		OciTp, (void*)sh,(void*)eh,oci_mode(md),ul_t(md),				\
		oci_status_name(stat)),stat : stat

#define OCIDBStartup_log_stat(impdbh,svchp,errhp,admhp,mode,flags,stat) \
	stat=OCIDBStartup(svchp,errhp,admhp,mode,flags);			\
	(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sOCIDBStartup(%p,%p,%p,%u,%u)=%s\n",				\
		OciTp, (void*)svchp,(void*)errhp,(void*)admhp,mode,flags,	\
		oci_status_name(stat)),stat : stat

#define OCIDBShutdown_log_stat(impdbh,svchp,errhp,admhp,mode,stat)  \
	stat=OCIDBShutdown(svchp,errhp,admhp,mode);				\
	(DBD_OCI_TRACEON(impdbh)) ? PerlIO_printf(DBD_OCI_TRACEFP(impdbh), \
		"%sOCIDBShutdown(%p,%p,%p,%u)=%s\n",				\
		OciTp, (void*)svchp,(void*)errhp,(void*)admhp,mode,		\
		oci_status_name(stat)),stat : stat


#endif /* !DBD_OCI_TRACEON */
