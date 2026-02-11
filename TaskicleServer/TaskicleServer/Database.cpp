#include "pch.h"
#include "Database.h"

static eck::CRefStrW s_DbFilePath{};
static eck::CRefStrW s_DbPvFilePath{};

static std::vector<sqlite3*> s_DbFreeConn{};
static std::vector<sqlite3*> s_DbPvFreeConn{};
static eck::CSrwLock s_DbConnLock{};

static BOOL DbpIsTriggerExists(sqlite3* pSqlite, std::string_view svTrigger) noexcept
{
    char* pszErrMsg{};
    constexpr char Sql[]{ R"(
SELECT COUNT(*) FROM sqlite_master
WHERE type='trigger' AND name=?
)" };
    sqlite3_stmt* pStmt;
    auto r = sqlite3_prepare_v3(pSqlite, EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << sqlite3_errmsg(pSqlite) << ")";
        return FALSE;
    }
    sqlite3_bind_text(pStmt, 1, svTrigger.data(),
        (int)svTrigger.size(), SQLITE_STATIC);
    BOOL b;
    if (sqlite3_step(pStmt) == SQLITE_ROW)
        b = !!sqlite3_column_int(pStmt, 0);
    else
        b = FALSE;
    sqlite3_finalize(pStmt);
    return b;
}

static int DbpTableCreateUser(sqlite3* pSqlite) noexcept
{
    /*
    role: enum class DbUserRole
    pw_hash: ver,salt,hash
        ver总为数字，版本1的定义：
        使用PBKDF2-HMAC-SHA256算法，salt为16字节，
        hash为32字节，迭代次数为10000
    */
    constexpr auto Sql = R"(
CREATE TABLE IF NOT EXISTS User (
    user_id         INTEGER     PRIMARY KEY AUTOINCREMENT,
    user_name       TEXT        NOT NULL UNIQUE,
    pw_hash         TEXT        NOT NULL,
    role            INTEGER     NOT NULL DEFAULT 0,
    create_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER))
);

CREATE TABLE IF NOT EXISTS UserSession (
    session_id      TEXT        PRIMARY KEY,
    user_id         INTEGER     NOT NULL,
    expire_at       INTEGER     NOT NULL
);
)";
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}
static int DbpTableCreateProject(sqlite3* pSqlite) noexcept
{
    constexpr auto Sql = R"(
CREATE TABLE IF NOT EXISTS Project (
    project_id      INTEGER     PRIMARY KEY,
    project_name    TEXT        NOT NULL,
    create_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER))
);

CREATE INDEX IF NOT EXISTS IdxProject_Name ON Project(project_id);
)";
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}
static int DbpTableCreateTask(sqlite3* pSqlite) noexcept
{
    constexpr auto Sql = R"(
CREATE TABLE IF NOT EXISTS Task (
    task_id         INTEGER     PRIMARY KEY,
    project_id      INTEGER     NOT NULL,
    task_name       TEXT        NOT NULL,
    status          INTEGER     NOT NULL DEFAULT 0,
    priority        INTEGER     NOT NULL DEFAULT 2,
    description     TEXT        DEFAULT '',
    create_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER)),
    update_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER)),
    expire_at       INTEGER     NOT NULL DEFAULT 0,
    assignee_id     INTEGER     NOT NULL,
    creator_id      INTEGER     NOT NULL,
    FOREIGN KEY(project_id) REFERENCES Project(project_id),
    FOREIGN KEY(assignee_id) REFERENCES User(user_id),
    FOREIGN KEY(creator_id) REFERENCES User(user_id)
);

CREATE INDEX IF NOT EXISTS IdxTask_TaskProjId ON Task(task_id, project_id);

CREATE TABLE IF NOT EXISTS TaskLog (
    id              INTEGER     PRIMARY KEY AUTOINCREMENT,
    task_id         INTEGER     NOT NULL,
    field_name      TEXT        NOT NULL,
    old_value       TEXT        NOT NULL,
    new_value       TEXT        NOT NULL,
    change_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER)),
    FOREIGN KEY(task_id) REFERENCES Task(task_id)
);

CREATE TABLE IF NOT EXISTS TaskComment (
    comm_id         INTEGER     PRIMARY KEY,
    task_id         INTEGER     NOT NULL,
    user_id         INTEGER     NOT NULL,
    content         TEXT        NOT NULL,
    create_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER)),
    modified        INTEGER     NOT NULL DEFAULT 0,
    FOREIGN KEY(task_id) REFERENCES Task(task_id),
    FOREIGN KEY(user_id) REFERENCES User(user_id)
);

CREATE INDEX IF NOT EXISTS IdxTaskComment_TaskId ON TaskComment(task_id);

CREATE TABLE IF NOT EXISTS TaskRelation (
    task_id         INTEGER     NOT NULL,
    relation_id     INTEGER     NOT NULL,
    relation_type   INTEGER     NOT NULL,
    PRIMARY KEY(task_id, relation_id),
    FOREIGN KEY(task_id) REFERENCES Task(task_id)
);

CREATE INDEX IF NOT EXISTS IdxTaskRelation_TaskRelationId ON TaskRelation(task_id, relation_id);
)";
    char* pszErrMsg{};
    int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
        return r;
    }

    auto FnCreateTrigger = [&](std::string_view svTriggerName, std::string_view svFieldName) -> int
        {
            if (DbpIsTriggerExists(pSqlite, svTriggerName))
                return SQLITE_OK;
            eck::CRefStrA rsSql{};
            rsSql
                .PushBack("CREATE TRIGGER IF NOT EXISTS "sv).PushBack(svTriggerName)
                .PushBack(" AFTER UPDATE ON Task WHEN OLD."sv)
                .PushBack(svFieldName).PushBack(" IS NOT NEW."sv).PushBack(svFieldName)
                .PushBack(" BEGIN "sv)
                .PushBack("INSERT INTO TaskLog(task_id, field_name, old_value, new_value)"sv)
                .PushBack("VALUES (OLD.task_id, '"sv).PushBack(svFieldName)
                .PushBack("', OLD."sv).PushBack(svFieldName)
                .PushBack(", NEW."sv).PushBack(svFieldName)
                .PushBack("); END;"sv);
            const auto r = sqlite3_exec(pSqlite, rsSql.Data(), nullptr, nullptr, &pszErrMsg);
            if (r != SQLITE_OK)
            {
                LOGE << "Sqlite error on " << svTriggerName << ": " << r << "(" << pszErrMsg << ")";
                sqlite3_free(pszErrMsg);
            }
            return r;
        };

    if ((r = FnCreateTrigger("TrUpdateTask_ProjectId"sv, "project_id"sv)) != SQLITE_OK)
        return r;
    if ((r = FnCreateTrigger("TrUpdateTask_TaskName"sv, "task_name"sv)) != SQLITE_OK)
        return r;
    if ((r = FnCreateTrigger("TrUpdateTask_Status"sv, "status"sv)) != SQLITE_OK)
        return r;
    if ((r = FnCreateTrigger("TrUpdateTask_Priority"sv, "priority"sv)) != SQLITE_OK)
        return r;
    if ((r = FnCreateTrigger("TrUpdateTask_Description"sv, "description"sv)) != SQLITE_OK)
        return r;
    if ((r = FnCreateTrigger("TrUpdateTask_ExpireAt"sv, "expire_at"sv)) != SQLITE_OK)
        return r;
    if ((r = FnCreateTrigger("TrUpdateTask_AssigneeId"sv, "assignee_id"sv)) != SQLITE_OK)
        return r;

    return SQLITE_OK;

    return r;
}
static int DbpTableCreatePageGroup(sqlite3* pSqlite) noexcept
{
    constexpr auto Sql = R"(
CREATE TABLE IF NOT EXISTS PageGroup (
    page_group_id   INTEGER     PRIMARY KEY,
    group_name      TEXT        NOT NULL,
    create_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER))
);
)";
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}
static int DbpTableCreatePage(sqlite3* pSqlite) noexcept
{
    constexpr auto Sql = R"(
CREATE TABLE IF NOT EXISTS Page (
    page_id         INTEGER     PRIMARY KEY,
    page_group_id   INTEGER     NOT NULL,
    page_name       TEXT        NOT NULL,
    create_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER)),
    has_draft       INTEGER     NOT NULL DEFAULT 0,
    FOREIGN KEY(page_group_id) REFERENCES PageGroup(page_group_id)
);

CREATE INDEX IF NOT EXISTS IdxPage_PageId ON Page(page_id, page_group_id);

CREATE TABLE IF NOT EXISTS PageVersion (
    page_ver_id     INTEGER     PRIMARY KEY AUTOINCREMENT,
    page_id         INTEGER     NOT NULL,
    create_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER)),
    FOREIGN KEY(page_id) REFERENCES Page(page_id)
);

CREATE INDEX IF NOT EXISTS IdxPageVersion_PageId ON PageVersion(page_id);
)";
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}
static int DbpTableCreateGlobalId(sqlite3* pSqlite) noexcept
{
    constexpr auto Sql = R"(
CREATE TABLE IF NOT EXISTS GlobalId (
    id              INTEGER     PRIMARY KEY
);

INSERT INTO GlobalId (id)
SELECT 0 WHERE NOT EXISTS (SELECT 1 FROM GlobalId);
)";
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}
static int DbpTableCreateAcl(sqlite3* pSqlite) noexcept
{
    // access: enum class DbAccess
    constexpr auto Sql = R"(
CREATE TABLE IF NOT EXISTS Acl (
    user_id         INTEGER     NOT NULL,
    entity_id       INTEGER     NOT NULL,
    access          INTEGER     NOT NULL DEFAULT 0,
    PRIMARY KEY(user_id, entity_id),
    FOREIGN KEY(user_id) REFERENCES User(user_id)
);

CREATE INDEX IF NOT EXISTS IdxAcl_TargetType ON Acl(user_id, entity_id);
)";
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}
static int DbpTableCreateTaskComment(sqlite3* pSqlite) noexcept
{
    constexpr auto Sql = R"(
CREATE TABLE IF NOT EXISTS TaskComment (
    comm_id         INTEGER     PRIMARY KEY AUTOINCREMENT,
    task_id         INTEGER     NOT NULL,
    user_id         INTEGER     NOT NULL,
    create_at       INTEGER     NOT NULL DEFAULT (CAST(unixepoch('subsecond') * 1000 AS INTEGER)),
    content         TEXT        NOT NULL,
    FOREIGN KEY(user_id) REFERENCES User(user_id),
    FOREIGN KEY(task_id) REFERENCES Task(task_id)
);
)";
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}
static int DbpViewCreateCoreEntity(sqlite3* pSqlite) noexcept
{
    constexpr auto Sql = R"(
CREATE VIEW IF NOT EXISTS CoreEntity AS
    SELECT
        page_group_id  AS entity_id,
        1              AS type,
        group_name     AS name,
        create_at,
        -1             AS container_id
    FROM PageGroup
UNION ALL
    SELECT page_id,    2, page_name,    create_at, page_group_id
    FROM Page
UNION ALL          
    SELECT project_id, 3, project_name, create_at, -1
    FROM Project       
UNION ALL          
    SELECT task_id,    4, task_name,    create_at, project_id
    FROM Task
;
)";
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}

int DbOpenFirst(std::wstring_view svFile, _Out_ sqlite3*& pSqlite) noexcept
{
    EckAssert(s_DbFilePath.IsEmpty());
    s_DbFilePath = svFile;
    const int r = sqlite3_open16(svFile.data(), &pSqlite);
    if (r != SQLITE_OK)
        LOGE << "Sqlite open failed: " << r;
    else
        sqlite3_busy_timeout(pSqlite, 6000);
    return r;
}
int DbOpen(_Out_ sqlite3*& pSqlite) noexcept
{
    {
        eck::CSrwWriteGuard _{ s_DbConnLock };
        if (!s_DbFreeConn.empty())
        {
            pSqlite = s_DbFreeConn.back();
            s_DbFreeConn.pop_back();
            return SQLITE_OK;
        }
    }
    const int r = sqlite3_open16(s_DbFilePath.Data(), &pSqlite);
    if (r != SQLITE_OK)
        LOGE << "Sqlite open failed: " << r;
    else
        sqlite3_busy_timeout(pSqlite, 6000);
    return r;
}
void DbClose(sqlite3* pSqlite) noexcept
{
    eck::CSrwWriteGuard _{ s_DbConnLock };
    s_DbFreeConn.emplace_back(pSqlite);
}

int DbInitializeTable(sqlite3* pSqlite) noexcept
{
    int r;
    r = DbpTableCreateUser(pSqlite);
    if (r != SQLITE_OK) return r;
    r = DbpTableCreateProject(pSqlite);
    if (r != SQLITE_OK) return r;
    r = DbpTableCreateTask(pSqlite);
    if (r != SQLITE_OK) return r;
    r = DbpTableCreatePageGroup(pSqlite);
    if (r != SQLITE_OK) return r;
    r = DbpTableCreatePage(pSqlite);
    if (r != SQLITE_OK) return r;
    r = DbpTableCreateGlobalId(pSqlite);
    if (r != SQLITE_OK) return r;
    r = DbpTableCreateAcl(pSqlite);
    if (r != SQLITE_OK) return r;
    r = DbpTableCreateTaskComment(pSqlite);
    if (r != SQLITE_OK) return r;
    r = DbpViewCreateCoreEntity(pSqlite);
    return r;
}

int DbIncrementId(sqlite3* pSqlite) noexcept
{
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite,
        R"(UPDATE GlobalId SET id = id + 1;)",
        nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}

void DbCleanup() noexcept
{
    eck::CSrwWriteGuard _{ s_DbConnLock };
    for (auto p : s_DbFreeConn)
        sqlite3_close(p);
    for (auto p : s_DbPvFreeConn)
        sqlite3_close(p);
    s_DbFreeConn.clear();
}


static int DbpPvCreateTablePageVersion(sqlite3* pSqlite) noexcept
{
    constexpr auto Sql = R"(
CREATE TABLE IF NOT EXISTS PageVersion (
    ver_id          INTEGER     PRIMARY KEY AUTOINCREMENT,
    page_id         INTEGER     NOT NULL,
    user_id         INTEGER     NOT NULL,
    last_ver_id     INTEGER     NOT NULL,
    has_snapshot    INTEGER     NOT NULL DEFAULT 0,
    create_at       DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    diff            BLOB,
    edit_count      INTEGER     NOT NULL,
    description     TEXT        DEFAULT NULL
);
)";
    char* pszErrMsg{};
    const int r = sqlite3_exec(pSqlite, Sql, nullptr, nullptr, &pszErrMsg);
    if (r != SQLITE_OK)
    {
        LOGE << "Sqlite error: " << r << "(" << pszErrMsg << ")";
        sqlite3_free(pszErrMsg);
    }
    return r;
}

int DbPvOpenFirst(std::wstring_view svFile, _Out_ sqlite3*& pSqlite) noexcept
{
    EckAssert(s_DbPvFilePath.IsEmpty());
    s_DbPvFilePath = svFile;
    const int r = sqlite3_open16(svFile.data(), &pSqlite);
    if (r != SQLITE_OK)
        LOGE << "Sqlite open failed: " << r;
    else
        sqlite3_busy_timeout(pSqlite, 6000);
    return r;
}

int DbPvOpen(_Out_ sqlite3*& pSqlite) noexcept
{
    {
        eck::CSrwWriteGuard _{ s_DbConnLock };
        if (!s_DbPvFreeConn.empty())
        {
            pSqlite = s_DbPvFreeConn.back();
            s_DbPvFreeConn.pop_back();
            return SQLITE_OK;
        }
    }
    const int r = sqlite3_open16(s_DbPvFilePath.Data(), &pSqlite);
    if (r != SQLITE_OK)
        LOGE << "Sqlite open failed: " << r;
    else
        sqlite3_busy_timeout(pSqlite, 6000);
    return r;
}

void DbPvClose(sqlite3* pSqlite) noexcept
{
    eck::CSrwWriteGuard _{ s_DbConnLock };
    s_DbPvFreeConn.emplace_back(pSqlite);
}

int DbPvInitializeTable(sqlite3* pSqlite) noexcept
{
    return DbpPvCreateTablePageVersion(pSqlite);
}