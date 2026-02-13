#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"
#include "Database.h"
#include "SqliteUtils.h"
#include "AccessCheck.h"

// Task: ReadChange
static void AwGetTaskLogList(const API_CTX& Ctx) noexcept
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
        constexpr char Sql[]{ R"(
SELECT t.field_name, t.old_value, t.new_value, t.change_at, t.user_id, u.user_name
FROM TaskLog AS t
JOIN Acl AS a ON a.entity_id = t.task_id
LEFT JOIN User AS u ON u.user_id = t.user_id
WHERE
    t.task_id = ? AND
    a.user_id = ? AND
    (a.access & ?) != 0
ORDER BY t.change_at DESC
LIMIT ? OFFSET ?;
)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }

        sqlite3_bind_int(pStmt, 1, iTaskId);
        sqlite3_bind_int(pStmt, 2, CkDbGetCurrentPseudoUser(Ctx));
        sqlite3_bind_int(pStmt, 3, int(DbAccess::ReadChange | DbAccess::FullControl));
        sqlite3_bind_int(pStmt, 4, cEntry);
        sqlite3_bind_int(pStmt, 5, nPage * cEntry);
        while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
        {
            const auto Obj = j.NewObject();
            Obj = {
                "field_name", SuColumnStringView(pStmt, 0),
                "old_value", SuColumnStringView(pStmt, 1),
                "new_value", SuColumnStringView(pStmt, 2),
                "change_at", sqlite3_column_int64(pStmt, 3),
                "user_id", sqlite3_column_int(pStmt, 4),
                "user_name", SuColumnStringView(pStmt, 5)
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
TKK_API_DEF_ENTRY(ApiGet_TaskLogList, AwGetTaskLogList)

// Task: WriteContent
static void AwInsertTaskRelation(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValId = jIn["/task_id"];
        const auto ValRelId = jIn["/relation_id"];
        const auto ValType = jIn["/relation_type"];
        if (!ValId.IsValid() || !ValRelId.IsValid() || !ValType.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValId.IsInt() || !ValRelId.IsInt() || !ValType.IsInt())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        if (!AclDbCheckCurrentUserAccess(Ctx,
            ValId.GetInt(), DbAccess::WriteContent, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        eck::CRefStrA rsSql{};
        int cCol{};

        PCSTR pszName{};
        int eStatus{ -1 };
        int ePriority{ -1 };
        PCSTR pszDesc{};
        // 制语句
        rsSql.Assign(R"(
INSERT INTO TaskRelation(task_id, relation_id, relation_type)
SELECT ?, ?, ?
WHERE EXISTS (SELECT 1 FROM
)"sv);
        switch ((DbRelationType)ValType.GetInt())
        {
        case DbRelationType::Task:
            rsSql.PushBack(R"( Task WHERE task_id = ?);)"sv);
            break;
        case DbRelationType::Page:
            rsSql.PushBack(R"( Page WHERE page_id = ?);)"sv);
            break;
        case DbRelationType::Tag:
            rsSql.PushBack(R"( Tag WHERE tag_id = ?);)"sv);
            break;
        default:
            rApi = ApiResult::InvalidEnum;
            goto Exit;
        }

        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            rsSql.Data(), rsSql.Size(), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, ValId.GetInt());
        sqlite3_bind_int(pStmt, 2, ValRelId.GetInt());
        sqlite3_bind_int(pStmt, 3, ValType.GetInt());
        sqlite3_bind_int(pStmt, 4, ValRelId.GetInt());

        r = sqlite3_step(pStmt);
        sqlite3_finalize(pStmt);

        if (r == SQLITE_DONE)
        {
            r = SQLITE_OK;
            if (!sqlite3_changes(Ctx.pExtra->pSqlite))
                rApi = ApiResult::NoEffect;
        }
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
TKK_API_DEF_ENTRY(ApiPost_InsertTaskRelation, AwInsertTaskRelation)

// Task: WriteContent
static void AwDeleteTaskRelation(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValTaskId = jIn["/task_id"];
        const auto ValRelId = jIn["/relation_id"];
        if (!ValTaskId.IsValid() || !ValRelId.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValTaskId.IsInt() || !ValRelId.IsInt())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        if (!AclDbCheckCurrentUserAccess(Ctx,
            ValTaskId.GetInt(), DbAccess::WriteContent, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(
DELETE FROM TaskRelation
WHERE task_id = ? AND relation_id = ?;
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
        sqlite3_bind_int(pStmt, 2, ValRelId.GetInt());
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
TKK_API_DEF_ENTRY(ApiPost_DeleteTaskRelation, AwDeleteTaskRelation)

// Task: ReadContent
static void AwGetTaskRelationList(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int cEntry{}, nPage{}, iTaskId{ DbIdInvalid };
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("task_id"))
            ApiParseInt(e.V, iTaskId);
    }

    if (cEntry <= 0 || cEntry > MaxQueryCount)
        cEntry = MaxQueryCount;

    Json::CMutDoc j{};
    const auto Arr = j.NewArray();

    if (iTaskId != DbIdInvalid)
    {
        if (!AclDbCheckCurrentUserAccess(Ctx,
            iTaskId, DbAccess::ReadContent, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(
SELECT r.relation_id, r.relation_type, COALESCE(t.task_name, p.page_name)
FROM TaskRelation AS r
LEFT JOIN Task AS t ON (r.relation_type = 1 AND t.task_id = r.relation_id)
LEFT JOIN Page AS p ON (r.relation_type = 2 AND p.page_id = r.relation_id)
WHERE r.task_id = ?;
)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, iTaskId);
        while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
        {
            const auto Obj = j.NewObject();
            Obj = {
                "relation_id", sqlite3_column_int(pStmt, 0),
                "relation_type", sqlite3_column_int(pStmt, 1),
                "name", SuColumnStringView(pStmt, 2),
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
TKK_API_DEF_ENTRY(ApiGet_TaskRelationList, AwGetTaskRelationList)