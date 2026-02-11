#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"
#include "Database.h"
#include "SqliteUtils.h"
#include "AccessCheck.h"

static ApiResult QueryTaskId(const API_CTX& Ctx, int iCommId,
    _Out_ int& iTaskId, _Out_ int& rSql) noexcept
{
    iTaskId = DbIdInvalid;
    sqlite3_stmt* pStmt;
    constexpr char SqlQuery[]{ R"(
SELECT task_id FROM TaskComment WHERE comm_id = ?;
)" };
    rSql = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(SqlQuery), 0, &pStmt, nullptr);
    if (rSql != SQLITE_OK)
        return ApiResult::Database;
    sqlite3_bind_int(pStmt, 1, iCommId);
    if (sqlite3_step(pStmt) != SQLITE_ROW)
    {
        sqlite3_finalize(pStmt);
        return ApiResult::NotFound;
    }
    iTaskId = sqlite3_column_int(pStmt, 0);
    sqlite3_finalize(pStmt);
    return ApiResult::Ok;
}

// Task: WriteComment
static void AwInsertTaskComment(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValTaskId = jIn["/task_id"];
        const auto ValContent = jIn["/content"];
        if (!ValTaskId.IsValid() || !ValContent.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValTaskId.IsInt() || !ValContent.IsString())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        const auto iUserId = CkDbGetCurrentUser(Ctx);
        if (!AclDbCheckAccess(Ctx, iUserId,
            ValTaskId.GetInt(), DbAccess::WriteComment, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(
INSERT INTO TaskComment(task_id, user_id, content)
VALUES (?, ?, ?);
)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }

        sqlite3_bind_int(pStmt, 1, ValTaskId.GetInt());
        sqlite3_bind_int(pStmt, 2, iUserId);
        SuBindJsonStringValue(pStmt, 3, ValContent);

        r = sqlite3_step(pStmt);
        sqlite3_finalize(pStmt);
        if (r == SQLITE_DONE)
            r = SQLITE_OK;
        else
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
    }
    else
        rApi = ApiResult::BadPayload;
Exit:
    Json::CMutDoc j{};
    j = {
        "r", r == SQLITE_OK ? rApi : ApiResult::Database,
        "r2", r,
        "err_msg", pszErrMsg,
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_InsertTaskComment, AwInsertTaskComment)

// Task: WriteComment
static void AwDeleteTaskComment(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValCommId = jIn["/comm_id"];
        if (!ValCommId.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValCommId.IsInt())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }
        const auto iCommId = ValCommId.GetInt();

        int iTaskId;
        rApi = QueryTaskId(Ctx, iCommId, iTaskId, r);
        if (rApi != ApiResult::Ok)
        {
            if (r != SQLITE_OK)
                pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }

        if (!AclDbCheckCurrentUserAccess(Ctx,
            iTaskId, DbAccess::WriteComment, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        sqlite3_stmt* pStmt;
        constexpr char Sql[]{ R"(DELETE FROM TaskComment WHERE comm_id = ?)" };
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, iCommId);
        r = sqlite3_step(pStmt);
        sqlite3_finalize(pStmt);
        if (r == SQLITE_DONE)
            r = SQLITE_OK;
        else
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
    }
    else
        rApi = ApiResult::BadPayload;
Exit:
    Json::CMutDoc j{};
    j = {
        "r", r == SQLITE_OK ? rApi : ApiResult::Database,
        "r2", r,
        "err_msg", pszErrMsg ? pszErrMsg : "",
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_DeleteTaskComment, AwDeleteTaskComment)

// TaskComment: Update
static void AwUpdateTaskComment(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValCommId = jIn["/comm_id"];
        const auto ValContent = jIn["/content"];
        if (!ValCommId.IsValid() || !ValContent.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValCommId.IsInt() || !ValContent.IsString())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }
        const auto iCommId = ValCommId.GetInt();

        int iTaskId;
        rApi = QueryTaskId(Ctx, iCommId, iTaskId, r);
        if (rApi != ApiResult::Ok)
        {
            if (r != SQLITE_OK)
                pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }

        if (!AclDbCheckCurrentUserAccess(Ctx,
            iTaskId, DbAccess::WriteComment, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(
UPDATE TaskComment
SET modified = 1, content = ? WHERE comm_id = ?)"
        };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }

        SuBindJsonStringValue(pStmt, 1, ValContent);
        sqlite3_bind_int(pStmt, 2, iCommId);

        r = sqlite3_step(pStmt);
        sqlite3_finalize(pStmt);
        if (r == SQLITE_DONE)
            r = SQLITE_OK;
        else
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
    }
    else
        rApi = ApiResult::BadPayload;
Exit:
    Json::CMutDoc j{};
    j = {
        "r", r == SQLITE_OK ? rApi : ApiResult::Database,
        "r2", r,
        "err_msg", pszErrMsg,
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiPost_UpdateTaskComment, AwUpdateTaskComment)

// Task: ReadComment
static void AwGetTaskCommentList(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int cEntry{}, nPage{}, iTaskId{ DbIdInvalid };
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("count"))
            ApiParseInt(e.V, cEntry);
        else if (TKK_API_HIT_QUERY("page"))
            ApiParseInt(e.V, nPage);
        else if (TKK_API_HIT_QUERY("task_id"))
            ApiParseInt(e.V, iTaskId);
    }

    if (cEntry <= 0 || cEntry > MaxQueryCount)
        cEntry = MaxQueryCount;

    Json::CMutDoc j{};
    const auto Arr = j.NewArray();

    if (iTaskId != DbIdInvalid)
    {
        if (!AclDbCheckCurrentUserAccess(Ctx,
            iTaskId, DbAccess::ReadComment, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(
SELECT t.comm_id, t.user_id, t.content, t.create_at, t.modified, u.user_name
FROM TaskComment AS t
JOIN User AS u
ON t.user_id = u.user_id
WHERE t.task_id = ?
ORDER BY t.create_at DESC
LIMIT ? OFFSET ?;
)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r == SQLITE_OK)
        {
            sqlite3_bind_int(pStmt, 1, iTaskId);
            sqlite3_bind_int(pStmt, 2, cEntry);
            sqlite3_bind_int(pStmt, 3, nPage * cEntry);
            while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
            {
                const auto Obj = j.NewObject();
                Obj = {
                    "comm_id", sqlite3_column_int(pStmt, 0),
                    "user_id", sqlite3_column_int(pStmt, 1),
                    "content", SuColumnStringView(pStmt, 2),
                    "create_at", sqlite3_column_int64(pStmt, 3),
                    "modified", !!sqlite3_column_int(pStmt, 4),
                    "user_name", SuColumnStringView(pStmt, 5),
                };
                Arr.ArrPushBack(Obj);
            }
            sqlite3_finalize(pStmt);
            if (r == SQLITE_DONE)
                r = SQLITE_OK;
            else
                pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
        }
        else
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
    }
    else
        rApi = ApiResult::RequiredFieldMissing;
Exit:
    j = {
        "r", r == SQLITE_OK ? rApi : ApiResult::Database,
        "r2", r,
        "err_msg", pszErrMsg,
        "data", Arr
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiGet_TaskCommentList, AwGetTaskCommentList)