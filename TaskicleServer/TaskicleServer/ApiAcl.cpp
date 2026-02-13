#include "pch.h"
#include "ServerApi.h"
#include "ApiPriv.h"
#include "Database.h"
#include "AccessCheck.h"

BOOL AclDbCheckAccess(const API_CTX& Ctx, int iUserId,
    int iEntityId, DbAccess eAccess, _Out_ int& r) noexcept
{
    if (iUserId == DbIdUserAdmin)
    {
        r = SQLITE_OK;
        return TRUE;
    }

    constexpr char Sql[]{ R"sql(
SELECT 
    CASE 
        WHEN (SELECT role FROM User WHERE user_id = :uid) = )sql" TKK_DBUR_ADMIN R"sql( THEN 1
        ELSE (
            SELECT COUNT(*)
            FROM Acl a
            WHERE a.user_id = :uid
                AND a.entity_id = :eid
                AND ((a.access & :e = :e) OR (a.access & )sql" TKK_DBAC_FULLCTRL R"sql( != 0))
        )
    END AS has_access;
)sql" };

    sqlite3_stmt* pStmt;
    r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return FALSE;
    sqlite3_bind_int(pStmt, sqlite3_bind_parameter_index(pStmt, ":uid"), iUserId);
    sqlite3_bind_int(pStmt, sqlite3_bind_parameter_index(pStmt, ":eid"), iEntityId);
    sqlite3_bind_int(pStmt, sqlite3_bind_parameter_index(pStmt, ":e"), (int)eAccess);
    r = sqlite3_step(pStmt);
    BOOL b;
    if (r == SQLITE_ROW)
    {
        r = SQLITE_OK;
        b = !!sqlite3_column_int(pStmt, 0);
    }
    else
        b = FALSE;
    sqlite3_finalize(pStmt);
    return b;
}

BOOL AclDbCheckCurrentUserAccess(const API_CTX& Ctx, int iEntityId,
    DbAccess eAccess, _Out_ int& r) noexcept
{
    return AclDbCheckAccess(Ctx, CkDbGetCurrentUser(Ctx), iEntityId, eAccess, r);
}

int AclDbOnEntityCreate(const API_CTX& Ctx, int iUserId) noexcept
{
    constexpr char Sql[]{ R"(
INSERT INTO Acl(user_id, entity_id, access)
VALUES (?, (SELECT id FROM GlobalId), ?),)"
"(" TKK_DBID_USER_ADMIN ", (SELECT id FROM GlobalId), " TKK_DBAC_ADMIN ")"
    };

    sqlite3_stmt* pStmt;
    int r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return r;
    sqlite3_bind_int(pStmt, 1, iUserId);
    sqlite3_bind_int(pStmt, 2, (int)DbAccess::Owner);
    r = sqlite3_step(pStmt);
    sqlite3_finalize(pStmt);
    if (r == SQLITE_DONE)
        r = SQLITE_OK;
    return r;
}
int AclDbOnEntityDelete(const API_CTX& Ctx, int iEntityId) noexcept
{
    constexpr char Sql[]{ R"(
DELETE FROM Acl WHERE entity_id = ?;
)" };
    sqlite3_stmt* pStmt;
    int r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
        EckStrAndLen(Sql), 0, &pStmt, nullptr);
    if (r != SQLITE_OK)
        return r;
    sqlite3_bind_int(pStmt, 1, iEntityId);
    r = sqlite3_step(pStmt);
    if (r == SQLITE_DONE)
        r = SQLITE_OK;
    sqlite3_finalize(pStmt);
    return r;
}

static void AwModifyAccess(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValUser = jIn["/user_id"];
        const auto ValEntity = jIn["/entity_id"];
        const auto ValAccess = jIn["/access"];
        const auto ValIsRemove = jIn["/is_remove"];
        if (!ValUser.IsValid() || !ValEntity.IsValid() ||
            !ValAccess.IsValid() || !ValIsRemove.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValUser.IsInt() || !ValEntity.IsInt() ||
            !ValAccess.IsInt() || !ValIsRemove.IsBool())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        const auto iEntityId = ValEntity.GetInt();
        if (!AclDbCheckCurrentUserAccess(Ctx, iEntityId, DbAccess::Unused, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        auto eAccess = (DbAccess)ValAccess.GetInt();
        eAccess &= ~(DbAccess::Owner | DbAccess::Unused);
        if (eAccess == DbAccess::None)
        {
            rApi = ApiResult::NoEffect;
            goto Exit;
        }

        const auto iUserId = ValUser.GetInt();
        const auto bRemove = ValIsRemove.GetBool();
        // 不能修改管理员和所有者
        if (AclDbCheckAccess(Ctx, iUserId, iEntityId, DbAccess::Unused, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        sqlite3_stmt* pStmt;
        if (bRemove)
        {
            constexpr char Sql[]{ R"(
UPDATE Acl SET access = (access & ~?)
WHERE user_id = ? AND entity_id = ?;
)" };
            r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
                EckStrAndLen(Sql), 0, &pStmt, nullptr);
        }
        else
        {
            constexpr char Sql[]{ R"(
UPDATE Acl SET access = (access | ?)
WHERE user_id = ? AND entity_id = ?;
)" };
            r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
                EckStrAndLen(Sql), 0, &pStmt, nullptr);
        }
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, (int)eAccess);
        sqlite3_bind_int(pStmt, 2, iUserId);
        sqlite3_bind_int(pStmt, 3, iEntityId);
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
TKK_API_DEF_ENTRY(ApiPost_ModifyAccess, AwModifyAccess)

static void AwModifyAccessUser(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    if (Json::CDoc jIn{ Ctx.pExtra->rbBody }; jIn.IsValid())
    {
        const auto ValUser = jIn["/user_id"];
        const auto ValEntity = jIn["/entity_id"];
        const auto ValIsRemove = jIn["/is_remove"];
        if (!ValUser.IsValid() || !ValEntity.IsValid() ||
            !ValIsRemove.IsValid())
        {
            rApi = ApiResult::RequiredFieldMissing;
            goto Exit;
        }
        if (!ValUser.IsInt() || !ValEntity.IsInt() ||
            !ValIsRemove.IsBool())
        {
            rApi = ApiResult::TypeMismatch;
            goto Exit;
        }

        const auto iEntityId = ValEntity.GetInt();
        if (!AclDbCheckCurrentUserAccess(Ctx, iEntityId, DbAccess::Unused, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        const auto iUserId = ValUser.GetInt();
        const auto bRemove = ValIsRemove.GetBool();
        // 不能修改管理员和所有者
        if (AclDbCheckAccess(Ctx, iUserId, iEntityId, DbAccess::Unused, r))
        {
            rApi = ApiResult::AccessDenied;
            goto Exit;
        }

        sqlite3_stmt* pStmt;
        if (bRemove)
        {
            constexpr char Sql[]{ "DELETE FROM Acl WHERE user_id = ? AND entity = ?" };
            r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
                EckStrAndLen(Sql), 0, &pStmt, nullptr);
        }
        else
        {
            constexpr char Sql[]{ "INSERT INTO Acl (user_id, entity_id) VALUES (?, ?)" };
            r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
                EckStrAndLen(Sql), 0, &pStmt, nullptr);
        }
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, iUserId);
        sqlite3_bind_int(pStmt, 2, iEntityId);
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
TKK_API_DEF_ENTRY(ApiPost_ModifyAccessUser, AwModifyAccessUser)

// 无权限要求
static void AwGetAcl(const API_CTX& Ctx) noexcept
{
    ApiResult rApi{ ApiResult::Ok };
    int r{};
    PCSTR pszErrMsg{};

    std::vector<QUERY_KV> vKv{};
    ApiParseQueryString(Ctx, vKv);
    int iUserId{ DbIdInvalid }, iEntityId{ DbIdInvalid };
    for (const auto& e : vKv)
    {
        if (TKK_API_HIT_QUERY("user_id"))
            ApiParseInt(e.V, iUserId);
        else if (TKK_API_HIT_QUERY("entity_id"))
            ApiParseInt(e.V, iEntityId);
    }

    Json::CMutDoc j{};
    const auto Arr = j.NewArray();

    if (iUserId != DbIdInvalid)
    {
        constexpr char Sql[]{ R"(SELECT entity_id, access FROM Acl WHERE user_id = ?;)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, iUserId);
        while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
        {
            const auto Obj = j.NewObject();
            Obj = {
                "entity_id", sqlite3_column_int(pStmt, 0),
                "access", sqlite3_column_int(pStmt, 1),
            };
            Arr.ArrPushBack(Obj);
        }
        sqlite3_finalize(pStmt);
        if (r == SQLITE_DONE)
            r = SQLITE_OK;
        else
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
    }
    else if (iEntityId != DbIdInvalid)
    {
        constexpr char Sql[]{ R"(SELECT user_id, access FROM Acl WHERE entity_id = ?;)" };
        sqlite3_stmt* pStmt;
        r = sqlite3_prepare_v3(Ctx.pExtra->pSqlite,
            EckStrAndLen(Sql), 0, &pStmt, nullptr);
        if (r != SQLITE_OK)
        {
            pszErrMsg = sqlite3_errmsg(Ctx.pExtra->pSqlite);
            goto Exit;
        }
        sqlite3_bind_int(pStmt, 1, iEntityId);
        while ((r = sqlite3_step(pStmt)) == SQLITE_ROW)
        {
            const auto Obj = j.NewObject();
            Obj = {
                "user_id", sqlite3_column_int(pStmt, 0),
                "access", sqlite3_column_int(pStmt, 1),
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
TKK_API_DEF_ENTRY(ApiGet_Acl, AwGetAcl)