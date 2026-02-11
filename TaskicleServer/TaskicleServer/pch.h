#pragma once
#define WIN32_LEAN_AND_MEAN		1

#define _CRTDBG_MAP_ALLOC	    1

#define ECK_OPT_NO_DARKMODE     1
#define ECK_OPT_NO_DX           1

#include "eck\PchInc.h"
#include "eck\SystemHelper.h"
#include "eck\CEvent.h"
#include "eck\ThreadPool.h"
#include "eck\CSrwLock.h"
#include "eck\Json.h"
#include "eck\FileHelper.h"
#include "eck\Utility2.h"
#include "eck\Crc.h"
#include "eck\Compress.h"
#include "eck\DateTimeHelper.h"
#include "eck\CTimeIdGenerator.h"
#include "eck\CTrivialBuffer.h"
#include "eck\EnDeCode.h"

#include <plog\Log.h>
#include <plog\Initializers\RollingFileInitializer.h>
#include <plog\Appenders\ColorConsoleAppender.h>
#include <sqlite3.h>
#include "HPSocket\HPSocket.h"
#include "dtl\dtl.hpp"

#include <txfw32.h>

using eck::PCVOID;
// using eck::PCBYTE;// HP已有

namespace Json = eck::Json;

using namespace std::literals;