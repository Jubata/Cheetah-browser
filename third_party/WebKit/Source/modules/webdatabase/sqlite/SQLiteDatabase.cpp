/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webdatabase/sqlite/SQLiteDatabase.h"

#include "modules/webdatabase/sqlite/SQLLog.h"
#include "modules/webdatabase/sqlite/SQLiteFileSystem.h"
#include "modules/webdatabase/sqlite/SQLiteStatement.h"
#include "modules/webdatabase/DatabaseAuthorizer.h"
#include "third_party/sqlite/sqlite3.h"

namespace blink {

const int kSQLResultDone = SQLITE_DONE;
const int kSQLResultOk = SQLITE_OK;
const int kSQLResultRow = SQLITE_ROW;
const int kSQLResultFull = SQLITE_FULL;
const int kSQLResultInterrupt = SQLITE_INTERRUPT;
const int kSQLResultConstraint = SQLITE_CONSTRAINT;

static const char kNotOpenErrorMessage[] = "database is not open";

SQLiteDatabase::SQLiteDatabase()
    : db_(nullptr),
      page_size_(-1),
      transaction_in_progress_(false),
#if DCHECK_IS_ON()
      sharable_(false),
#endif
      opening_thread_(0),
      open_error_(SQLITE_ERROR),
      open_error_message_(),
      last_changes_count_(0) {
}

SQLiteDatabase::~SQLiteDatabase() {
  Close();
}

bool SQLiteDatabase::Open(const String& filename) {
  Close();

  open_error_ = SQLiteFileSystem::OpenDatabase(filename, &db_);
  if (open_error_ != SQLITE_OK) {
    open_error_message_ =
        db_ ? sqlite3_errmsg(db_) : "sqlite_open returned null";
    DLOG(ERROR) << "SQLite database failed to load from " << filename
                << "\nCause - " << open_error_message_.data();
    sqlite3_close(db_);
    db_ = nullptr;
    return false;
  }

  open_error_ = sqlite3_extended_result_codes(db_, 1);
  if (open_error_ != SQLITE_OK) {
    open_error_message_ = sqlite3_errmsg(db_);
    DLOG(ERROR) << "SQLite database error when enabling extended errors - "
                << open_error_message_.data();
    sqlite3_close(db_);
    db_ = nullptr;
    return false;
  }

  if (IsOpen())
    opening_thread_ = CurrentThread();
  else
    open_error_message_ = "sqlite_open returned null";

  if (!SQLiteStatement(*this, "PRAGMA temp_store = MEMORY;").ExecuteCommand())
    DLOG(ERROR) << "SQLite database could not set temp_store to memory";

  // Foreign keys are not supported by WebDatabase.  Make sure foreign key
  // support is consistent if SQLite has SQLITE_DEFAULT_FOREIGN_KEYS.
  if (!SQLiteStatement(*this, "PRAGMA foreign_keys = OFF;").ExecuteCommand())
    DLOG(ERROR) << "SQLite database could not turn off foreign_keys";

  return IsOpen();
}

void SQLiteDatabase::Close() {
  if (db_) {
    // FIXME: This is being called on the main thread during JS GC.
    // <rdar://problem/5739818>
    // DCHECK_EQ(currentThread(), m_openingThread);
    sqlite3* db = db_;
    {
      MutexLocker locker(database_closing_mutex_);
      db_ = nullptr;
    }
    sqlite3_close(db);
  }

  opening_thread_ = 0;
  open_error_ = SQLITE_ERROR;
  open_error_message_ = CString();
}

void SQLiteDatabase::SetMaximumSize(int64_t size) {
  if (size < 0)
    size = 0;

  int current_page_size = PageSize();

  DCHECK(current_page_size || !db_);
  int64_t new_max_page_count = current_page_size ? size / current_page_size : 0;

  MutexLocker locker(authorizer_lock_);
  EnableAuthorizer(false);

  SQLiteStatement statement(
      *this, "PRAGMA max_page_count = " + String::Number(new_max_page_count));
  statement.Prepare();
  if (statement.Step() != kSQLResultRow)
    DLOG(ERROR) << "Failed to set maximum size of database to " << size
                << " bytes";

  EnableAuthorizer(true);
}

int SQLiteDatabase::PageSize() {
  // Since the page size of a database is locked in at creation and therefore
  // cannot be dynamic, we can cache the value for future use.
  if (page_size_ == -1) {
    MutexLocker locker(authorizer_lock_);
    EnableAuthorizer(false);

    SQLiteStatement statement(*this, "PRAGMA page_size");
    page_size_ = statement.GetColumnInt(0);

    EnableAuthorizer(true);
  }

  return page_size_;
}

int64_t SQLiteDatabase::FreeSpaceSize() {
  int64_t freelist_count = 0;

  {
    MutexLocker locker(authorizer_lock_);
    EnableAuthorizer(false);
    // Note: freelist_count was added in SQLite 3.4.1.
    SQLiteStatement statement(*this, "PRAGMA freelist_count");
    freelist_count = statement.GetColumnInt64(0);
    EnableAuthorizer(true);
  }

  return freelist_count * PageSize();
}

int64_t SQLiteDatabase::TotalSize() {
  int64_t page_count = 0;

  {
    MutexLocker locker(authorizer_lock_);
    EnableAuthorizer(false);
    SQLiteStatement statement(*this, "PRAGMA page_count");
    page_count = statement.GetColumnInt64(0);
    EnableAuthorizer(true);
  }

  return page_count * PageSize();
}

void SQLiteDatabase::SetBusyTimeout(int ms) {
  if (db_)
    sqlite3_busy_timeout(db_, ms);
  else
    SQL_DVLOG(1) << "BusyTimeout set on non-open database";
}

bool SQLiteDatabase::ExecuteCommand(const String& sql) {
  return SQLiteStatement(*this, sql).ExecuteCommand();
}

bool SQLiteDatabase::TableExists(const String& tablename) {
  if (!IsOpen())
    return false;

  String statement =
      "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '" +
      tablename + "';";

  SQLiteStatement sql(*this, statement);
  sql.Prepare();
  return sql.Step() == SQLITE_ROW;
}

int SQLiteDatabase::RunVacuumCommand() {
  if (!ExecuteCommand("VACUUM;"))
    SQL_DVLOG(1) << "Unable to vacuum database -" << LastErrorMsg();
  return LastError();
}

int SQLiteDatabase::RunIncrementalVacuumCommand() {
  MutexLocker locker(authorizer_lock_);
  EnableAuthorizer(false);

  if (!ExecuteCommand("PRAGMA incremental_vacuum"))
    SQL_DVLOG(1) << "Unable to run incremental vacuum - " << LastErrorMsg();

  EnableAuthorizer(true);
  return LastError();
}

int64_t SQLiteDatabase::LastInsertRowID() {
  if (!db_)
    return 0;
  return sqlite3_last_insert_rowid(db_);
}

void SQLiteDatabase::UpdateLastChangesCount() {
  if (!db_)
    return;

  last_changes_count_ = sqlite3_total_changes(db_);
}

int SQLiteDatabase::LastChanges() {
  if (!db_)
    return 0;

  return sqlite3_total_changes(db_) - last_changes_count_;
}

int SQLiteDatabase::LastError() {
  return db_ ? sqlite3_errcode(db_) : open_error_;
}

const char* SQLiteDatabase::LastErrorMsg() {
  if (db_)
    return sqlite3_errmsg(db_);
  return open_error_message_.IsNull() ? kNotOpenErrorMessage
                                      : open_error_message_.data();
}

int SQLiteDatabase::AuthorizerFunction(void* user_data,
                                       int action_code,
                                       const char* parameter1,
                                       const char* parameter2,
                                       const char* /*databaseName*/,
                                       const char* /*trigger_or_view*/) {
  DatabaseAuthorizer* auth = static_cast<DatabaseAuthorizer*>(user_data);
  DCHECK(auth);

  switch (action_code) {
    case SQLITE_CREATE_INDEX:
      return auth->CreateIndex(parameter1, parameter2);
    case SQLITE_CREATE_TABLE:
      return auth->CreateTable(parameter1);
    case SQLITE_CREATE_TEMP_INDEX:
      return auth->CreateTempIndex(parameter1, parameter2);
    case SQLITE_CREATE_TEMP_TABLE:
      return auth->CreateTempTable(parameter1);
    case SQLITE_CREATE_TEMP_TRIGGER:
      return auth->CreateTempTrigger(parameter1, parameter2);
    case SQLITE_CREATE_TEMP_VIEW:
      return auth->CreateTempView(parameter1);
    case SQLITE_CREATE_TRIGGER:
      return auth->CreateTrigger(parameter1, parameter2);
    case SQLITE_CREATE_VIEW:
      return auth->CreateView(parameter1);
    case SQLITE_DELETE:
      return auth->AllowDelete(parameter1);
    case SQLITE_DROP_INDEX:
      return auth->DropIndex(parameter1, parameter2);
    case SQLITE_DROP_TABLE:
      return auth->DropTable(parameter1);
    case SQLITE_DROP_TEMP_INDEX:
      return auth->DropTempIndex(parameter1, parameter2);
    case SQLITE_DROP_TEMP_TABLE:
      return auth->DropTempTable(parameter1);
    case SQLITE_DROP_TEMP_TRIGGER:
      return auth->DropTempTrigger(parameter1, parameter2);
    case SQLITE_DROP_TEMP_VIEW:
      return auth->DropTempView(parameter1);
    case SQLITE_DROP_TRIGGER:
      return auth->DropTrigger(parameter1, parameter2);
    case SQLITE_DROP_VIEW:
      return auth->DropView(parameter1);
    case SQLITE_INSERT:
      return auth->AllowInsert(parameter1);
    case SQLITE_PRAGMA:
      return auth->AllowPragma(parameter1, parameter2);
    case SQLITE_READ:
      return auth->AllowRead(parameter1, parameter2);
    case SQLITE_SELECT:
      return auth->AllowSelect();
    case SQLITE_TRANSACTION:
      return auth->AllowTransaction();
    case SQLITE_UPDATE:
      return auth->AllowUpdate(parameter1, parameter2);
    case SQLITE_ATTACH:
      return auth->AllowAttach(parameter1);
    case SQLITE_DETACH:
      return auth->AllowDetach(parameter1);
    case SQLITE_ALTER_TABLE:
      return auth->AllowAlterTable(parameter1, parameter2);
    case SQLITE_REINDEX:
      return auth->AllowReindex(parameter1);
#if SQLITE_VERSION_NUMBER >= 3003013
    case SQLITE_ANALYZE:
      return auth->AllowAnalyze(parameter1);
    case SQLITE_CREATE_VTABLE:
      return auth->CreateVTable(parameter1, parameter2);
    case SQLITE_DROP_VTABLE:
      return auth->DropVTable(parameter1, parameter2);
    case SQLITE_FUNCTION:
      return auth->AllowFunction(parameter2);
#endif
    default:
      NOTREACHED();
      return kSQLAuthDeny;
  }
}

void SQLiteDatabase::SetAuthorizer(DatabaseAuthorizer* auth) {
  if (!db_) {
    NOTREACHED() << "Attempt to set an authorizer on a non-open SQL database";
    return;
  }

  MutexLocker locker(authorizer_lock_);

  authorizer_ = auth;

  EnableAuthorizer(true);
}

void SQLiteDatabase::EnableAuthorizer(bool enable) {
  if (authorizer_ && enable)
    sqlite3_set_authorizer(db_, SQLiteDatabase::AuthorizerFunction,
                           authorizer_.Get());
  else
    sqlite3_set_authorizer(db_, NULL, nullptr);
}

bool SQLiteDatabase::IsAutoCommitOn() const {
  return sqlite3_get_autocommit(db_);
}

bool SQLiteDatabase::TurnOnIncrementalAutoVacuum() {
  SQLiteStatement statement(*this, "PRAGMA auto_vacuum");
  int auto_vacuum_mode = statement.GetColumnInt(0);
  int error = LastError();

  // Finalize statement to not block potential VACUUM.
  statement.Finalize();

  // Check if we got an error while trying to get the value of the auto_vacuum
  // flag.  If we got a SQLITE_BUSY error, then there's probably another
  // transaction in progress on this database. In this case, keep the current
  // value of the auto_vacuum flag and try to set it to INCREMENTAL the next
  // time we open this database. If the error is not SQLITE_BUSY, then we
  // probably ran into a more serious problem and should return false (to log an
  // error message).
  if (error != SQLITE_ROW)
    return false;

  switch (auto_vacuum_mode) {
    case kAutoVacuumIncremental:
      return true;
    case kAutoVacuumFull:
      return ExecuteCommand("PRAGMA auto_vacuum = 2");
    case kAutoVacuumNone:
    default:
      if (!ExecuteCommand("PRAGMA auto_vacuum = 2"))
        return false;
      RunVacuumCommand();
      error = LastError();
      return (error == SQLITE_OK);
  }
}

}  // namespace blink
