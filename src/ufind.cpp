/*
The author of this software MAKES NO WARRANTY as to the RELIABILITY,
SUITABILITY, or USABILITY of this software. USE IT AT YOUR OWN RISK.
For more details please read LICENSE file included with this file.
*/

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(push, 0)
#include <Windows.h> //wall warnings
#include <io.h> // for _setmode
#include <fcntl.h> // for _O_WTEXT
#include <stdio.h>
#pragma warning(pop)

#define IMPLEMENTATION
#define ULIB_SET_CONTROL_C_HANDLER
#include "ulib_string_utils.h"
#include "ulib_win_listdir.h"
#include "ulib_win_api.h"
#ifdef _WIN64
#define TYPE "(x64)"
#else
#define TYPE "(x86)"
#endif

using namespace ulib;
static struct options_
{
    ulib__bool caseSensitive;
    ulib__bool searchAll;
    ulib__bool matchPath;
    ulib__bool matchFilesOnly;
    ulib__bool matchDirsOnly;
    ulib__bool statistics;
    ulib__bool recurse;
    ulib__bool logMode;
}options = { ULIB_FALSE };

static struct statistics_
{
    ulib__uint64 matchFileCount;
    ulib__uint64 matchDirCount;
}statistics = { 0 };

enum LogLevels
{
    Silent = 0,
    Normal,
    Verbose
};

static timer_struct printTimer = { 0 };

#define MAX_COMMAND_LINE_ARGS 100U

static const char* ApplicationVersion = "2.0.4";
static const char* BuildNumber = "03031163756";
static ulib__uint8      patternsCount = 0u;
static TCHAR*           patterns[MAX_COMMAND_LINE_ARGS] = { ULIB_NULL };
static ulib__uint8      pathsCount = 0u;
static ulib__uint8      pathsAt = 0u;
static TCHAR*           paths[MAX_COMMAND_LINE_ARGS] = { ULIB_NULL };
static TCHAR            originalPath[ULIB_MAX_WINDOWS_PATH];
static ulib__int32      logLevel = Normal;
static ListDirData      listDirData;

static void QuickHelp()
{
    _TLOG(_T("ufind %s v%hs build %hs by Croitor Cristian"), _T(TYPE), ApplicationVersion, BuildNumber);
    _TLOG(_T("NO WARRANTY IS EXPRESSED OR IMPLIED. USE AT YOUR OWN RISK."));
    _TLOG(_T("%sUsage:"), _TULIB_EOL);
    _TLOG(_T("   ufind [path][file pattern ...][/option ...][-option ...][--longoption ...]"));
    _TLOG(_T("Options:"));
    _TLOG(_T("         /r -r --recursive            Search recursively"));
    _TLOG(_T("         /f -f --files                Search for files only"));
    _TLOG(_T("         /d -d --dirs                 Search for directories only"));
    _TLOG(_T("         /c -c --match-case           Case sensitive search"));
    _TLOG(_T("         /a -a --additionalPath       Additional path"));
    _TLOG(_T("         /p -p --path                 Match the pattern against full path"));
    _TLOG(_T("         /i -i --info                 Display statistics info"));
    _TLOG(_T("         /s -s --silent               Silent mode"));
    _TLOG(_T("         /l -l --log                  Log mode"));
    _TLOG(_T("         /h -h --help                 Display help"));
    exit(EXIT_SUCCESS);
}

static void Man()
{
    _tprintf(_T("ufind %s v%hs by Croitor Cristian\n"), _T(TYPE), ApplicationVersion);
    _tprintf(_T("NO WARRANTY IS EXPRESSED OR IMPLIED. USE AT YOUR OWN RISK.\n"));
    _tprintf(_T("\nNAME\n"));
    _tprintf(_T("     ufind - Search for files or directories.\n"));

    _tprintf(_T("\nSYNOPSIS\n"));
    _tprintf(_T("     ufind [path][file pattern...][-option ...][/option ...]\
[--longoption ...]\n"));
    _tprintf(_T("\nDESCRIPTION\n"));
    _tprintf(_T("     ufind searches for files or folders recursively in a \
directory hierarchy.\n"));
    _tprintf(_T("     Multiple search patterns can be supplied in the command \
line.\n"));
    _tprintf(_T("     By default the search is case-insensitive.\n"));
    _tprintf(_T("     Note on wildcard rules:\n"));
    _tprintf(_T("     * will search for any character(s) following, *.* must \
contain a '.'\n"));
    _tprintf(_T("\nOPTIONS\n"));
    _tprintf(_T("     /r -r --recursive\n"));
    _tprintf(_T("         Search recursively in the given path.\n"));
    _tprintf(_T("     \n     /f -f --files\n"));
    _tprintf(_T("         Search for files only excluding any directories that\
 match the pattern.\n"));
    _tprintf(_T("     \n     /d -d --dirs\n"));
    _tprintf(_T("         Search for directories only excluding any files that \
match the pattern.\n"));
    _tprintf(_T("     \n     /c -c --match-case\n"));
    _tprintf(_T("         Case sensitive search.\n"));
    _tprintf(_T("     \n     /a -a --additionalPath <additional path>\n"));
    _tprintf(_T("         Additional search path.\n"));
    _tprintf(_T("     \n     /p -p --path\n"));
    _tprintf(_T("         Match the pattern against full path not only \
directory or file name.\n         Example: "));
    _tprintf(_T("d:\\>ufind c: *nvidia*nvdrs*.* -r -p\n"));
    _tprintf(_T("         Output:  c:\\Windows\\System32\\drivers\\NVIDIA \
Corporation\\Drs\\nvdrsdb.bin\n"));
    _tprintf(_T("     \n     /i -i --info\n"));
    _tprintf(_T("         At the end of the search, statistics info will be \
displayed.\n"));
    _tprintf(_T("     \n     /s -s --silent\n"));
    _tprintf(_T("         Silent mode, the only information that will be \
displayed is the search\n         status, and at the end the minimal search \
result.\n"));
    _tprintf(_T("     \n     /l -l --log\n"));
    _tprintf(_T("         Log mode - ONLY the files and directories found will\
 be displayed, no\n         additional info, use this when the output is \
redirected to a file.\n"));
    _tprintf(_T("     \n     /h -h --help\n"));
    _tprintf(_T("         Display help.\n"));
    _tprintf(_T("     \n     /?\n"));
    _tprintf(_T("         Quick help.\n"));
    exit(EXIT_SUCCESS);
}

static void Cleanup()
{
    for (int i = 0; i < pathsCount; ++i)
    {
        ULIB_FREE(paths[i]);
    }
}

static void GetPath(TCHAR* str)
{
    ulib__int32 maxPathCount = ULIB_MAX_WINDOWS_PATH * sizeof(TCHAR);
    TCHAR* path = (TCHAR*)malloc((ulib__SizeType)maxPathCount);
    if (!path)
    {
        Cleanup();
        _TLOG_FATAL(_T(" cannot allocate memory for path strings %s\n"),
                       _UlibGetSystemLastErrorString());
    }
    if (_tcscmp(str, _T(".")) == 0)
    {
        if (_tgetcwd(path, maxPathCount) == ULIB_NULL)
        {
            Cleanup();
            _TLOG_FATAL(_T(" getting path %s\n"),
                        _UlibGetSystemLastErrorString());
        }
    }
    else // Windows only - determine if relative path
    {
        if (str[1] != _T(":")[0])
        {
            if (!_tfullpath(path, str, ULIB_MAX_WINDOWS_PATH))
            {
                Cleanup();
                _TLOG_FATAL(_T(" getting path %s\n"),
                            _UlibGetSystemLastErrorString());
            }
        }
        else
        {
            _tcscpy(path, str);
        }
    }
    paths[pathsCount++] = path;
}

static void ProcessCmdLine(int argc, TCHAR** argv)
{
#define TWO 2u
#define FOUR 4u
    ulib::ulib__uint8 index = 1u; // First arg is the current exe
    if (argc == 1)
    {
        options.searchAll = ULIB_TRUE;
    }
    if (argc > 100)
    {
        _TLOG_FATAL(_T("ufind support max 100 arguments\n"));
    }
    //--argc; // Get rid of the [0] arg - the exe
    while (index < argc)
    {
        if (ORCompareMultipleStrings(TWO, argv[index], _T("/?")))
        {
            QuickHelp();
        }
        if (ORCompareMultipleStrings(FOUR, argv[index], _T("/i"), _T("-i"), _T("--info")))
        {
            options.statistics = ULIB_TRUE;
        }
        else if (ORCompareMultipleStrings(FOUR, argv[index], _T("/h"), _T("-h"), _T("--help")))
        {
            Man();
        }
        else if (ORCompareMultipleStrings(FOUR, argv[index], _T("/f"), _T("-f"), _T("--files")))
        {
            if (options.matchDirsOnly)
            {
                Cleanup();
                _TLOG_FATAL(_T("conflicting option - match dirs only\n"));
            }
            options.matchFilesOnly = ULIB_TRUE;
        }
        else if (ORCompareMultipleStrings(FOUR, argv[index], _T("/d"), _T("-d"), _T("--dirs")))
        {
            if (options.matchFilesOnly)
            {
                Cleanup();
                _TLOG_FATAL(_T("conflicting option - match files only\n"));
            }
            options.matchDirsOnly = ULIB_TRUE;
        }
        else if (ORCompareMultipleStrings(FOUR, argv[index], _T("/p"), _T("-p"), _T("--path")))
        {
            options.matchPath = ULIB_TRUE;
        }
        else if (ORCompareMultipleStrings(FOUR, argv[index], _T("/c"), _T("-c"), _T("--match-case")))
        {
            options.caseSensitive = ULIB_TRUE;
        }
        else if (ORCompareMultipleStrings(FOUR, argv[index], _T("/l"), _T("-l"), _T("--log")))
        {
            options.logMode = ULIB_TRUE;
        }
        else if (ORCompareMultipleStrings(FOUR, argv[index], _T("/s"), _T("-s"), _T("--silent")))
        {
            logLevel = Silent;
        }
        else if (ORCompareMultipleStrings(FOUR, argv[index], _T("/r"), _T("-r"), _T("--recursive")))
        {
            options.recurse = ULIB_TRUE;
        }
        else if (ORCompareMultipleStrings(FOUR, argv[index], _T("/a"), _T("-a"), _T("--additionalPath")))
        {
            if (index < (argc - 1))// At least one more argument present
            {
                index++;
                GetPath(argv[index]);
            }
            else
            {
                Cleanup();
                _TLOG_FATAL(_T("missing additional path.\n"));
            }
        }
        else if (argv[index][0] == (TCHAR)_T("/")[0] ||
                 argv[index][0] == (TCHAR)_T("-")[0])
        {
            Cleanup();
            _TLOG_FATAL(_T("invalid option %s\n"), argv[index]);
        }
        else
        {
            if (pathsCount == 0)
            {
                GetPath(argv[index]);
            }
            else
            {
                patterns[patternsCount++] = argv[index];
            }
        }
        ++index;
    }
    if (patternsCount == 0)
    {
        options.searchAll = ULIB_TRUE;
    }
    if (!options.caseSensitive)
    {
        for (int i = 0; i < patternsCount; ++i)
        {
            ToLowerString(patterns[i]);
        }
    }
    if (pathsCount == 0)
    {
        paths[pathsCount] =  _tgetcwd(ULIB_NULL, 0);
        if (!paths[pathsCount++]) {
            _TLOG_FATAL(_T("Error getting path"));
        }
    }
#undef TWO
#undef FOUR
}

__forceinline void Print(TCHAR* name, ulib__uint64* matchCount)
{
    if (logLevel > Silent)
    {
        if (!options.logMode)
        {
            _tprintf(_T("                                             \r"));
            if (matchCount == &statistics.matchDirCount) { // DIR
                _tprintf(_T("[D] "));
            }
        }
        _putts(name);
    }
}

static void Match(TCHAR* pth, TCHAR* fileName, ulib__uint64* matchCount)
{
    TCHAR* printPath = ULIB_NULL;
    TCHAR* searchString = ULIB_NULL;
    if (!options.logMode)
    {
        if (((listDirData.totalFiles & 0xeFF) == 0))
        {
            _tprintf(_T("                          \r"));
            _tprintf(_T("Searching: %llu files\r"), listDirData.totalFiles);
        }
    }
    if (options.searchAll)
    {
        ++(*matchCount);
        Print(pth, matchCount);
        return;
    }
    if (options.matchPath)
    {
        _tcscpy(originalPath, pth);
        printPath = originalPath;
        searchString = pth;
    }
    else
    {
        printPath = pth;
        searchString = fileName;
    }
    if (!options.caseSensitive)
    {
        ToLowerString(searchString);
    }
    for (int x = 0; x < patternsCount; ++x)
    {
        if (WildcardMatch(patterns[x], searchString) == ULIB_TRUE)
        {
            Print(printPath, matchCount);
            ++(*matchCount);
            return;
        }
    }
}

static void FileCallBack(TCHAR* pth, TCHAR* filename)
{
    Match(pth, filename , &statistics.matchFileCount);
}

static void DirCallBack(TCHAR* pth, TCHAR* filename)
{
    Match(pth, filename, &statistics.matchDirCount);
}

int _tmain(int argumentCount, TCHAR** arguments)
{
    if (_setmode(_fileno(stdout), _O_WTEXT) == -1)
    {
        _TLOG_FATAL(_T("setting console to wchar mode"));
    }
    SetControlCHandler();
    ProcessCmdLine(argumentCount, arguments);
    INIT_LISTDIRDATA(listDirData)
    listDirData.processFile = FileCallBack;
    listDirData.processDirectory = DirCallBack;
    if (options.matchFilesOnly)
    {
        listDirData.processDirectory = ULIB_NULL;
    }
    else if (options.matchDirsOnly)
    {
        listDirData.processFile = ULIB_NULL;
    }
    listDirData.shouldExit = &ulib_CTRL_C_Handler_shouldExit;
    listDirData.recurse = options.recurse;
    double elapsed;
    BEGIN_TIMED_BLOCK(program)
    for (; pathsAt < pathsCount;)
    {
        listDirData.dir = paths[pathsAt++];
        if (ListDir(&listDirData) == ULIB_FILE_NOT_FOUND)
        {
            _TLOG_FATAL(_T("No such file or directory"));
        }
    }
    END_TIMED_BLOCK(program, elapsed)
    if ((logLevel >= Silent || options.statistics) && (!options.logMode))
    {
        _tprintf(_T("                                                         \
        \r"));
        _tprintf(_T("Searched %llu file%s and %llu director%s and found %llu \
item%s\n"),
            listDirData.totalFiles,
            listDirData.totalFiles == 1 ? _T("") : _T("s"),
            listDirData.totalDirs,
            listDirData.totalDirs == 1 ? _T("y") : _T("ies"),
            statistics.matchFileCount + statistics.matchDirCount,
            statistics.matchFileCount + statistics.matchDirCount == 1 ?
                                                             _T("") : _T("s"));
    }
    if (options.statistics)
    {
        _tprintf(_T("Found    : %llu file%s\n"),
                   statistics.matchFileCount,
                   statistics.matchFileCount == 1 ? _T("") : _T("s"));
        _tprintf(_T("Found    : %llu director%s\n"),
                   statistics.matchDirCount,
                   statistics.matchDirCount == 1 ? _T("y") : _T("ies"));
        _tprintf(_T("Time     : %.6f s\n"), elapsed);
    }
    Cleanup();
    ulib_CTRL_C_Handler_okToExit = ULIB_TRUE;
    fflush(stdout);
    return (EXIT_SUCCESS);
}
