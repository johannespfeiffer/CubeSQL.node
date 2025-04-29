/*
 *  cubesql.c
 *
 *  (c) 2006-2024 SQLabs srl -- All Rights Reserved
 *  Author: Marco Bambini (MB)
 *
 */

#include "cubesql.h"
#include "csql.h"

// maximum number of socket descriptor to try to connect to
// this change is required to support IPv4/IPv6 connections
#define	MAX_SOCK_LIST	6

// MARK: cubeSQL -
const char *cubesql_version (void) {
	return CUBESQL_SDK_VERSION;
}

int cubesql_connect (csqldb **db, const char *host, int port, const char *username, const char *password, int timeout, int encryption) {
	return cubesql_connect_token(db, host, port, username, password, timeout, encryption, NULL, kFALSE, NULL, NULL, NULL, NULL);
}

int cubesql_connect_ssl (csqldb **db, const char *host, int port, const char *username, const char *password, int timeout, const char *ssl_certificate_path) {
	return cubesql_connect_token(db, host, port, username, password, timeout, CUBESQL_ENCRYPTION_SSL, NULL, kFALSE, ssl_certificate_path, NULL, NULL, NULL);
}

int cubesql_connect_token (csqldb **db, const char *host, int port, const char *username, const char *password, int timeout, int encryption, char *token, int useOldProtocol, const char *ssl_certificate, const char *root_certificate, const char *ssl_certificate_password, const char *ssl_chiper_list) {
	csqldb	*rdb = NULL;
	int		is_ssl = encryption_is_ssl(encryption);
	
	// try to adjust encryption parameter
	if (encryption == 128) encryption = CUBESQL_ENCRYPTION_AES128;
	else if (encryption == 192) encryption = CUBESQL_ENCRYPTION_AES192;
	else if (encryption == 256) encryption = CUBESQL_ENCRYPTION_AES256;
	else if (is_ssl) useOldProtocol = kFALSE;
	
	#if CUBESQL_DISABLE_SSL_ENCRYPTION
	if (is_ssl) return CUBESQL_SSL_DISABLED_ERROR;
	#endif
	
	// sanity check parameters
	if ((host == NULL) || (username == NULL) || (password == NULL)) return CUBESQL_PARAMETER_ERROR;
	if ((encryption != CUBESQL_ENCRYPTION_NONE) && (encryption != CUBESQL_ENCRYPTION_AES128) &&
		(encryption != CUBESQL_ENCRYPTION_AES192) && (encryption != CUBESQL_ENCRYPTION_AES256) &&
		(is_ssl == kFALSE)) return CUBESQL_PARAMETER_ERROR;
	if (port <= 0) port = CUBESQL_DEFAULT_PORT;
	if (timeout < 0) timeout = CUBESQL_DEFAULT_TIMEOUT;
	
	// init library and winsock under Win32
	csql_libinit();
	
	// allocate db struct
	rdb = csql_dbinit (host, port, username, password, timeout, encryption,
					   ssl_certificate, root_certificate, ssl_certificate_password, ssl_chiper_list);
	if (rdb == NULL) {
		if (is_ssl) return CUBESQL_SSL_CERT_ERROR;
		return CUBESQL_MEMORY_ERROR;
	}
	if (useOldProtocol == kTRUE) rdb->useOldProtocol = kTRUE;
	*db = rdb;
	
	if (token != NULL) cubesql_settoken(rdb, token);
	return csql_connect (rdb, encryption);
}

int cubesql_connect_old_protocol (csqldb **db, const char *host, int port, const char *username, const char *password, int timeout, int encryption) {
	return cubesql_connect_token(db, host, port, username, password, timeout, encryption, NULL, kTRUE, NULL, NULL, NULL, NULL);
}

void cubesql_disconnect (csqldb *db, int gracefully) {
	if (!db) return;
	
	// clear errors first
	cubesql_clear_errors(db);
	
	// sanity check on socket
	if (db->sockfd <= 0) return;
	
	// disconnect
	if (gracefully == kTRUE) {
		csql_initrequest(db, 0, 0, kCOMMAND_CLOSE, kNO_SELECTOR);
		csql_netwrite(db, NULL, 0, NULL, 0);
		csql_netread(db, -1, -1, kFALSE, NULL, 1);
	}
	
	// close socket and free db
	csql_socketclose(db);
	csql_dbfree(db);
}

int cubesql_execute (csqldb *db, const char *sql) {
	// clear errors first
	cubesql_clear_errors(db);
	
	// check for trace function
	if (db->trace) db->trace(sql, db->data);
	
	// send sql statement
	if (csql_send_statement (db, kCOMMAND_EXECUTE, sql, kFALSE, kFALSE) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	// read replay
	return csql_netread(db, -1, -1, kFALSE, NULL, NO_TIMEOUT);
}

csqlc *cubesql_select (csqldb *db, const char *sql, int is_serverside) {
	// serverside is disabled in this version
	
	// clear errors first
	cubesql_clear_errors(db);
	
	// check for trace function
	if (db->trace) db->trace(sql, db->data);
	
	// send sql statement
	if (csql_send_statement (db, kCOMMAND_SELECT, sql, kFALSE, kFALSE) != CUBESQL_NOERR) return NULL;
	
	// read the cursor
	return csql_read_cursor(db, NULL);
}

int cubesql_commit (csqldb *db) {
	return cubesql_execute(db, "COMMIT;");
}

int cubesql_rollback (csqldb *db) {
	return cubesql_execute(db, "ROLLBACK;");
}

int cubesql_begintransaction (csqldb *db) {
	return cubesql_execute(db, "BEGIN TRANSACTION;");
}

int cubesql_bind (csqldb *db, const char *sql, char **colvalue, int *colsize, int *coltype, int ncols) {
	// clear errors first
	cubesql_clear_errors(db);
	return csql_bindexecute(db, sql, colvalue, colsize, coltype, ncols);
}

int cubesql_ping (csqldb *db) {
	return cubesql_execute(db, "PING;");
}

int64 cubesql_changes (csqldb *db) {
	csqlc	*cursor = NULL;
	int64	nchanges = 0;
	
	// send sql statement
	if (csql_send_statement (db, kCOMMAND_SELECT, "SELECT changes();", kFALSE, kFALSE) != CUBESQL_NOERR) return 0;
	
	// read the cursor
	cursor = csql_read_cursor(db, NULL);
	if (!cursor) return 0;
	
	nchanges = cubesql_cursor_int64(cursor, 1, 1, 0);
	
	cubesql_cursor_free(cursor);
	return nchanges;
}

void cubesql_cancel (csqldb *db) {
	if (db->sockfd <= 0) return;
		
	bsd_shutdown(db->sockfd, SHUT_RDWR);
	closesocket(db->sockfd);
	db->sockfd = 0;
}

int	cubesql_errcode (csqldb *db) {
	return db->errcode;
}

char *cubesql_errmsg (csqldb *db) {
	return db->errmsg;
}

void cubesql_set_trace_callback (csqldb *db, cubesql_trace_callback trace_ptr, void *data) {
	db->trace = trace_ptr;
	db->data = data;
}

// MARK: -

int cubesql_set_database (csqldb *db, const char *dbname) {
	char sql[512];
	
	if (!db || db->sockfd <= 0) return CUBESQL_ERR;
	
	if (dbname) {
		snprintf(sql, sizeof(sql), "USE DATABASE '%s';", dbname);
	} else {
		snprintf(sql, sizeof(sql), "UNSET CURRENT DATABASE;");
	}
	
	return cubesql_execute(db, sql);
}

int64 cubesql_affected_rows (csqldb *db) {
	csqlc *c = NULL;
	int64 value = 0;
	
	if (!db || db->sockfd <= 0) return 0;
	
	c = cubesql_select(db, "SHOW CHANGES;", kFALSE);
	if (c == NULL) return 0;
	
	value = cubesql_cursor_int64 (c, 1, 1, 0);
	cubesql_cursor_free(c);
	
	return value;
}

int64 cubesql_last_inserted_rowID (csqldb *db) {
	csqlc *c = NULL;
	int64 value = 0;
	
	if (!db || db->sockfd <= 0) return 0;
	
	c = cubesql_select(db, "SHOW LASTROWID;", kFALSE);
	if (c == NULL) return 0;
	
	value = cubesql_cursor_int64 (c, 1, 1, 0);
	cubesql_cursor_free(c);
	
	return value;
}

// MARK: - Binary Data -

int cubesql_send_data (csqldb *db, const char *buffer, int len) {
	int err = csql_sendchunk(db, (char *)buffer, len, 0, kFALSE);
	if (err != CUBESQL_NOERR) return err;
	return csql_netread(db, -1, -1, kTRUE, NULL, NO_TIMEOUT);
}

int cubesql_send_enddata (csqldb *db) {
	return csql_ack(db, kCOMMAND_ENDCHUNK);
}

char *cubesql_receive_data (csqldb *db, int *len, int *is_end_chunk) {
	char *data = csql_receivechunk (db, len, is_end_chunk);
	csql_ack(db, 0);
	return data;
}

// MARK: - Cursor -

int cubesql_cursor_numrows (csqlc *c) {
	if (c->server_side) return -1;
	return c->nrows;
}

int cubesql_cursor_numcolumns (csqlc *c) {
	return c->ncols;
}

int cubesql_cursor_currentrow (csqlc *c) {
	return c->current_row;
}

int cubesql_cursor_seek (csqlc *c, int index) {
	if (c->server_side == kTRUE) {
		if (index != CUBESQL_SEEKNEXT) return kFALSE;
		if (c->eof == kTRUE) return kFALSE;
		return (csql_cursor_step(c) == CUBESQL_NOERR) ? kTRUE : kFALSE;
	}
		
	if (index == CUBESQL_SEEKNEXT) index = c->current_row + 1;
	else if (index == CUBESQL_SEEKFIRST) index = 1;
	else if (index == CUBESQL_SEEKPREV) index = c->current_row - 1;
	else if (index == CUBESQL_SEEKLAST) index = c->nrows;
	
	if ((c->nrows != -1) && (index > c->nrows)) {c->eof = kTRUE; return kFALSE;}
	if (index < 0) return kFALSE;
	c->eof = (index == c->nrows + 1) ? kTRUE : kFALSE;
	c->current_row = index;
	
	return kTRUE;
}

int cubesql_cursor_iseof (csqlc *c) {
	if (c->nrows == 0) c->eof = kTRUE; 
	return c->eof;
}

int cubesql_cursor_columntype (csqlc *c, int index) {
	if ((index <= 0) || (index > c->ncols)) return -1;
	if (c->has_rowid) return c->types[index];
	else return c->types[index-1];
}

int cubesql_cursor_columntypebind (csqlc *c, int index) {
	//char *v = NULL;
	//int  vlen = 0;
	int  type;
	if ((index <= 0) || (index > c->ncols)) return -1;
	if (c->has_rowid) type = c->types[index];
	else type = c->types[index-1];
	
	// check for special NULL value
	//v = cubesql_cursor_field (c, CUBESQL_CURROW, index, &vlen);
	//if ((v == NULL) || (vlen == -1)) return kBIND_NULL;
	
	if (type == CUBESQL_Type_Integer) return CUBESQL_BIND_INTEGER;
	if (type == CUBESQL_Type_Float) return CUBESQL_BIND_DOUBLE;
	if (type == CUBESQL_Type_Blob) return CUBESQL_BIND_BLOB;
	
	return CUBESQL_BIND_TEXT;
}

char *cubesql_cursor_field (csqlc *c, int row, int column, int *len) {
	char	*result;
	int 	i, n;
	int		v1 = 0, v2 = 0, nindex = 0, cnum, rnum;
	
	if (len) *len = 0;
	if ((column != CUBESQL_ROWID) && ((column <= 0) || (column > c->ncols))) return NULL;
	if (row > c->nrows) return NULL;
	if (row < -2) return NULL;
	
	// row CUBESQL_CURROW means current row
	if (row == CUBESQL_CURROW) row = c->current_row;
	
	// row CUBESQL_COLNAME means to get column names
	if (row == CUBESQL_COLNAME) {
		result = c->names;
		if (c->has_rowid) result += strlen(result)+1;
		for (i=0; i<column-1; i++) result += strlen(result)+1;
		if (len) *len = (int)strlen(result);
		return result;
	}
	
	// row CUBESQL_COLTABLE means get table name
	if (row == CUBESQL_COLTABLE) {
		if (c->tables == NULL) {
			if (len) *len = -1;
			return NULL;
		}
		result = c->tables;
		if (c->has_rowid) result += strlen(result)+1;
		for (i=0; i<column-1; i++) result += strlen(result)+1;
		if (len) *len = (int)strlen(result);
		return result;
	}
	
	if (column == CUBESQL_ROWID) {
		if (c->has_rowid == kFALSE) return NULL;
		column = 0;
	}
	
	// I think I can avoid a lot of crashes with with trick
	if (c->nrows == 0) return NULL;
	
	// check for special custom created cursor
	if (c->cursor_id == -1) {
		n = ((row-1) * c->ncols) + (column-1);
		result = c->buffer[n];
		if (len) *len = c->size0[n];
		return result;
	}
	
	// first find out the right index buffer
	if (c->nbuffer) {
		// search in current buffer first (90% of the time it should be true)
		if (c->current_buffer == 0) v1 = 0;
		else v1 = c->rowcount[c->current_buffer-1];
		v2 = c->rowcount[c->current_buffer];
		if ((row>=v1) && (row<=v2)) {
			nindex = c->current_buffer;
			goto found_buffer;
		}
		
		// then search in the next buffer
		v1 = c->rowcount[c->current_buffer];
		if (c->current_buffer == c->nbuffer-1) v2 = c->rowcount[c->current_buffer];
		else v2 = c->rowcount[c->current_buffer + 1];
		if ((row>=v1) && (row<=v2)) {
			nindex = c->current_buffer + 1;
			goto found_buffer;
		}
		
		// otherwise perform a linear search
		for (i=0; i<c->nbuffer; i++)
		{
			if (i == 0) v1 = 0;
			else v1 = c->rowcount[i-1];
			v2 = c->rowcount[i];
			
			if ((row>=v1) && (row<=v2)) {
				nindex = i;
				goto found_buffer;
			}
		}
		return NULL;
	}
	
found_buffer:
	if (c->nbuffer) {
		row = row - v1;
		if (c->current_buffer != nindex) {
			cnum = c->ncols;
			if (c->has_rowid) cnum++;
			rnum = v2 - v1;
			
			c->current_buffer = nindex;
			c->psum = c->rowsum[nindex];
			if (nindex == 0) {
				c->data = c->data0;
				c->size = c->size0;
			} else {
				c->size = (int *) c->buffer[nindex];
				c->data = (char *) c->size + (rnum * cnum * sizeof(int));
			}
		}
	}
		
	// compute index inside the cursor
	if ((c->has_rowid) && (column != CUBESQL_ROWID)) n = ((row-1) * (c->ncols + 1)) + (column);
	else n = ((row-1) * c->ncols) + (column-1);
	
	if (n < 0) n = 0;
	if (n > 0) result = c->data + c->psum[n-1];
	else result = c->data;// + c->psum[n];
	if (len) *len = c->size[n];
	// special NULL value case
	if (c->size[n] == -1) result = NULL;
	
	return result;
}

int64 cubesql_cursor_rowid (csqlc *c, int row) {
	int	 len = 0;
	char *rowid, buf[64] = {0};
	
	rowid = cubesql_cursor_field(c, row, CUBESQL_ROWID, &len);
	if ((rowid == NULL) || (len == 0)) return 0;
	
	if (len > sizeof(buf)-1) len = sizeof(buf)-1;
	memcpy(buf, rowid, len);
	
	return strtoll(buf, NULL, 0);
}

int cubesql_cursor_int (csqlc *c, int row, int column, int default_value) {
	char *field, buf[64] = {0};
	int	 len;
	
	field = cubesql_cursor_field(c, row, column, &len);
	if ((field == NULL) || (len <= 0)) return default_value;
	
	if (len > sizeof(buf)-1) len = sizeof(buf)-1;
	memcpy(buf, field, len);
	return (int)strtol(buf, NULL, 0);
}

int64 cubesql_cursor_int64 (csqlc *c, int row, int column, int64 default_value) {
	char *field, buf[64] = {0};
	int	 len;
	
	field = cubesql_cursor_field(c, row, column, &len);
	if ((field == NULL) || (len <= 0)) return default_value;
	
	if (len > sizeof(buf)-1) len = sizeof(buf)-1;
	memcpy(buf, field, len);
	return strtoll(buf, NULL, 0);
}


double cubesql_cursor_double (csqlc *c, int row, int column, double default_value) {
	char *field, buf[64] = {0};
	int	 len;
	
	field = cubesql_cursor_field(c, row, column, &len);
	if ((field == NULL) || (len <= 0)) return default_value;
	
	if (len > sizeof(buf)-1) len = sizeof(buf)-1;
	memcpy(buf, field, len);
	return strtod(buf, NULL);
}

char *cubesql_cursor_cstring (csqlc *c, int row, int column) {
	char *field, *s;
	int	 len;
	
	field = cubesql_cursor_field(c, row, column, &len);
	if ((field == NULL) || (len < 0)) return NULL;
	
	s = (char *) calloc(1, len+1);
	if (s == NULL) return NULL;
	
	memcpy(s, field, len);
	return s;
}

char *cubesql_cursor_cstring_static (csqlc *c, int row, int column, char *staticbuffer, int bufferlen) {
	char *field;
	int	 len;
	
	field = cubesql_cursor_field(c, row, column, &len);
	if ((field == NULL) || (len < 0)) return NULL;
	
	if (len > bufferlen-1) len =  bufferlen-1;
	memcpy(staticbuffer, field, len);
	staticbuffer[len] = 0;
	
	return staticbuffer;
}

void cubesql_cursor_free (csqlc *c) {
	int i;
	
	if (c == NULL) return;
	
	// close the cursor on server side also
	if (c->server_side) csql_cursor_close(c);
	
	// check for special custom created cursor
	if (c->cursor_id == -1) {
		if (c->names) free(c->names);
		if (c->types) free(c->types);
		if (c->buffer) {
			for (i=0; i< c->nrows * c->ncols; i++)
			free(c->buffer[i]);
			free(c->buffer);
		}
		if (c->size0) free(c->size0);
		free(c);
		return;
	}
	
	if ((c->server_side) && (c->p0 != c->p))
		free(c->p0);
	
	// no chuck case
	if (c->nbuffer == 0) {
		free(c->p);
		free(c->psum);
		free(c);
		return;
	}
	
	// check case
	free(c->rowcount);
	for (i=0; i<c->nbuffer; i++) {
		free (c->buffer[i]);
		free (c->rowsum[i]);
	}
	
	free(c);
}

// MARK: - VM -

csqlvm *cubesql_vmprepare (csqldb *db, const char *sql) {
	csqlvm	*vm = NULL;
	
	// clear errors first
	cubesql_clear_errors(db);
	
	// check for trace function
	if (db->trace) db->trace(sql, db->data);
	
	// send sql statement
	if (csql_send_statement (db, kVM_PREPARE, sql, kFALSE, kFALSE) != CUBESQL_NOERR) return NULL;
	
	// read replay
	if (csql_netread(db, -1, -1, kFALSE, NULL, NO_TIMEOUT) != CUBESQL_NOERR) return NULL;
	
	// allocate space for csqlvm
	vm = (csqlvm *) malloc (sizeof(csqlvm));
	if (vm == NULL) return NULL;
	
	vm->db = db;
	vm->vmindex = 0;
	return vm;
}

int cubesql_vmbind_int (csqlvm *vm, int index, int intvalue) {
	char	value[256];
	
	// convert int to text
	snprintf(value, sizeof(value), "%d", intvalue);
	return csql_bind_value(vm->db, index, CUBESQL_BIND_INTEGER, value, -1);
}

int cubesql_vmbind_double (csqlvm *vm, int index, double dvalue) {
	char	value[256];
	
	// convert double to text
	snprintf(value, sizeof(value), "%f", dvalue);
	return csql_bind_value(vm->db, index, CUBESQL_BIND_DOUBLE, value, -1);
}

int cubesql_vmbind_text (csqlvm *vm, int index, char *value, int len) {
	return csql_bind_value(vm->db, index, CUBESQL_BIND_TEXT, value, -1);
}

int cubesql_vmbind_blob (csqlvm *vm, int index, void *value, int len) {
	return csql_bind_value(vm->db, index, CUBESQL_BIND_BLOB, (char *)value, len);
}

int cubesql_vmbind_null (csqlvm *vm, int index) {
	return csql_bind_value(vm->db, index, CUBESQL_BIND_NULL, NULL, 0);
}

int cubesql_vmbind_int64 (csqlvm *vm, int index, int64 int64value) {
	char	value[256];
	
	// convert int to text
	snprintf(value, sizeof(value), "%lld", int64value);
	return csql_bind_value(vm->db, index, CUBESQL_BIND_INT64, value, -1);
}

int cubesql_vmbind_zeroblob (csqlvm *vm, int index, int len) {
	return csql_bind_value(vm->db, index, CUBESQL_BIND_ZEROBLOB, NULL, len);
}

int cubesql_vmexecute (csqlvm *vm) {
	csqldb *db = vm->db;
	
	// clear errors first
	cubesql_clear_errors(db);
	
	// send VMEXECUTE command
	csql_initrequest(db, 0, 0, kVM_EXECUTE, kNO_SELECTOR);
	csql_netwrite(db, NULL, 0, NULL, 0);
	
	// read replay
	return csql_netread(db, -1, -1, kFALSE, NULL, NO_TIMEOUT);
}

csqlc *cubesql_vmselect (csqlvm *vm) {
	csqldb *db = vm->db;
	
	// clear errors first
	cubesql_clear_errors(db);
	
	// send VMSELECT command
	csql_initrequest(db, 0, 0, kVM_SELECT, kNO_SELECTOR);
	csql_netwrite(db, NULL, 0, NULL, 0);
	
	// read the cursor
	return csql_read_cursor(db, NULL);
}

int cubesql_vmclose (csqlvm *vm) {
	if (!vm) return CUBESQL_NOERR;
	
	csqldb *db = vm->db;
	
	csql_initrequest(db, 0, 0, kVM_CLOSE, kNO_SELECTOR);
	csql_netwrite(db, NULL, 0, NULL, 0);
	csql_netread(db, -1, -1, kFALSE, NULL, NO_TIMEOUT);
	
	free(vm);
	return CUBESQL_NOERR;
}

// MARK: - Private -

void cubesql_clear_errors (csqldb *db) {
	if (db) {
		db->errcode = CUBESQL_NOERR;
		db->errmsg[0] = 0;
	}
}

csqldb *cubesql_cursor_db (csqlc *cursor) {
	return cursor->db;
}

void cubesql_setuserptr (csqldb *db, void *userptr) {
	db->userptr = userptr;
}

void *cubesql_getuserptr (csqldb *db) {
	return db->userptr;
}

void cubesql_settoken (csqldb *db, char *token) {
	if ((token) && (strlen(token)==0)) token = NULL;
	db->token = token;
}

void cubesql_sethostverification (csqldb *db, char *hostverification) {
	if ((hostverification) && (strlen(hostverification)==0)) hostverification = NULL;
	db->hostverification = hostverification;
}

char *cubesql_gettoken (csqldb *db) {
	return db->token;
}

csqlc *cubesql_cursor_create (csqldb *db, int nrows, int ncolumns, int *types, char **names) {
	csqlc *cursor = NULL;
	char  *p = NULL, *s = NULL;
	int  i, len = 0;
	
	// simple sanity check
	if ((nrows < 0) || (ncolumns <= 0) || (types == NULL) || (names == NULL)) return NULL;
	
	// allocate cursor
	cursor = csql_cursor_alloc(db);
	if (cursor == NULL) return NULL;
	
	// init fields
	cursor->server_side = kFALSE;
	cursor->has_rowid = kFALSE;
	cursor->ncols = ncolumns;
	cursor->nrows = 0;
	cursor->current_row = -1;
	cursor->cursor_id = -1; // means custom created
	
	// allocate memory for names and types
	for (i=0; i< cursor->ncols; i++) {
		p = names[i];
		if (p == NULL) p = "";
		len += strlen(p)+1;
	}
	cursor->names = (char*) malloc(len);
	if (cursor->names == NULL) goto abort;
		
	cursor->types = (int*) malloc(cursor->ncols * sizeof(int));
	if (cursor->types == NULL) goto abort;
		
	if (nrows > 0)
		cursor->nalloc = nrows;
	else
		cursor->nalloc = kDEFAULT_ALLOC_ROWS;
		
	cursor->buffer = (char**) malloc(sizeof(char*) * cursor->ncols * cursor->nalloc);
	if (cursor->buffer == NULL) goto abort;
		
	cursor->size0 = (int*) malloc(sizeof(int) * cursor->ncols * cursor->nalloc);
	if (cursor->size0 == NULL) goto abort;
	
	// set column names
	p = cursor->names;
	for (i=0; i< cursor->ncols; i++) {
		s = names[i];
		if (s == NULL) s = "";
		len = (int)strlen(s)+1;
		memcpy(p, s, len);
		p += len;
	}
	
	// set columns types
	for (i=0; i< cursor->ncols; i++) {
		cursor->types[i] = types[i];
	}
	
	return cursor;
	
abort:
	cubesql_cursor_free(cursor);
	return NULL;
}

int cubesql_cursor_addrow (csqlc *cursor, char **row, int *len) {
	int i, j, index, rlen;
	
	// row can be added to a custom created cursor only
	if (cursor->cursor_id != -1) return kFALSE;
	
	// check if there is enough space for the new row
	index = cursor->nrows * cursor->ncols;
	if (cursor->nalloc < index + cursor->ncols) {
		int newsize = cursor->nalloc + (kDEFAULT_ALLOC_ROWS * 2);
		
		cursor->buffer = (char**) realloc(cursor->data, sizeof(char*) * cursor->ncols * newsize);
		if (cursor->buffer == NULL) return kFALSE;
		
		cursor->size0 = (int*) realloc(cursor->size, sizeof(int) * cursor->ncols * newsize);
		if (cursor->size0 == NULL) return kFALSE;
		
		cursor->nalloc = newsize;
	}
	
	// append new row to the cursor
	for (j=0, i=index; j < cursor->ncols; j++, i++) {
		rlen = len[j];
		if (rlen < 0) rlen = 0;
		
		cursor->buffer[i] = (char *) malloc (rlen);
		if ((cursor->buffer[i] == NULL) && (rlen > 0)) return kFALSE;
		
		if ((row[j]) && (rlen)) memcpy (cursor->buffer[i], row[j], rlen);
		cursor->size0[i] = len[j];
	}
	
	cursor->nrows++;
	if (cursor->current_row == -1) cursor->current_row = 1;
	
	return kTRUE;
}

void cubesql_seterror(csqldb *db, int errcode, const char *errmsg) {
	csql_seterror(db, errcode, errmsg);
}

// MARK: - Reserved -

void csql_libinit (void) {
	#ifdef WIN32
	WSADATA wsaData;
	#else
	struct sigaction act;
	#endif
	static int lib_inited = kFALSE;
	
	if (lib_inited == kFALSE) {
		lib_inited = kTRUE;
		csql_static_randinit();
		csql_gen_tabs();
		
		#ifdef WIN32
		WSAStartup(MAKEWORD(2,2), &wsaData);
		#else
		// IGNORE SIGPIPE and SIGABORT
		act.sa_handler = SIG_IGN;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		sigaction(SIGPIPE, &act, (struct sigaction *)NULL);
		sigaction(SIGABRT, &act, (struct sigaction *)NULL);
		#endif
	}
}

csqldb *csql_dbinit (const char *host, int port, const char *username, const char *password, int timeout, int encryption, const char *ssl_certificate, const char *root_certificate, const char *ssl_certificate_password, const char *ssl_chiper_list) {
	csqldb	*db = NULL;
	
	db = (csqldb *) malloc (sizeof(csqldb));
	if (db == NULL) return NULL;
	
	// zero all the struct
	bzero(db, sizeof(csqldb));
	
	// save db fields
	db->port = port;
	db->timeout = timeout;
	db->encryption = encryption;
	db->token = NULL;
	db->useOldProtocol = kFALSE;
	db->verifyPeer = kFALSE;
	
	snprintf((char *) db->host, sizeof(db->host), "%s", host);
	snprintf((char *) db->username, sizeof(db->username),  "%s", username);
	snprintf((char *) db->password, sizeof(db->password),  "%s", password);
	
	#ifndef CUBESQL_DISABLE_SSL_ENCRYPTION
	if (encryption_is_ssl(encryption) == kTRUE) {
		if (tls_init() < 0) {
			fprintf(stderr, "Error while initializing TLS library.");
			goto load_ssl_abort;
		}
		
		struct tls_config *tls_conf = tls_config_new();
		if (!tls_conf) {
			fprintf(stderr, "Error while initializing a new TLS configuration.");
			goto load_ssl_abort;
		}
		
		if (ssl_certificate_password) {
			int rc = tls_config_set_key_file(tls_conf, ssl_certificate_password);
			if (rc < 0) {
				fprintf(stderr, "Error in tls_config_set_key_file: %s.", tls_config_error(tls_conf));
				goto load_ssl_abort;
			}
		}
		
		#ifdef TLS_DEFAULT_CA_FILE
		if (!root_certificate) root_certificate = TLS_DEFAULT_CA_FILE
		#endif
		
		if (root_certificate) {
			int rc = tls_config_set_ca_file(tls_conf, root_certificate);
			if (rc < 0) {
				fprintf(stderr, "Error in tls_config_set_ca_file: %s.", tls_config_error(tls_conf));
				goto load_ssl_abort;
			}
		} else {
			// if no root certificate is provided then disable certificate and name verification
			tls_config_insecure_noverifycert(tls_conf);
			tls_config_insecure_noverifyname(tls_conf);
		}
		
		if (ssl_certificate) {
			int rc = tls_config_set_cert_file(tls_conf, ssl_certificate);
			if (rc < 0) {
				fprintf(stderr, "Error in tls_config_set_cert_file: %s.", tls_config_error(tls_conf));
				goto load_ssl_abort;
			}
		}
		
		// apply cipher list
		if (ssl_chiper_list) {
			int rc = tls_config_set_ciphers(tls_conf, ssl_chiper_list);
			if (rc < 0) {
				// report error but not abort
				fprintf(stderr, "Error in tls_config_set_ciphers: %s.", tls_config_error(tls_conf));
			}
		}
		
		struct tls *tls_context = tls_client();
		if (!tls_context) {
			fprintf(stderr, "Error while initializing a new TLS client.");
			goto load_ssl_abort;
		}
		
		// apply configuration to context
		int rc = tls_configure(tls_context, tls_conf);
		if (rc < 0) {
			fprintf(stderr, "Error in tls_configure: %s.", tls_error(tls_context));
			goto load_ssl_abort;
		}
		
		// save TLS context
		db->tls_context = tls_context;
	}
	#endif

	return db;
	
	#ifndef CUBESQL_DISABLE_SSL_ENCRYPTION
load_ssl_abort:
	// TODO: cleanup TLS
	return NULL;
	#endif
}

void csql_dbfree (csqldb *db) {
	if (db->inbuffer) free(db->inbuffer);
	free(db);
}

void csql_socketclose (csqldb *db) {
	if (db->sockfd <= 0) return;
	
	#ifndef CUBESQL_DISABLE_SSL_ENCRYPTION
	if (db->tls_context) {
		tls_close(db->tls_context);
		tls_free(db->tls_context);
		db->tls_context = NULL;
	}
	#endif
	
	bsd_shutdown(db->sockfd, SHUT_RDWR);
	closesocket(db->sockfd);
}

int csql_socketconnect (csqldb *db) {
	// apparently a listening IPv4 socket can accept incoming connections from only IPv4 clients
	// so I must explicitly connect using IPv4 if I want to be able to connect with older cubeSQL versions
	// https://stackoverflow.com/questions/16480729/connecting-ipv4-client-to-ipv6-server-connection-refused
	
	// ipv4/ipv6 specific variables
	struct sockaddr_storage serveraddr;
	struct addrinfo hints, *addr_list = NULL, *addr;
	
	// ipv6 code from https://www.ibm.com/support/knowledgecenter/ssw_ibm_i_72/rzab6/xip6client.htm
	memset(&hints, 0x00, sizeof(hints));
	hints.ai_flags    = AI_NUMERICSERV;
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	// check if we were provided the address of the server using
	// inet_pton() to convert the text form of the address to binary form.
	// If it is numeric then we want to prevent getaddrinfo() from doing any name resolution.
	int rc = inet_pton(AF_INET, (const char *) db->host, &serveraddr);
	if (rc == 1) { /* valid IPv4 text address? */
		hints.ai_family = AF_INET;
		hints.ai_flags |= AI_NUMERICHOST;
	}
	else {
		rc = inet_pton(AF_INET6, (const char *) db->host, &serveraddr);
		if (rc == 1) { /* valid IPv6 text address? */
			hints.ai_family = AF_INET6;
			hints.ai_flags |= AI_NUMERICHOST;
		}
	}
	
	// get the address information for the server using getaddrinfo()
	char port_string[256];
	snprintf(port_string, sizeof(port_string), "%d", db->port);
	rc = getaddrinfo((const char *) db->host, port_string, &hints, &addr_list);
	if (rc != 0 || addr_list == NULL) {
		csql_seterror(db, ERR_SOCKET, "Error while resolving getaddrinfo (host not found)");
		return -1;
	}
	
	const char *lastConnectionErrorMessage = NULL;
	int sock_index = 0;
	int sock_current = 0;
	int sock_list[MAX_SOCK_LIST] = {0};
	for (addr = addr_list; addr != NULL; addr = addr->ai_next, ++sock_index) {
		if (sock_index >= MAX_SOCK_LIST) break;

		// placeholder; successfully instantiated sockets will have > 0
		sock_list[sock_index] = -1;
		
		// display protocol specific formatted address
		/*
		char szHost[256], szPort[16];
		getnameinfo(addr->ai_addr, addr->ai_addrlen, szHost, sizeof(szHost), szPort, sizeof(szPort), NI_NUMERICHOST | NI_NUMERICSERV);
		printf("getnameinfo(): host=%s, port=%s, family=%d\n", szHost, szPort, addr->ai_family);
		*/
		sock_current = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (sock_current < 0) continue;
		
		// set socket options
		int len = 1;
		bsd_setsockopt(sock_current, SOL_SOCKET, SO_KEEPALIVE, (const char *) &len, sizeof(len));
		len = 1;
		bsd_setsockopt(sock_current, IPPROTO_TCP, TCP_NODELAY, (const char *) &len, sizeof(len));
		#ifdef SO_NOSIGPIPE
		len = 1;
		bsd_setsockopt(sock_current, SOL_SOCKET, SO_NOSIGPIPE, (const char *) &len, sizeof(len));
		#endif
		
		// by default, an IPv6 socket created on Windows Vista and later only operates over the IPv6 protocol
		// in order to make an IPv6 socket into a dual-stack socket, the setsockopt function must be called
		if (addr->ai_family == AF_INET6) {
			#ifdef WIN32
			DWORD ipv6only = 0;
			#else
			int   ipv6only = 0;
			#endif
			bsd_setsockopt(sock_current, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&ipv6only, sizeof(ipv6only));
		}
		
		// turn on non-blocking
		unsigned long ioctl_blocking = 1;    /* ~0; //TRUE; */
		ioctl(sock_current, FIONBIO, &ioctl_blocking);
		
		// initiate non-blocking connect ignoring return code
		rc = connect(sock_current, addr->ai_addr, addr->ai_addrlen);
		if (rc < 0) {
			if (errno != 0 && errno != EINTR && errno != EAGAIN & errno != EINPROGRESS)
			{
				// obvious error (e.g. network not reachable) - don't use this socket
				lastConnectionErrorMessage = strerror(errno);
				/*
				printf("connect error: %s (%i)\n", strerror(errno), errno);
				*/
				closesocket(sock_current);
				continue;
			}
		}
		
		// add sock_current to internal list of trying to connect sockets
		sock_list[sock_index] = sock_current;
	}
	
	// free not more needed memory
	freeaddrinfo(addr_list);
	
	// calculate the connection timeout and reset timers
	int connect_timeout = (db->timeout > 0) ? db->timeout : CUBESQL_DEFAULT_TIMEOUT;
	time_t start = time(NULL);
	time_t now = start;
	rc = 0;
	
	int sockfd = 0;
	fd_set write_fds;
	fd_set except_fds;
	struct timeval tv;

	while ((now - start) < connect_timeout) {
		FD_ZERO(&write_fds);
		FD_ZERO(&except_fds);
		
		tv.tv_sec = connect_timeout;
		tv.tv_usec = 0;

		for (int i=0; i<MAX_SOCK_LIST; ++i) {
			if (sock_list[i] > 0) {
				FD_SET(sock_list[i], &write_fds);
				FD_SET(sock_list[i], &except_fds);

				rc = bsd_select(sock_list[i] + 1, NULL, &write_fds, &except_fds, &tv);
				
				if (rc == 0) {
					// timeout - we can't use this socket
					closesocket(sock_list[i]);
					sock_list[i] = -1;
					continue;
				}

				if (rc < 0) {
					if (errno == 0 || errno == EINTR || errno == EAGAIN || errno == EINPROGRESS) {
						// socket is still connecting
						continue;
					}
					
					// socket bsd_select error - don't use this socket
					/*
					printf("socket bsd_select error: %s (%i)\n", strerror(errno), errno);
					*/
					lastConnectionErrorMessage = strerror(errno);
					closesocket(sock_list[i]);
					sock_list[i] = -1;
					continue;
				}
			}
		}
		
		// check for error
		for (int i=0; i<MAX_SOCK_LIST; ++i) {
			if (sock_list[i] > 0) {
				if (FD_ISSET(sock_list[i], &except_fds)) {
					int err = csql_socketerror(sock_list[i]);
					if (err > 0) {
						lastConnectionErrorMessage = strerror(err);
					}

					closesocket(sock_list[i]);
					sock_list[i] = -1;
				}
			}
		}
		
		// check which file descriptor is ready for writing (need to check for socket error also)
		for (int i=0; i<MAX_SOCK_LIST; ++i) {
			if (sock_list[i] > 0) {
				if (FD_ISSET(sock_list[i], &write_fds)) {
					int err = csql_socketerror(sock_list[i]);
					if (err > 0) {
						lastConnectionErrorMessage = strerror(err);

						closesocket(sock_list[i]);
						sock_list[i] = -1;
					} else {
						//use this socket - break loop (use first one, don't overwrite sockfd with next possible one)
						sockfd = sock_list[i];
						break;
					}
				}
			}
		}

		// check if a valid descriptor has been found
		if (sockfd != 0) break;

		// are there still unclosed sockets trying to connect?
		int remainingSocketCount = 0;
		for (int i=0; i<MAX_SOCK_LIST; ++i) {
			if (sock_list[i] > 0) {
				remainingSocketCount++;
			}
		}

		// no sockets remaining - break loop
		if (remainingSocketCount < 1) break;
		
		// no socket ready yet
		now = time(NULL);
	}
	
	// cleanup: close unneeded, still opened sockets
	for (int i=0; i<MAX_SOCK_LIST; ++i) {
		if ((sock_list[i] > 0) && (sock_list[i] != sockfd)) closesocket(sock_list[i]);
	}
	
	// bail if no socket has been connected because of an error
	if ((sockfd <= 0) && lastConnectionErrorMessage && (strstr(lastConnectionErrorMessage, "Unknown") == NULL)) {
		char errorConnectMessage[1024];

		// set error message
		snprintf(errorConnectMessage, sizeof(errorConnectMessage), "An error occurred while trying to connect: %s", lastConnectionErrorMessage);
		csql_seterror(db, ERR_SOCKET, errorConnectMessage);
		return -1;
	}
	
	// bail if there was a timeout
	if ((time(NULL) - start) >= connect_timeout) {
		csql_seterror(db, ERR_SOCKET_TIMEOUT, "Connection timeout while trying to connect");
		return -1;
	}
	
	// bail if no socket has been connected (but we didn't catch an error message to be shown)
	if (sockfd <= 0) {
		// set error message
		csql_seterror(db, ERR_SOCKET, "An error occurred while trying to connect");
		return -1;
	}

	// turn off non-blocking
	int ioctl_blocking = 0;	/* ~0; //TRUE; */
	ioctl(sockfd, FIONBIO, &ioctl_blocking);
	
	// socket is connected - now check for SSL
	#ifndef CUBESQL_DISABLE_SSL_ENCRYPTION
	if (encryption_is_ssl(db->encryption)) {
		int rc = tls_connect_socket(db->tls_context, sockfd, db->host);
		if (rc < 0) {
			db->errcode = ERR_SSL;
			snprintf(db->errmsg, sizeof(db->errmsg), "Error on tls_connect_socket: %s", tls_error(db->tls_context));
			closesocket(sockfd);
			return -1;
		}
		db->encryption -= CUBESQL_ENCRYPTION_SSL;
	}
	#endif
	
	return sockfd;
}

int csql_bind_value (csqldb *db, int index, int bindtype, char *value, int len) {
	int field_size[1];
	int nfields = 0, nsizedim = 0, packet_size = 0, datasize = 0;
	
	if ((bindtype == CUBESQL_BIND_NULL) || (bindtype == CUBESQL_BIND_ZEROBLOB)) {
		value = NULL;
	} else {
		if (!value) {value = ""; len = 0;}
		
		// build packet
		if (value) {
			if (len == -1) len = (int)strlen(value);
			nfields = 1;
			nsizedim = sizeof(int) * nfields;
			datasize = len;
			packet_size = datasize + nsizedim;
			field_size[0] = htonl(datasize);
		}
	}
	
	// prepare BIND command
	csql_initrequest(db, packet_size, nfields, kVM_BIND, kNO_SELECTOR);
	db->request.flag3 = (unsigned char) bindtype;
	db->request.reserved1 = htons(index);
	if (bindtype == CUBESQL_BIND_ZEROBLOB) db->request.expandedSize = htonl(len);
	
	// send request
	csql_netwrite(db, (char *) field_size, nsizedim, (char *)value, datasize);
	
	// read reply
	return csql_netread(db, -1, -1, kFALSE, NULL, NO_TIMEOUT);
}

int csql_bindexecute(csqldb *db, const char *sql, char **colvalue, int *colsize, int *coltype, int nvalues) {
	int i;
	
	// check for trace function
	if (db->trace) db->trace(sql, db->data);
	
	// send sql statement first
	if (csql_send_statement(db, kCOMMAND_CHUNK_BIND, sql, kFALSE, kFALSE) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	// read and check header first
	if (csql_socketread(db, kTRUE, NO_TIMEOUT) != CUBESQL_NOERR) return CUBESQL_ERR;
	if (csql_checkheader(db, -1, -1, NULL) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	// send individual fields
	for (i=0; i<nvalues; i++) {
		// fix to null values
		if ((coltype[i] == CUBESQL_BIND_NULL) || (coltype[i] == CUBESQL_BIND_TEXT && colvalue[i] == NULL)) {
			colvalue[i] = "";
			colsize[i] = 0;
		}
		
		// includes the terminal 0
		if (coltype[i] != CUBESQL_BIND_BLOB) {
			colsize[i]++;
		}
		
		if (csql_sendchunk(db, colvalue[i], colsize[i], coltype[i], kTRUE) == CUBESQL_ERR)
			return CUBESQL_ERR;
		
		// read and check header first
		if (csql_socketread(db, kTRUE, NO_TIMEOUT) != CUBESQL_NOERR) return CUBESQL_ERR;
		if (csql_checkheader(db, -1, -1, NULL) != CUBESQL_NOERR) return CUBESQL_ERR;
	}
	
	// send BIND FINALIZE command
	return csql_ack(db, kBIND_FINALIZE);
}

int csql_send_statement (csqldb *db, int command_type, const char *sql, int is_partial, int server_side) {
	int field_size[1];
	int nfields, nsizedim, packet_size, datasize = 0;
	
	nfields = 1;
	nsizedim = sizeof(int) * nfields;
	datasize = (int)strlen(sql) + 1;
	
	// build packet
	packet_size = datasize + nsizedim;
	csql_initrequest(db, packet_size, nfields, command_type, kNO_SELECTOR);
	field_size[0] = htonl(datasize);
	
	if (command_type == kCOMMAND_SELECT) {
		if (server_side == kTRUE) SETBIT(db->request.flag1, CLIENT_REQUEST_SERVER_SIDE);
	}
	else if (is_partial == kTRUE)
		SETBIT(db->request.flag1, CLIENT_PARTIAL_PACKET);
	
	return csql_netwrite(db, (char *) field_size, nsizedim, (char *) sql, datasize);
}

csqlc *csql_read_cursor (csqldb *db, csqlc *existing_c) {
	csqlc	*c = NULL;
	int		index, gdone = kFALSE, is_partial = kFALSE;
	int		has_tables, has_rowid, nfields, server_rowcount, server_colcount, cursor_colcount;
	char	*buffer;
	int		i, nrows, ncols, count, data_seek = 0, end_chuck;
	int		*server_types, *server_sizes, *server_sum;
	char	*server_names, *server_data, *server_tables;
	
	// allocate basic cursor struct
	if (existing_c == NULL) {
		index = 0;
		c = csql_cursor_alloc(db);
	}
	else {
		index = 1;
		c = existing_c;
	}
			
	if (c == NULL) {
		csql_seterror(db, CUBESQL_MEMORY_ERROR, "Unable to allocate cursor struct");
		return NULL;
	}

	// loop to receive cursor
	do {
		if (csql_netread (db, -1, -1, kFALSE, &end_chuck, NO_TIMEOUT) != CUBESQL_NOERR) goto abort;
		if (end_chuck == kTRUE) {
			
			gdone = kTRUE;
			if (c->server_side) c->eof = kTRUE;
			//else if (db->client_version == k2007PROTOCOL) csql_ack(db, kCHUNK_OK);
			continue;
		}
		
		// decode reply
		has_tables = kFALSE;
		has_rowid = kFALSE;
		if (TESTBIT(db->reply.flag1, SERVER_HAS_TABLE_NAME)) has_tables = kTRUE;
		if (TESTBIT(db->reply.flag1, SERVER_PARTIAL_PACKET)) is_partial = kTRUE;
		if (TESTBIT(db->reply.flag1, SERVER_HAS_ROWID_COLUMN)) has_rowid = kTRUE;
		if (TESTBIT(db->reply.flag1, SERVER_SERVER_SIDE)) c->server_side = kTRUE;
		if (c->server_side) is_partial = kFALSE;
		
		nfields = ntohl(db->reply.numFields);
		server_rowcount = ntohl(db->reply.rows);
		server_colcount = ntohl(db->reply.cols);
		cursor_colcount = (has_rowid ? server_colcount-1 : server_colcount);
		nrows = server_rowcount;
		ncols = cursor_colcount;
		
		// adjust pointers
		buffer = db->inbuffer;
		if (c->server_side == kFALSE)
			server_sum = (int *) malloc(server_rowcount * server_colcount * sizeof(int));
		else
		{
			if (c->psum == NULL)
				server_sum = (int *) malloc(server_rowcount * server_colcount * sizeof(int));
			else
				server_sum = c->psum;
		}
		if (server_sum == NULL) goto abort_memory;
		
		// set buffers
		server_tables = NULL;
		if (index == 0) {
			char	*temp;
			int		len;
			
			if (c->server_side) c->p0 = buffer;
			server_types = (int *) buffer;
			server_sizes = (int *) (buffer + (sizeof(int) * server_colcount));
			server_names = (char *) server_sizes + (server_rowcount * server_colcount * sizeof(int));
			server_data = server_names;
			temp = server_names;
			for (i=0; i < server_colcount; i++) {
				len = (int)strlen(temp) + 1;
				data_seek += len;
				temp += len;
				server_types[i] = ntohl(server_types[i]);
			}
			
			if (has_tables) {
				server_tables = server_data + data_seek;
				temp = server_tables;
				for (i=0; i < server_colcount; i++) {
					len = (int)strlen(temp) + 1;
					data_seek += len;
					temp += len;
				}
			}
			server_data += data_seek;
			c->data_seek = data_seek;
		} else {
			server_types = NULL;
			server_names = NULL;
			server_tables = NULL;
			server_sizes = (int *) buffer;
			server_data = (char *) server_sizes + (server_rowcount * server_colcount * sizeof(int));
		}
		
		// adjust endianess of the size buffer and compute the sum buffer
		count = server_colcount * server_rowcount;
		for (i=0; i < count; i++) {
			server_sizes[i] = ntohl(server_sizes[i]);
			if (server_sizes[i] == -1) {
				// special NULL case
				if (i == 0) server_sum[i] = 0;
				else server_sum[i] = server_sum[i-1];
			} else {
				if (i == 0) server_sum[i] = server_sizes[i];
				else server_sum[i] = server_sizes[i] + server_sum[i-1];
			}
		}
		
		if ((is_partial) && (c->nbuffer >= c->nalloc)) {
			if (csql_cursor_reallocate (c) == kFALSE) goto abort_memory;
		}
		
		// adjust others counters/pointers
		if (index == 0) {
			c->types = server_types;
			c->size = server_sizes;
			c->names = server_names;
			c->tables = server_tables;
			c->data = server_data;
			c->psum = server_sum;
			
			// to speedup cubesql_cursor_value in the in_chunk case
			c->data0 = server_data;
			c->size0 = server_sizes;
		}
		
		// adjust pointers for server side cursors
		if ((c->server_side) && (index > 0)) {
			c->index++;
			if ((c->index > 1) && (c->p0 != (char *)c->size)) free(c->size);
			c->types = (int *) c->p0;
			c->names = (char *) (c->p0 + (sizeof(int) * server_colcount));
			c->size = server_sizes;
			c->data = server_data;
			c->psum = server_sum;
			
			// to speedup csqlcursor_value in the in_chunk case
			c->data0 = c->data;
			c->size0 = c->size;
		}
		 
		//if (db->protocol == k2009PROTOCOL) c->cursor_id = ntohs(db->reply.index);
		c->has_rowid = has_rowid;
		c->nrows += nrows;
		c->ncols = ncols;
		
		if (is_partial == kFALSE) {
			c->p = buffer;
			c->psum = server_sum;
		} else {
			c->buffer[c->nbuffer] = buffer;
			c->rowsum[c->nbuffer] = server_sum;
			c->rowcount[c->nbuffer] = c->nrows;
			c->nbuffer++;
		}
		
		// reset inbuffer
		db->inbuffer = NULL;
		db->insize = 0;
		
		// send ACK only in case of chunk cursor
		if ((is_partial == kTRUE) && (c->server_side == kFALSE)) csql_ack(db, kCHUNK_OK);
		else gdone = kTRUE;
		index++;
	}
	while (gdone != kTRUE);
	return c;

abort_memory:
	csql_seterror(db, CUBESQL_MEMORY_ERROR, "Not enought memory to allocate buffer required to build the cursor");
	
abort:
	if ((c) && (existing_c != NULL)) cubesql_cursor_free(c);
	return NULL;
}

int csql_connect_encrypted (csqldb *db) {
	unsigned char	rand1[kRANDPOOLSIZE], rand2[kRANDPOOLSIZE], rand3[kRANDPOOLSIZE];
	unsigned char	hash1[SHA1_DIGEST_SIZE], hash2[SHA1_DIGEST_SIZE], hash3[SHA1_DIGEST_SIZE];
	char			buffer1[SHA1_DIGEST_SIZE+kRANDPOOLSIZE];
	char			hash[SHA1_DIGEST_SIZE*2+2];
	char			*p = NULL;
	int				len, nfields, nsizedim, datasize0, packet_size, field_size[4];
	int				encryption = db->encryption;
	int				len2 = 0, is_token = kFALSE;
	char			*token = NULL, *enc_token = NULL;
	csql_aes_encrypt_ctx ctx[1];
	csql_aes_decrypt_ctx ctxd[1];
	
	// reset encryption
	// because session key hasn't yet been computed
	db->encryption = CUBESQL_ENCRYPTION_NONE;
	
	// check if its a token connect
	token = cubesql_gettoken(db);
	if (token != NULL) is_token = kTRUE;
	
	// ENCRYPT CONNECT PHASE 1
	// CLIENT SENDS HASH(USERNAME,X)
	// AND AESCBC(X;H(X),H(H(P))) WHERE X is a 20-byte random number
	
	// Generate the 20-byte random number X
	csql_rand_fill((char *)rand1);
	
	// Compute H(X)
	hash_field ((unsigned char*) hash1, (const char *)rand1, kRANDPOOLSIZE, 1);
	
	// Compute H(H(P))
	hash_field ((unsigned char*) hash2, (const char *)db->password, (int)strlen(db->password), 2);
	
	// Prepare the 128 bit encryption key
	csql_aes_encrypt_key ((unsigned char*) hash2, 16, ctx);
	
	// Prepare X;H(X)
	memcpy (buffer1, rand1, kRANDPOOLSIZE);
	memcpy (buffer1+kRANDPOOLSIZE, hash1, SHA1_DIGEST_SIZE);
	
	// Generate AESCBC(X;H(X),H(H(P)))
	csql_rand_fill((char *)rand2);
	len = encrypt_buffer (buffer1, SHA1_DIGEST_SIZE+kRANDPOOLSIZE, (char *)rand2, ctx);
	
	// PHASE 1: SEND DATA
	
	// build packet
	nfields = 2;
	nsizedim = sizeof(int) * nfields;
	
	if (db->useOldProtocol == kFALSE) {
		hex_hash_field2(hash, db->username, rand2);
		datasize0 = (int)strlen(hash) + 1;
		p = (char *)hash;
	} else {
		p = (char *)db->username;
		datasize0 = (int)strlen(db->username) + 1;
	}
	
	packet_size = datasize0 + len + nsizedim;
	csql_initrequest(db, packet_size, nfields, kCOMMAND_CONNECT, (is_token) ? kENCRYPT_TOKEN_CONNECT1 : kENCRYPT_CONNECT_PHASE1);
	db->request.encryptedPacket = encryption;
	field_size[0] = htonl(datasize0);
	field_size[1] = htonl(len);
	
	// send header request
	if (csql_socketwrite(db, (char *)&db->request, kHEADER_SIZE) != CUBESQL_NOERR) goto abort_connect;
	
	// send size array
	if (csql_socketwrite(db, (char *)field_size, nsizedim) != CUBESQL_NOERR) goto abort_connect;
	
	// send username
	if (csql_socketwrite(db, (char *)p, datasize0) != CUBESQL_NOERR) goto abort_connect;
	
	// send rand2
	if (csql_socketwrite(db, (char *)rand2, BLOCK_LEN) != CUBESQL_NOERR) goto abort_connect;
	
	// send encrypted data
	if (csql_socketwrite(db, (char *)buffer1, SHA1_DIGEST_SIZE+kRANDPOOLSIZE) != CUBESQL_NOERR) goto abort_connect;
	
	// ENCRYPT CONNECT PHASE 1.5
	// CLIENT RECEIVES A 20B RANDOM NUMBER FROM THE SERVER:
	// AESCBC(Y;H(Y),H(H(P))) WHERE Y is a 20-byte random number
	
	// read reply (encrypted random pool)
	if (csql_netread(db, BLOCK_LEN + kRANDPOOLSIZE + SHA1_DIGEST_SIZE, 1, kFALSE, NULL, CONNECT_TIMEOUT) != CUBESQL_NOERR) goto abort_connect;
	
	// Decrypt message using H(H(P))
	// Prepare the 128 bit decryption key
	csql_aes_decrypt_key ((unsigned char*) hash2, 16, ctxd);
	decrypt_buffer(db->inbuffer, db->insize, ctxd);
	
	// Now inbuffer is Y;H(Y)
	// Generate H(Y) from Y and compares it to the H(Y) sent by the server 
	hash_field (hash3, (const char *)db->inbuffer, kRANDPOOLSIZE, 1);
	if (memcmp((const void *)hash3, (const void *) (db->inbuffer+kRANDPOOLSIZE), SHA1_DIGEST_SIZE) != 0) goto abort_connect;
	
	// Generate SessionKey by both client and server
	generate_session_key(db, encryption, (char *)hash2, (char *)rand1, (char *)db->inbuffer);
	
	// ENCRYPT CONNECT PHASE 2
	// CLIENT SENDS AESCBC(H(P),S) where S is the session_key
	
	// Compute H(P)
	hash_field ((unsigned char*) hash2, (const char *)db->password, (int)strlen(db->password), 1);
	csql_rand_fill ((char *)rand2);
	len = encrypt_buffer ((char *)hash2, SHA1_DIGEST_SIZE, (char *)rand2, db->encryptkey);
	
	if (is_token) {
		int tlen = 0;
		csql_rand_fill ((char *)rand3);
		tlen = (int)strlen(token)+1;
		enc_token = (char *) malloc(tlen + kRANDPOOLSIZE);
		if (enc_token == NULL) goto abort_connect;
		enc_token[0] = 0;
		strcpy(enc_token, token);
		len2 = encrypt_buffer ((char *)enc_token, tlen, (char *)rand3, db->encryptkey);
	}
	
	// PHASE 1: SEND DATA
	nfields = 1;
	if (is_token) nfields++;
	nsizedim = sizeof(int) * nfields;
	
	// build packet
	packet_size = len + nsizedim;
	if (is_token) packet_size += len2;
	csql_initrequest(db, packet_size, nfields, kCOMMAND_CONNECT, (is_token) ? kENCRYPT_TOKEN_CONNECT2 : kENCRYPT_CONNECT_PHASE2);
	field_size[0] = htonl(len);
	if (is_token) field_size[1] = htonl(len2);
	
	// send header request
	if (csql_socketwrite(db, (char *)&db->request, kHEADER_SIZE) != CUBESQL_NOERR) goto abort_connect;
	
	// send size array
	if (csql_socketwrite(db, (char *)field_size, nsizedim) != CUBESQL_NOERR) goto abort_connect;
	
	// send rand2
	if (csql_socketwrite(db, (char *)rand2, BLOCK_LEN) != CUBESQL_NOERR) goto abort_connect;
	
	// send hash2
	if (csql_socketwrite(db, (char *)hash2, SHA1_DIGEST_SIZE) != CUBESQL_NOERR) goto abort_connect;
	
	// send token
	if (is_token) {
		if (csql_socketwrite(db, (char *)rand3, BLOCK_LEN) != CUBESQL_NOERR) goto abort_connect;
		if (csql_socketwrite(db, (char *)enc_token, (int)strlen(token)+1) != CUBESQL_NOERR) goto abort_connect;
	}
	
	// read header reply and sanity check it
	if (csql_netread(db, 0, 0, kFALSE, NULL, CONNECT_TIMEOUT) != CUBESQL_NOERR) goto abort_connect;
	
	if ((is_token) && (enc_token)) free(enc_token);
	db->encryption = encryption;
	return CUBESQL_NOERR;
	
abort_connect:
	if ((is_token) && (enc_token)) free(enc_token);
	db->encryption = encryption;
	return CUBESQL_ERR;
}

int csql_connect (csqldb *db, int encryption) {
	int		field_size[2];
	int		is_token = kFALSE;
	int		nfields, nsizedim, packet_size, datasize = 0;
	char	hval[SHA1_DIGEST_SIZE];
	char	hash[SHA1_DIGEST_SIZE*2+2];
	char	*token = NULL, *p = NULL;
	
	db->sockfd = csql_socketconnect(db);
	if (db->sockfd <= 0) goto abort_connect;
	
	if (encryption_is_ssl(encryption)) encryption -= CUBESQL_ENCRYPTION_SSL;
	if (encryption != CUBESQL_ENCRYPTION_NONE) return csql_connect_encrypted (db);
	
	// check if its a token connect
	token = cubesql_gettoken(db);
	if (token != NULL) is_token = kTRUE;
	
	// CLEAR CONNECT PHASE 1
	// CLIENT SENDS HASH(USERNAME)
	// AND RECEIVE A 20bytes RANDOM POOL DATA FROM THE SERVER
	// in the old protocol username was sent in clear
	nfields = 1;
	nsizedim = sizeof(int) * nfields;
	if (db->useOldProtocol == kFALSE) {
		hex_hash_field(hash, db->username, (int)strlen(db->username));
		datasize = (int)strlen(hash) + 1;
		p = (char *)hash;
	} else {
		datasize = (int)strlen(db->username) + 1;
		p = (char *)db->username;
	}
	
	// build packet
	packet_size = datasize + nsizedim;
	csql_initrequest(db, packet_size, nfields, kCOMMAND_CONNECT, (is_token) ? kCLEAR_TOKEN_CONNECT1 : kCLEAR_CONNECT_PHASE1);
	field_size[0] = htonl(datasize);
	
	// send header
	if (csql_socketwrite(db, (char *)&db->request, kHEADER_SIZE) != CUBESQL_NOERR) goto abort_connect;
	
	// send size array
	if (csql_socketwrite(db, (char *)field_size, nsizedim) != CUBESQL_NOERR) goto abort_connect;
	
	// send hash (username)
	if (csql_socketwrite(db, (char *)p, datasize) != CUBESQL_NOERR) goto abort_connect;
	
	// read random pool
	if (csql_netread(db, kRANDPOOLSIZE, 1, kFALSE, NULL, CONNECT_TIMEOUT) != CUBESQL_NOERR) goto abort_connect;
	
	// CLEAR CONNECT PHASE 2
	// CLIENT ENCRYPT PASSWORD WITH THE RANDOM POOL
	// AND SEND ITS HASH TO THE SERVER
	
	// encrypt password with random pool
	random_hash_field((unsigned char *)hval, (const char *)db->inbuffer, db->password);
	
	nfields = 1;
	if (is_token) nfields++;
	nsizedim = sizeof(int) * nfields;
	datasize = SHA1_DIGEST_SIZE;
	if (is_token) datasize += strlen(token)+1;
	
	// build packet
	packet_size = datasize + nsizedim;
	csql_initrequest(db, packet_size, nfields, kCOMMAND_CONNECT, (is_token) ? kCLEAR_TOKEN_CONNECT2 : kCLEAR_CONNECT_PHASE2);
	field_size[0] = htonl(SHA1_DIGEST_SIZE);
	if (is_token) field_size[1] = htonl(strlen(token)+1);
	
	// send header
	if (csql_socketwrite(db, (char *)&db->request, kHEADER_SIZE) != CUBESQL_NOERR) goto abort_connect;
	
	// send size array
	if (csql_socketwrite(db, (char *)field_size, nsizedim) != CUBESQL_NOERR) goto abort_connect;
	
	// send SH1 (Encrypted password)
	if (csql_socketwrite(db, (char *)hval, SHA1_DIGEST_SIZE) != CUBESQL_NOERR) goto abort_connect;
	
	// send token
	if (is_token) if (csql_socketwrite(db, (char *)token, (int)strlen(token)+1) != CUBESQL_NOERR) goto abort_connect;
	
	// read header reply and sanity check it
	if (csql_netread(db, 0, 0, kFALSE, NULL, CONNECT_TIMEOUT) != CUBESQL_NOERR) goto abort_connect;
	
	return CUBESQL_NOERR;
	
abort_connect:
	return CUBESQL_ERR;
}

int csql_netread (csqldb *db, int expected_size, int expected_nfields, int is_chunk, int *end_chunk, int timeout) {
	int is_end_chunk = kFALSE;
	
	// read header first
	if (csql_socketread(db, kTRUE, timeout) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	// check header
	if (csql_checkheader(db, expected_size, expected_nfields, &is_end_chunk) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	// check end_chunk case
	if (end_chunk) *end_chunk = is_end_chunk;
	if ((is_chunk) && (is_end_chunk)) {*end_chunk = is_end_chunk; return CUBESQL_NOERR;}
	if (db->toread == 0) return CUBESQL_NOERR;
	
	// if there is something more to read, then read everything into the inbuffer
	if (csql_checkinbuffer(db) != CUBESQL_NOERR) return CUBESQL_ERR;
	if (csql_socketread(db, kFALSE, timeout) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	// check if packet is encrypted
	if (db->reply.encryptedPacket != CUBESQL_ENCRYPTION_NONE)
		decrypt_buffer(db->inbuffer, db->toread, db->decryptkey);
	
	// check if packet is compressed
	if (TESTBIT(db->reply.flag1, SERVER_COMPRESSED_PACKET)) {
		int		exp_size = ntohl(db->reply.expandedSize);
		uLong	zExpSize = (uLong)exp_size;
		char	*buffer;
		
		buffer = (char *) malloc(exp_size);
		if (buffer == NULL) {
			csql_seterror(db, CUBESQL_MEMORY_ERROR, "Not enought memory to allocate buffer required by the cursor");
			return CUBESQL_ERR;
		}
		
		if (uncompress((Bytef *)buffer, &zExpSize, (Bytef *)db->inbuffer, (uLong)db->toread) != Z_OK) {
			csql_seterror(db, CUBESQL_ZLIB_ERROR, "An error occurred while trying to uncompress received cursor");
			free(buffer);
			return CUBESQL_ERR;
		}
		
		free (db->inbuffer);
		db->inbuffer = buffer;
		db->insize = exp_size;
	}
	
	return CUBESQL_NOERR;
}

int csql_checkinbuffer (csqldb *db) {
	if (db->insize >= db->toread) return CUBESQL_NOERR;
	
	if (db->inbuffer) free(db->inbuffer);
	db->insize = 0;
	db->inbuffer = (char *) malloc (db->toread);
	if (db->inbuffer == NULL) {
		csql_seterror(db, CUBESQL_MEMORY_ERROR, "Unable to allocate inbuffer");
		return CUBESQL_ERR;
	}
		
	db->insize = db->toread;
	return CUBESQL_NOERR;
}

int csql_netwrite (csqldb *db, char *size_array, int nsize_array, char *buffer, int nbuffer) {
	char rand1[kRANDPOOLSIZE], *encbuffer = NULL;
	
	// send header request
	if (csql_socketwrite(db, (char *)&db->request, kHEADER_SIZE) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	// send size array
	if (size_array) if (csql_socketwrite(db, size_array, nsize_array) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	// send buffer as is if it's a not encrypted channel
	if (db->encryption == CUBESQL_ENCRYPTION_NONE) {
		if (buffer) if (csql_socketwrite(db, buffer, nbuffer) != CUBESQL_NOERR) return CUBESQL_ERR;
		return CUBESQL_NOERR;
	}
	
	// case buffer is NULL
	if (buffer == NULL) return CUBESQL_NOERR;
	
	// generate random pool and encrypt buffer
	csql_rand_fill(rand1);
	encbuffer = (char *) malloc (nbuffer+1);
	if (encbuffer == NULL) {
		csql_seterror(db, CUBESQL_MEMORY_ERROR, "Unable to allocate encbuffer");
		return CUBESQL_ERR;
	}
	memcpy (encbuffer, buffer, nbuffer);
	encrypt_buffer ((char *)encbuffer, nbuffer, rand1, db->encryptkey);
	
	// send random pool
	if (csql_socketwrite(db, rand1, BLOCK_LEN) != CUBESQL_NOERR) goto abort;
	
	// send encrypted buffer
	if (csql_socketwrite(db, encbuffer, nbuffer) != CUBESQL_NOERR) goto abort;
	
	if (encbuffer) free(encbuffer);
	return CUBESQL_NOERR;

abort:
	if (encbuffer) free(encbuffer);
	return CUBESQL_ERR;
}

int csql_sendchunk (csqldb *db, char *buffer, int bufferlen, int buffertype, int is_bind) {
	int		err, bsize, is_compressed;
	char	*b, *dest;
	uLong	newlen;
	
	b = buffer;
	dest = NULL;
	bsize = bufferlen;
	is_compressed = kFALSE;
	
	// try to compress buffer, in case of error just use the uncompressed one
	newlen = compressBound(bufferlen);;
	dest = (char *) malloc (newlen);
	if (dest != NULL) {
		if (compress2((Bytef*)dest, &newlen, (Bytef*)buffer, (uLong)bufferlen, Z_DEFAULT_COMPRESSION) == Z_OK) {
			b = dest;
			bsize = (int)newlen;
			is_compressed = kTRUE;
		} else {free(dest); dest = NULL;}
	}
	
	// build packet, the chunk command never sends the field_size, nfield should be set to 1
	if (is_bind) {
		csql_initrequest(db, bsize, 1, kCOMMAND_CHUNK_BIND, kBIND_STEP);
		db->request.flag3 = (unsigned char) buffertype;
	}
	else csql_initrequest(db, bsize, 1, kCOMMAND_CHUNK, kNO_SELECTOR);
	SETBIT(db->request.flag1, CLIENT_PARTIAL_PACKET);
	
	if (is_compressed == kTRUE) {
		SETBIT(db->request.flag1, CLIENT_COMPRESSED_PACKET);
		db->request.expandedSize = htonl(bufferlen);
	}
	
	err = csql_netwrite(db, NULL, 0, b, bsize);
	if (dest != NULL) free(b);
	
	return err;
}

char *csql_receivechunk (csqldb *db, int *len, int *is_end_chunk) {
	int err = csql_netread(db, -1, -1, kTRUE, is_end_chunk, NO_TIMEOUT);
	if (err == CUBESQL_ERR) csql_ack(db, kCHUNK_ABORT);
	if (err != CUBESQL_NOERR) return NULL;
	
	*len = db->insize;
	return db->inbuffer;
}

int csql_ack(csqldb *db, int chunk_code) {
	if (chunk_code == kCOMMAND_ENDCHUNK) {
		csql_initrequest(db, 0, 0, kCOMMAND_ENDCHUNK, kNO_SELECTOR);
		csql_netwrite(db, NULL, 0, NULL, 0);
		goto read_reply;
	}
	
	if ((chunk_code == kBIND_FINALIZE) || (chunk_code == kBIND_ABORT)) {
		csql_initrequest(db, 0, 0, kCOMMAND_CHUNK_BIND, chunk_code);
		csql_netwrite(db, NULL, 0, NULL, 0);
		goto read_reply;
	}
	
	// other cases
	csql_initrequest(db, 0, 0, kCOMMAND_CHUNK, chunk_code);
	return csql_netwrite(db, NULL, 0, NULL, 0);
	
read_reply:	
	return csql_netread(db, -1, -1, kFALSE, NULL, NO_TIMEOUT);
}

int csql_socketwrite (csqldb *db, const char *buffer, int nbuffer) {
	int fd, ret, nwritten, nleft = nbuffer;
	const char *ptr = buffer;
	fd_set except_fds;
	fd_set write_fds;
	struct timeval tv;
	
	fd = db->sockfd;
	while (nleft > 0) {
		FD_ZERO(&write_fds);
		FD_SET(fd, &write_fds);
		FD_ZERO(&except_fds);
		FD_SET(fd, &except_fds);
		
		tv.tv_sec = db->timeout;
		tv.tv_usec = 0;
		
		ret = bsd_select(fd+1, NULL, &write_fds, &except_fds, &tv);
		
		// something wrong occurred
		if (FD_ISSET(fd, &except_fds)) {
			csql_seterror(db, ERR_SOCKET, "select returns except_fds inside csql_socketwrite");
			return CUBESQL_ERR;
		}
		
		// check if it is a real error
		if (ret == -1) {
			int err = csql_socketerror(fd);
			if (err == 0) continue;
			
			csql_seterror(db, err, "An error occurred inside csql_socketwrite");
			return CUBESQL_ERR;
		}
		
		// ret = 0 means timeout
		if (ret <= 0) {
			csql_seterror(db, ERR_SOCKET_TIMEOUT, "A timeout error occurred inside csql_socketwrite");
			return CUBESQL_ERR;
		}
		
		if (FD_ISSET(fd, &write_fds)) {
			FD_CLR(fd, &write_fds);
			
			#ifndef CUBESQL_DISABLE_SSL_ENCRYPTION
			nwritten = (db->tls_context) ? (int)tls_write(db->tls_context, ptr, nleft) : (int)sock_write(fd, ptr, nleft);
			#else
			nwritten = (int)sock_write(fd, ptr, nleft);
			#endif

			if (nwritten <= 0) {
				csql_seterror(db, ERR_SOCKET_WRITE, "An error occurred while trying to execute sock_write");
				return CUBESQL_ERR;
			}
			
			nleft -= nwritten;
			ptr += nwritten;
		}
	}
	
	return CUBESQL_NOERR;
}

int csql_socketread (csqldb *db, int is_header, int timeout) {
	int		nread, nleft, ret, fd = db->sockfd;
	char	*ptr;
	fd_set read_fds;
	fd_set except_fds;
	struct timeval tv;
	
	if (is_header == kTRUE) {
		ptr = (char *)&db->reply;
		nleft = kHEADER_SIZE;
	}
	else {
		ptr = (char *)db->inbuffer;
		nleft = db->toread;
	}
	
	while (1) {
		FD_ZERO(&read_fds);
		FD_SET(fd, &read_fds);
		FD_ZERO(&except_fds);
		FD_SET(fd, &except_fds);
		
		if (timeout != NO_TIMEOUT) {
			tv.tv_sec = timeout;
			tv.tv_usec = 0;
			ret = bsd_select(fd+1, &read_fds, NULL, &except_fds, &tv);
		} else
			ret = bsd_select(fd+1, &read_fds, NULL, &except_fds, NULL);
		
		if (FD_ISSET(fd, &except_fds)) {
			// this may only happen on Windows
			csql_seterror(db, ERR_SOCKET_READ, "select returns except_fds inside csql_socketread");
			return CUBESQL_ERR;
		}
		
		// check if it is a real error
		if (ret == -1) {
			int err = csql_socketerror(fd);
			if (err == 0) continue;
			
			csql_seterror(db, err, "An error occurred while executing csql_socketread");
		}
		
		// ret = 0 means timeout
		if (ret <= 0) {
			csql_seterror(db, ERR_SOCKET_TIMEOUT, "A timeout error occurred inside csql_socketread");
			return CUBESQL_ERR;
		}
		
		#ifndef CUBESQL_DISABLE_SSL_ENCRYPTION
		nread = (db->tls_context) ? (int)tls_read(db->tls_context, ptr, nleft) : (int)sock_read(fd, ptr, nleft);
		#else
		nread = (int)sock_read(fd, ptr, nleft);
		#endif
		
		if (nread == -1 || nread == 0) {
			csql_seterror(db, ERR_SOCKET_READ, "An error occurred while executing sock_read");
			return CUBESQL_ERR;
		}
		
		nleft -= nread;
		ptr += nread;
		
		if (nleft == 0)
			return CUBESQL_NOERR;
	}
	
	return CUBESQL_NOERR;
}

void csql_seterror(csqldb *db, int errcode, const char *errmsg) {
	db->errcode = errcode;
	strncpy(db->errmsg, errmsg, sizeof(db->errmsg));
}

int csql_socketerror (int fd) {
	int			err, sockerr, err2;
	socklen_t	errlen = sizeof(err);
	
	sockerr = bsd_getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen);
	err2 = errno;
	
	if (sockerr < 0)
		return -1;
	
	if (err == 0 || err == EINTR || err == EAGAIN || err == EINPROGRESS)
		return 0;
	
	return err;	
}

int csql_checkheader(csqldb *db, int expected_size, int expected_nfields, int *end_chunk) {
	outhead *header = &db->reply;
	unsigned int	signature;
	int	err, dsize, nfields;
	
	if (end_chunk) *end_chunk = kFALSE;
	db->toread = 0;
	
	if (header == NULL) {
		csql_seterror (db, ERR_WRONG_HEADER, "Received a NULL header from the server");
		return CUBESQL_ERR;
	}
	
	signature = ntohl(header->signature);
	if (signature != PROTOCOL_SIGNATURE) {
		csql_seterror (db, ERR_WRONG_SIGNATURE, "Wrong SIGNATURE HEADER from the server");
		return CUBESQL_ERR;
	}
	
	err = ntohs(header->errorCode);
	if (err == END_CHUNK) {
		if (end_chunk) *end_chunk = kTRUE;
		err = 0;
	}
	
	dsize = ntohl(header->packetSize);
	if ((err == 0) && (expected_size != -1) && (expected_size != dsize)) {
		csql_seterror (db, ERR_WRONG_SIGNATURE, "Wrong PACKET SIZE received from the server");
		return CUBESQL_ERR;
	}
	db->toread = dsize;
	
	nfields = ntohl(header->numFields);
	if ((err == 0) && (expected_nfields != -1) && (expected_nfields != nfields)) {
		csql_seterror (db, ERR_WRONG_SIGNATURE, "Wrong NUMBER OF FIELDS received from the server");
		return CUBESQL_ERR;
	}
	
	if ((err != 0) && (dsize)) {
		// read error string
		// it is an error and it is dsize long, so check if we can reuse the static buffer
		int	use_static = kFALSE;
		
		if (dsize < sizeof(db->errmsg)) {
			use_static = kTRUE;
			db->inbuffer = db->errmsg;
			db->inbuffer[dsize] = 0;
			db->insize = dsize;
			db->errcode = err;
		}
		else if (csql_checkinbuffer(db) != CUBESQL_NOERR) return CUBESQL_ERR;
		
		if (csql_socketread(db, kFALSE, NO_TIMEOUT) != CUBESQL_NOERR) return CUBESQL_ERR;
		db->toread = 0;
		
		if (db->reply.encryptedPacket != CUBESQL_ENCRYPTION_NONE)
			decrypt_buffer(db->inbuffer, dsize, db->decryptkey);
		
		if (use_static == kFALSE) csql_seterror (db, err, db->inbuffer);
		if ((use_static == kFALSE) && (db->inbuffer)) free(db->inbuffer);
		
		db->inbuffer = NULL;
		db->insize = 0;
		return CUBESQL_ERR;
	}
	
	return CUBESQL_NOERR;
}

void csql_initrequest (csqldb *db, int packetsize, int nfields, char command, char selector) {
	inhead *request = &db->request;
	
	bzero(request, sizeof(inhead));
	request->signature = htonl(PROTOCOL_SIGNATURE);
	
	if ((packetsize != 0) && (db->encryption != CUBESQL_ENCRYPTION_NONE)) packetsize += BLOCK_LEN;
	
	request->packetSize = htonl(packetsize);
	request->command = command;
	request->selector = selector;
	request->flag1 = kEMPTY_FIELD;
	SETBIT(request->flag1, CLIENT_SUPPORT_COMPRESSION);
	request->flag2 = kEMPTY_FIELD;
	request->flag3 = kEMPTY_FIELD;
	request->encryptedPacket = db->encryption;
	request->numFields = htonl(nfields);
	request->expandedSize = 0;
	request->timeout = htonl(db->timeout);
	
	if (db->useOldProtocol == kTRUE)
		request->protocolVersion = k2007PROTOCOL;
	else
		request->protocolVersion = k2011PROTOCOL;
}

csqlc *csql_cursor_alloc (csqldb *db)
{
	csqlc *cursor = NULL;
	
	cursor = (csqlc*) malloc (sizeof(csqlc));
	if (cursor == NULL) return NULL;
	
	// initialize cursor structure
	bzero (cursor, sizeof(csqlc));
	cursor->db = db;
	cursor->current_row = 1;
	
	return cursor;
}

int csql_cursor_reallocate (csqlc *c) {
	if (c->nalloc == 0) {
		c->buffer = (char**) malloc(sizeof(char*) * kNUMBUFFER);
		if (c->buffer == NULL) return kFALSE;
		
		c->rowsum = (int**) malloc(sizeof(int*) * kNUMBUFFER);
		if (c->rowsum == NULL) return kFALSE;
		
		c->rowcount = (int*) malloc(sizeof(int) * kNUMBUFFER);
		if (c->rowcount == NULL) return kFALSE;
		
		c->nalloc = kNUMBUFFER;
	} else {
		
		char **tmp1;
		int	 **tmp2;
		int *tmp3;
		int	 oldsize, newsize;
		
		oldsize = sizeof(char*) * c->nalloc;
		newsize = oldsize + (sizeof(char*) * kNUMBUFFER);
		tmp1 = (char**) realloc(c->buffer, newsize);
		if (tmp1 == NULL) return kFALSE;
		c->buffer = tmp1;
		
		oldsize = sizeof(int*) * c->nalloc;
		newsize = oldsize + (sizeof(int*) * kNUMBUFFER);
		tmp2 = (int**) realloc(c->rowsum, newsize);
		if (tmp2 == NULL) return kFALSE;
		c->rowsum = tmp2;
		
		oldsize = sizeof(int) * c->nalloc;
		newsize = oldsize + (sizeof(int) * kNUMBUFFER);
		tmp3 = (int*) realloc(c->rowcount, newsize);
		if (tmp3 == NULL) return kFALSE;
		c->rowcount = tmp3;
		
		c->nalloc += kNUMBUFFER;
	}	
	
	return kTRUE;
}

int csql_cursor_step (csqlc *c) {
	// prepare header request
	csql_initrequest(c->db, 0, 0, kCOMMAND_CURSOR_STEP, kNO_SELECTOR);
	
	// send header request
	if (csql_socketwrite(c->db, (char *)&c->db->request, kHEADER_SIZE) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	// receive row
	if (csql_read_cursor(c->db, c) == NULL) return CUBESQL_ERR;
	
	return CUBESQL_NOERR;
}

int csql_cursor_close (csqlc *c) {
	// prepare header request
	csql_initrequest(c->db, 0, 0, kCOMMAND_CURSOR_CLOSE, kNO_SELECTOR);
	
	// send header request
	if (csql_socketwrite(c->db, (char *)&c->db->request, kHEADER_SIZE) != CUBESQL_NOERR) return CUBESQL_ERR;
	
	return csql_netread(c->db, -1, -1, kFALSE, NULL, NO_TIMEOUT);
}

// MARK: - SSL -

const char *cubesql_sslversion (void) {
	#ifndef CUBESQL_DISABLE_SSL_ENCRYPTION
	return SSLeay_version(0);
	#else
	return NULL;
	#endif
}

unsigned long cubesql_sslversion_num (void) {
	#ifndef CUBESQL_DISABLE_SSL_ENCRYPTION
	return 0x3080200fL;
	#endif
	return 0;
}

int encryption_is_ssl (int encryption) {
	if ((encryption == CUBESQL_ENCRYPTION_SSL) || (encryption == CUBESQL_ENCRYPTION_SSL_AES128) ||
		(encryption == CUBESQL_ENCRYPTION_SSL_AES192) || (encryption == CUBESQL_ENCRYPTION_SSL_AES256)) return kTRUE;
	return kFALSE;
}

// MARK: - Utils -

void hash_field (unsigned char hval[], const char *field, int len, int times) {
	// SHA1(P)
	csql_sha1((unsigned char *)hval, (const unsigned char*)field, len);
	
	// SHA1(SHA1(P))
	if (times == 2)
		csql_sha1((unsigned char *)hval, (const unsigned char *)hval, SHA1_DIGEST_SIZE);
}

void hex_hash_field (char result[], const char *field, int len) {
	unsigned char hval[SHA1_DIGEST_SIZE];
	
	csql_sha1(hval, (const unsigned char *)field, len);
	
	// convert result
	// result must be SHA1_DIGEST_SIZE*2+2 long
	snprintf(result, SHA1_DIGEST_SIZE*2+2, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			 hval[0],hval[1],hval[2],hval[3],hval[4],hval[5],hval[6],hval[7],hval[8],hval[9],hval[10],
			 hval[11],hval[12],hval[13],hval[14],hval[15],hval[16],hval[17],hval[18],hval[19]);
}

void hex_hash_field2 (char result[], const char *field, unsigned char *randpoll) {
	unsigned char hval[SHA1_DIGEST_SIZE];
	unsigned char randhex[BLOCK_LEN*2+2];
	char		  buffer[2048];
	int			  len = 0;
	
	if (strlen(field) > 256) return;
	
	// hexify randpool
	snprintf((char *)randhex, sizeof(randhex), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			 randpoll[0],randpoll[1],randpoll[2],randpoll[3],randpoll[4],randpoll[5],randpoll[6],randpoll[7],randpoll[8],
			 randpoll[9],randpoll[10],randpoll[11],randpoll[12],randpoll[13],randpoll[14],randpoll[15]);
	
	//printf("randpool %s\n", randhex);
	
	// concat two fields
	len = snprintf(buffer, sizeof(buffer), "%s%s", field, randhex);
	
	// compute hash
	csql_sha1(hval, (const unsigned char *)buffer, len);
	
	// convert result
	// result must be SHA1_DIGEST_SIZE*2+2 long
	snprintf(result, SHA1_DIGEST_SIZE*2+2, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			 hval[0],hval[1],hval[2],hval[3],hval[4],hval[5],hval[6],hval[7],hval[8],hval[9],hval[10],
			 hval[11],hval[12],hval[13],hval[14],hval[15],hval[16],hval[17],hval[18],hval[19]);
	
	//printf("result %s\n", result);
}

void random_hash_field (unsigned char hval[], const char *randpoll, const char *field) {
	char buffer[SHA1_DIGEST_SIZE+kRANDPOOLSIZE];
	char hval1[SHA1_DIGEST_SIZE];
	char hval2[SHA1_DIGEST_SIZE];
	
	// SHA1(P)
	csql_sha1((unsigned char *)hval1, (const unsigned char*)field, (int)strlen(field));
	
	// SHA1(SHA1(P))
	csql_sha1((unsigned char *)hval2, (const unsigned char *)hval1, SHA1_DIGEST_SIZE);
	
	memcpy(buffer, randpoll, kRANDPOOLSIZE);
	memcpy(buffer + kRANDPOOLSIZE, hval2, SHA1_DIGEST_SIZE);
	
	// SHA1(R;SHA1(SHA1(P)))
	csql_sha1((unsigned char *)hval, (const unsigned char *)buffer, kRANDPOOLSIZE+SHA1_DIGEST_SIZE);
}

int encrypt_buffer (char *buffer, int dim, char random[], csql_aes_encrypt_ctx ctx[1]) {
	char    dbuf[2 * BLOCK_LEN];
	int		i, len, index=0;
	char	*b1, *b2;
	
	memcpy(dbuf, random, BLOCK_LEN);
	
	if (dim < BLOCK_LEN) {
		// if the buffer is less than one block
		memcpy(dbuf + BLOCK_LEN, buffer, dim);
		
		// xor the file bytes with the random pool
		for(i = 0; i < dim; ++i)
			dbuf[i + BLOCK_LEN] ^= dbuf[i];
		
		// encrypt the top 16 bytes of the buffer
		csql_aes_encrypt((const unsigned char*) (dbuf + dim), (unsigned char*)(dbuf + dim), ctx);
		
		// copy back encrypted data
		memcpy(random, dbuf, BLOCK_LEN);
		memcpy(buffer, dbuf + BLOCK_LEN, dim);
		
		return (dim+BLOCK_LEN);
	}
	
	b1 = dbuf;
	b2 = buffer;
	len = dim;
	
	do {				
		// do CBC chaining prior to encryption for current block (in b2)
		for(i = 0; i < BLOCK_LEN; ++i)
			b1[i] ^= b2[i];
		
		// encrypt the block (now in b1)
		csql_aes_encrypt((const unsigned char*)b1, (unsigned char*)b1, ctx);
		
		len -= BLOCK_LEN;
		
		if (index == 0)
			memcpy(buffer, b1, BLOCK_LEN);
		
		// advance the buffer pointers
		if (len >= BLOCK_LEN) {
			b2 = buffer + (index * BLOCK_LEN);
			b1 = b2 + BLOCK_LEN;
		}
		index++;
	}
	while (len >= BLOCK_LEN);
	
	if (len != 0) {	
		char b3[BLOCK_LEN];
		char back[BLOCK_LEN];
		
		memcpy(b3, buffer + (index * BLOCK_LEN), len);
		
		// xor ciphertext into last block
		for(i = 0; i < len; ++i)
			b3[i] ^= b1[i];
		
		// move 'stolen' ciphertext into last block
		for(i = len; i < BLOCK_LEN; ++i)
			b3[i] = b1[i];
		
		// encrypt this block
	   	csql_aes_encrypt((const unsigned char*) b3, (unsigned char*) b3, ctx);
		
		// save b1
		memcpy(back, b1, BLOCK_LEN);
		
		// write b3
		memcpy(buffer+dim-len-BLOCK_LEN, b3, BLOCK_LEN);
		
		// write b1
		memcpy(buffer+dim-len, back, len);
	}
	
	return (dim + BLOCK_LEN);
}

int decrypt_buffer (char *buffer, int dim, csql_aes_decrypt_ctx ctx[1]) {
	int 	len, nextlen, i, index=0;
	char	*b1, *b2;
	char	buf[BLOCK_LEN], b3[BLOCK_LEN];
	
	if (dim < 2 * BLOCK_LEN) {
		len = dim - BLOCK_LEN;
		
		// decrypt from position len to position len + BLOCK_LEN
		csql_aes_decrypt((const unsigned char*) (buffer + len), (unsigned char*) (buffer + len), ctx);
		
		// undo the CBC chaining
		for(i = 0; i < len; ++i)
			buffer[i] ^= buffer[i + BLOCK_LEN];
		
		return 0;
	}
	
	b1 = buffer;
	b2 = b1 + BLOCK_LEN;
	len = dim - BLOCK_LEN;
	
	do {
		nextlen = len - BLOCK_LEN;
		if (nextlen > BLOCK_LEN) nextlen = BLOCK_LEN;
		
		// decrypt the b2 block
		csql_aes_decrypt((const unsigned char*) b2, (unsigned char*) buf, ctx);
		
		if(nextlen == 0 || nextlen == BLOCK_LEN) {
			// no ciphertext stealing
			// unchain CBC using the previous ciphertext block in b1
			for(i = 0; i < BLOCK_LEN; ++i)
				buf[i] ^= b1[i];
			
			memcpy(buffer + index*BLOCK_LEN, buf, BLOCK_LEN);
			
			index++;
			len -= BLOCK_LEN;
			if (len == 0) return 0;
			
			b1 = b2;
			b2 = b2 + BLOCK_LEN;
		}
		else {	
			// partial last block - use ciphertext stealing
			
			len = nextlen;
			// thanks to GuardMalloc
			//memcpy (b3, b2+BLOCK_LEN, BLOCK_LEN);
			memcpy (b3, b2+BLOCK_LEN, len);
			
			// produce last 'len' bytes of plaintext by xoring with
			// the lowest 'len' bytes of next block b3 - C[N-1]
			for(i = 0; i < len; ++i)
				buf[i] ^= b3[i];
			
			// reconstruct the C[N-1] block in b3 by adding in the
			// last (BLOCK_LEN - len) bytes of C[N-2] in b2
			for(i = len; i < BLOCK_LEN; ++i)
				b3[i] = buf[i];
			
			// decrypt the C[N-1] block in b3
			csql_aes_decrypt((const unsigned char*) b3, (unsigned char*) b3, ctx);
			
			// produce the last but one plaintext block by xoring with
			// the last but two ciphertext block
			for(i = 0; i < BLOCK_LEN; ++i)
				b3[i] ^= b1[i];
			
			memcpy(buffer + index*BLOCK_LEN, b3, BLOCK_LEN);
			index++;
			memcpy(buffer + index*BLOCK_LEN, buf, nextlen);
			
			return 0;
		}
	}
	while (1);
	
	return 0;
}

int generate_session_key (csqldb *db, int encryption, char *password, char *rand1, char *rand2) {
	int i, keyLen = 0;
	char dummy1[SHA1_DIGEST_SIZE+kRANDPOOLSIZE+kRANDPOOLSIZE];
	char dummy2[kRANDPOOLSIZE];
	char s1[SHA1_DIGEST_SIZE];
	char s2[SHA1_DIGEST_SIZE];
	char session_key[32];
	
	// password is H(H(P)) (len: SHA1_DIGEST_SIZE = 160bits)
	// rand1 is X (len: kRANDPOOLSIZE = 20bytes)
	// rand2 is Y (len: kRANDPOOLSIZE = 20bytes)
	
	// dummy1 is H(H(P));X;Y
	memcpy(dummy1, password, SHA1_DIGEST_SIZE);
	memcpy(dummy1+SHA1_DIGEST_SIZE, rand1, kRANDPOOLSIZE);
	memcpy(dummy1+SHA1_DIGEST_SIZE+kRANDPOOLSIZE, rand2, kRANDPOOLSIZE);
	// s1 is H(H(H(P));X;Y) (len = 20 bytes)
	hash_field((unsigned char *)s1, (const char *)dummy1, SHA1_DIGEST_SIZE+kRANDPOOLSIZE+kRANDPOOLSIZE, 1);
	
	// dummy2 is X^Y
	for (i=0; i<kRANDPOOLSIZE; i++) dummy2[i] = rand1[i] ^ rand2[i];
	// s2 is H(X^Y) (len = 20 bytes)
	hash_field((unsigned char *)s2, (const char *)dummy2, kRANDPOOLSIZE, 1);
	
	bzero(session_key, 32);
	switch (encryption)
	{
		case CUBESQL_ENCRYPTION_NONE:
			keyLen = 0;
			break;
			
		case CUBESQL_ENCRYPTION_AES128:
			keyLen = 16;
			memcpy(session_key, s1, 16);
			break;
			
		case CUBESQL_ENCRYPTION_AES192:
			keyLen = 24;
			memcpy(session_key, s1, 20);
			memcpy(session_key, s2, 4);
			break;
			
		case CUBESQL_ENCRYPTION_AES256:
			keyLen = 32;
			memcpy(session_key, s1, 20);
			memcpy(session_key, s2, 12);
			break;
	}
	
	// generate enc/dec keys
	csql_aes_encrypt_key ((unsigned char*) session_key, keyLen, db->encryptkey);
	csql_aes_decrypt_key ((unsigned char*) session_key, keyLen, db->decryptkey);
	
	return keyLen;
}

int wildcmp(const char *wild, const char *string) {
	// Written by Jack Handy
	// http://www.codeproject.com/Articles/1088/Wildcard-string-compare-globbing
	// Modified by Marco Bambini
	const char *cp = NULL, *mp = NULL;
	
	while ((*string) && (*wild != '*')) {
		if ((toupper(*wild) != toupper(*string)) && (*wild != '?')) {
			return 0;
		}
		wild++;
		string++;
	}
	
	while (*string) {
		if (*wild == '*') {
			if (!*++wild) {
				return 1;
			}
			mp = wild;
			cp = string+1;
		} else if ((toupper(*wild) == toupper(*string)) || (*wild == '?')) {
			wild++;
			string++;
		} else {
			wild = mp;
			string = cp++;
		}
	}
	
	while (*wild == '*') {
		wild++;
	}
	
	return !*wild;
}
