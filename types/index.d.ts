declare module 'cubesql.node' {
    export interface Database {
        dbPointer: any;
        connect(host: string, port: number, username: string, password: string, timeout: number, encryption: number): any;
        setDatabase(db: any, databaseName: string): void;
        selectSQL(db: any, query: string): any;
        getCursorField(cursor: any, fieldType: number, column: number): string;
        getCursorFieldBuffer(cursor: any, fieldType: number, column: number): Buffer;
        getCursorNumColumns(cursor: any): number;
        isCursorEOF(cursor: any): boolean;
        seekCursor(cursor: any, seekType: number): void;
        freeCursor(cursor: any): void;
        getErrorMessage(db: any): string;
    }

    export interface Cursor {
        cursorPointer: any;
    }

    export const CUBESQL_ENCRYPTION_NONE: number;
    export const CUBESQL_ENCRYPTION_AES128: number;
    export const CUBESQL_ENCRYPTION_AES192: number;
    export const CUBESQL_ENCRYPTION_AES256: number;
    export const CUBESQL_ENCRYPTION_SSL: number;
    export const CUBESQL_COLNAME: number;
    export const CUBESQL_CURROW: number;
    export const CUBESQL_COLTABLE: number;
    export const CUBESQL_ROWID: number;
    export const CUBESQL_SEEKNEXT: number;
    export const CUBESQL_SEEKFIRST: number;
    export const CUBESQL_SEEKLAST: number;
    export const CUBESQL_SEEKPREV: number;

    export function getCubeSQLVersion(): string;
    export function connectToCubeSQL(host: string, port: number, username: string, password: string, timeout: number, encryption: number): Database;
    export function connectToCubeSQLSSL(host: string, port: number, username: string, password: string, timeout: number, sslCertificatePath: string): Database;
    export function disconnectFromCubeSQL(db: Database): void;
    export function executeSQL(db: Database, sql: string): number;
    export function selectSQL(db: Database, sql: string): Cursor;
    export function commitTransaction(db: Database): number;
    export function rollbackTransaction(db: Database): number;
    export function beginTransaction(db: Database): number;
    export function bindSQL(db: Database, sql: string, colvalue: string[], colsize: number[], coltype: number[], ncols: number): number;
    export function pingCubeSQL(db: Database): number;
    export function cancelCubeSQL(db: Database): void;
    export function getErrorCode(db: Database): number;
    export function getErrorMessage(db: Database): string;
    export function getChanges(db: Database): number;
    export function setTraceCallback(db: Database, callback: (message: string) => void): void;
    export function setDatabase(db: Database, dbname: string): number;
    export function getAffectedRows(db: Database): number;
    export function getLastInsertedRowID(db: Database): number;
    export function sleepMilliseconds(ms: number): void;
    export function sendData(db: Database, buffer: Buffer, length: number): number;
    export function sendEndData(db: Database): number;
    export function receiveData(db: Database): { data: Buffer; isEndChunk: boolean };
    export function prepareVM(db: Database, sql: string): any;
    export function bindVMInt(vm: any, index: number, value: number): number;
    export function bindVMDouble(vm: any, index: number, value: number): number;
    export function bindVMText(vm: any, index: number, value: string): number;
    export function bindVMNull(vm: any, index: number): number;
    export function bindVMInt64(vm: any, index: number, value: number): number;
    export function bindVMZeroBlob(vm: any, index: number, length: number): number;
    export function executeVM(vm: any): number;
    export function selectVM(vm: any): Cursor;
    export function closeVM(vm: any): number;
    export function getCursorNumRows(cursor: Cursor): number;
    export function getCursorNumColumns(cursor: Cursor): number;
    export function getCursorCurrentRow(cursor: Cursor): number;
    export function seekCursor(cursor: Cursor, index: number): number;
    export function isCursorEOF(cursor: Cursor): boolean;
    export function getCursorColumnType(cursor: Cursor, index: number): number;
    export function getCursorField(cursor: Cursor, row: number, column: number): string;
    export function getCursorFieldBuffer(cursor: Cursor, row: number, column: number): Buffer;
    export function getCursorRowID(cursor: Cursor, row: number): number;
    export function getCursorInt64(cursor: Cursor, row: number, column: number, defaultValue: number): number;
    export function getCursorInt(cursor: Cursor, row: number, column: number, defaultValue: number): number;
    export function getCursorDouble(cursor: Cursor, row: number, column: number, defaultValue: number): number;
    export function getCursorCString(cursor: Cursor, row: number, column: number): string;
    export function getCursorCStringStatic(cursor: Cursor, row: number, column: number, staticBuffer: Buffer): string;
    export function freeCursor(cursor: Cursor): void;
}