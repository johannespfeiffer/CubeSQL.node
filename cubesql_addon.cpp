#include <napi.h>
#include "CubeSQL-SDK/C_SDK/cubesql.h"

// Wrapper for cubesql_version
Napi::String GetCubeSQLVersion(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, cubesql_version());
}

// Wrapper for cubesql_connect
Napi::Value ConnectToCubeSQL(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 6 || !info[0].IsString() || !info[1].IsNumber() ||
        !info[2].IsString() || !info[3].IsString() || !info[4].IsNumber() || !info[5].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: host (string), port (number), username (string), password (string), timeout (number), encryption (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string host = info[0].As<Napi::String>();
    int port = info[1].As<Napi::Number>();
    std::string username = info[2].As<Napi::String>();
    std::string password = info[3].As<Napi::String>();
    int timeout = info[4].As<Napi::Number>();
    int encryption = info[5].As<Napi::Number>();

    csqldb* db = nullptr;
    int result = cubesql_connect(&db, host.c_str(), port, username.c_str(), password.c_str(), timeout, encryption);

    if (result != CUBESQL_NOERR || db == nullptr) {
        Napi::Error::New(env, "Failed to connect to CubeSQL server").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = Napi::Object::New(env);
    dbObject.Set("dbPointer", Napi::External<csqldb>::New(env, db));
    return dbObject;
}

// Wrapper for cubesql_disconnect
void DisconnectFromCubeSQL(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return;
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return;
    }
    cubesql_disconnect(db, kTRUE);
}

// Implementation for ConnectToCubeSQLSSL
Napi::Value ConnectToCubeSQLSSL(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 7 || !info[0].IsString() || !info[1].IsNumber() ||
        !info[2].IsString() || !info[3].IsString() || !info[4].IsNumber() ||
        !info[5].IsString()) {
        Napi::TypeError::New(env, "Expected arguments: host (string), port (number), username (string), password (string), timeout (number), ssl_certificate_path (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string host = info[0].As<Napi::String>();
    int port = info[1].As<Napi::Number>();
    std::string username = info[2].As<Napi::String>();
    std::string password = info[3].As<Napi::String>();
    int timeout = info[4].As<Napi::Number>();
    std::string ssl_certificate_path = info[5].As<Napi::String>();

    csqldb* db = nullptr;
    int result = cubesql_connect_ssl(&db, host.c_str(), port, username.c_str(), password.c_str(), timeout, ssl_certificate_path.c_str());

    if (result != CUBESQL_NOERR) {
        return Napi::Number::New(env, result);
    }

    // Store the db pointer in a JavaScript object for further use
    Napi::Object dbObject = Napi::Object::New(env);
    dbObject.Set("dbPointer", Napi::External<csqldb>::New(env, db));

    return dbObject;
}

// Implementation for ExecuteSQL
Napi::Value ExecuteSQL(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected arguments: dbObject (object), sql (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string sql = info[1].As<Napi::String>();

    int result = cubesql_execute(db, sql.c_str());
    return Napi::Number::New(env, result);
}

// Implementation for SelectSQL
Napi::Value SelectSQL(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected arguments: dbObject (object), sql (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string sql = info[1].As<Napi::String>();

    csqlc* cursor = cubesql_select(db, sql.c_str(), kFALSE);
    if (!cursor) {
        return env.Null();
    }

    // Store the cursor pointer in a JavaScript object for further use
    Napi::Object cursorObject = Napi::Object::New(env);
    cursorObject.Set("cursorPointer", Napi::External<csqlc>::New(env, cursor));

    return cursorObject;
}

// Implementation for CommitTransaction
Napi::Value CommitTransaction(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_commit(db);
    return Napi::Number::New(env, result);
}

// Implementation for RollbackTransaction
Napi::Value RollbackTransaction(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_rollback(db);
    return Napi::Number::New(env, result);
}

// Implementation for BeginTransaction
Napi::Value BeginTransaction(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_begintransaction(db);
    return Napi::Number::New(env, result);
}

// Implementation for BindSQL
Napi::Value BindSQL(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 6 || !info[0].IsObject() || !info[1].IsString() ||
        !info[2].IsArray() || !info[3].IsArray() || !info[4].IsArray() || !info[5].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: dbObject (object), sql (string), colvalue (array), colsize (array), coltype (array), ncols (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string sql = info[1].As<Napi::String>();

    Napi::Array colvalueArray = info[2].As<Napi::Array>();
    Napi::Array colsizeArray = info[3].As<Napi::Array>();
    Napi::Array coltypeArray = info[4].As<Napi::Array>();
    int ncols = info[5].As<Napi::Number>();

    std::vector<char*> colvalue(ncols);
    std::vector<int> colsize(ncols);
    std::vector<int> coltype(ncols);

    for (int i = 0; i < ncols; i++) {
        colvalue[i] = strdup(colvalueArray.Get(i).As<Napi::String>().Utf8Value().c_str());
        colsize[i] = colsizeArray.Get(i).As<Napi::Number>();
        coltype[i] = coltypeArray.Get(i).As<Napi::Number>();
    }

    int result = cubesql_bind(db, sql.c_str(), colvalue.data(), colsize.data(), coltype.data(), ncols);

    for (int i = 0; i < ncols; i++) {
        free(colvalue[i]);
    }

    return Napi::Number::New(env, result);
}

// Implementation for PingCubeSQL
Napi::Value PingCubeSQL(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_ping(db);
    return Napi::Number::New(env, result);
}

// Implementation for CancelCubeSQL
void CancelCubeSQL(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return;
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return;
    }
    cubesql_cancel(db);
}

// Implementation for GetErrorCode
Napi::Value GetErrorCode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_errcode(db);
    return Napi::Number::New(env, result);
}

// Implementation for GetErrorMessage
Napi::Value GetErrorMessage(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    char* errmsg = cubesql_errmsg(db);
    return Napi::String::New(env, errmsg);
}

// Implementation for GetChanges
Napi::Value GetChanges(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int64_t changes = cubesql_changes(db);
    return Napi::Number::New(env, static_cast<double>(changes));
}

// Implementation for SetTraceCallback
void SetTraceCallback(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "Expected arguments: dbObject (object), callback (function)").ThrowAsJavaScriptException();
        return;
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return;
    }
    Napi::Function callback = info[1].As<Napi::Function>();
    auto traceCallback = [](const char* message, void* data) {
        Napi::FunctionReference* ref = static_cast<Napi::FunctionReference*>(data);
        ref->Call({Napi::String::New(ref->Env(), message)});
    };

    auto* callbackRef = new Napi::FunctionReference(Napi::Persistent(callback));
    cubesql_set_trace_callback(db, traceCallback, callbackRef);
}

// Implementation for SetDatabase
Napi::Value SetDatabase(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected arguments: dbObject (object), dbname (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string dbname = info[1].As<Napi::String>();

    int result = cubesql_set_database(db, dbname.c_str());
    return Napi::Number::New(env, result);
}

// Implementation for GetAffectedRows
Napi::Value GetAffectedRows(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int64_t affectedRows = cubesql_affected_rows(db);
    return Napi::Number::New(env, static_cast<double>(affectedRows));
}

// Implementation for GetLastInsertedRowID
Napi::Value GetLastInsertedRowID(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int64_t lastRowID = cubesql_last_inserted_rowID(db);
    return Napi::Number::New(env, static_cast<double>(lastRowID));
}

// Implementation for SleepMilliseconds
void SleepMilliseconds(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected argument: milliseconds (number)").ThrowAsJavaScriptException();
        return;
    }

    int milliseconds = info[0].As<Napi::Number>();
    cubesql_mssleep(milliseconds);
}

// Implementation for SendData
Napi::Value SendData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsBuffer() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: dbObject (object), buffer (Buffer), length (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    Napi::Buffer<char> buffer = info[1].As<Napi::Buffer<char>>();
    int length = info[2].As<Napi::Number>();

    int result = cubesql_send_data(db, buffer.Data(), length);
    return Napi::Number::New(env, result);
}

// Implementation for SendEndData
Napi::Value SendEndData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_send_enddata(db);
    return Napi::Number::New(env, result);
}

// Implementation for ReceiveData
Napi::Value ReceiveData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: dbObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int len = 0;
    int is_end_chunk = 0;
    char* data = cubesql_receive_data(db, &len, &is_end_chunk);

    if (!data) {
        return env.Null();
    }

    Napi::Object result = Napi::Object::New(env);
    result.Set("data", Napi::Buffer<char>::Copy(env, data, len));
    result.Set("isEndChunk", Napi::Boolean::New(env, is_end_chunk));

    return result;
}

// Implementation for PrepareVM
Napi::Value PrepareVM(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected arguments: dbObject (object), sql (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object dbObject = info[0].As<Napi::Object>();
    csqldb* db = dbObject.Get("dbPointer").As<Napi::External<csqldb>>().Data();
    if (!db) {
        Napi::Error::New(env, "Invalid database pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string sql = info[1].As<Napi::String>();

    csqlvm* vm = cubesql_vmprepare(db, sql.c_str());
    if (!vm) {
        return env.Null();
    }

    Napi::Object vmObject = Napi::Object::New(env);
    vmObject.Set("vmPointer", Napi::External<csqlvm>::New(env, vm));

    return vmObject;
}

// Implementation for BindVMInt
Napi::Value BindVMInt(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: vmObject (object), index (number), intValue (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object vmObject = info[0].As<Napi::Object>();
    csqlvm* vm = vmObject.Get("vmPointer").As<Napi::External<csqlvm>>().Data();
    if (!vm) {
        Napi::Error::New(env, "Invalid VM pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int index = info[1].As<Napi::Number>();
    int intValue = info[2].As<Napi::Number>();

    int result = cubesql_vmbind_int(vm, index, intValue);
    return Napi::Number::New(env, result);
}

// Implementation for BindVMDouble
Napi::Value BindVMDouble(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: vmObject (object), index (number), doubleValue (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object vmObject = info[0].As<Napi::Object>();
    csqlvm* vm = vmObject.Get("vmPointer").As<Napi::External<csqlvm>>().Data();
    if (!vm) {
        Napi::Error::New(env, "Invalid VM pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int index = info[1].As<Napi::Number>();
    double doubleValue = info[2].As<Napi::Number>();

    int result = cubesql_vmbind_double(vm, index, doubleValue);
    return Napi::Number::New(env, result);
}

// Implementation for BindVMText
Napi::Value BindVMText(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsString()) {
        Napi::TypeError::New(env, "Expected arguments: vmObject (object), index (number), textValue (string)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object vmObject = info[0].As<Napi::Object>();
    csqlvm* vm = vmObject.Get("vmPointer").As<Napi::External<csqlvm>>().Data();
    if (!vm) {
        Napi::Error::New(env, "Invalid VM pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int index = info[1].As<Napi::Number>();
    std::string textValue = info[2].As<Napi::String>();

    int result = cubesql_vmbind_text(vm, index, const_cast<char*>(textValue.c_str()), textValue.length());
    return Napi::Number::New(env, result);
}

// Implementation for BindVMNull
Napi::Value BindVMNull(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: vmObject (object), index (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object vmObject = info[0].As<Napi::Object>();
    csqlvm* vm = vmObject.Get("vmPointer").As<Napi::External<csqlvm>>().Data();
    if (!vm) {
        Napi::Error::New(env, "Invalid VM pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int index = info[1].As<Napi::Number>();

    int result = cubesql_vmbind_null(vm, index);
    return Napi::Number::New(env, result);
}

// Implementation for BindVMInt64
Napi::Value BindVMInt64(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: vmObject (object), index (number), int64Value (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object vmObject = info[0].As<Napi::Object>();
    csqlvm* vm = vmObject.Get("vmPointer").As<Napi::External<csqlvm>>().Data();
    if (!vm) {
        Napi::Error::New(env, "Invalid VM pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int index = info[1].As<Napi::Number>();
    int64_t int64Value = info[2].As<Napi::Number>().Int64Value();

    int result = cubesql_vmbind_int64(vm, index, int64Value);
    return Napi::Number::New(env, result);
}

// Implementation for BindVMZeroBlob
Napi::Value BindVMZeroBlob(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: vmObject (object), index (number), length (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object vmObject = info[0].As<Napi::Object>();
    csqlvm* vm = vmObject.Get("vmPointer").As<Napi::External<csqlvm>>().Data();
    if (!vm) {
        Napi::Error::New(env, "Invalid VM pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int index = info[1].As<Napi::Number>();
    int length = info[2].As<Napi::Number>();

    int result = cubesql_vmbind_zeroblob(vm, index, length);
    return Napi::Number::New(env, result);
}

// Implementation for ExecuteVM
Napi::Value ExecuteVM(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: vmObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object vmObject = info[0].As<Napi::Object>();
    csqlvm* vm = vmObject.Get("vmPointer").As<Napi::External<csqlvm>>().Data();
    if (!vm) {
        Napi::Error::New(env, "Invalid VM pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_vmexecute(vm);
    return Napi::Number::New(env, result);
}

// Implementation for SelectVM
Napi::Value SelectVM(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: vmObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object vmObject = info[0].As<Napi::Object>();
    csqlvm* vm = vmObject.Get("vmPointer").As<Napi::External<csqlvm>>().Data();
    if (!vm) {
        Napi::Error::New(env, "Invalid VM pointer").ThrowAsJavaScriptException();
        return env.Null();
    }

    csqlc* cursor = cubesql_vmselect(vm);
    if (!cursor) {
        return env.Null();
    }

    Napi::Object cursorObject = Napi::Object::New(env);
    cursorObject.Set("cursorPointer", Napi::External<csqlc>::New(env, cursor));

    return cursorObject;
}

// Implementation for CloseVM
Napi::Value CloseVM(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: vmObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object vmObject = info[0].As<Napi::Object>();
    csqlvm* vm = vmObject.Get("vmPointer").As<Napi::External<csqlvm>>().Data();
    if (!vm) {
        Napi::Error::New(env, "Invalid VM pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_vmclose(vm);
    return Napi::Number::New(env, result);
}

// Implementation for GetCursorNumRows
Napi::Value GetCursorNumRows(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: cursorObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_cursor_numrows(cursor);
    return Napi::Number::New(env, result);
}

// Implementation for GetCursorNumColumns
Napi::Value GetCursorNumColumns(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: cursorObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_cursor_numcolumns(cursor);
    return Napi::Number::New(env, result);
}

// Implementation for GetCursorCurrentRow
Napi::Value GetCursorCurrentRow(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: cursorObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_cursor_currentrow(cursor);
    return Napi::Number::New(env, result);
}

// Implementation for SeekCursor
Napi::Value SeekCursor(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), index (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int index = info[1].As<Napi::Number>();

    int result = cubesql_cursor_seek(cursor, index);
    return Napi::Number::New(env, result);
}

// Implementation for IsCursorEOF
Napi::Value IsCursorEOF(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: cursorObject (object)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int result = cubesql_cursor_iseof(cursor);
    return Napi::Boolean::New(env, result);
}

// Implementation for GetCursorColumnType
Napi::Value GetCursorColumnType(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), index (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int index = info[1].As<Napi::Number>();

    int result = cubesql_cursor_columntype(cursor, index);
    return Napi::Number::New(env, result);
}

// Implementation for GetCursorField
Napi::Value GetCursorField(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), row (number), column (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int row = info[1].As<Napi::Number>();
    int column = info[2].As<Napi::Number>();

    int len = 0;
    char* field = cubesql_cursor_field(cursor, row, column, &len);

    if (!field) {
        return env.Null();
    }

    return Napi::String::New(env, std::string(field, len));
}

Napi::Value GetCursorFieldBuffer(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), row (number), column (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int row = info[1].As<Napi::Number>();
    int column = info[2].As<Napi::Number>();

    int len = 0;
    char* field = cubesql_cursor_field(cursor, row, column, &len);

    if (!field) {
        return env.Null();
    }

    // Return binary data as Napi::Buffer (instead of a string)
    return Napi::Buffer<char>::New(env, field, len);
}

// Implementation for GetCursorRowID
Napi::Value GetCursorRowID(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), row (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int row = info[1].As<Napi::Number>();

    int64_t rowID = cubesql_cursor_rowid(cursor, row);
    return Napi::Number::New(env, static_cast<double>(rowID));
}

// Implementation for GetCursorInt64
Napi::Value GetCursorInt64(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber() || !info[3].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), row (number), column (number), defaultValue (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int row = info[1].As<Napi::Number>();
    int column = info[2].As<Napi::Number>();
    int64_t defaultValue = info[3].As<Napi::Number>().Int64Value();

    int64_t result = cubesql_cursor_int64(cursor, row, column, defaultValue);
    return Napi::Number::New(env, static_cast<double>(result));
}

// Implementation for GetCursorInt
Napi::Value GetCursorInt(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber() || !info[3].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), row (number), column (number), defaultValue (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int row = info[1].As<Napi::Number>();
    int column = info[2].As<Napi::Number>();
    int defaultValue = info[3].As<Napi::Number>();

    int result = cubesql_cursor_int(cursor, row, column, defaultValue);
    return Napi::Number::New(env, result);
}

// Implementation for GetCursorDouble
Napi::Value GetCursorDouble(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber() || !info[3].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), row (number), column (number), defaultValue (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int row = info[1].As<Napi::Number>();
    int column = info[2].As<Napi::Number>();
    double defaultValue = info[3].As<Napi::Number>();

    double result = cubesql_cursor_double(cursor, row, column, defaultValue);
    return Napi::Number::New(env, result);
}

// Implementation for GetCursorCString
Napi::Value GetCursorCString(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), row (number), column (number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int row = info[1].As<Napi::Number>();
    int column = info[2].As<Napi::Number>();

    char* result = cubesql_cursor_cstring(cursor, row, column);
    if (!result) {
        return env.Null();
    }

    return Napi::String::New(env, result);
}

// Implementation for GetCursorCStringStatic
Napi::Value GetCursorCStringStatic(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4 || !info[0].IsObject() || !info[1].IsNumber() || !info[2].IsNumber() || !info[3].IsBuffer()) {
        Napi::TypeError::New(env, "Expected arguments: cursorObject (object), row (number), column (number), staticBuffer (Buffer)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return env.Null();
    }
    int row = info[1].As<Napi::Number>();
    int column = info[2].As<Napi::Number>();
    Napi::Buffer<char> staticBuffer = info[3].As<Napi::Buffer<char>>();

    char* result = cubesql_cursor_cstring_static(cursor, row, column, staticBuffer.Data(), staticBuffer.Length());
    if (!result) {
        return env.Null();
    }

    return Napi::String::New(env, result);
}

// Implementation for FreeCursor
void FreeCursor(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected argument: cursorObject (object)").ThrowAsJavaScriptException();
        return;
    }

    Napi::Object cursorObject = info[0].As<Napi::Object>();
    csqlc* cursor = cursorObject.Get("cursorPointer").As<Napi::External<csqlc>>().Data();
    if (!cursor) {
        Napi::Error::New(env, "Invalid cursor pointer").ThrowAsJavaScriptException();
        return;
    }
    cubesql_cursor_free(cursor);
}

// Initialize the addon
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "getCubeSQLVersion"), Napi::Function::New(env, GetCubeSQLVersion));
    exports.Set(Napi::String::New(env, "connectToCubeSQL"), Napi::Function::New(env, ConnectToCubeSQL));
    exports.Set(Napi::String::New(env, "connectToCubeSQLSSL"), Napi::Function::New(env, ConnectToCubeSQLSSL));
    exports.Set(Napi::String::New(env, "disconnectFromCubeSQL"), Napi::Function::New(env, DisconnectFromCubeSQL));
    exports.Set(Napi::String::New(env, "executeSQL"), Napi::Function::New(env, ExecuteSQL));
    exports.Set(Napi::String::New(env, "selectSQL"), Napi::Function::New(env, SelectSQL));
    exports.Set(Napi::String::New(env, "commitTransaction"), Napi::Function::New(env, CommitTransaction));
    exports.Set(Napi::String::New(env, "rollbackTransaction"), Napi::Function::New(env, RollbackTransaction));
    exports.Set(Napi::String::New(env, "beginTransaction"), Napi::Function::New(env, BeginTransaction));
    exports.Set(Napi::String::New(env, "bindSQL"), Napi::Function::New(env, BindSQL));
    exports.Set(Napi::String::New(env, "pingCubeSQL"), Napi::Function::New(env, PingCubeSQL));
    exports.Set(Napi::String::New(env, "cancelCubeSQL"), Napi::Function::New(env, CancelCubeSQL));
    exports.Set(Napi::String::New(env, "getErrorCode"), Napi::Function::New(env, GetErrorCode));
    exports.Set(Napi::String::New(env, "getErrorMessage"), Napi::Function::New(env, GetErrorMessage));
    exports.Set(Napi::String::New(env, "getChanges"), Napi::Function::New(env, GetChanges));
    exports.Set(Napi::String::New(env, "setTraceCallback"), Napi::Function::New(env, SetTraceCallback));
    exports.Set(Napi::String::New(env, "setDatabase"), Napi::Function::New(env, SetDatabase));
    exports.Set(Napi::String::New(env, "getAffectedRows"), Napi::Function::New(env, GetAffectedRows));
    exports.Set(Napi::String::New(env, "getLastInsertedRowID"), Napi::Function::New(env, GetLastInsertedRowID));
    exports.Set(Napi::String::New(env, "sleepMilliseconds"), Napi::Function::New(env, SleepMilliseconds));
    exports.Set(Napi::String::New(env, "sendData"), Napi::Function::New(env, SendData));
    exports.Set(Napi::String::New(env, "sendEndData"), Napi::Function::New(env, SendEndData));
    exports.Set(Napi::String::New(env, "receiveData"), Napi::Function::New(env, ReceiveData));
    exports.Set(Napi::String::New(env, "prepareVM"), Napi::Function::New(env, PrepareVM));
    exports.Set(Napi::String::New(env, "bindVMInt"), Napi::Function::New(env, BindVMInt));
    exports.Set(Napi::String::New(env, "bindVMDouble"), Napi::Function::New(env, BindVMDouble));
    exports.Set(Napi::String::New(env, "bindVMText"), Napi::Function::New(env, BindVMText));
    exports.Set(Napi::String::New(env, "bindVMNull"), Napi::Function::New(env, BindVMNull));
    exports.Set(Napi::String::New(env, "bindVMInt64"), Napi::Function::New(env, BindVMInt64));
    exports.Set(Napi::String::New(env, "bindVMZeroBlob"), Napi::Function::New(env, BindVMZeroBlob));
    exports.Set(Napi::String::New(env, "executeVM"), Napi::Function::New(env, ExecuteVM));
    exports.Set(Napi::String::New(env, "selectVM"), Napi::Function::New(env, SelectVM));
    exports.Set(Napi::String::New(env, "closeVM"), Napi::Function::New(env, CloseVM));
    exports.Set(Napi::String::New(env, "getCursorNumRows"), Napi::Function::New(env, GetCursorNumRows));
    exports.Set(Napi::String::New(env, "getCursorNumColumns"), Napi::Function::New(env, GetCursorNumColumns));
    exports.Set(Napi::String::New(env, "getCursorCurrentRow"), Napi::Function::New(env, GetCursorCurrentRow));
    exports.Set(Napi::String::New(env, "seekCursor"), Napi::Function::New(env, SeekCursor));
    exports.Set(Napi::String::New(env, "isCursorEOF"), Napi::Function::New(env, IsCursorEOF));
    exports.Set(Napi::String::New(env, "getCursorColumnType"), Napi::Function::New(env, GetCursorColumnType));
    exports.Set(Napi::String::New(env, "getCursorField"), Napi::Function::New(env, GetCursorField));
    exports.Set(Napi::String::New(env, "getCursorFieldBuffer"), Napi::Function::New(env, GetCursorFieldBuffer));
    exports.Set(Napi::String::New(env, "getCursorRowID"), Napi::Function::New(env, GetCursorRowID));
    exports.Set(Napi::String::New(env, "getCursorInt64"), Napi::Function::New(env, GetCursorInt64));
    exports.Set(Napi::String::New(env, "getCursorInt"), Napi::Function::New(env, GetCursorInt));
    exports.Set(Napi::String::New(env, "getCursorDouble"), Napi::Function::New(env, GetCursorDouble));
    exports.Set(Napi::String::New(env, "getCursorCString"), Napi::Function::New(env, GetCursorCString));
    exports.Set(Napi::String::New(env, "getCursorCStringStatic"), Napi::Function::New(env, GetCursorCStringStatic));
    exports.Set(Napi::String::New(env, "freeCursor"), Napi::Function::New(env, FreeCursor));

    // Export all constants from CubeSQL-SDK
    exports.Set(Napi::String::New(env, "CUBESQL_ENCRYPTION_NONE"), Napi::Number::New(env, CUBESQL_ENCRYPTION_NONE));
    exports.Set(Napi::String::New(env, "CUBESQL_ENCRYPTION_AES128"), Napi::Number::New(env, CUBESQL_ENCRYPTION_AES128));
    exports.Set(Napi::String::New(env, "CUBESQL_ENCRYPTION_AES192"), Napi::Number::New(env, CUBESQL_ENCRYPTION_AES192));
    exports.Set(Napi::String::New(env, "CUBESQL_ENCRYPTION_AES256"), Napi::Number::New(env, CUBESQL_ENCRYPTION_AES256));
    exports.Set(Napi::String::New(env, "CUBESQL_ENCRYPTION_SSL"), Napi::Number::New(env, CUBESQL_ENCRYPTION_SSL));
    exports.Set(Napi::String::New(env, "CUBESQL_COLNAME"), Napi::Number::New(env, CUBESQL_COLNAME));
    exports.Set(Napi::String::New(env, "CUBESQL_CURROW"), Napi::Number::New(env, CUBESQL_CURROW));
    exports.Set(Napi::String::New(env, "CUBESQL_COLTABLE"), Napi::Number::New(env, CUBESQL_COLTABLE));
    exports.Set(Napi::String::New(env, "CUBESQL_ROWID"), Napi::Number::New(env, CUBESQL_ROWID));
    exports.Set(Napi::String::New(env, "CUBESQL_SEEKNEXT"), Napi::Number::New(env, CUBESQL_SEEKNEXT));
    exports.Set(Napi::String::New(env, "CUBESQL_SEEKFIRST"), Napi::Number::New(env, CUBESQL_SEEKFIRST));
    exports.Set(Napi::String::New(env, "CUBESQL_SEEKLAST"), Napi::Number::New(env, CUBESQL_SEEKLAST));
    exports.Set(Napi::String::New(env, "CUBESQL_SEEKPREV"), Napi::Number::New(env, CUBESQL_SEEKPREV));
    return exports;
}

NODE_API_MODULE(cubesql_addon, Init)