// xrCore.cpp : Defines the entry point for the DLL application.
//
#include "stdafx.h"
#pragma hdrstop

#include <mmsystem.h>
#include <objbase.h>

#include "xrCore.h"
#include "Math/MathUtil.hpp"
#include "xrCore/_std_extensions.h"

#pragma comment(lib, "winmm.lib")

XRCORE_API xrCore Core;

static u32 init_counter = 0;

void xrCore::Initialize(pcstr _ApplicationName, LogCallback cb, bool init_fs, pcstr fs_fname, bool plugin)
{
    xr_strcpy(ApplicationName, _ApplicationName);
    if (0 == init_counter)
    {
        CalculateBuildId();
        PluginMode = plugin;
        // Init COM so we can use CoCreateInstance
        // HRESULT co_res =

        Params = xr_strdup(GetCommandLine());

        if (!strstr(Params, "-weather"))
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        initParamFlags();

        string_path fn, dr, di;

        // application path

        GetModuleFileName(GetModuleHandle("xrCore"), fn, sizeof(fn));

        _splitpath(fn, dr, di, nullptr, nullptr);
        strconcat(sizeof(ApplicationPath), ApplicationPath, dr, di);

        GetCurrentDirectory(sizeof(WorkingPath), WorkingPath);

        // User/Comp Name
        DWORD sz_user = sizeof(UserName);
        GetUserName(UserName, &sz_user);

        DWORD sz_comp = sizeof(CompName);
        GetComputerName(CompName, &sz_comp);

        Memory._initialize();
        
        InitLog();

        Msg("%s %s build %d, %s\n", "Touch of ray 4.3", GetBuildConfiguration(), buildId, buildDate);
        Msg("command line %s\n", Params);
        _initialize_cpu();
        R_ASSERT(CPU::ID.hasFeature(CpuFeature::Sse));
        XRay::Math::Initialize();
        // xrDebug::Initialize ();

        rtc_initialize();

        xr_FS = std::make_unique<CLocatorAPI>();

        xr_EFS = std::make_unique<EFS_Utils>();
        //. R_ASSERT (co_res==S_OK);
    }
    if (init_fs)
    {
        u32 flags = 0u;
        if (strstr(Params, "-build") != nullptr)
            flags |= CLocatorAPI::flBuildCopy;
        if (strstr(Params, "-ebuild") != nullptr)
            flags |= CLocatorAPI::flBuildCopy | CLocatorAPI::flEBuildCopy;

        flags |= CLocatorAPI::flScanAppRoot;

        if (strstr(Params, "-file_activity") != nullptr)
            flags |= CLocatorAPI::flDumpFileActivity;

        FS._initialize(flags, nullptr, fs_fname);
        EFS._initialize();
    }
    SetLogCB(cb);
    init_counter++;
}

void xrCore::initParamFlags()
{
    if (strstr(Params, "-verboselog"))
        ParamFlags.set(ParamFlag::verboselog, true);

    if (strstr(Params, "-dbgbullet"))
        ParamFlags.set(ParamFlag::dbgbullet, true);

    if (strstr(Params, "-dev"))
        ParamFlags.set(ParamFlag::dev, true);

    if (strstr(Params, "-genbump"))
        ParamFlags.set(ParamFlag::genbump, true);
}

#include "compression_ppmd_stream.h"
extern compression::ppmd::stream* trained_model;

void xrCore::_destroy()
{
    --init_counter;
    if (0 == init_counter)
    {
        FS._destroy();
        EFS._destroy();
        xr_FS.reset();
        xr_EFS.reset();

        if (trained_model)
        {
            void* buffer = trained_model->buffer();
            xr_free(buffer);
            xr_delete(trained_model);
        }

        xr_free(Params);
        Memory._destroy();
    }
}

constexpr pcstr xrCore::GetBuildConfiguration()
{
#ifdef NDEBUG
#ifdef XR_X64
    return "Rx64";
#else
    return "Rx86";
#endif
#else
#ifdef XR_X64
	return "Dx64";
#else
	return "Dx86";
#endif
#endif
}

void xrCore::CalculateBuildId()
{
    const int startDay = 31;
    const int startMonth = 1;
    const int startYear = 1999;
    const char* monthId[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const int daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    buildDate = __DATE__;
    int days;
    int months = 0;
    int years;
    string16 month;
    string256 buffer;
    xr_strcpy(buffer, buildDate);
    sscanf(buffer, "%s %d %d", month, &days, &years);
    for (int i = 0; i < 12; i++)
    {
        if (xr_stricmp(monthId[i], month))
            continue;
        months = i;
        break;
    }
    buildId = (years - startYear) * 365 + days - startDay;
    for (int i = 0; i < months; i++)
        buildId += daysInMonth[i];
    for (int i = 0; i < startMonth - 1; i++)
        buildId -= daysInMonth[i];
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD ul_reason_for_call, LPVOID lpvReserved)
{
    switch (ul_reason_for_call)
    {
    /*
    По сути это не рекомендуемый Microsoft, но повсеместно используемый способ повышения точности
    соблюдения и измерения временных интревалов функциями Sleep, QueryPerformanceCounter,
    timeGetTime и GetTickCount.
    Функция действует на всю операционную систему в целом (!) и нет необходимости вызывать её при
    старте нового потока. Вызов timeEndPeriod специалисты Microsoft считают обязательным.
    Есть подозрения, что Windows сама устанавливает максимальную точность при старте таких
    приложений как, например, игры. Тогда есть шанс, что вызов timeBeginPeriod здесь бессмысленен.
    Недостатком данного способа является то, что он приводит к общему замедлению работы как
    текущего приложения, так и всей операционной системы.
    Ещё можно посмотреть ссылки:
        https://msdn.microsoft.com/en-us/library/vs/alm/dd757624(v=vs.85).aspx
        https://users.livejournal.com/-winnie/151099.html
        https://github.com/tebjan/TimerTool
    */
    case DLL_PROCESS_ATTACH: timeBeginPeriod(1); break;
    case DLL_PROCESS_DETACH: timeEndPeriod  (1); break;
    }
    return TRUE;
}

