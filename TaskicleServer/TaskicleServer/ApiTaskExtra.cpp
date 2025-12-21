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
    const auto Arr = j.NewArr();

    if (iTaskId != DbIdInvalid)
    {
        constexpr char Sql[]{ R"(
SELECT t.field_name, t.old_value, t.new_value, strftime('%s', t.change_at)
FROM TaskLog AS t
JOIN Acl AS a
ON a.entity_id = t.task_id
WHERE
    t.task_id = ? AND
    a.user_id = ? AND
    (a.access & ?) != 0
ORDER BY t.task_id ASC
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
        sqlite3_bind_int(pStmt, 2, CkDbGetCurrentUser(Ctx));
        sqlite3_bind_int(pStmt, 3, int(DbAccess::ReadChange | DbAccess::FullControl));
        sqlite3_bind_int(pStmt, 4, cEntry);
        sqlite3_bind_int(pStmt, 5, nPage * cEntry);
        while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
        {
            const auto Obj = j.NewObj();
            Obj = {
                "field_name", SuColumnStringView(pStmt, 0),
                "old_value", SuColumnStringView(pStmt, 1),
                "new_value", SuColumnStringView(pStmt, 2),
                "change_at", sqlite3_column_int64(pStmt, 3),
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
        rsSql.DupString(EckStrAndLen(R"(
INSERT INTO TaskRelation(task_id, relation_id, relation_type)
SELECT ?, ?, ?
WHERE EXISTS (SELECT 1 FROM
)"));
        switch ((DbRelationType)ValType.GetInt())
        {
        case DbRelationType::Task:
            rsSql.PushBack(EckStrAndLen(R"( Task WHERE task_id = ?);)"));
            break;
        case DbRelationType::Tag:
            rsSql.PushBack(EckStrAndLen(R"( Tag WHERE tag_id = ?);)"));
            break;
        case DbRelationType::Article:
            rsSql.PushBack(EckStrAndLen(R"( Article WHERE article_id = ?;)"));
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
    const auto Arr = j.NewArr();

    if (!AclDbCheckCurrentUserAccess(Ctx,
        iTaskId, DbAccess::ReadContent, r))
    {
        rApi = ApiResult::AccessDenied;
        goto Exit;
    }

    if (iTaskId != DbIdInvalid)
    {
        constexpr char Sql[]{ R"(
SELECT relation_id, relation_type FROM TaskRelation
WHERE task_id = ?;
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
            const auto Obj = j.NewObj();
            Obj = {
                "relation_id", sqlite3_column_int(pStmt, 0),
                "relation_type", sqlite3_column_int(pStmt, 1),
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