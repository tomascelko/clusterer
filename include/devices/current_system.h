#if defined(WIN32)
#include "windows.h"
#elif _APPLE_
#include "windows.h"
#elif __linux__ || __unix__
#include "unistd.h"
#endif

class current_system
{
    static long system_memory()
    {

    }

    static long free_memory()
    {
        
        #if defined(WIN32)
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        return  status.ullAvailPhys;

        #elif _APPLE_

        #elif __linux__ || __unix__
       
        long pages = sysconf(_SC_AVPHYS_PAGES);
        long page_size = sysconf(_SC_PAGE_SIZE);
        return pages * page_size;
        #endif
    
    };
};