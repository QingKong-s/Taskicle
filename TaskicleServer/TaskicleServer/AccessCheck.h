#pragma once
BOOL AclDbCheckAccess(const API_CTX& Ctx, int iUserId,
    int iEntityId, DbAccess eAccess, _Out_ int& r) noexcept;
BOOL AclDbCheckCurrentUserAccess(const API_CTX& Ctx,
    int iEntityId, DbAccess eAccess, _Out_ int& r) noexcept;

// WARNING 必须在事务中调用，使用GlobalId的当前值作为entity_id
int AclDbOnEntityCreate(const API_CTX& Ctx, int iUserId) noexcept;
// WARNING 必须在事务中调用
int AclDbOnEntityDelete(const API_CTX& Ctx, int iEntityId) noexcept;

int CkDbGetCurrentUser(const API_CTX& Ctx) noexcept;
int CkDbGetCurrentPseudoUser(const API_CTX& Ctx) noexcept;

BOOL UmIsAdministrator(const API_CTX& Ctx, int id) noexcept;