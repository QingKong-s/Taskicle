#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"
#include "Database.h"
#include "SqliteUtils.h"
#include "AccessCheck.h"

// Project: CreateEntity
static void AwInsertTask(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValProjId = jIn["/project_id"];
        if (!ValProjId.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValProjId.IsInt())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        const auto iUserId = CkDbGetCurrentUser(Ctx);
        if (!AclDbCheckAccess(Ctx, iUserId,
            ValProjId.GetInt(), DbAccess::CreateEntity, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        eck::CRefStrA rsSql{};
        int cCol{};

        std::string_view svName{};
        int eStatus{ -1 };
        int ePriority{ -1 };
        std::string_view svDesc{};
        UINT64 tExpire{};
        // 制语句
        rsSql.Assign(R"(INSERT INTO Task(task_id, project_id, creator_id, assignee_id)"sv);

        const auto ValName = jIn["/task_name"];
        if (ValName.IsValid() && ValName.IsString())
        {
            svName = { ValName.GetString(), ValName.GetLength() };
            if (svName.empty())
                svName = "Untitled Task"sv;
            rsSql.PushBack(EckStrAndLen(",task_name"));
            ++cCol;
        }
        const auto ValStatus = jIn["/status"];
        if (ValStatus.IsValid() && ValStatus.IsInt())
        {
            eStatus = ValStatus.GetInt();
            rsSql.PushBack(EckStrAndLen(",status"));
            ++cCol;
        }
        const auto ValPriority = jIn["/priority"];
        if (ValPriority.IsValid() && ValPriority.IsInt())
        {
            ePriority = ValPriority.GetInt();
            rsSql.PushBack(EckStrAndLen(",priority"));
            ++cCol;
        }
        const auto ValDesc = jIn["/description"];
        if (ValDesc.IsValid() && ValDesc.IsString())
        {
            svDesc = { ValDesc.GetString(), ValDesc.GetLength() };
            if (!svDesc.empty())
            {
                rsSql.PushBack(EckStrAndLen(",description"));
                ++cCol;
            }
        }
        const auto ValExpire = jIn["/expire_at"];
        if (ValExpire.IsValid() && ValExpire.IsInt())
        {
            tExpire = ValExpire.GetUInt64();
            if (tExpire)
            {
                rsSql.PushBack(EckStrAndLen(",expire_at"));
                ++cCol;
            }
        }

        rsSql.PushBack(EckStrAndLen(R"(
)VALUES ((SELECT id FROM GlobalId), ?, ?, ?
)"));
        EckCounterNV(cCol)
            rsSql.PushBack(EckStrAndLen(",?"));
        rsSql.PushBack(EckStrAndLen(");"));

        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            rsSql.Data(), rsSql.Size(), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        // 绑定
        int idxCol = 1;
        // project_id
        sqlite3_bind_int(pStmt, idxCol++, ValProjId.GetInt());
        // creator_id
        sqlite3_bind_int(pStmt, idxCol++, iUserId);
        // assignee_id
        const auto ValAssignee = jIn["/assignee_id"];
        sqlite3_bind_int(pStmt, idxCol++,
            (ValAssignee.IsValid() && ValAssignee.IsInt()) ?
            ValAssignee.GetInt() :
            iUserId);
        //
        if (!svName.empty())
            sqlite3_bind_text(pStmt, idxCol++,
                svName.data(), (int)svName.size(), SQLITE_STATIC);
        if (eStatus >= 0)
            sqlite3_bind_int(pStmt, idxCol++, eStatus);
        if (ePriority >= 0)
            sqlite3_bind_int(pStmt, idxCol++, ePriority);
        if (!svDesc.empty())
            sqlite3_bind_text(pStmt, idxCol++,
                svDesc.data(), (int)svDesc.size(), SQLITE_STATIC);
        if (tExpire)
            sqlite3_bind_int64(pStmt, idxCol++, tExpire);

        CSqliteTransaction Tx{ Ctx.pExtra->pSqlite };
        r = DbIncrementId(Ctx.pExtra->pSqlite);
        if (r == SQLITE_OK)
        {
            r = sqlite3_step(pStmt);
            if (r == SQLITE_DONE)
                r = AclDbOnEntityCreate(Ctx, iUserId);
        }
        sqlite3_finalize(pStmt);

        if (r == SQLITE_OK)
            Tx.Commit();
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
TKK_API_DEF_ENTRY(ApiPost_InsertTask, AwInsertTask)

// Task: Delete
static void AwDeleteTask(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValId = jIn["/task_id"];
        if (!ValId.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValId.IsInt())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        if (!AclDbCheckCurrentUserAccess(Ctx,
            ValId.GetInt(), DbAccess::Delete, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        constexpr char Sql[]{ R"(DELETE FROM Task WHERE task_id = ?)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, ValId.GetInt());

        CSqliteTransaction Tx{ Ctx.pExtra->pSqlite };
        r = sqlite3_step(pStmt);
        sqlite3_finalize(pStmt);
        if (r == SQLITE_DONE)
            r = AclDbOnEntityDelete(Ctx, ValId.GetInt());

        if (r == SQLITE_OK)
            Tx.Commit();
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
TKK_API_DEF_ENTRY(ApiPost_DeleteTask, AwDeleteTask)

// Task: WriteContent
static void AwUpdateTask(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValId = jIn["/task_id"];
        if (!ValId.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValId.IsInt())
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

        std::string_view svName{};
        int eStatus{ -1 };
        int ePriority{ -1 };
        std::string_view svDesc{};
        UINT64 tExpire{};
        int iAssigneeId{ DbIdInvalid };
        // 制语句
        rsSql.Assign(R"(UPDATE Task SET update_at=CAST(unixepoch('subsecond') * 1000 AS INTEGER),)"sv);

        const auto ValName = jIn["/task_name"];
        if (ValName.IsValid() && ValName.IsString())
        {
            svName = { ValName.GetString(), ValName.GetLength() };
            if (!svName.empty())
            {
                rsSql.PushBack(EckStrAndLen("task_name=?,"));
                ++cCol;
            }
        }
        const auto ValStatus = jIn["/status"];
        if (ValStatus.IsValid() && ValStatus.IsInt())
        {
            eStatus = ValStatus.GetInt();
            rsSql.PushBack(EckStrAndLen("status=?,"));
            ++cCol;
        }
        const auto ValPriority = jIn["/priority"];
        if (ValPriority.IsValid() && ValPriority.IsInt())
        {
            ePriority = ValPriority.GetInt();
            rsSql.PushBack(EckStrAndLen("priority=?,"));
            ++cCol;
        }
        const auto ValDesc = jIn["/description"];
        if (ValDesc.IsValid())
        {
            svDesc = { ValDesc.GetString(), ValDesc.GetLength() };
            if (!svDesc.empty())
            {
                rsSql.PushBack(EckStrAndLen("description=?,"));
                ++cCol;
            }
        }
        const auto ValExpire = jIn["/expire_at"];
        if (ValExpire.IsValid() && ValExpire.IsNumber())
        {
            tExpire = ValExpire.GetUInt64();
            if (tExpire)
            {
                rsSql.PushBack(EckStrAndLen("expire_at=?,"));
                ++cCol;
            }
        }
        const auto ValAssignee = jIn["/assignee_id"];
        if (ValAssignee.IsValid() && ValAssignee.IsInt())
        {
            iAssigneeId = ValAssignee.GetInt();
            rsSql.PushBack(EckStrAndLen("assignee_id=?,"));
            ++cCol;
        }

        if (!cCol)
        {
            r = SQLITE_OK;
            rApi = ApiResult::NoField;
            goto Exit;
        }

        rsSql.PopBack();// 最后一个逗号
        rsSql.PushBack(EckStrAndLen(" WHERE task_id = ?;"));

        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            rsSql.Data(), rsSql.Size(), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        // 绑定
        int idxCol = 1;
        if (!svName.empty())
            sqlite3_bind_text(pStmt, idxCol++,
                svName.data(), (int)svName.size(), SQLITE_STATIC);
        if (eStatus >= 0)
            sqlite3_bind_int(pStmt, idxCol++, eStatus);
        if (ePriority >= 0)
            sqlite3_bind_int(pStmt, idxCol++, ePriority);
        if (!svDesc.empty())
            sqlite3_bind_text(pStmt, idxCol++,
                svDesc.data(), (int)svDesc.size(), SQLITE_STATIC);
        if (tExpire)
            sqlite3_bind_int64(pStmt, idxCol++, tExpire);
        if (iAssigneeId != DbIdInvalid)
            sqlite3_bind_int(pStmt, idxCol++, iAssigneeId);
        sqlite3_bind_int(pStmt, idxCol++, ValId.GetInt());

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
TKK_API_DEF_ENTRY(ApiPost_UpdateTask, AwUpdateTask)

// Task: ReadContent
static void AwGetTaskList(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int cEntry{}, nPage{}, iProjId{ DbIdInvalid };
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("count"))
            ApiParseInt(e.V, cEntry);
        else if (TKK_API_HIT_QUERY("page"))
            ApiParseInt(e.V, nPage);
        else if (TKK_API_HIT_QUERY("project_id"))
            ApiParseInt(e.V, iProjId);
    }

    if (cEntry <= 0 || cEntry > MaxQueryCount)
        cEntry = MaxQueryCount;

    Json::CMutDoc j{};
    const auto Arr = j.NewArray();

    constexpr char Sql[]{ R"(
SELECT
    t.task_id, t.task_name, t.status, t.priority,
    t.description, t.create_at, t.update_at,
    t.expire_at, t.assignee_id, t.creator_id
FROM Task AS t
JOIN Acl AS a
ON a.entity_id = t.task_id
WHERE
    t.project_id = ? AND
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
    sqlite3_bind_int(pStmt, 1, iProjId);
    sqlite3_bind_int(pStmt, 2, CkDbGetCurrentPseudoUser(Ctx));
    sqlite3_bind_int(pStmt, 3, int(DbAccess::ReadContent | DbAccess::FullControl));
    sqlite3_bind_int(pStmt, 4, cEntry);
    sqlite3_bind_int(pStmt, 5, nPage * cEntry);
    while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        const auto Obj = j.NewObject();
        Obj = {
            "task_id", sqlite3_column_int(pStmt, 0),
            "task_name", SuColumnStringView(pStmt, 1),
            "status", sqlite3_column_int(pStmt, 2),
            "priority", sqlite3_column_int(pStmt, 3),
            "description", SuColumnStringView(pStmt, 4),
            "create_at", sqlite3_column_int64(pStmt, 5),
            "update_at", sqlite3_column_int64(pStmt, 6),
            "expire_at", sqlite3_column_int64(pStmt, 7),
            "assignee_id", sqlite3_column_int(pStmt, 8),
            "creator_id", sqlite3_column_int(pStmt, 9),
        };
        Arr.ArrPushBack(Obj);
    }
    sqlite3_finalize(pStmt);
    if (r == SQLITE_DONE)
        r = SQLITE_OK;
    else
        pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
Exit:
    j = {
        "r", r == SQLITE_OK ? rApi : ApiResult::Database,
        "r2", r,
        "err_msg", pszErrMsg,
        "data", Arr
    };
    ApiSendResponseJson(Ctx, j);
}
TKK_API_DEF_ENTRY(ApiGet_TaskList, AwGetTaskList)