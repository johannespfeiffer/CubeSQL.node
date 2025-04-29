# CubeSQL.node
Wrapper for Marco Bambinis [native C SDK](https://github.com/cubesql/sdk.git)

## Usage Example
```ts
import * as cubesql from 'cubesql.node';
import { Database } from 'cubesql.node';

const db = cubesql.connectToCubeSQL('host', 4430, 'username', 'password', 12, cubesql.CUBESQL_ENCRYPTION_NONE);
if (!db) {
    console.error('Failed to connect to CubeSQL:', cubesql.getErrorMessage(db));
    process.exit(1);
}
cubesql.setDatabase(db, 'MyDatabase.sdb');

const result = cubesql.selectSQL('MyDatabase.sdb', 'SELECT * FROM mytable');
if (!result) {
    throw new Error('Failed to fetch table content');
}

const columns: string[] = [];
for (let col = 1; col <= cubesql.getCursorNumColumns(result); col++) {
    columns.push(cubesql.getCursorField(result, cubesql.CUBESQL_COLNAME, col));
}

const rows: string[][] = [];
while (!cubesql.isCursorEOF(result)) {
    const row: string[] = [];
    for (let col = 1; col <= cubesql.getCursorNumColumns(result); col++) {
        row.push(cubesql.getCursorField(result, cubesql.CUBESQL_CURROW, col));
    }
    rows.push(row);
    cubesql.seekCursor(result, cubesql.CUBESQL_SEEKNEXT);
}
cubesql.freeCursor(result);

console.log({ columns, rows });
```

## Installation from Source

### MacOS

Download 
```
git clone https://github.com/johannespfeiffer/CubeSQL.node.git
```

Install dependencies and build
```
brew install libressl
npm i
```


## Third Party Components

This package bundles the CubeSQL SDK licensed under the MIT License.  
See [CubeSQL SDK](https://github.com/sqlabs/CubeSQL-SDK) for more information.