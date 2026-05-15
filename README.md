# sqlite

<img src="https://img.shields.io/badge/Donna-sqlite-FF6347?style=for-the-badge" alt="Donna sqlite"/>
<a href="https://donna-lang.github.io/sqlite/"><img src="https://img.shields.io/badge/Docs-Read-2F81F7?style=for-the-badge" alt="Docs - Read"/></a>
<img src="https://img.shields.io/github/actions/workflow/status/donna-lang/sqlite/test.yml?branch=main&label=Test&style=for-the-badge" alt="Test status"/>

SQLite support for the [Donna](https://github.com/donna-lang/donna) programming language.

## Overview

`sqlite` lets you open, query, and modify SQLite databases from Donna.

The SQLite library is vendored as the [amalgamation](https://sqlite.org/amalgamation.html), so no system installation is required. You get open/close, parameterised prepared statements, typed column values, and a `query` convenience function that collects all rows in one call.

## Installation

Add to your `donna.toml` as a dependency:

```toml
[dependencies]
sqlite = { git = "https://github.com/donna-lang/sqlite", version = ">=0.1.0 and <1.0.0" }
```

Then import the module:

```donna
import sqlite
```

## Quick start

Open an in-memory database and run a query:

```donna
import sqlite

pub fn main() -> Nil:
  case sqlite.open(":memory:"):
    sqlite.Err(e) -> echo e
    sqlite.Ok(db) ->
      let _ = sqlite.exec(db, "CREATE TABLE users (id INTEGER, name TEXT)")
      let _ = sqlite.exec(db, "INSERT INTO users VALUES (1, 'Donna')")
      case sqlite.query(db, "SELECT id, name FROM users"):
        sqlite.Err(e) -> echo e
        sqlite.Ok(rows) -> echo "ok"
      sqlite.close(db)
```

Use prepared statements and bound parameters:

```donna
import sqlite

pub fn insert(db: sqlite.Db, id: Int, name: String) -> sqlite.Result(Nil):
  case sqlite.prepare(db, "INSERT INTO users VALUES (?, ?)"):
    sqlite.Err(e) -> sqlite.Err(e)
    sqlite.Ok(stmt) ->
      let _ = sqlite.bind_int(stmt, 1, id)
      let _ = sqlite.bind_text(stmt, 2, name)
      let result = sqlite.step(stmt)
      let _ = sqlite.finalize(stmt)
      case result:
        sqlite.Err(e) -> sqlite.Err(e)
        sqlite.Ok(_) -> sqlite.Ok(Nil)
```

Read column values from a result row:

```donna
import sqlite

pub fn read_row(row: List(sqlite.Value)) -> String:
  case row:
    [] -> ""
    [v, ..rest] ->
      case v:
        sqlite.Null -> "null"
        sqlite.Int(n) -> "int"
        sqlite.Float(f) -> "float"
        sqlite.Text(s) -> s
        sqlite.Blob(hex) -> "blob:" <> hex
```

Run tests:

```sh
donna test
```

## API

For API Reference visit the generated docs [here](https://donna-lang.github.io/sqlite/)

## Types

| Type | Description |
|------|-------------|
| `Db` | An open database connection |
| `Stmt` | A prepared statement |
| `Value` | A column value: `Null`, `Int(Int)`, `Float(Float)`, `Text(String)`, `Blob(String)` |
| `Result(a)` | `Ok(a)` or `Err(String)` |

## Core functions

| Function | Description |
|----------|-------------|
| `open(path)` | Open a database; use `":memory:"` for in-memory |
| `close(db)` | Close the database |
| `exec(db, sql)` | Execute SQL with no result rows |
| `query(db, sql)` | Execute SELECT and collect all rows |
| `prepare(db, sql)` | Prepare a statement with `?` placeholders |
| `bind_text/int/float/null(stmt, idx, value)` | Bind a parameter (1-based) |
| `step(stmt)` | Advance to the next row: `Ok(True)` = row, `Ok(False)` = done |
| `column(stmt, idx)` | Read a column value (0-based) |
| `column_name(stmt, idx)` | Read a column name (0-based) |
| `column_count(stmt)` | Number of columns |
| `finalize(stmt)` | Release a prepared statement |
| `reset(stmt)` | Reset a statement for re-execution |
| `last_insert_rowid(db)` | Rowid of the last INSERT |
| `changes(db)` | Rows affected by the last DML statement |

## Licence

MIT
