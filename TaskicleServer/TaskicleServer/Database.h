#pragma once
// WARNING 不得随意修改本文件中的所有常量值

constexpr inline int DbIdInvalid = -1;
constexpr inline int DbIdContainerPageGroup = -2;
constexpr inline int DbIdContainerProject = -3;
constexpr inline int DbIdUserEveryone = -4;
constexpr inline int DbPvIdVersionLatest = -5;

enum class DbRelationType
{
    Min,
    Task,
    Article,
    Tag,
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
    // 与Owner相同，区别是Owner不能被移除，并且表示创建者的特殊含义
    Admin = (1u << 1),
    // 不适用于PageGroup/Project
    ReadContent = (1u << 8),
    // 不适用于PageGroup/Project
    WriteContent = (1u << 9),
    // 删除实体本身
    DeleteMe = (1u << 10),
    // 没有修改记录的实体不适用
    ReadChange = (1u << 11),
    // 仅Task
    ReadComment = (1u << 12),
    // 仅Task
    WriteComment = (1u << 13),
    //
    Rename = (1u << 14),
    // 以下仅适用于组
    // 判断是否可操作页面/任务，使用对应PageGroup/Project的ID
    // 判断是否可操作组，使用DbIdContainerPageGroup/DbIdContainerProject
    CreateEntity = (1u << 15),
    DeleteEntity = (1u << 16),
    // 修改权限
    WriteAcl = (1u << 17),

    FullControl = Owner | Admin,
};
ECK_ENUM_BIT_FLAGS(DbAccess);

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