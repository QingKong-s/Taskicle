#pragma once
// WARNING 不得随意修改本文件中的所有常量值

constexpr inline int DbIdInvalid = -1;
constexpr inline int DbIdContainerPageGroup = -2;
constexpr inline int DbIdContainerProject = -3;
constexpr inline int DbIdUserGuest = -4;
constexpr inline int DbIdUserAdmin = -5;

#define TKK_DBID_USER_ADMIN "-5"

constexpr inline int DbPvIdVersionLatest = -1;

enum class DbRelationType
{
    Min,
    Task,
    Page,
    Tag,
    File,
    Max,
};
enum class DbPageType
{
    Min,
    Markdown,
    Max,
};
enum class DbUserRole
{
    Min,
    Normal,
    Admin,
    Max,
};
#define TKK_DBUR_ADMIN      "2"
enum class DbCoreEntity
{
    Min,
    PageGroup,
    Page,
    Project,
    Task,
    Max
};

// 实体权限掩码，用于下列类型：
// PageGroup/Project/Page/Task
enum class DbAccess : UINT
{
    None = 0,
    // 此权限可进行所有操作，不能将其用于DbIdEveryone
    Owner = (1u << 0),
    // 不适用于PageGroup/Project
    ReadContent = (1u << 1),
    // 不适用于PageGroup/Project
    WriteContent = (1u << 2),
    // 删除实体本身
    Delete = (1u << 3),
    // 仅Task
    WriteComment = (1u << 4),
    //
    Rename = (1u << 5),
    // 以下仅适用于组
    // 判断是否可操作页面/任务，使用对应PageGroup/Project的ID
    // 判断是否可操作组，使用DbIdContainerPageGroup/DbIdContainerProject
    CreateEntity = (1u << 6),
    DeleteEntity = (1u << 7),
    // 没有修改记录的实体不适用
    ReadChange = (1u << 8),

    FullControl = Owner,
};
ECK_ENUM_BIT_FLAGS(DbAccess);

#define TKK_DBAC_ADMIN      "1"
#define TKK_DBAC_FULLCTRL   "1"

int DbOpenFirst(std::wstring_view svFile, _Out_ sqlite3*& pSqlite) noexcept;
int DbOpen(_Out_ sqlite3*& pSqlite) noexcept;
void DbClose(sqlite3* pSqlite) noexcept;
int DbInitializeTable(sqlite3* pSqlite) noexcept;
int DbIncrementId(sqlite3* pSqlite) noexcept;
void DbCleanup() noexcept;

int DbPvOpenFirst(std::wstring_view svFile, _Out_ sqlite3*& pSqlite) noexcept;
int DbPvOpen(_Out_ sqlite3*& pSqlite) noexcept;
void DbPvClose(sqlite3* pSqlite) noexcept;
int DbPvInitializeTable(sqlite3* pSqlite) noexcept;