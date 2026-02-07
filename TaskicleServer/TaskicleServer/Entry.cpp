#include "pch.h"

#include "eck\Env.h"

#include "CServer.h"
#include "Database.h"

#ifdef _DEBUG
#  ifdef _WIN64
#    pragma comment(lib, "HPSocket/Lib/x64/HPSocket_UD.lib")
#  else
#    pragma comment(lib, "HPSocket/Lib/x86/HPSocket_UD.lib")
#  endif
#else
#  ifdef _WIN64
#    pragma comment(lib, "HPSocket/Lib/x64/HPSocket_U.lib")
#  else
#    pragma comment(lib, "HPSocket/Lib/x86/HPSocket_U.lib")
#  endif
#endif


int wmain(int argc, WCHAR** argv)
{
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    // 初始化ECK
    eck::INITPARAM ip{};
    ip.uFlags = eck::EIF_CONSOLE_APP;
    UINT uErr;
    const auto eInitRet = eck::Initialize(NtCurrentImageBaseHInst(), &ip, &uErr);
    if (eInitRet != eck::InitStatus::Ok)
    {
        EckDbgPrintFormatMessage(uErr);
        eck::CRefStrW rs{};
        rs.Format(L"Init failed: %d(0x%08X)", (int)eInitRet, uErr);
        MessageBoxW(nullptr, rs.Data(), L"Error", MB_ICONERROR);
        return 0;
    }
    auto rsFileTemp{ eck::GetRunningPath() };
    const auto cchRunningPath = rsFileTemp.Size();
    // 初始化plog
    rsFileTemp.PushBack(EckStrAndLen(L"\\log.log"));
    plog::init(plog::info, rsFileTemp.Data(), 65565, 100);
    plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::get()->addAppender(&consoleAppender);
    LOGI << "Server started.";
    // 读取配置

    // 初始化数据库
    EckAssert(sqlite3_threadsafe());

    sqlite3* pSqlite{};
    // 主库
    rsFileTemp.ReSize(cchRunningPath);
    rsFileTemp.PushBack(EckStrAndLen(L"\\res\\db.db"));
    if (DbOpenFirst(rsFileTemp.Data(), pSqlite) != SQLITE_OK)
        goto Exit;
    if (DbInitializeTable(pSqlite) != SQLITE_OK)
    {
        DbClose(pSqlite);
        goto Exit;
    }
    DbClose(pSqlite);
    // 文章版本库
    rsFileTemp.ReSize(cchRunningPath);
    rsFileTemp.PushBack(EckStrAndLen(L"\\res\\db_page.db"));
    if (DbPvOpenFirst(rsFileTemp.Data(), pSqlite) != SQLITE_OK)
        goto Exit;
    if (DbPvInitializeTable(pSqlite) != SQLITE_OK)
    {
        DbPvClose(pSqlite);
        goto Exit;
    }
    DbPvClose(pSqlite);
    // 启动http服务器
    if (const auto r = CServer::Start(); r != SE_OK)
    {
        LOGE << "Start server failed: " << r;
        goto Exit;
    }

    std::cin.get();
Exit:
    CServer::Stop();
    DbCleanup();
    eck::Uninitialize();
    return 0;
}