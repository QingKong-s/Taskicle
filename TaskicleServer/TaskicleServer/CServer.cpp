#include "pch.h"
#include "CServer.h"
#include "ServerApi.h"
#include "Database.h"

constexpr static size_t MaxBodySize = 2 * 1024 * 1024;

EnHttpParseResult CServer::OnHeadersComplete(IHttpServer* pSender, CONNID dwConnId)
{
    LOGI << dwConnId;
    EXTRA* pExtra;
    pSender->GetConnectionExtra(dwConnId, (void**)&pExtra);
    if (pExtra)
        pExtra->rbBody.Clear();
    return HPR_OK;
}

EnHttpParseResult CServer::OnBody(IHttpServer* pSender, CONNID dwConnId,
    const BYTE* pData, int iLength)
{
    LOGI << dwConnId << ": "sv << iLength;
    EXTRA* pExtra;
    pSender->GetConnectionExtra(dwConnId, (void**)&pExtra);
    if (!pExtra)
        return HPR_OK;
    if (pExtra->rbBody.Size() + iLength > MaxBodySize)
        return HPR_ERROR;
    pExtra->rbBody.PushBack(pData, iLength);
    return HPR_OK;
}

EnHttpParseResult CServer::OnMessageComplete(IHttpServer* pSender, CONNID dwConnId)
{
    EXTRA* pExtra;
    pSender->GetConnectionExtra(dwConnId, (void**)&pExtra);
    if (!pExtra)
        return HPR_OK;
    const auto pszPath = pSender->GetUrlField(dwConnId, HUF_PATH);
    const auto cchPath = eck::TcsLen(pszPath);

    const API_CTX Ctx
    {
        .pSender = pSender,
        .dwConnId = dwConnId,
        .pExtra = pExtra,
    };
#define TKK_HIT_PATH(x) eck::TcsIsStartWithLen2I(pszPath, cchPath, EckStrAndLen(x))

    if ((cchPath == 1 && *pszPath == '/') ||
        TKK_HIT_PATH("/index.html") ||
        TKK_HIT_PATH("/article") ||
        TKK_HIT_PATH("/task"))
        return ApiGet_Index(Ctx);

    else if (TKK_HIT_PATH("/api/proj_insert"))
        return ApiPost_InsertProject(Ctx);
    else if (TKK_HIT_PATH("/api/proj_delete"))
        return ApiPost_DeleteProject(Ctx);
    else if (TKK_HIT_PATH("/api/proj_update"))
        return ApiPost_UpdateProject(Ctx);
    else if (TKK_HIT_PATH("/api/proj_list"))
        return ApiGet_ProjectList(Ctx);

    else if (TKK_HIT_PATH("/api/task_insert"))
        return ApiPost_InsertTask(Ctx);
    else if (TKK_HIT_PATH("/api/task_delete"))
        return ApiPost_DeleteTask(Ctx);
    else if (TKK_HIT_PATH("/api/task_update"))
        return ApiPost_UpdateTask(Ctx);
    else if (TKK_HIT_PATH("/api/task_list"))
        return ApiGet_TaskList(Ctx);

    else if (TKK_HIT_PATH("/api/task_comm_insert"))
        return ApiPost_InsertTaskComment(Ctx);
    else if (TKK_HIT_PATH("/api/task_comm_delete"))
        return ApiPost_DeleteTaskComment(Ctx);
    else if (TKK_HIT_PATH("/api/task_comm_update"))
        return ApiPost_UpdateTaskComment(Ctx);
    else if (TKK_HIT_PATH("/api/task_comm_list"))
        return ApiGet_TaskCommentList(Ctx);

    else if (TKK_HIT_PATH("/api/task_log"))
        return ApiGet_TaskLogList(Ctx);
    else if (TKK_HIT_PATH("/api/task_relation_insert"))
        return ApiPost_InsertTaskRelation(Ctx);
    else if (TKK_HIT_PATH("/api/task_relation_delete"))
        return ApiPost_DeleteTaskRelation(Ctx);
    else if (TKK_HIT_PATH("/api/task_relation"))
        return ApiGet_TaskRelationList(Ctx);

    else if (TKK_HIT_PATH("/api/page_group_insert"))
        return ApiPost_InsertPageGroup(Ctx);
    else if (TKK_HIT_PATH("/api/page_group_delete"))
        return ApiPost_DeletePageGroup(Ctx);
    else if (TKK_HIT_PATH("/api/page_group_update"))
        return ApiPost_UpdatePageGroup(Ctx);
    else if (TKK_HIT_PATH("/api/page_group_list"))
        return ApiGet_PageGroupList(Ctx);

    else if (TKK_HIT_PATH("/api/page_insert"))
        return ApiPost_InsertPage(Ctx);
    else if (TKK_HIT_PATH("/api/page_delete"))
        return ApiPost_DeletePage(Ctx);
    else if (TKK_HIT_PATH("/api/page_update"))
        return ApiPost_UpdatePage(Ctx);
    else if (TKK_HIT_PATH("/api/page_list"))
        return ApiGet_PageList(Ctx);

    else if (TKK_HIT_PATH("/api/page_save"))
        return ApiPost_PageSave(Ctx);
    else if (TKK_HIT_PATH("/api/page_load"))
        return ApiGet_PageLoad(Ctx);

    else if (TKK_HIT_PATH("/api/page_version_list"))
        return ApiGet_PageVersionList(Ctx);
    else if (TKK_HIT_PATH("/api/page_version_content"))
        return ApiGet_PageVersionContent(Ctx);

    else if (TKK_HIT_PATH("/api/login"))
        return ApiGet_Login(Ctx);
    else if (TKK_HIT_PATH("/api/register"))
        return ApiPost_Register(Ctx);

    else if (TKK_HIT_PATH("/api/search"))
        return ApiGet_SearchEntity(Ctx);

    else
        return ApiGet_ResourceFile(Ctx);

#undef TKK_HIT_PATH
    return HPR_OK;
}

EnHttpParseResult CServer::OnParseError(IHttpServer* pSender,
    CONNID dwConnId, int iErrorCode, LPCSTR lpszErrorDesc)
{
    return HPR_OK;
}

EnHandleResult CServer::OnAccept(ITcpServer* pSender, CONNID dwConnId, UINT_PTR soClient)
{
    const auto pExtra = new EXTRA{};
    pSender->SetConnectionExtra(dwConnId, pExtra);
    WCHAR szIp[24];
    int cchIp{ ARRAYSIZE(szIp) - 1 };
    USHORT usPort;
    pSender->GetRemoteAddress(dwConnId, szIp, cchIp, usPort);
    // 返回长度包含结尾NULL
    szIp[cchIp - 1] = L':';
    LOGI << dwConnId << ": Accepted, IP: "sv
        << std::wstring_view{ szIp, (size_t)cchIp } << usPort;
    return HR_OK;
}

EnHandleResult CServer::OnClose(ITcpServer* pSender,
    CONNID dwConnId, EnSocketOperation enOperation, int iErrorCode)
{
    EXTRA* pExtra;
    pSender->GetConnectionExtra(dwConnId, (void**)&pExtra);
    pSender->SetConnectionExtra(dwConnId, nullptr);
    pExtra->DecRef();
    return HR_OK;
}

EnSocketError CServer::Start() noexcept
{
    s_pInst = new CServer{};
    s_pServer = HP_Create_HttpServer(s_pInst);
    if (s_pServer->Start(L"0.0.0.0", 8081))
        return SE_OK;
    return s_pServer->GetLastError();
}

void CServer::Stop() noexcept
{
    if (s_pServer)
    {
        s_pServer->Stop();
        HP_Destroy_HttpServer(s_pServer);
        s_pServer = nullptr;
    }
    delete s_pInst;
    s_pInst = nullptr;
}

ConnectionData::~ConnectionData()
{
    if (pSqlite)
        DbClose(pSqlite);
    if (pSqlitePv)
        DbPvClose(pSqlitePv);
}

int ConnectionData::OpenDatabase() noexcept
{
    if (pSqlite)
        return SQLITE_OK;
    return DbOpen(pSqlite);
}

int ConnectionData::OpenPageDatabase() noexcept
{
    if (pSqlitePv)
        return SQLITE_OK;
    return DbPvOpen(pSqlitePv);
}