#include "pch.h"
#include "CServer.h"
#include "ServerApi.h"
#include "Database.h"

constexpr static size_t MaxBodySize = 2 * 1024 * 1024;

using FApiEntry = EnHttpParseResult(*)(const API_CTX&);
const static std::unordered_map<std::string_view, FApiEntry> ApiMap
{
    { "/index.html"sv,               ApiGet_Index               },
    { "/article"sv,                  ApiGet_Index               },
    { "/task"sv,                     ApiGet_Index               },
    { "/api/proj_insert"sv,          ApiPost_InsertProject      },
    { "/api/proj_delete"sv,          ApiPost_DeleteProject      },
    { "/api/proj_update"sv,          ApiPost_UpdateProject      },
    { "/api/proj_list"sv,            ApiGet_ProjectList         },
    { "/api/task_insert"sv,          ApiPost_InsertTask         },
    { "/api/task_delete"sv,          ApiPost_DeleteTask         },
    { "/api/task_update"sv,          ApiPost_UpdateTask         },
    { "/api/task_list"sv,            ApiGet_TaskList            },
    { "/api/task_comm_insert"sv,     ApiPost_InsertTaskComment  },
    { "/api/task_comm_delete"sv,     ApiPost_DeleteTaskComment  },
    { "/api/task_comm_update"sv,     ApiPost_UpdateTaskComment  },
    { "/api/task_comm_list"sv,       ApiGet_TaskCommentList     },
    { "/api/task_log"sv,             ApiGet_TaskLogList         },
    { "/api/task_relation_insert"sv, ApiPost_InsertTaskRelation },
    { "/api/task_relation_delete"sv, ApiPost_DeleteTaskRelation },
    { "/api/task_relation"sv,        ApiGet_TaskRelationList    },
    { "/api/page_group_insert"sv,    ApiPost_InsertPageGroup    },
    { "/api/page_group_delete"sv,    ApiPost_DeletePageGroup    },
    { "/api/page_group_update"sv,    ApiPost_UpdatePageGroup    },
    { "/api/page_group_list"sv,      ApiGet_PageGroupList       },
    { "/api/page_insert"sv,          ApiPost_InsertPage         },
    { "/api/page_delete"sv,          ApiPost_DeletePage         },
    { "/api/page_update"sv,          ApiPost_UpdatePage         },
    { "/api/page_list"sv,            ApiGet_PageList            },
    { "/api/page_save"sv,            ApiPost_PageSave           },
    { "/api/page_load"sv,            ApiGet_PageLoad            },
    { "/api/page_version_list"sv,    ApiGet_PageVersionList     },
    { "/api/page_version_content"sv, ApiGet_PageVersionContent  },
    { "/api/login"sv,                ApiGet_Login               },
    { "/api/register"sv,             ApiPost_Register           },
    { "/api/search"sv,               ApiGet_SearchEntity        },
    { "/api/acl"sv,                  ApiGet_Acl                 },
    { "/api/modify_acl"sv,           ApiPost_ModifyAccess       },
    { "/api/modify_acl_user"sv,      ApiPost_ModifyAccessUser   },
};

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

    eck::CRefStrA rsPathLower(cchPath);
    for (size_t i{}; auto& e : rsPathLower)
        e = eck::TchToLower(pszPath[i++]);

    const API_CTX Ctx
    {
        .pSender = pSender,
        .dwConnId = dwConnId,
        .pExtra = pExtra,
    };

    if (cchPath == 1 && *pszPath == '/')
        return ApiGet_Index(Ctx);
    const auto it = ApiMap.find(rsPathLower.ToStringView());
    if (it != ApiMap.end())
        return it->second(Ctx);
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