#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "oric_core.h"
#include "resource.h"

#define APP_CLASS_NAME     L"OricExplorerMainWindow"
#define VISUAL_CLASS_NAME  L"OricExplorerVisualCanvas"
#define EMU_DLG_CLASS_NAME    L"OricExplorerEmuConfigDlg"
#define RENAME_DLG_CLASS_NAME L"OricExplorerRenameDlg"

#define INI_FILENAME       L"OricExplorer.ini"
#define INI_SEC_SETTINGS   L"Settings"
#define INI_SEC_EMULATOR   L"Emulator"
#define INI_SEC_RECENT     L"Recent"
#define WM_APP_EXP_ACTIVATE (WM_APP + 1)

typedef struct {
    wchar_t name[MAX_PATH];
    wchar_t fullPath[MAX_PATH];
    bool isDir;
    bool isParent;
    uint64_t size;
    FILETIME ftWrite;
} ExpItem;

typedef struct {
    HWND hwndMain;

    /* Explorer mode controls */
    HWND hwndExpDriveLbl;
    HWND hwndExpDrive;
    HWND hwndExpPathLbl;
    HWND hwndExpPath;
    HWND hwndExpUp;
    HWND hwndExpList;

    /* Analyzer mode controls */
    HWND hwndBackBtn;
    HWND hwndList;

    /* Shared left filter & right inspector sidebar */
    HWND hwndFilterLabel;
    HWND hwndFilterEdit;
    HWND hwndSideTitle;
    HWND hwndTabs;
    HWND hwndSideEdit;
    HWND hwndCodeEdit;
    HWND hwndVisualCanvas;
    HWND hwndCodeToggleBtn;
    HWND hwndCopyBtn;
    HWND hwndSavePngBtn;
    HWND hwndStatus;

    HFONT hFontUi;
    HFONT hFontUiBold;
    HFONT hFontMono;

    bool inExplorerMode;
    wchar_t startDir[MAX_PATH];
    wchar_t iniPath[MAX_PATH];
    wchar_t browseDir[MAX_PATH];

    ExpItem *expItems;
    size_t expCount;
    size_t expCap;
    int expSelected;
    wchar_t expLastPath[MAX_PATH];
    int expLastTopIndex;
    int expFallbackIndex;
    wchar_t expSavedFilter[128];
    bool populatingExpList;
    bool suppressFilterChange;

    /* Explorer selection live preview cache */
    wchar_t expPreviewPath[MAX_PATH];
    uint8_t *expPreviewBuffer;
    size_t expPreviewSize;
    OricMediaReport expPreviewReport;
    int expPreviewEntryIdx;
    bool hasExpPreview;

    OricLang lang;
    OricMediaReport report;
    int selectedIndex;
    int activeTab;       /* 0=Summary, 1=Code/Hex, 2=Screen/Map */
    bool forceHexMode;

    int sortCol;
    bool sortAsc;

    uint8_t *rawBuffer;
    size_t rawSize;
    wchar_t loadedPath[MAX_PATH];

    wchar_t recentFiles[IDM_FILE_RECENT_MAX][MAX_PATH];
    int recentCount;

    wchar_t tapEmuPath[MAX_PATH];
    wchar_t tapEmuArgs[MAX_PATH];
    wchar_t dskEmuPath[MAX_PATH];
    wchar_t dskEmuArgs[MAX_PATH];

    /* Cached visual preview state */
    uint32_t *screenPixels;
    int screenW;
    int screenH;
    wchar_t visualTitle[160];
    uint8_t diskMap[ORIC_MAX_MAP_SIDES][ORIC_MAX_MAP_TRACKS][ORIC_MAX_MAP_SECTORS];
    bool hasDiskMap;

    /* Resizable left/right panel splitter */
    double splitRatio;   /* Right panel fraction of available width */
    RECT splitRect;
    bool isDraggingSplit;
} AppState;

static AppState g_app;

typedef struct {
    HWND hwndDlg;
    HWND hwndParent;
    HWND hwndTapEdit;
    HWND hwndTapArgs;
    HWND hwndDskEdit;
    HWND hwndDskArgs;
    bool done;
    bool saved;
} EmuDlgState;

/* Forward declarations */
static void PopulateExplorerList(void);
static void UpdateSidebarAll(void);
static void UpdateStatusBar(void);
static void LayoutChildren(HWND hwnd);
static void SyncViewModeVisibility(void);
static void LoadOricFile(const wchar_t *filePath);
static void ReturnToExplorer(void);

static int ScaleDpi(HWND hwnd, int px) {
    HDC hdc = GetDC(hwnd);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) ReleaseDC(hwnd, hdc);
    return MulDiv(px, dpi, 96);
}

static bool wcs_contains_icase(const wchar_t *haystack, const wchar_t *needle) {
    if (!needle || needle[0] == L'\0') return true;
    if (!haystack) return false;
    size_t hlen = wcslen(haystack);
    size_t nlen = wcslen(needle);
    if (nlen > hlen) return false;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        size_t j = 0;
        while (j < nlen && towlower(haystack[i + j]) == towlower(needle[j])) {
            j++;
        }
        if (j == nlen) return true;
    }
    return false;
}

static bool has_oric_extension(const wchar_t *name) {
    if (!name) return false;
    const wchar_t *dot = wcsrchr(name, L'.');
    if (!dot) return false;
    return (_wcsicmp(dot, L".tap") == 0 ||
            _wcsicmp(dot, L".dsk") == 0 ||
            _wcsicmp(dot, L".rom") == 0 ||
            _wcsicmp(dot, L".bin") == 0);
}

static bool is_valid_directory(const wchar_t *path) {
    if (!path || path[0] == L'\0') return false;
    DWORD attr = GetFileAttributesW(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

/* -------------------- INI Persistence (Current Directory) -------------------- */

static void EnsureIniHasUtf16Bom(void) {
    if (g_app.iniPath[0] == L'\0') return;
    if (GetFileAttributesW(g_app.iniPath) != INVALID_FILE_ATTRIBUTES) return;

    HANDLE hFile = CreateFileW(g_app.iniPath, GENERIC_WRITE, 0, NULL,
                               CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        static const uint8_t bom[2] = { 0xFFu, 0xFEu };
        DWORD written = 0;
        WriteFile(hFile, bom, 2, &written, NULL);
        CloseHandle(hFile);
    }
}

static void SaveIniString(const wchar_t *section, const wchar_t *key, const wchar_t *value) {
    if (g_app.iniPath[0] == L'\0') return;
    EnsureIniHasUtf16Bom();
    WritePrivateProfileStringW(section, key, value ? value : L"", g_app.iniPath);
}

static void InitWorkingPaths(void) {
    DWORD len = GetCurrentDirectoryW(MAX_PATH, g_app.startDir);
    if (len == 0 || len >= MAX_PATH) {
        wcscpy(g_app.startDir, L".");
    }
    size_t sl = wcslen(g_app.startDir);
    if (sl > 3 && g_app.startDir[sl - 1] == L'\\') {
        g_app.startDir[sl - 1] = L'\0';
    }

    if (wcslen(g_app.startDir) + wcslen(INI_FILENAME) + 2 < MAX_PATH) {
        if (g_app.startDir[wcslen(g_app.startDir) - 1] == L'\\') {
            swprintf(g_app.iniPath, MAX_PATH, L"%ls%ls", g_app.startDir, INI_FILENAME);
        } else {
            swprintf(g_app.iniPath, MAX_PATH, L"%ls\\%ls", g_app.startDir, INI_FILENAME);
        }
    } else {
        wcscpy(g_app.iniPath, INI_FILENAME);
    }
}

static void LoadSettingsFromIni(void) {
    g_app.lang = ORIC_LANG_EN;
    g_app.recentCount = 0;
    g_app.expSelected = -1;
    g_app.expFallbackIndex = -1;
    g_app.expLastTopIndex = 0;
    g_app.expLastPath[0] = L'\0';
    g_app.expSavedFilter[0] = L'\0';
    g_app.tapEmuPath[0] = L'\0';
    wcscpy(g_app.tapEmuArgs, L"\"%s\"");
    g_app.dskEmuPath[0] = L'\0';
    wcscpy(g_app.dskEmuArgs, L"\"%s\"");
    g_app.splitRatio = 0.40;
    wcsncpy(g_app.browseDir, g_app.startDir, MAX_PATH - 1);
    g_app.browseDir[MAX_PATH - 1] = L'\0';

    wchar_t buf[MAX_PATH];
    DWORD n = GetPrivateProfileStringW(INI_SEC_SETTINGS, L"Language", L"",
                                       buf, MAX_PATH, g_app.iniPath);
    if (n > 0 && buf[0] != L'\0') {
        g_app.lang = oric_lang_from_code(buf);
    } else {
        LANGID uiLang = GetUserDefaultUILanguage();
        switch (PRIMARYLANGID(uiLang)) {
        case LANG_FRENCH:  g_app.lang = ORIC_LANG_FR; break;
        case LANG_GERMAN:  g_app.lang = ORIC_LANG_DE; break;
        case LANG_SPANISH: g_app.lang = ORIC_LANG_ES; break;
        default:           g_app.lang = ORIC_LANG_EN; break;
        }
    }

    int splitPermille = (int)GetPrivateProfileIntW(INI_SEC_SETTINGS, L"SplitRatio", 400, g_app.iniPath);
    if (splitPermille >= 150 && splitPermille <= 800) {
        g_app.splitRatio = (double)splitPermille / 1000.0;
    }

    n = GetPrivateProfileStringW(INI_SEC_SETTINGS, L"LastBrowseDir", L"",
                                 buf, MAX_PATH, g_app.iniPath);
    if (n > 0 && is_valid_directory(buf)) {
        wcsncpy(g_app.browseDir, buf, MAX_PATH - 1);
        g_app.browseDir[MAX_PATH - 1] = L'\0';
    }

    GetPrivateProfileStringW(INI_SEC_EMULATOR, L"TapPath", L"",
                             g_app.tapEmuPath, MAX_PATH, g_app.iniPath);
    n = GetPrivateProfileStringW(INI_SEC_EMULATOR, L"TapArgs", L"\"%s\"",
                                 g_app.tapEmuArgs, MAX_PATH, g_app.iniPath);
    if (n == 0) wcscpy(g_app.tapEmuArgs, L"\"%s\"");

    GetPrivateProfileStringW(INI_SEC_EMULATOR, L"DskPath", L"",
                             g_app.dskEmuPath, MAX_PATH, g_app.iniPath);
    n = GetPrivateProfileStringW(INI_SEC_EMULATOR, L"DskArgs", L"\"%s\"",
                                 g_app.dskEmuArgs, MAX_PATH, g_app.iniPath);
    if (n == 0) wcscpy(g_app.dskEmuArgs, L"\"%s\"");

    for (int i = 0; i < IDM_FILE_RECENT_MAX; i++) {
        wchar_t key[16];
        swprintf(key, 16, L"File%d", i);
        n = GetPrivateProfileStringW(INI_SEC_RECENT, key, L"", buf, MAX_PATH, g_app.iniPath);
        if (n > 0 && buf[0] != L'\0') {
            wcsncpy(g_app.recentFiles[g_app.recentCount], buf, MAX_PATH - 1);
            g_app.recentFiles[g_app.recentCount][MAX_PATH - 1] = L'\0';
            g_app.recentCount++;
        }
    }
}

static void SaveEmulatorSettingsToIni(void) {
    SaveIniString(INI_SEC_EMULATOR, L"TapPath", g_app.tapEmuPath);
    SaveIniString(INI_SEC_EMULATOR, L"TapArgs", g_app.tapEmuArgs);
    SaveIniString(INI_SEC_EMULATOR, L"DskPath", g_app.dskEmuPath);
    SaveIniString(INI_SEC_EMULATOR, L"DskArgs", g_app.dskEmuArgs);
}

static void SaveRecentFilesToIni(void) {
    for (int i = 0; i < IDM_FILE_RECENT_MAX; i++) {
        wchar_t key[16];
        swprintf(key, 16, L"File%d", i);
        SaveIniString(INI_SEC_RECENT, key,
                      (i < g_app.recentCount) ? g_app.recentFiles[i] : L"");
    }
}

static void AddRecentFile(const wchar_t *path) {
    if (!path || path[0] == L'\0') return;
    int existing = -1;
    for (int i = 0; i < g_app.recentCount; i++) {
        if (_wcsicmp(g_app.recentFiles[i], path) == 0) {
            existing = i;
            break;
        }
    }
    if (existing >= 0) {
        for (int i = existing; i > 0; i--) {
            wcscpy(g_app.recentFiles[i], g_app.recentFiles[i - 1]);
        }
    } else {
        int top = (g_app.recentCount < IDM_FILE_RECENT_MAX) ? g_app.recentCount : (IDM_FILE_RECENT_MAX - 1);
        for (int i = top; i > 0; i--) {
            wcscpy(g_app.recentFiles[i], g_app.recentFiles[i - 1]);
        }
        if (g_app.recentCount < IDM_FILE_RECENT_MAX) {
            g_app.recentCount++;
        }
    }
    wcsncpy(g_app.recentFiles[0], path, MAX_PATH - 1);
    g_app.recentFiles[0][MAX_PATH - 1] = L'\0';
    SaveRecentFilesToIni();
}

static bool UpdateRecentFilePath(const wchar_t *oldPath, const wchar_t *newPath) {
    if (!oldPath || !newPath) return false;
    bool changed = false;
    for (int i = 0; i < g_app.recentCount; i++) {
        if (_wcsicmp(g_app.recentFiles[i], oldPath) == 0) {
            wcsncpy(g_app.recentFiles[i], newPath, MAX_PATH - 1);
            g_app.recentFiles[i][MAX_PATH - 1] = L'\0';
            changed = true;
        }
    }
    if (changed) SaveRecentFilesToIni();
    return changed;
}

static bool RemoveRecentFilePath(const wchar_t *targetPath) {
    if (!targetPath) return false;
    bool changed = false;
    int writeIdx = 0;
    for (int i = 0; i < g_app.recentCount; i++) {
        if (_wcsicmp(g_app.recentFiles[i], targetPath) == 0) {
            changed = true;
            continue;
        }
        if (writeIdx != i) {
            wcscpy(g_app.recentFiles[writeIdx], g_app.recentFiles[i]);
        }
        writeIdx++;
    }
    if (changed) {
        g_app.recentCount = writeIdx;
        SaveRecentFilesToIni();
    }
    return changed;
}

/* -------------------- Fonts & Menus -------------------- */

static void CreateAppFonts(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    int dpiY = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    if (hdc) ReleaseDC(hwnd, hdc);

    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -MulDiv(9, dpiY, 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(lf.lfFaceName, L"Segoe UI");
    g_app.hFontUi = CreateFontIndirectW(&lf);

    lf.lfWeight = FW_SEMIBOLD;
    g_app.hFontUiBold = CreateFontIndirectW(&lf);

    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -MulDiv(9, dpiY, 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(lf.lfFaceName, L"Consolas");
    g_app.hFontMono = CreateFontIndirectW(&lf);
}

static void RebuildMenuBar(HWND hwnd) {
    HMENU hOld = GetMenu(hwnd);
    HMENU hBar = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN, oric_tr(g_app.lang, STR_MENU_FILE_OPEN));

    HMENU hRecent = CreatePopupMenu();
    if (g_app.recentCount == 0) {
        AppendMenuW(hRecent, MF_STRING | MF_GRAYED, 0,
                    oric_tr(g_app.lang, STR_MENU_FILE_RECENT_NONE));
    } else {
        for (int i = 0; i < g_app.recentCount; i++) {
            wchar_t itemTxt[MAX_PATH + 8];
            swprintf(itemTxt, MAX_PATH + 8, L"&%d  %ls", i + 1, g_app.recentFiles[i]);
            AppendMenuW(hRecent, MF_STRING, (UINT_PTR)(IDM_FILE_RECENT_BASE + i), itemTxt);
        }
    }
    AppendMenuW(hFile, MF_POPUP, (UINT_PTR)hRecent, oric_tr(g_app.lang, STR_MENU_FILE_RECENT));
    AppendMenuW(hFile, MF_STRING, IDM_FILE_CLOSE, oric_tr(g_app.lang, STR_MENU_FILE_CLOSE));
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, oric_tr(g_app.lang, STR_MENU_FILE_EXIT));

    HMENU hEdit = CreatePopupMenu();
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_COPY, oric_tr(g_app.lang, STR_MENU_EDIT_COPY));

    HMENU hTools = CreatePopupMenu();
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_EXTRACT_BIN, oric_tr(g_app.lang, STR_MENU_TOOLS_EXTRACT_BIN));
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_EXPORT_TAP,  oric_tr(g_app.lang, STR_MENU_TOOLS_EXPORT_TAP));
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_EXPORT_WAV,  oric_tr(g_app.lang, STR_MENU_TOOLS_EXPORT_WAV));
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_EXPORT_PNG,  oric_tr(g_app.lang, STR_MENU_TOOLS_EXPORT_PNG));
    AppendMenuW(hTools, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_LAUNCH_EMU,  oric_tr(g_app.lang, STR_MENU_TOOLS_LAUNCH_EMU));
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_CONFIG_EMU,  oric_tr(g_app.lang, STR_MENU_TOOLS_CONFIG_EMU));

    HMENU hLang = CreatePopupMenu();
    AppendMenuW(hLang, MF_STRING, IDM_LANG_EN, oric_tr(g_app.lang, STR_MENU_LANG_EN));
    AppendMenuW(hLang, MF_STRING, IDM_LANG_FR, oric_tr(g_app.lang, STR_MENU_LANG_FR));
    AppendMenuW(hLang, MF_STRING, IDM_LANG_DE, oric_tr(g_app.lang, STR_MENU_LANG_DE));
    AppendMenuW(hLang, MF_STRING, IDM_LANG_ES, oric_tr(g_app.lang, STR_MENU_LANG_ES));
    CheckMenuRadioItem(hLang, IDM_LANG_EN, IDM_LANG_ES,
                       IDM_LANG_EN + (UINT)g_app.lang, MF_BYCOMMAND);

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, oric_tr(g_app.lang, STR_MENU_HELP_ABOUT));

    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hFile,  oric_tr(g_app.lang, STR_MENU_FILE));
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hEdit,  oric_tr(g_app.lang, STR_MENU_EDIT));
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hTools, oric_tr(g_app.lang, STR_MENU_TOOLS));
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hLang,  oric_tr(g_app.lang, STR_MENU_LANG));
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp,  oric_tr(g_app.lang, STR_MENU_HELP));

    SetMenu(hwnd, hBar);
    DrawMenuBar(hwnd);
    if (hOld) {
        DestroyMenu(hOld);
    }
}

/* -------------------- File Explorer -------------------- */

static void FreeExplorerItems(void) {
    free(g_app.expItems);
    g_app.expItems = NULL;
    g_app.expCount = 0;
    g_app.expCap = 0;
    g_app.expSelected = -1;
}

static bool AddExplorerItem(const ExpItem *item) {
    if (g_app.expCount >= g_app.expCap) {
        size_t newCap = (g_app.expCap == 0) ? 64u : g_app.expCap * 2u;
        ExpItem *nb = (ExpItem *)realloc(g_app.expItems, newCap * sizeof(ExpItem));
        if (!nb) return false;
        g_app.expItems = nb;
        g_app.expCap = newCap;
    }
    g_app.expItems[g_app.expCount++] = *item;
    return true;
}

static int CmpExpItems(const void *pa, const void *pb) {
    const ExpItem *a = (const ExpItem *)pa;
    const ExpItem *b = (const ExpItem *)pb;
    if (a->isParent != b->isParent) return a->isParent ? -1 : 1;
    if (a->isDir != b->isDir) return a->isDir ? -1 : 1;
    return _wcsicmp(a->name, b->name);
}

static void PopulateDriveCombo(void) {
    if (!g_app.hwndExpDrive) return;
    SendMessageW(g_app.hwndExpDrive, CB_RESETCONTENT, 0, 0);

    wchar_t drives[512];
    DWORD len = GetLogicalDriveStringsW(511, drives);
    if (len == 0 || len >= 511) return;

    int selIdx = -1;
    int idx = 0;
    const wchar_t *p = drives;
    while (*p != L'\0') {
        SendMessageW(g_app.hwndExpDrive, CB_ADDSTRING, 0, (LPARAM)p);
        if (towupper(g_app.browseDir[0]) == towupper(p[0]) && g_app.browseDir[1] == L':') {
            selIdx = idx;
        }
        p += wcslen(p) + 1;
        idx++;
    }
    if (selIdx >= 0) {
        SendMessageW(g_app.hwndExpDrive, CB_SETCURSEL, (WPARAM)selIdx, 0);
    }
}

static void SetupExplorerColumns(HWND hwndExpList, HWND hwndMain) {
    struct {
        OricStrId titleId;
        int width;
        int fmt;
    } cols[] = {
        { STR_EXP_COL_NAME, 250, LVCFMT_LEFT  },
        { STR_EXP_COL_TYPE, 150, LVCFMT_LEFT  },
        { STR_EXP_COL_SIZE, 100, LVCFMT_RIGHT },
        { STR_EXP_COL_DATE, 140, LVCFMT_LEFT  }
    };
    for (int i = 0; i < 4; i++) {
        LVCOLUMNW lvc;
        memset(&lvc, 0, sizeof(lvc));
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
        lvc.fmt = cols[i].fmt;
        lvc.cx = ScaleDpi(hwndMain, cols[i].width);
        lvc.pszText = (LPWSTR)oric_tr(g_app.lang, cols[i].titleId);
        lvc.iSubItem = i;
        ListView_InsertColumn(hwndExpList, i, &lvc);
    }
}

static void UpdateExplorerColumnHeaders(void) {
    static const OricStrId colIds[4] = {
        STR_EXP_COL_NAME, STR_EXP_COL_TYPE, STR_EXP_COL_SIZE, STR_EXP_COL_DATE
    };
    for (int i = 0; i < 4; i++) {
        LVCOLUMNW lvc;
        memset(&lvc, 0, sizeof(lvc));
        lvc.mask = LVCF_TEXT;
        lvc.pszText = (LPWSTR)oric_tr(g_app.lang, colIds[i]);
        ListView_SetColumn(g_app.hwndExpList, i, &lvc);
    }
}

static const wchar_t *GetExplorerTypeLabel(const ExpItem *it) {
    if (it->isDir) return oric_tr(g_app.lang, STR_EXP_TYPE_FOLDER);
    const wchar_t *dot = wcsrchr(it->name, L'.');
    if (dot) {
        if (_wcsicmp(dot, L".tap") == 0) return oric_tr(g_app.lang, STR_EXP_TYPE_TAP);
        if (_wcsicmp(dot, L".dsk") == 0) return oric_tr(g_app.lang, STR_EXP_TYPE_DSK);
    }
    return oric_tr(g_app.lang, STR_EXP_TYPE_ROM);
}

static void FormatFileTimeStr(const FILETIME *ft, wchar_t *out, size_t outChars) {
    FILETIME localFt;
    SYSTEMTIME st;
    if (ft->dwLowDateTime == 0 && ft->dwHighDateTime == 0) {
        wcscpy(out, L"");
        return;
    }
    if (FileTimeToLocalFileTime(ft, &localFt) && FileTimeToSystemTime(&localFt, &st)) {
        swprintf(out, outChars, L"%04u-%02u-%02u %02u:%02u",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    } else {
        wcscpy(out, L"");
    }
}

static void NavigateExplorerToEx(const wchar_t *dirPath, const wchar_t *selectChildPath) {
    if (!dirPath || dirPath[0] == L'\0') return;
    wchar_t clean[MAX_PATH];
    wcsncpy(clean, dirPath, MAX_PATH - 1);
    clean[MAX_PATH - 1] = L'\0';

    for (size_t i = 0; clean[i] != L'\0'; i++) {
        if (clean[i] == L'/') clean[i] = L'\\';
    }

    size_t len = wcslen(clean);
    while (len > 3 && clean[len - 1] == L'\\') {
        clean[--len] = L'\0';
    }
    if (len == 2 && clean[1] == L':') {
        clean[2] = L'\\';
        clean[3] = L'\0';
    }

    if (!is_valid_directory(clean)) return;

    bool dirChanged = (_wcsicmp(clean, g_app.browseDir) != 0);
    wcsncpy(g_app.browseDir, clean, MAX_PATH - 1);
    g_app.browseDir[MAX_PATH - 1] = L'\0';
    SaveIniString(INI_SEC_SETTINGS, L"LastBrowseDir", g_app.browseDir);

    if (selectChildPath && selectChildPath[0] != L'\0') {
        wcsncpy(g_app.expLastPath, selectChildPath, MAX_PATH - 1);
        g_app.expLastPath[MAX_PATH - 1] = L'\0';
    } else if (dirChanged) {
        g_app.expLastPath[0] = L'\0';
    }
    g_app.expLastTopIndex = 0;
    g_app.expFallbackIndex = -1;

    if (dirChanged) {
        g_app.expSavedFilter[0] = L'\0';
        if (g_app.inExplorerMode && g_app.hwndFilterEdit) {
            g_app.suppressFilterChange = true;
            SetWindowTextW(g_app.hwndFilterEdit, L"");
            g_app.suppressFilterChange = false;
        }
    }

    if (g_app.hwndExpPath) {
        SetWindowTextW(g_app.hwndExpPath, g_app.browseDir);
    }
    PopulateDriveCombo();
    PopulateExplorerList();
    if (g_app.inExplorerMode) {
        UpdateSidebarAll();
        UpdateStatusBar();
    }
}

static void NavigateExplorerTo(const wchar_t *dirPath) {
    NavigateExplorerToEx(dirPath, NULL);
}

static void GoToParentDirectory(void) {
    size_t len = wcslen(g_app.browseDir);
    if (len <= 3) return; /* Drive root like C:\ */

    wchar_t childDir[MAX_PATH];
    wcsncpy(childDir, g_app.browseDir, MAX_PATH - 1);
    childDir[MAX_PATH - 1] = L'\0';

    wchar_t parent[MAX_PATH];
    wcsncpy(parent, g_app.browseDir, MAX_PATH - 1);
    parent[MAX_PATH - 1] = L'\0';

    wchar_t *slash = wcsrchr(parent, L'\\');
    if (slash) {
        if (slash == parent + 2 && parent[1] == L':') {
            slash[1] = L'\0';
        } else {
            *slash = L'\0';
        }
        NavigateExplorerToEx(parent, childDir);
        if (g_app.inExplorerMode && g_app.hwndExpList && GetFocus() == g_app.hwndExpUp) {
            SetFocus(g_app.hwndExpList);
        }
    }
}

static void PopulateExplorerList(void) {
    if (!g_app.hwndExpList) return;
    g_app.populatingExpList = true;
    FreeExplorerItems();

    wchar_t filter[128] = {0};
    if (g_app.inExplorerMode && g_app.hwndFilterEdit) {
        GetWindowTextW(g_app.hwndFilterEdit, filter, 128);
    }

    /* Parent directory [..] if not root */
    size_t bLen = wcslen(g_app.browseDir);
    if (bLen > 3 && !(bLen == 3 && g_app.browseDir[1] == L':')) {
        ExpItem up;
        memset(&up, 0, sizeof(up));
        wcscpy(up.name, L"..");
        up.isDir = true;
        up.isParent = true;
        AddExplorerItem(&up);
    }

    wchar_t pattern[MAX_PATH];
    if (bLen > 0 && g_app.browseDir[bLen - 1] == L'\\') {
        swprintf(pattern, MAX_PATH, L"%ls*", g_app.browseDir);
    } else {
        swprintf(pattern, MAX_PATH, L"%ls\\*", g_app.browseDir);
    }

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
                continue;
            }
            bool isDir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (!isDir && !has_oric_extension(ffd.cFileName)) {
                continue;
            }
            if (filter[0] != L'\0' && !wcs_contains_icase(ffd.cFileName, filter)) {
                continue;
            }

            ExpItem it;
            memset(&it, 0, sizeof(it));
            wcsncpy(it.name, ffd.cFileName, MAX_PATH - 1);
            it.isDir = isDir;
            it.isParent = false;
            it.size = ((uint64_t)ffd.nFileSizeHigh << 32) | ffd.nFileSizeLow;
            it.ftWrite = ffd.ftLastWriteTime;

            if (bLen > 0 && g_app.browseDir[bLen - 1] == L'\\') {
                swprintf(it.fullPath, MAX_PATH, L"%ls%ls", g_app.browseDir, ffd.cFileName);
            } else {
                swprintf(it.fullPath, MAX_PATH, L"%ls\\%ls", g_app.browseDir, ffd.cFileName);
            }
            AddExplorerItem(&it);
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }

    if (g_app.expCount > 1) {
        qsort(g_app.expItems, g_app.expCount, sizeof(ExpItem), CmpExpItems);
    }

    SendMessageW(g_app.hwndExpList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_app.hwndExpList);

    int matchRow = -1;
    int matchIdx = -1;
    for (size_t i = 0; i < g_app.expCount; i++) {
        const ExpItem *it = &g_app.expItems[i];
        wchar_t dispName[MAX_PATH + 4];
        if (it->isDir) {
            swprintf(dispName, MAX_PATH + 4, L"[%ls]", it->name);
        } else {
            wcsncpy(dispName, it->name, MAX_PATH + 3);
            dispName[MAX_PATH + 3] = L'\0';
        }

        LVITEMW lvi;
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = dispName;
        lvi.lParam = (LPARAM)i;
        int row = ListView_InsertItem(g_app.hwndExpList, &lvi);
        if (row < 0) continue;

        ListView_SetItemText(g_app.hwndExpList, row, 1, (LPWSTR)GetExplorerTypeLabel(it));

        wchar_t sizeBuf[48];
        if (it->isDir) {
            wcscpy(sizeBuf, L"<DIR>");
        } else {
            swprintf(sizeBuf, 48, oric_tr(g_app.lang, STR_SIZE_BYTES_FMT), (unsigned)it->size);
        }
        ListView_SetItemText(g_app.hwndExpList, row, 2, sizeBuf);

        wchar_t dateBuf[48];
        FormatFileTimeStr(&it->ftWrite, dateBuf, 48);
        ListView_SetItemText(g_app.hwndExpList, row, 3, dateBuf);

        if (matchRow < 0 && g_app.expLastPath[0] != L'\0' && !it->isParent &&
            _wcsicmp(it->fullPath, g_app.expLastPath) == 0) {
            matchRow = row;
            matchIdx = (int)i;
        }
    }

    int itemCount = ListView_GetItemCount(g_app.hwndExpList);
    if (matchRow < 0 && g_app.expFallbackIndex >= 0 && itemCount > 0) {
        matchRow = (g_app.expFallbackIndex < itemCount) ? g_app.expFallbackIndex : (itemCount - 1);
        LVITEMW q;
        memset(&q, 0, sizeof(q));
        q.mask = LVIF_PARAM;
        q.iItem = matchRow;
        if (ListView_GetItem(g_app.hwndExpList, &q)) {
            matchIdx = (int)q.lParam;
        }
    }

    if (g_app.expLastTopIndex > 0 && itemCount > 0) {
        int clampedTop = (g_app.expLastTopIndex < itemCount) ? g_app.expLastTopIndex : (itemCount - 1);
        RECT rcItem;
        if (ListView_GetItemRect(g_app.hwndExpList, 0, &rcItem, LVIR_BOUNDS)) {
            int rowH = rcItem.bottom - rcItem.top;
            if (rowH > 0) {
                ListView_Scroll(g_app.hwndExpList, 0, clampedTop * rowH);
            }
        }
    }

    if (matchRow >= 0) {
        ListView_SetItemState(g_app.hwndExpList, matchRow,
                              LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        SendMessageW(g_app.hwndExpList, LVM_SETSELECTIONMARK, 0, (LPARAM)matchRow);
        ListView_EnsureVisible(g_app.hwndExpList, matchRow, FALSE);
        g_app.expSelected = matchIdx;
    }

    g_app.expLastTopIndex = 0;
    g_app.expFallbackIndex = -1;
    g_app.populatingExpList = false;

    SendMessageW(g_app.hwndExpList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_app.hwndExpList, NULL, TRUE);
}

static void ActivateExplorerItemAt(int row) {
    if (row < 0) return;

    LVITEMW lvi;
    memset(&lvi, 0, sizeof(lvi));
    lvi.mask = LVIF_PARAM;
    lvi.iItem = row;
    if (!ListView_GetItem(g_app.hwndExpList, &lvi)) return;

    size_t idx = (size_t)lvi.lParam;
    if (idx >= g_app.expCount) return;

    const ExpItem *it = &g_app.expItems[idx];
    if (it->isParent) {
        GoToParentDirectory();
    } else if (it->isDir) {
        NavigateExplorerTo(it->fullPath);
    } else {
        LoadOricFile(it->fullPath);
    }
}

static void ActivateSelectedExplorerItem(void) {
    int sel = ListView_GetNextItem(g_app.hwndExpList, -1, LVNI_SELECTED);
    ActivateExplorerItemAt(sel);
}

static const ExpItem *GetSelectedExplorerItem(int *outRow) {
    if (outRow) *outRow = -1;
    if (!g_app.inExplorerMode || !g_app.hwndExpList) return NULL;
    int sel = ListView_GetNextItem(g_app.hwndExpList, -1, LVNI_SELECTED);
    if (sel < 0) return NULL;

    LVITEMW lvi;
    memset(&lvi, 0, sizeof(lvi));
    lvi.mask = LVIF_PARAM;
    lvi.iItem = sel;
    if (!ListView_GetItem(g_app.hwndExpList, &lvi)) return NULL;

    size_t idx = (size_t)lvi.lParam;
    if (idx >= g_app.expCount) return NULL;
    if (outRow) *outRow = sel;
    return &g_app.expItems[idx];
}

typedef struct {
    HWND hwndDlg;
    HWND hwndEdit;
    const wchar_t *initialName;
    wchar_t resultName[MAX_PATH];
    bool done;
    bool accepted;
} RenameDlgState;

static bool HasInvalidWin32FilenameChars(const wchar_t *name) {
    if (!name || name[0] == L'\0') return true;
    if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) return true;
    size_t len = wcslen(name);
    if (len > 0 && name[len - 1] == L'.') return true;
    for (size_t i = 0; i < len; i++) {
        wchar_t c = name[i];
        if ((unsigned)c < 32u ||
            c == L'\\' || c == L'/' || c == L':' || c == L'*' ||
            c == L'?'  || c == L'"' || c == L'<' || c == L'>' || c == L'|') {
            return true;
        }
    }
    return false;
}

static LRESULT CALLBACK RenameDlgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RenameDlgState *st = (RenameDlgState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        st = (RenameDlgState *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);
        st->hwndDlg = hwnd;

        HINSTANCE hInst = GetModuleHandleW(NULL);
        int pad = ScaleDpi(hwnd, 14);
        int w = ScaleDpi(hwnd, 420);
        int fullW = w - 2 * pad;
        int lblH = ScaleDpi(hwnd, 18);
        int rowH = ScaleDpi(hwnd, 24);
        int btnW = ScaleDpi(hwnd, 104);
        int btnH = ScaleDpi(hwnd, 28);
        int y = pad;

        HWND hLbl = CreateWindowExW(0, L"STATIC", oric_tr(g_app.lang, STR_RENAME_LABEL),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            pad, y, fullW, lblH, hwnd, NULL, hInst, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)g_app.hFontUiBold, TRUE);
        y += lblH + ScaleDpi(hwnd, 6);

        st->hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            st->initialName ? st->initialName : L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            pad, y, fullW, rowH, hwnd, (HMENU)(UINT_PTR)IDC_RENAME_EDIT, hInst, NULL);
        SendMessageW(st->hwndEdit, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);
        SendMessageW(st->hwndEdit, EM_LIMITTEXT, (WPARAM)(MAX_PATH - 1), 0);
        y += rowH + ScaleDpi(hwnd, 14);

        HWND hOk = CreateWindowExW(0, L"BUTTON", oric_tr(g_app.lang, STR_RENAME_DLG_TITLE),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            w - pad - 2 * btnW - ScaleDpi(hwnd, 8), y, btnW, btnH,
            hwnd, (HMENU)(UINT_PTR)IDC_RENAME_OK, hInst, NULL);
        SendMessageW(hOk, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        HWND hCancel = CreateWindowExW(0, L"BUTTON", oric_tr(g_app.lang, STR_EMU_CANCEL),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            w - pad - btnW, y, btnW, btnH,
            hwnd, (HMENU)(UINT_PTR)IDC_RENAME_CANCEL, hInst, NULL);
        SendMessageW(hCancel, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);
        return 0;
    }

    case WM_COMMAND:
        if (!st) break;
        switch (LOWORD(wParam)) {
        case IDC_RENAME_OK: {
            wchar_t raw[MAX_PATH] = {0};
            GetWindowTextW(st->hwndEdit, raw, MAX_PATH);

            const wchar_t *start = raw;
            while (*start == L' ' || *start == L'\t') start++;
            wchar_t trimmed[MAX_PATH];
            wcsncpy(trimmed, start, MAX_PATH - 1);
            trimmed[MAX_PATH - 1] = L'\0';
            size_t len = wcslen(trimmed);
            while (len > 0 && (trimmed[len - 1] == L' ' || trimmed[len - 1] == L'\t')) {
                trimmed[--len] = L'\0';
            }

            if (trimmed[0] == L'\0') {
                MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_RENAME_EMPTY),
                            oric_tr(g_app.lang, STR_ERR_RENAME_TITLE), MB_OK | MB_ICONWARNING);
                SetFocus(st->hwndEdit);
                return 0;
            }
            if (HasInvalidWin32FilenameChars(trimmed)) {
                MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_RENAME_CHARS),
                            oric_tr(g_app.lang, STR_ERR_RENAME_TITLE), MB_OK | MB_ICONWARNING);
                SetFocus(st->hwndEdit);
                return 0;
            }

            wcsncpy(st->resultName, trimmed, MAX_PATH - 1);
            st->resultName[MAX_PATH - 1] = L'\0';
            st->accepted = true;
            st->done = true;
            return 0;
        }
        case IDC_RENAME_CANCEL:
            st->accepted = false;
            st->done = true;
            return 0;
        }
        break;

    case WM_CLOSE:
        if (st) {
            st->accepted = false;
            st->done = true;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool OpenRenameDialog(HWND hwndParent, const wchar_t *initialName, bool isDir,
                             wchar_t *outName, size_t outChars) {
    if (!initialName || !outName || outChars == 0) return false;

    HINSTANCE hInst = GetModuleHandleW(NULL);
    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RenameDlgWndProc;
        wc.hInstance = hInst;
        wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = RENAME_DLG_CLASS_NAME;
        RegisterClassExW(&wc);
        s_registered = true;
    }

    int clientW = ScaleDpi(hwndParent, 420);
    int clientH = ScaleDpi(hwndParent, 120);
    RECT adj = { 0, 0, clientW, clientH };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&adj, style, FALSE);
    int winW = adj.right - adj.left;
    int winH = adj.bottom - adj.top;

    RECT prc;
    GetWindowRect(hwndParent, &prc);
    int posX = prc.left + ((prc.right - prc.left) - winW) / 2;
    int posY = prc.top + ((prc.bottom - prc.top) - winH) / 2;

    RenameDlgState st;
    memset(&st, 0, sizeof(st));
    st.initialName = initialName;

    EnableWindow(hwndParent, FALSE);
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        RENAME_DLG_CLASS_NAME,
        oric_tr(g_app.lang, STR_RENAME_DLG_TITLE),
        style,
        posX, posY, winW, winH,
        hwndParent, NULL, hInst, &st);

    if (!hDlg) {
        EnableWindow(hwndParent, TRUE);
        return false;
    }

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    SetFocus(st.hwndEdit);
    if (!isDir) {
        const wchar_t *dot = wcsrchr(initialName, L'.');
        if (dot && dot > initialName) {
            SendMessageW(st.hwndEdit, EM_SETSEL, 0, (LPARAM)(dot - initialName));
        } else {
            SendMessageW(st.hwndEdit, EM_SETSEL, 0, -1);
        }
    } else {
        SendMessageW(st.hwndEdit, EM_SETSEL, 0, -1);
    }

    MSG msg;
    while (!st.done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                st.accepted = false;
                st.done = true;
                break;
            }
            if (msg.wParam == VK_RETURN) {
                HWND focus = GetFocus();
                if (focus != GetDlgItem(hDlg, IDC_RENAME_CANCEL)) {
                    SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDC_RENAME_OK, BN_CLICKED), 0);
                    continue;
                }
            }
        }
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hwndParent, TRUE);
    SetForegroundWindow(hwndParent);
    DestroyWindow(hDlg);

    if (st.accepted) {
        wcsncpy(outName, st.resultName, outChars - 1);
        outName[outChars - 1] = L'\0';
    }
    return st.accepted;
}

static void FreeExplorerPreview(void);

static void RenameSelectedExplorerItem(HWND hwnd) {
    int selRow = -1;
    const ExpItem *it = GetSelectedExplorerItem(&selRow);
    if (!it || it->isParent) return;

    wchar_t oldName[MAX_PATH];
    wchar_t oldPath[MAX_PATH];
    bool isDir = it->isDir;
    wcsncpy(oldName, it->name, MAX_PATH - 1);
    oldName[MAX_PATH - 1] = L'\0';
    wcsncpy(oldPath, it->fullPath, MAX_PATH - 1);
    oldPath[MAX_PATH - 1] = L'\0';

    wchar_t newName[MAX_PATH] = {0};
    if (!OpenRenameDialog(hwnd, oldName, isDir, newName, MAX_PATH)) {
        SetFocus(g_app.hwndExpList);
        return;
    }

    if (wcscmp(oldName, newName) == 0) {
        SetFocus(g_app.hwndExpList);
        return;
    }

    wchar_t newPath[MAX_PATH] = {0};
    size_t bLen = wcslen(g_app.browseDir);
    int nLen = (bLen > 0 && g_app.browseDir[bLen - 1] == L'\\')
                   ? swprintf(newPath, MAX_PATH, L"%ls%ls", g_app.browseDir, newName)
                   : swprintf(newPath, MAX_PATH, L"%ls\\%ls", g_app.browseDir, newName);
    if (nLen < 0 || nLen >= MAX_PATH) {
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_RENAME_FAIL),
                    oric_tr(g_app.lang, STR_ERR_RENAME_TITLE), MB_OK | MB_ICONERROR);
        SetFocus(g_app.hwndExpList);
        return;
    }

    int topIdx = g_app.hwndExpList ? ListView_GetTopIndex(g_app.hwndExpList) : 0;
    FreeExplorerPreview();

    if (!MoveFileW(oldPath, newPath)) {
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_RENAME_FAIL),
                    oric_tr(g_app.lang, STR_ERR_RENAME_TITLE), MB_OK | MB_ICONERROR);
        UpdateSidebarAll();
        UpdateStatusBar();
        SetFocus(g_app.hwndExpList);
        return;
    }

    if (UpdateRecentFilePath(oldPath, newPath)) {
        RebuildMenuBar(hwnd);
    }

    wcsncpy(g_app.expLastPath, newPath, MAX_PATH - 1);
    g_app.expLastPath[MAX_PATH - 1] = L'\0';
    g_app.expLastTopIndex = topIdx;
    g_app.expFallbackIndex = selRow;

    PopulateExplorerList();
    UpdateSidebarAll();
    UpdateStatusBar();

    if (g_app.hwndStatus) {
        wchar_t st[MAX_PATH + 64];
        swprintf(st, MAX_PATH + 64, oric_tr(g_app.lang, STR_STATUS_RENAMED_FMT), newName);
        SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0, (LPARAM)st);
    }
    SetFocus(g_app.hwndExpList);
}

static void DeleteSelectedExplorerItem(HWND hwnd) {
    int selRow = -1;
    const ExpItem *it = GetSelectedExplorerItem(&selRow);
    if (!it || it->isParent) return;

    wchar_t delName[MAX_PATH];
    wchar_t delPath[MAX_PATH + 2];
    wcsncpy(delName, it->name, MAX_PATH - 1);
    delName[MAX_PATH - 1] = L'\0';
    memset(delPath, 0, sizeof(delPath));
    wcsncpy(delPath, it->fullPath, MAX_PATH - 1);

    int topIdx = g_app.hwndExpList ? ListView_GetTopIndex(g_app.hwndExpList) : 0;
    FreeExplorerPreview();

    SHFILEOPSTRUCTW shfo;
    memset(&shfo, 0, sizeof(shfo));
    shfo.hwnd = hwnd;
    shfo.wFunc = FO_DELETE;
    shfo.pFrom = delPath;
    shfo.fFlags = FOF_ALLOWUNDO;

    int res = SHFileOperationW(&shfo);
    if (res != 0 && !shfo.fAnyOperationsAborted) {
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_DELETE_FAIL),
                    oric_tr(g_app.lang, STR_ERR_DELETE_TITLE), MB_OK | MB_ICONERROR);
        UpdateSidebarAll();
        UpdateStatusBar();
        SetFocus(g_app.hwndExpList);
        return;
    }

    if (shfo.fAnyOperationsAborted) {
        UpdateSidebarAll();
        UpdateStatusBar();
        SetFocus(g_app.hwndExpList);
        return;
    }

    if (RemoveRecentFilePath(delPath)) {
        RebuildMenuBar(hwnd);
    }

    g_app.expLastPath[0] = L'\0';
    g_app.expLastTopIndex = topIdx;
    g_app.expFallbackIndex = selRow;

    PopulateExplorerList();
    UpdateSidebarAll();
    UpdateStatusBar();

    if (g_app.hwndStatus) {
        wchar_t st[MAX_PATH + 64];
        swprintf(st, MAX_PATH + 64, oric_tr(g_app.lang, STR_STATUS_DELETED_FMT), delName);
        SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0, (LPARAM)st);
    }
    SetFocus(g_app.hwndExpList);
}

/* -------------------- Visual Canvas Painting & PNG Export -------------------- */

static const OricMediaReport *GetActiveMediaReport(const uint8_t **outBuf, size_t *outSize, int *outEntryIdx) {
    if (!g_app.inExplorerMode) {
        if (outBuf) *outBuf = g_app.rawBuffer;
        if (outSize) *outSize = g_app.rawSize;
        if (outEntryIdx) *outEntryIdx = g_app.selectedIndex;
        return &g_app.report;
    }
    if (g_app.hasExpPreview) {
        if (outBuf) *outBuf = g_app.expPreviewBuffer;
        if (outSize) *outSize = g_app.expPreviewSize;
        if (outEntryIdx) *outEntryIdx = g_app.expPreviewEntryIdx;
        return &g_app.expPreviewReport;
    }
    if (outBuf) *outBuf = NULL;
    if (outSize) *outSize = 0;
    if (outEntryIdx) *outEntryIdx = -1;
    return NULL;
}

static void RenderDiskMapToBgra(uint32_t **outPixels, int *outW, int *outH) {
    *outPixels = NULL;
    *outW = 0;
    *outH = 0;
    const OricMediaReport *rep = GetActiveMediaReport(NULL, NULL, NULL);
    if (!g_app.hasDiskMap || !rep || rep->mediaType != ORIC_MEDIA_DSK) return;

    uint32_t sides = rep->dsk.sides;
    uint32_t tracks = rep->dsk.tracks;
    uint32_t sectors = rep->dsk.sectorsPerTrack;
    if (sides == 0) sides = 1;
    if (sides > 2) sides = 2;
    if (tracks == 0) tracks = 42;
    if (tracks > ORIC_MAX_MAP_TRACKS) tracks = ORIC_MAX_MAP_TRACKS;
    if (sectors == 0) sectors = 17;
    if (sectors > ORIC_MAX_MAP_SECTORS) sectors = ORIC_MAX_MAP_SECTORS;

    int pad = 16;
    int cell = 10;
    int sideGap = 14;
    int w = pad * 2 + (int)tracks * cell;
    int h = pad * 2 + (int)sides * (int)sectors * cell + ((sides > 1) ? sideGap : 0);

    uint32_t *pix = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!pix) return;

    for (int i = 0; i < w * h; i++) {
        pix[i] = 0xFF141820u; /* background */
    }

    for (uint32_t side = 0; side < sides; side++) {
        int baseY = pad + (int)side * ((int)sectors * cell + sideGap);
        for (uint32_t t = 0; t < tracks; t++) {
            for (uint32_t s = 0; s < sectors; s++) {
                uint8_t st = g_app.diskMap[side][t][s];
                uint32_t col = 0xFF24623Eu; /* Free */
                if (st == ORIC_SECTOR_USED) col = 0xFF2D76D2u;
                else if (st == ORIC_SECTOR_SYSTEM) col = 0xFFD97706u;
                else if (st == ORIC_SECTOR_SELECTED) col = 0xFFE11D48u;

                int x0 = pad + (int)t * cell;
                int y0 = baseY + (int)s * cell;
                for (int dy = 0; dy < cell - 1; dy++) {
                    for (int dx = 0; dx < cell - 1; dx++) {
                        pix[(y0 + dy) * w + (x0 + dx)] = col;
                    }
                }
            }
        }
    }

    *outPixels = pix;
    *outW = w;
    *outH = h;
}

static LRESULT CALLBACK VisualCanvasProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int cw = rc.right - rc.left;
        int ch = rc.bottom - rc.top;

        HDC hMem = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, cw > 0 ? cw : 1, ch > 0 ? ch : 1);
        HGDIOBJ hOldBmp = SelectObject(hMem, hBmp);

        HBRUSH hBg = CreateSolidBrush(RGB(20, 24, 32));
        FillRect(hMem, &rc, hBg);
        DeleteObject(hBg);

        SetBkMode(hMem, TRANSPARENT);
        SetTextColor(hMem, RGB(220, 228, 240));
        SelectObject(hMem, g_app.hFontUiBold);

        int pad = ScaleDpi(hwnd, 10);
        int hdrH = ScaleDpi(hwnd, 24);

        if (g_app.screenPixels && g_app.screenW > 0 && g_app.screenH > 0) {
            RECT rHdr = { pad, pad / 2, cw - pad, pad / 2 + hdrH };
            DrawTextW(hMem, g_app.visualTitle, -1, &rHdr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            int areaX = pad;
            int areaY = pad + hdrH;
            int areaW = cw - 2 * pad;
            int areaH = ch - areaY - pad;
            if (areaW > 16 && areaH > 16) {
                double scaleX = (double)areaW / (double)g_app.screenW;
                double scaleY = (double)areaH / (double)g_app.screenH;
                double scale = (scaleX < scaleY) ? scaleX : scaleY;
                int drawW = (int)(g_app.screenW * scale);
                int drawH = (int)(g_app.screenH * scale);
                int drawX = areaX + (areaW - drawW) / 2;
                int drawY = areaY + (areaH - drawH) / 2;

                RECT rBorder = { drawX - 2, drawY - 2, drawX + drawW + 2, drawY + drawH + 2 };
                HBRUSH hBrBorder = CreateSolidBrush(RGB(70, 82, 102));
                FrameRect(hMem, &rBorder, hBrBorder);
                DeleteObject(hBrBorder);

                BITMAPINFO bmi;
                memset(&bmi, 0, sizeof(bmi));
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = g_app.screenW;
                bmi.bmiHeader.biHeight = -g_app.screenH; /* top-down */
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                SetStretchBltMode(hMem, COLORONCOLOR);
                StretchDIBits(hMem,
                              drawX, drawY, drawW, drawH,
                              0, 0, g_app.screenW, g_app.screenH,
                              g_app.screenPixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
            }
        } else {
            const OricMediaReport *rep = GetActiveMediaReport(NULL, NULL, NULL);
            if (g_app.hasDiskMap && rep && rep->mediaType == ORIC_MEDIA_DSK) {
                RECT rHdr = { pad, pad / 2, cw - pad, pad / 2 + hdrH };
                DrawTextW(hMem, g_app.visualTitle, -1, &rHdr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                uint32_t sides = rep->dsk.sides;
                uint32_t tracks = rep->dsk.tracks;
                uint32_t sectors = rep->dsk.sectorsPerTrack;
            if (sides == 0) sides = 1;
            if (sides > 2) sides = 2;
            if (tracks == 0) tracks = 42;
            if (tracks > ORIC_MAX_MAP_TRACKS) tracks = ORIC_MAX_MAP_TRACKS;
            if (sectors == 0) sectors = 17;
            if (sectors > ORIC_MAX_MAP_SECTORS) sectors = ORIC_MAX_MAP_SECTORS;

            int legendH = ScaleDpi(hwnd, 28);
            int gridX = pad;
            int gridY = pad + hdrH;
            int gridW = cw - 2 * pad;
            int gridH = ch - gridY - legendH - pad;

            HBRUSH hBrFree = CreateSolidBrush(RGB(36, 98, 62));
            HBRUSH hBrUsed = CreateSolidBrush(RGB(45, 118, 210));
            HBRUSH hBrSys  = CreateSolidBrush(RGB(217, 119, 6));
            HBRUSH hBrSel  = CreateSolidBrush(RGB(225, 29, 72));

            if (gridW > 40 && gridH > 40) {
                int sideGap = (sides > 1) ? ScaleDpi(hwnd, 10) : 0;
                int eachH = (gridH - sideGap * (int)(sides - 1)) / (int)sides;

                for (uint32_t side = 0; side < sides; side++) {
                    int baseTop = gridY + (int)side * (eachH + sideGap);
                    int cellW = gridW / (int)tracks;
                    int cellH = eachH / (int)sectors;
                    if (cellW < 3) cellW = 3;
                    if (cellH < 3) cellH = 3;

                    for (uint32_t t = 0; t < tracks; t++) {
                        for (uint32_t s = 0; s < sectors; s++) {
                            uint8_t st = g_app.diskMap[side][t][s];
                            HBRUSH br = hBrFree;
                            if (st == ORIC_SECTOR_USED) br = hBrUsed;
                            else if (st == ORIC_SECTOR_SYSTEM) br = hBrSys;
                            else if (st == ORIC_SECTOR_SELECTED) br = hBrSel;

                            RECT cell;
                            cell.left = gridX + (int)t * cellW;
                            cell.top = baseTop + (int)s * cellH;
                            cell.right = cell.left + cellW - 1;
                            cell.bottom = cell.top + cellH - 1;
                            if (cell.right <= cell.left) cell.right = cell.left + 1;
                            if (cell.bottom <= cell.top) cell.bottom = cell.top + 1;
                            FillRect(hMem, &cell, br);
                        }
                    }
                }
            }

            int ly = ch - legendH;
            int sq = ScaleDpi(hwnd, 12);
            HBRUSH legendBr[4] = { hBrFree, hBrUsed, hBrSys, hBrSel };
            const wchar_t *legendLabels = oric_tr(g_app.lang, STR_VISUAL_MAP_LEGEND);
            int lx = pad;
            SelectObject(hMem, g_app.hFontUi);
            for (int k = 0; k < 4; k++) {
                RECT rb = { lx, ly + (legendH - sq) / 2, lx + sq, ly + (legendH - sq) / 2 + sq };
                FillRect(hMem, &rb, legendBr[k]);
                lx += sq + ScaleDpi(hwnd, 4);
                wchar_t tok[32];
                const wchar_t *p = legendLabels;
                for (int skip = 0; skip < k && p; skip++) {
                    const wchar_t *sep = wcsstr(p, L"  ");
                    p = sep ? (sep + 2) : NULL;
                }
                if (p) {
                    const wchar_t *sep = wcsstr(p, L"  ");
                    size_t n = sep ? (size_t)(sep - p) : wcslen(p);
                    if (n >= 32) n = 31;
                    wcsncpy(tok, p, n);
                    tok[n] = L'\0';
                    RECT rt = { lx, ly, lx + ScaleDpi(hwnd, 68), ch };
                    DrawTextW(hMem, tok, -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    lx += ScaleDpi(hwnd, 68);
                }
            }

                DeleteObject(hBrFree);
                DeleteObject(hBrUsed);
                DeleteObject(hBrSys);
                DeleteObject(hBrSel);
            } else {
                SelectObject(hMem, g_app.hFontUi);
                SetTextColor(hMem, RGB(155, 165, 185));
                RECT rMsg = { pad * 2, pad * 2, cw - pad * 2, ch - pad * 2 };
                DrawTextW(hMem, oric_tr(g_app.lang, STR_VISUAL_EMPTY), -1, &rMsg,
                          DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            }
        }

        BitBlt(hdc, 0, 0, cw, ch, hMem, 0, 0, SRCCOPY);
        SelectObject(hMem, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(hMem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* -------------------- Sidebar Updates (Summary, Code/Hex, Screen/Map) -------------------- */

static void UpdateTabsText(void) {
    static const OricStrId tabIds[3] = { STR_TAB_SUMMARY, STR_TAB_CODE, STR_TAB_VISUAL };
    for (int i = 0; i < 3; i++) {
        TCITEMW tie;
        memset(&tie, 0, sizeof(tie));
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)oric_tr(g_app.lang, tabIds[i]);
        TabCtrl_SetItem(g_app.hwndTabs, i, &tie);
    }
}

static void SyncTabVisibility(void) {
    ShowWindow(g_app.hwndSideEdit,      (g_app.activeTab == 0) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndCodeEdit,      (g_app.activeTab == 1) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndCodeToggleBtn, (g_app.activeTab == 1) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndVisualCanvas,  (g_app.activeTab == 2) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndCopyBtn,       (g_app.activeTab != 2) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndSavePngBtn,    (g_app.activeTab == 2) ? SW_SHOW : SW_HIDE);
    SetWindowTextW(g_app.hwndCodeToggleBtn,
                   oric_tr(g_app.lang, g_app.forceHexMode ? STR_BTN_MODE_HEX_AUTO : STR_BTN_MODE_AUTO_HEX));
    SetWindowTextW(g_app.hwndSavePngBtn, oric_tr(g_app.lang, STR_BTN_SAVE_PNG));
}

static void SyncViewModeVisibility(void) {
    BOOL exp = g_app.inExplorerMode ? TRUE : FALSE;

    ShowWindow(g_app.hwndExpDriveLbl, exp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndExpDrive,    exp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndExpPathLbl,  exp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndExpPath,     exp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndExpUp,       exp ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app.hwndExpList,     exp ? SW_SHOW : SW_HIDE);

    ShowWindow(g_app.hwndBackBtn,     exp ? SW_HIDE : SW_SHOW);
    ShowWindow(g_app.hwndList,        exp ? SW_HIDE : SW_SHOW);
}

static void RefreshCodeInspector(void) {
    const uint8_t *buf = NULL;
    size_t bufSize = 0;
    int selIdx = -1;
    const OricMediaReport *rep = GetActiveMediaReport(&buf, &bufSize, &selIdx);

    if (!rep || !buf || rep->mediaType == ORIC_MEDIA_NONE) {
        SetWindowTextW(g_app.hwndCodeEdit, oric_tr(g_app.lang, STR_CODE_PLACEHOLDER));
        return;
    }

    size_t bufChars = 65536u;
    if (selIdx >= 0 && (size_t)selIdx < rep->entryCount) {
        size_t est = (size_t)rep->entries[selIdx].sizeBytes * 8u + 4096u;
        if (est > bufChars) bufChars = est;
        if (bufChars > 1048576u) bufChars = 1048576u;
    }
    wchar_t *out = (wchar_t *)malloc(bufChars * sizeof(wchar_t));
    if (!out) return;
    out[0] = L'\0';

    if (rep->mediaType == ORIC_MEDIA_ROM) {
        uint16_t base = rep->rom.baseAddr;
        size_t startOff = 0;
        if (selIdx >= 0 && (size_t)selIdx < rep->entryCount) {
            uint16_t target = rep->entries[selIdx].startAddr;
            if (target >= base && (size_t)(target - base) < bufSize) {
                startOff = (size_t)(target - base);
            }
        }
        if (g_app.forceHexMode) {
            int hdr = swprintf(out, bufChars, oric_tr(g_app.lang, STR_CODE_HEADER_HEX_FMT),
                               rep->fileName, base, bufSize);
            oric_format_hex_dump(buf, bufSize, base, 4096u,
                                 out + (hdr > 0 ? hdr : 0), bufChars - (hdr > 0 ? (size_t)hdr : 0));
        } else {
            int hdr = swprintf(out, bufChars, oric_tr(g_app.lang, STR_CODE_HEADER_DISASM_FMT),
                               rep->fileName, (uint16_t)(base + startOff), bufSize);
            oric_disassemble_6502(buf, bufSize, base, startOff, 450u,
                                  out + (hdr > 0 ? hdr : 0), bufChars - (hdr > 0 ? (size_t)hdr : 0));
        }
        SetWindowTextW(g_app.hwndCodeEdit, out);
        free(out);
        return;
    }

    if (selIdx < 0 || (size_t)selIdx >= rep->entryCount) {
        SetWindowTextW(g_app.hwndCodeEdit, oric_tr(g_app.lang, STR_CODE_PLACEHOLDER));
        free(out);
        return;
    }

    const OricFileEntry *e = &rep->entries[selIdx];
    uint8_t *payload = NULL;
    size_t payloadSize = 0;
    if (!oric_extract_payload(buf, bufSize, rep, (size_t)selIdx, &payload, &payloadSize)) {
        SetWindowTextW(g_app.hwndCodeEdit, oric_tr(g_app.lang, STR_CODE_PLACEHOLDER));
        free(out);
        return;
    }

    uint16_t baseAddr = e->hasStartEnd ? e->startAddr : 0x0501u;
    if (g_app.forceHexMode) {
        int hdr = swprintf(out, bufChars, oric_tr(g_app.lang, STR_CODE_HEADER_HEX_FMT),
                           e->name, baseAddr, payloadSize);
        oric_format_hex_dump(payload, payloadSize, baseAddr, 4096u,
                             out + (hdr > 0 ? hdr : 0), bufChars - (hdr > 0 ? (size_t)hdr : 0));
    } else if (e->isBasic) {
        int hdr = swprintf(out, bufChars, oric_tr(g_app.lang, STR_CODE_HEADER_BASIC_FMT),
                           e->name, payloadSize);
        size_t pos = (hdr > 0) ? (size_t)hdr : 0;
        if (!oric_detokenize_basic(payload, payloadSize, baseAddr, out + pos, bufChars - pos)) {
            hdr = swprintf(out, bufChars, oric_tr(g_app.lang, STR_CODE_HEADER_HEX_FMT),
                           e->name, baseAddr, payloadSize);
            oric_format_hex_dump(payload, payloadSize, baseAddr, 4096u,
                                 out + (hdr > 0 ? hdr : 0), bufChars - (hdr > 0 ? (size_t)hdr : 0));
        }
    } else {
        int hdr = swprintf(out, bufChars, oric_tr(g_app.lang, STR_CODE_HEADER_DISASM_FMT),
                           e->name, baseAddr, payloadSize);
        size_t startOff = 0;
        if (e->hasExec && e->execAddr >= baseAddr && (size_t)(e->execAddr - baseAddr) < payloadSize) {
            startOff = (size_t)(e->execAddr - baseAddr);
        }
        oric_disassemble_6502(payload, payloadSize, baseAddr, startOff, 450u,
                              out + (hdr > 0 ? hdr : 0), bufChars - (hdr > 0 ? (size_t)hdr : 0));
    }

    SetWindowTextW(g_app.hwndCodeEdit, out);
    free(payload);
    free(out);
}

static void RefreshVisualPreview(void) {
    free(g_app.screenPixels);
    g_app.screenPixels = NULL;
    g_app.screenW = 0;
    g_app.screenH = 0;
    g_app.hasDiskMap = false;
    g_app.visualTitle[0] = L'\0';

    const uint8_t *buf = NULL;
    size_t bufSize = 0;
    int selIdx = -1;
    const OricMediaReport *rep = GetActiveMediaReport(&buf, &bufSize, &selIdx);

    if (rep && buf && rep->mediaType != ORIC_MEDIA_NONE && rep->mediaType != ORIC_MEDIA_ROM) {
        int screenIdx = selIdx;
        if (g_app.inExplorerMode) {
            screenIdx = -1;
            for (size_t i = 0; i < rep->entryCount; i++) {
                if (rep->entries[i].isScreen) {
                    screenIdx = (int)i;
                    break;
                }
            }
        }

        if (screenIdx >= 0 && (size_t)screenIdx < rep->entryCount) {
            const OricFileEntry *e = &rep->entries[screenIdx];
            uint8_t *payload = NULL;
            size_t payloadSize = 0;
            if (oric_extract_payload(buf, bufSize, rep, (size_t)screenIdx, &payload, &payloadSize)) {
                OricScreenMode mode = oric_detect_screen_mode(e, payloadSize);
                if (mode != ORIC_SCR_NONE &&
                    oric_render_screen(payload, payloadSize, mode,
                                       &g_app.screenPixels, &g_app.screenW, &g_app.screenH)) {
                    swprintf(g_app.visualTitle, 160,
                             oric_tr(g_app.lang, STR_VISUAL_SCREEN_HDR_FMT),
                             e->name, (unsigned)g_app.screenW, (unsigned)g_app.screenH,
                             (mode == ORIC_SCR_HIRES) ? L"HIRES" : L"TEXT 40x28");
                }
                free(payload);
            }
        }

        if (!g_app.screenPixels && rep->mediaType == ORIC_MEDIA_DSK) {
            oric_mark_selected_disk_sectors(buf, bufSize, rep,
                                            g_app.inExplorerMode ? -1 : selIdx,
                                            g_app.diskMap);
            g_app.hasDiskMap = true;
            swprintf(g_app.visualTitle, 160,
                     oric_tr(g_app.lang, STR_VISUAL_MAP_HDR_FMT),
                     rep->dsk.dosLabel,
                     rep->dsk.sides,
                     rep->dsk.tracks,
                     rep->dsk.sectorsPerTrack);
        }
    }

    if (g_app.hwndVisualCanvas) {
        InvalidateRect(g_app.hwndVisualCanvas, NULL, FALSE);
    }
}

static bool HasSelectedExplorerMediaFile(wchar_t *outPath, size_t maxChars) {
    if (!g_app.inExplorerMode || !g_app.hwndExpList) return false;
    int sel = ListView_GetNextItem(g_app.hwndExpList, -1, LVNI_SELECTED);
    if (sel < 0) return false;
    LVITEMW lvi;
    memset(&lvi, 0, sizeof(lvi));
    lvi.mask = LVIF_PARAM;
    lvi.iItem = sel;
    if (!ListView_GetItem(g_app.hwndExpList, &lvi)) return false;
    size_t idx = (size_t)lvi.lParam;
    if (idx >= g_app.expCount) return false;
    if (g_app.expItems[idx].isDir) return false;
    if (outPath && maxChars > 0) {
        wcsncpy(outPath, g_app.expItems[idx].fullPath, maxChars - 1);
        outPath[maxChars - 1] = L'\0';
    }
    return true;
}

static void FreeExplorerPreview(void) {
    free(g_app.expPreviewBuffer);
    g_app.expPreviewBuffer = NULL;
    g_app.expPreviewSize = 0;
    g_app.expPreviewPath[0] = L'\0';
    g_app.expPreviewEntryIdx = -1;
    g_app.hasExpPreview = false;
    oric_report_free(&g_app.expPreviewReport);
    oric_report_init(&g_app.expPreviewReport);
}

static void SyncExplorerPreview(void) {
    wchar_t selPath[MAX_PATH] = {0};
    if (!g_app.inExplorerMode || !HasSelectedExplorerMediaFile(selPath, MAX_PATH)) {
        FreeExplorerPreview();
        return;
    }

    if (g_app.hasExpPreview && _wcsicmp(selPath, g_app.expPreviewPath) == 0) {
        if (g_app.expPreviewReport.lang != g_app.lang &&
            g_app.expPreviewBuffer && g_app.expPreviewSize > 0) {
            oric_analyze_buffer(g_app.expPreviewBuffer, g_app.expPreviewSize,
                                g_app.expPreviewPath, g_app.lang, &g_app.expPreviewReport);
        }
        return;
    }

    FreeExplorerPreview();

    HANDLE hFile = CreateFileW(selPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    LARGE_INTEGER liSize;
    if (!GetFileSizeEx(hFile, &liSize) || liSize.QuadPart <= 0 || liSize.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(hFile);
        return;
    }

    DWORD fileSize = (DWORD)liSize.QuadPart;
    uint8_t *buf = (uint8_t *)malloc(fileSize);
    if (!buf) {
        CloseHandle(hFile);
        return;
    }

    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, buf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!ok || bytesRead != fileSize) {
        free(buf);
        return;
    }

    g_app.expPreviewBuffer = buf;
    g_app.expPreviewSize = bytesRead;
    wcsncpy(g_app.expPreviewPath, selPath, MAX_PATH - 1);
    g_app.expPreviewPath[MAX_PATH - 1] = L'\0';

    oric_analyze_buffer(g_app.expPreviewBuffer, g_app.expPreviewSize,
                        g_app.expPreviewPath, g_app.lang, &g_app.expPreviewReport);
    g_app.hasExpPreview = true;

    g_app.expPreviewEntryIdx = -1;
    if (g_app.expPreviewReport.entryCount > 0) {
        g_app.expPreviewEntryIdx = 0;
        for (size_t i = 0; i < g_app.expPreviewReport.entryCount; i++) {
            if (!g_app.expPreviewReport.entries[i].isScreen) {
                g_app.expPreviewEntryIdx = (int)i;
                break;
            }
        }
    }
}

static void FormatExplorerMediaSummary(wchar_t *out, size_t outChars) {
    if (!out || outChars == 0) return;
    out[0] = L'\0';
    oric_format_summary(&g_app.expPreviewReport, g_app.lang, -1, out, outChars);

    if (g_app.expPreviewReport.mediaType != ORIC_MEDIA_ROM &&
        g_app.expPreviewReport.entryCount > 0) {
        size_t pos = wcslen(out);
        if (pos + 128 < outChars) {
            int hdr = swprintf(out + pos, outChars - pos,
                               oric_tr(g_app.lang, STR_EXP_PREVIEW_CONTENTS_FMT),
                               g_app.expPreviewReport.entryCount);
            if (hdr > 0) pos += (size_t)hdr;
        }

        size_t limit = g_app.expPreviewReport.entryCount;
        if (limit > 80u) limit = 80u;
        for (size_t i = 0; i < limit && pos + 192 < outChars; i++) {
            const OricFileEntry *e = &g_app.expPreviewReport.entries[i];
            wchar_t addrStr[48];
            if (e->hasStartEnd && e->hasExec) {
                swprintf(addrStr, 48, L"$%04X-$%04X (Ex:$%04X)",
                         e->startAddr, e->endAddr, e->execAddr);
            } else if (e->hasStartEnd) {
                swprintf(addrStr, 48, L"$%04X-$%04X", e->startAddr, e->endAddr);
            } else {
                wcscpy(addrStr, L"-");
            }
            wchar_t crcStr[24];
            if (e->hasCrc32) {
                swprintf(crcStr, 24, L"0x%08X", e->crc32);
            } else {
                wcscpy(crcStr, L"-");
            }
            int n = swprintf(out + pos, outChars - pos,
                             L"%3zu. %-12ls [%-7ls] %6u  %-10ls  %ls%ls\r\n",
                             i + 1u,
                             e->name,
                             e->typeLabel,
                             e->sizeBytes,
                             crcStr,
                             addrStr,
                             e->isAutorun ? L" [AUTO]" : L"");
            if (n > 0) pos += (size_t)n;
        }
        if (g_app.expPreviewReport.entryCount > limit && pos + 48 < outChars) {
            swprintf(out + pos, outChars - pos, L"... (+%zu)\r\n",
                     g_app.expPreviewReport.entryCount - limit);
        }
    }
}

static void UpdateSidebarAll(void) {
    if (g_app.inExplorerMode) {
        SyncExplorerPreview();
        if (g_app.hasExpPreview) {
            size_t cap = 16384u;
            wchar_t *summary = (wchar_t *)malloc(cap * sizeof(wchar_t));
            if (summary) {
                FormatExplorerMediaSummary(summary, cap);
                SetWindowTextW(g_app.hwndSideEdit, summary);
                free(summary);
            }
        } else {
            wchar_t summary[2048];
            swprintf(summary, 2048, oric_tr(g_app.lang, STR_EXP_SUMMARY_FMT), g_app.browseDir);
            SetWindowTextW(g_app.hwndSideEdit, summary);
        }
    } else {
        wchar_t summary[8192];
        oric_format_summary(&g_app.report, g_app.lang, g_app.selectedIndex, summary, 8192);
        SetWindowTextW(g_app.hwndSideEdit, summary);
    }

    RefreshCodeInspector();
    RefreshVisualPreview();

    BOOL hasMedia = (!g_app.inExplorerMode && g_app.report.mediaType != ORIC_MEDIA_NONE);
    BOOL hasSelPayload = (hasMedia && g_app.selectedIndex >= 0 &&
                          (size_t)g_app.selectedIndex < g_app.report.entryCount &&
                          (g_app.report.mediaType == ORIC_MEDIA_TAP ||
                           g_app.report.mediaType == ORIC_MEDIA_DSK));
    BOOL canExtractBin = hasSelPayload || (hasMedia && g_app.report.mediaType == ORIC_MEDIA_ROM);
    BOOL hasImage = (g_app.screenPixels != NULL && g_app.screenW > 0 && g_app.screenH > 0) || g_app.hasDiskMap;
    BOOL canLaunchEmu = hasMedia || HasSelectedExplorerMediaFile(NULL, 0);

    EnableWindow(g_app.hwndCopyBtn, g_app.inExplorerMode || hasMedia);
    EnableWindow(g_app.hwndSavePngBtn, hasImage);

    HMENU hMenu = GetMenu(g_app.hwndMain);
    if (hMenu) {
        EnableMenuItem(hMenu, IDM_EDIT_COPY,         MF_BYCOMMAND | ((g_app.inExplorerMode || hasMedia) ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, IDM_FILE_CLOSE,        MF_BYCOMMAND | (hasMedia ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, IDM_TOOLS_EXTRACT_BIN, MF_BYCOMMAND | (canExtractBin ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, IDM_TOOLS_EXPORT_TAP,  MF_BYCOMMAND | (hasSelPayload ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, IDM_TOOLS_EXPORT_WAV,  MF_BYCOMMAND | (hasSelPayload ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, IDM_TOOLS_EXPORT_PNG,  MF_BYCOMMAND | (hasImage ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, IDM_TOOLS_LAUNCH_EMU,  MF_BYCOMMAND | (canLaunchEmu ? MF_ENABLED : MF_GRAYED));
    }
}

static const wchar_t *GetMediaFormatLabel(const OricMediaReport *rep) {
    const wchar_t *fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_UNKNOWN);
    if (!rep) return fmtText;

    if (rep->mediaType == ORIC_MEDIA_TAP) {
        fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_TAP);
    } else if (rep->mediaType == ORIC_MEDIA_DSK) {
        switch (rep->dsk.dosType) {
        case ORIC_DOS_STRATSED: fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_DSK_STRATSED); break;
        case ORIC_DOS_FTDOS:    fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_DSK_FTDOS); break;
        case ORIC_DOS_ORICDOS:  fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_DSK_ORICDOS); break;
        default:
            fmtText = (rep->dsk.containerType == ORIC_DSK_ORICDISK)
                          ? oric_tr(g_app.lang, STR_STATUS_FMT_DSK_ORICDISK)
                          : oric_tr(g_app.lang, STR_STATUS_FMT_DSK_MFM);
            break;
        }
    } else if (rep->mediaType == ORIC_MEDIA_ROM) {
        if (rep->rom.isKnownOfficial) {
            if (rep->rom.isOric1) fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_ROM_ORIC1);
            else if (rep->rom.isAtmos) fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_ROM_ATMOS);
            else if (rep->rom.isTelestrat) fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_ROM_TELESTRAT);
            else fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_ROM_OFFICIAL);
        } else {
            fmtText = oric_tr(g_app.lang, STR_STATUS_FMT_ROM_OTHER);
        }
    }
    return fmtText;
}

static void UpdateStatusBar(void) {
    if (!g_app.hwndStatus) return;

    if (g_app.inExplorerMode) {
        wchar_t st[MAX_PATH + 64];
        size_t itemCnt = g_app.expCount;
        if (itemCnt > 0 && g_app.expItems && g_app.expItems[0].isParent) {
            itemCnt--;
        }
        swprintf(st, MAX_PATH + 64, oric_tr(g_app.lang, STR_EXP_STATUS_FMT),
                 itemCnt, g_app.browseDir);
        SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0, (LPARAM)st);

        if (g_app.hasExpPreview && g_app.expPreviewReport.mediaType != ORIC_MEDIA_NONE) {
            SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 1,
                         (LPARAM)GetMediaFormatLabel(&g_app.expPreviewReport));
            wchar_t countBuf[64];
            if (g_app.expPreviewReport.mediaType == ORIC_MEDIA_ROM) {
                swprintf(countBuf, 64, oric_tr(g_app.lang, STR_STATUS_COUNT_ROM_FMT),
                         g_app.expPreviewReport.rawFileSize, g_app.expPreviewReport.rom.crc32);
            } else {
                swprintf(countBuf, 64, oric_tr(g_app.lang, STR_STATUS_COUNT_ITEMS_FMT),
                         g_app.expPreviewReport.entryCount, g_app.expPreviewReport.rawFileSize);
            }
            SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 2, (LPARAM)countBuf);
        } else {
            SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 1,
                         (LPARAM)oric_tr(g_app.lang, STR_STATUS_NOMEDIA));
            SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 2,
                         (LPARAM)oric_tr(g_app.lang, STR_STATUS_READY));
        }
        return;
    }

    if (g_app.report.mediaType == ORIC_MEDIA_NONE) {
        SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0,
                     (LPARAM)oric_tr(g_app.lang, STR_STATUS_READY));
        SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 1,
                     (LPARAM)oric_tr(g_app.lang, STR_STATUS_NOMEDIA));
        SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 2,
                     (LPARAM)oric_tr(g_app.lang, STR_STATUS_ZERO_ITEMS));
        return;
    }

    SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0, (LPARAM)g_app.report.statusMessage);
    SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 1, (LPARAM)GetMediaFormatLabel(&g_app.report));

    wchar_t countBuf[64];
    if (g_app.report.mediaType == ORIC_MEDIA_ROM) {
        swprintf(countBuf, 64, oric_tr(g_app.lang, STR_STATUS_COUNT_ROM_FMT),
                 g_app.report.rawFileSize, g_app.report.rom.crc32);
    } else {
        swprintf(countBuf, 64, oric_tr(g_app.lang, STR_STATUS_COUNT_ITEMS_FMT),
                 g_app.report.entryCount, g_app.report.rawFileSize);
    }
    SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 2, (LPARAM)countBuf);
}

/* -------------------- Analyzer ListView Filtering & Sorting -------------------- */

static void SetupListViewColumns(HWND hwndList, HWND hwndMain) {
    struct {
        OricStrId titleId;
        int width;
        int fmt;
    } cols[] = {
        { STR_COL_NAME,  145, LVCFMT_LEFT  },
        { STR_COL_TYPE,   95, LVCFMT_LEFT  },
        { STR_COL_SIZE,   75, LVCFMT_RIGHT },
        { STR_COL_ADDR,  125, LVCFMT_LEFT  },
        { STR_COL_FLAGS, 120, LVCFMT_LEFT  },
        { STR_COL_CRC32,  88, LVCFMT_LEFT  }
    };

    for (int i = 0; i < 6; i++) {
        LVCOLUMNW lvc;
        memset(&lvc, 0, sizeof(lvc));
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
        lvc.fmt = cols[i].fmt;
        lvc.cx = ScaleDpi(hwndMain, cols[i].width);
        lvc.pszText = (LPWSTR)oric_tr(g_app.lang, cols[i].titleId);
        lvc.iSubItem = i;
        ListView_InsertColumn(hwndList, i, &lvc);
    }
}

static void UpdateListViewColumnHeaders(void) {
    static const OricStrId colIds[6] = {
        STR_COL_NAME, STR_COL_TYPE, STR_COL_SIZE, STR_COL_ADDR, STR_COL_FLAGS, STR_COL_CRC32
    };
    for (int i = 0; i < 6; i++) {
        LVCOLUMNW lvc;
        memset(&lvc, 0, sizeof(lvc));
        lvc.mask = LVCF_TEXT;
        lvc.pszText = (LPWSTR)oric_tr(g_app.lang, colIds[i]);
        ListView_SetColumn(g_app.hwndList, i, &lvc);
    }
}

static int CALLBACK CompareListItems(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
    (void)lParamSort;
    if (lParam1 < 0 || (size_t)lParam1 >= g_app.report.entryCount ||
        lParam2 < 0 || (size_t)lParam2 >= g_app.report.entryCount) {
        return 0;
    }
    const OricFileEntry *a = &g_app.report.entries[lParam1];
    const OricFileEntry *b = &g_app.report.entries[lParam2];
    int cmp = 0;

    switch (g_app.sortCol) {
    case 0: cmp = _wcsicmp(a->name, b->name); break;
    case 1: cmp = _wcsicmp(a->typeLabel, b->typeLabel); break;
    case 2:
        cmp = (a->sizeBytes < b->sizeBytes) ? -1 : ((a->sizeBytes > b->sizeBytes) ? 1 : 0);
        break;
    case 3: {
        uint32_t va = a->hasStartEnd ? a->startAddr : 0x10000u;
        uint32_t vb = b->hasStartEnd ? b->startAddr : 0x10000u;
        cmp = (va < vb) ? -1 : ((va > vb) ? 1 : _wcsicmp(a->extraDetail, b->extraDetail));
        break;
    }
    case 4: cmp = _wcsicmp(a->flagsLabel, b->flagsLabel); break;
    case 5: {
        if (a->hasCrc32 != b->hasCrc32) {
            cmp = a->hasCrc32 ? -1 : 1;
        } else if (a->hasCrc32) {
            cmp = (a->crc32 < b->crc32) ? -1 : ((a->crc32 > b->crc32) ? 1 : 0);
        } else {
            cmp = 0;
        }
        break;
    }
    default: cmp = 0; break;
    }

    return g_app.sortAsc ? cmp : -cmp;
}

static void PopulateListView(void) {
    wchar_t filter[128] = {0};
    if (!g_app.inExplorerMode && g_app.hwndFilterEdit) {
        GetWindowTextW(g_app.hwndFilterEdit, filter, 128);
    }

    SendMessageW(g_app.hwndList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_app.hwndList);

    int rowToSelect = -1;
    for (size_t i = 0; i < g_app.report.entryCount; i++) {
        const OricFileEntry *e = &g_app.report.entries[i];

        wchar_t crcBuf[24];
        if (e->hasCrc32) {
            swprintf(crcBuf, 24, L"0x%08X", e->crc32);
        } else {
            wcscpy(crcBuf, L"-");
        }

        if (filter[0] != L'\0') {
            if (!wcs_contains_icase(e->name, filter) &&
                !wcs_contains_icase(e->typeLabel, filter) &&
                !wcs_contains_icase(e->flagsLabel, filter) &&
                !wcs_contains_icase(crcBuf, filter)) {
                continue;
            }
        }

        int curCount = ListView_GetItemCount(g_app.hwndList);
        LVITEMW lvi;
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = curCount;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)e->name;
        lvi.lParam = (LPARAM)i;
        int row = ListView_InsertItem(g_app.hwndList, &lvi);
        if (row < 0) continue;

        ListView_SetItemText(g_app.hwndList, row, 1, (LPWSTR)e->typeLabel);

        wchar_t sizeBuf[48];
        swprintf(sizeBuf, 48, oric_tr(g_app.lang, STR_SIZE_BYTES_FMT), e->sizeBytes);
        ListView_SetItemText(g_app.hwndList, row, 2, sizeBuf);

        wchar_t addrBuf[96];
        if (g_app.report.mediaType == ORIC_MEDIA_ROM) {
            wcsncpy(addrBuf, e->extraDetail, 95);
            addrBuf[95] = L'\0';
        } else if (e->hasStartEnd && e->hasExec) {
            swprintf(addrBuf, 96, L"$%04X-$%04X (Ex:$%04X)",
                     e->startAddr, e->endAddr, e->execAddr);
        } else if (e->hasStartEnd) {
            swprintf(addrBuf, 96, L"$%04X - $%04X", e->startAddr, e->endAddr);
        } else {
            wcscpy(addrBuf, L"-");
        }
        ListView_SetItemText(g_app.hwndList, row, 3, addrBuf);
        ListView_SetItemText(g_app.hwndList, row, 4, (LPWSTR)e->flagsLabel);
        ListView_SetItemText(g_app.hwndList, row, 5, crcBuf);

        if ((int)i == g_app.selectedIndex) {
            rowToSelect = row;
        }
    }

    if (g_app.sortCol >= 0) {
        ListView_SortItems(g_app.hwndList, CompareListItems, 0);
        if (g_app.selectedIndex >= 0) {
            int cnt = ListView_GetItemCount(g_app.hwndList);
            for (int r = 0; r < cnt; r++) {
                LVITEMW q;
                memset(&q, 0, sizeof(q));
                q.mask = LVIF_PARAM;
                q.iItem = r;
                if (ListView_GetItem(g_app.hwndList, &q) && (int)q.lParam == g_app.selectedIndex) {
                    rowToSelect = r;
                    break;
                }
            }
        }
    }

    if (rowToSelect >= 0) {
        ListView_SetItemState(g_app.hwndList, rowToSelect,
                              LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }

    SendMessageW(g_app.hwndList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_app.hwndList, NULL, TRUE);
}

/* -------------------- Media Loading & Exports -------------------- */

static void FreeLoadedBuffer(void) {
    free(g_app.rawBuffer);
    g_app.rawBuffer = NULL;
    g_app.rawSize = 0;
    g_app.loadedPath[0] = L'\0';
    free(g_app.screenPixels);
    g_app.screenPixels = NULL;
    g_app.screenW = 0;
    g_app.screenH = 0;
    g_app.hasDiskMap = false;
}

static void ReturnToExplorer(void) {
    FreeLoadedBuffer();
    oric_report_free(&g_app.report);
    oric_report_init(&g_app.report);
    g_app.selectedIndex = -1;
    g_app.sortCol = -1;
    g_app.activeTab = 0;
    TabCtrl_SetCurSel(g_app.hwndTabs, 0);
    ListView_DeleteAllItems(g_app.hwndList);

    g_app.inExplorerMode = true;
    g_app.suppressFilterChange = true;
    SetWindowTextW(g_app.hwndFilterEdit, g_app.expSavedFilter);
    g_app.suppressFilterChange = false;
    SetWindowTextW(g_app.hwndMain, oric_tr(g_app.lang, STR_APP_TITLE));

    SyncViewModeVisibility();
    SyncTabVisibility();
    LayoutChildren(g_app.hwndMain);
    PopulateExplorerList();
    UpdateSidebarAll();
    UpdateStatusBar();
    SetFocus(g_app.hwndExpList);
}

static void RefreshUiLanguage(OricLang newLang) {
    if ((int)newLang < 0 || newLang >= ORIC_LANG_COUNT) return;
    g_app.lang = newLang;
    SaveIniString(INI_SEC_SETTINGS, L"Language", oric_lang_code(newLang));

    RebuildMenuBar(g_app.hwndMain);
    UpdateListViewColumnHeaders();
    UpdateExplorerColumnHeaders();
    UpdateTabsText();
    SetWindowTextW(g_app.hwndExpDriveLbl, oric_tr(g_app.lang, STR_EXP_DRIVE_LABEL));
    SetWindowTextW(g_app.hwndExpPathLbl,  oric_tr(g_app.lang, STR_EXP_PATH_LABEL));
    SetWindowTextW(g_app.hwndExpUp,       oric_tr(g_app.lang, STR_BTN_UP_DIR));
    SetWindowTextW(g_app.hwndBackBtn,     oric_tr(g_app.lang, STR_BTN_BACK_EXPLORER));
    SetWindowTextW(g_app.hwndFilterLabel, oric_tr(g_app.lang, STR_FILTER_LABEL));
    SetWindowTextW(g_app.hwndSideTitle,   oric_tr(g_app.lang, STR_SIDEBAR_TITLE));
    SetWindowTextW(g_app.hwndCopyBtn,     oric_tr(g_app.lang, STR_BTN_COPY));
    SyncTabVisibility();

    if (!g_app.inExplorerMode && g_app.rawBuffer && g_app.rawSize > 0) {
        int prevSel = g_app.selectedIndex;
        oric_analyze_buffer(g_app.rawBuffer, g_app.rawSize,
                            g_app.loadedPath, g_app.lang, &g_app.report);
        g_app.selectedIndex = (prevSel >= 0 && (size_t)prevSel < g_app.report.entryCount)
                                  ? prevSel
                                  : -1;
        PopulateListView();

        wchar_t titleBuf[360];
        swprintf(titleBuf, 360, oric_tr(g_app.lang, STR_APP_TITLE_FILE_FMT), g_app.report.fileName);
        SetWindowTextW(g_app.hwndMain, titleBuf);
    } else {
        g_app.expLastTopIndex = g_app.hwndExpList ? ListView_GetTopIndex(g_app.hwndExpList) : 0;
        g_app.expFallbackIndex = g_app.hwndExpList
                                     ? ListView_GetNextItem(g_app.hwndExpList, -1, LVNI_SELECTED)
                                     : -1;
        PopulateExplorerList();
        SetWindowTextW(g_app.hwndMain, oric_tr(g_app.lang, STR_APP_TITLE));
    }

    UpdateSidebarAll();
    UpdateStatusBar();
}

static void LoadOricFile(const wchar_t *filePath) {
    if (!filePath || filePath[0] == L'\0') return;

    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_app.hwndMain,
                    oric_tr(g_app.lang, STR_ERR_OPEN_MSG),
                    oric_tr(g_app.lang, STR_ERR_OPEN_TITLE), MB_OK | MB_ICONERROR);
        return;
    }

    LARGE_INTEGER liSize;
    if (!GetFileSizeEx(hFile, &liSize) || liSize.QuadPart <= 0 || liSize.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(hFile);
        MessageBoxW(g_app.hwndMain,
                    oric_tr(g_app.lang, STR_ERR_SIZE_MSG),
                    oric_tr(g_app.lang, STR_ERR_OPEN_TITLE), MB_OK | MB_ICONWARNING);
        return;
    }

    DWORD fileSize = (DWORD)liSize.QuadPart;
    uint8_t *buffer = (uint8_t *)malloc(fileSize);
    if (!buffer) {
        CloseHandle(hFile);
        MessageBoxW(g_app.hwndMain,
                    oric_tr(g_app.lang, STR_ERR_MEM_MSG),
                    oric_tr(g_app.lang, STR_ERR_MEM_TITLE), MB_OK | MB_ICONERROR);
        return;
    }

    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hFile, buffer, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!readOk || bytesRead != fileSize) {
        free(buffer);
        MessageBoxW(g_app.hwndMain,
                    oric_tr(g_app.lang, STR_ERR_READ_MSG),
                    oric_tr(g_app.lang, STR_ERR_OPEN_TITLE), MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t fullPathBuf[MAX_PATH];
    DWORD fpn = GetFullPathNameW(filePath, MAX_PATH, fullPathBuf, NULL);
    const wchar_t *resolvedPath = (fpn > 0 && fpn < MAX_PATH) ? fullPathBuf : filePath;

    wchar_t dirBuf[MAX_PATH];
    wcsncpy(dirBuf, resolvedPath, MAX_PATH - 1);
    dirBuf[MAX_PATH - 1] = L'\0';
    bool hasValidDir = false;
    wchar_t *sl = wcsrchr(dirBuf, L'\\');
    if (sl) {
        if (sl == dirBuf + 2 && dirBuf[1] == L':') sl[1] = L'\0';
        else *sl = L'\0';
        hasValidDir = is_valid_directory(dirBuf);
    }

    bool sameDir = (hasValidDir && _wcsicmp(dirBuf, g_app.browseDir) == 0);
    if (g_app.inExplorerMode && sameDir) {
        g_app.expLastTopIndex = g_app.hwndExpList ? ListView_GetTopIndex(g_app.hwndExpList) : 0;
        g_app.expFallbackIndex = g_app.hwndExpList
                                     ? ListView_GetNextItem(g_app.hwndExpList, -1, LVNI_SELECTED)
                                     : -1;
        if (g_app.hwndFilterEdit) {
            GetWindowTextW(g_app.hwndFilterEdit, g_app.expSavedFilter, 128);
        } else {
            g_app.expSavedFilter[0] = L'\0';
        }
    } else {
        g_app.expLastTopIndex = 0;
        g_app.expFallbackIndex = -1;
        g_app.expSavedFilter[0] = L'\0';
    }

    FreeExplorerPreview();
    FreeLoadedBuffer();
    g_app.rawBuffer = buffer;
    g_app.rawSize = bytesRead;
    wcsncpy(g_app.loadedPath, resolvedPath, MAX_PATH - 1);
    g_app.loadedPath[MAX_PATH - 1] = L'\0';
    wcsncpy(g_app.expLastPath, resolvedPath, MAX_PATH - 1);
    g_app.expLastPath[MAX_PATH - 1] = L'\0';

    /* Remember containing folder for File Explorer */
    if (hasValidDir) {
        wcsncpy(g_app.browseDir, dirBuf, MAX_PATH - 1);
        g_app.browseDir[MAX_PATH - 1] = L'\0';
        SaveIniString(INI_SEC_SETTINGS, L"LastBrowseDir", g_app.browseDir);
        if (g_app.hwndExpPath) SetWindowTextW(g_app.hwndExpPath, g_app.browseDir);
        PopulateDriveCombo();
    }

    AddRecentFile(g_app.loadedPath);
    RebuildMenuBar(g_app.hwndMain);

    g_app.inExplorerMode = false;
    g_app.selectedIndex = -1;
    g_app.sortCol = -1;
    g_app.suppressFilterChange = true;
    SetWindowTextW(g_app.hwndFilterEdit, L"");
    g_app.suppressFilterChange = false;
    oric_analyze_buffer(g_app.rawBuffer, g_app.rawSize,
                        g_app.loadedPath, g_app.lang, &g_app.report);

    SyncViewModeVisibility();
    SyncTabVisibility();
    LayoutChildren(g_app.hwndMain);
    PopulateListView();
    UpdateSidebarAll();
    UpdateStatusBar();

    wchar_t titleBuf[360];
    swprintf(titleBuf, 360, oric_tr(g_app.lang, STR_APP_TITLE_FILE_FMT), g_app.report.fileName);
    SetWindowTextW(g_app.hwndMain, titleBuf);
    SetFocus(g_app.hwndList);
}

static size_t append_filter_pair(wchar_t *buf, size_t pos, size_t maxChars,
                                 const wchar_t *label, const wchar_t *pattern) {
    size_t l1 = wcslen(label) + 1;
    size_t l2 = wcslen(pattern) + 1;
    if (pos + l1 + l2 + 1 >= maxChars) return pos;
    memcpy(buf + pos, label, l1 * sizeof(wchar_t));
    pos += l1;
    memcpy(buf + pos, pattern, l2 * sizeof(wchar_t));
    pos += l2;
    buf[pos] = L'\0';
    return pos;
}

static void OpenFileViaDialog(HWND hwnd) {
    wchar_t filePath[MAX_PATH] = {0};
    wchar_t filterBuf[512] = {0};
    size_t fpos = 0;
    fpos = append_filter_pair(filterBuf, fpos, 512, oric_tr(g_app.lang, STR_DLG_FILTER_ALL_ORIC), L"*.tap;*.dsk;*.rom;*.bin");
    fpos = append_filter_pair(filterBuf, fpos, 512, oric_tr(g_app.lang, STR_DLG_FILTER_TAP),      L"*.tap");
    fpos = append_filter_pair(filterBuf, fpos, 512, oric_tr(g_app.lang, STR_DLG_FILTER_DSK),      L"*.dsk");
    fpos = append_filter_pair(filterBuf, fpos, 512, oric_tr(g_app.lang, STR_DLG_FILTER_ROM),      L"*.rom;*.bin");
    append_filter_pair(filterBuf, fpos, 512,        oric_tr(g_app.lang, STR_DLG_FILTER_ALL),      L"*.*");

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filterBuf;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = g_app.browseDir;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = oric_tr(g_app.lang, STR_DLG_OPEN_TITLE);

    if (GetOpenFileNameW(&ofn)) {
        LoadOricFile(filePath);
    }
}

static bool WriteBytesToFile(HWND hwnd, const wchar_t *path, const uint8_t *data, size_t size) {
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_WRITE_MSG),
                    oric_tr(g_app.lang, STR_ERR_MEM_TITLE), MB_OK | MB_ICONERROR);
        return false;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, (DWORD)size, &written, NULL);
    CloseHandle(hFile);
    if (!ok || written != (DWORD)size) {
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_WRITE_MSG),
                    oric_tr(g_app.lang, STR_ERR_MEM_TITLE), MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

static void SanitizeSuggestedName(const wchar_t *src, const wchar_t *ext, wchar_t *dst, size_t dstChars) {
    size_t p = 0;
    for (size_t i = 0; src && src[i] != L'\0' && p + 10 < dstChars; i++) {
        wchar_t c = src[i];
        if (c == L'\\' || c == L'/' || c == L':' || c == L'*' ||
            c == L'?'  || c == L'"' || c == L'<' || c == L'>' || c == L'|') {
            c = L'_';
        }
        dst[p++] = c;
    }
    if (p == 0) {
        wcscpy(dst, L"oric_export");
        p = wcslen(dst);
    }
    swprintf(dst + p, dstChars - p, L"%ls", ext);
}

static void ExportSelectedMedia(HWND hwnd, int mode /* 0=BIN, 1=TAP, 2=WAV */) {
    if (g_app.inExplorerMode || !g_app.rawBuffer || g_app.report.mediaType == ORIC_MEDIA_NONE) return;

    uint8_t *payload = NULL;
    size_t payloadSize = 0;
    const OricFileEntry *entry = NULL;

    if (g_app.selectedIndex >= 0 && (size_t)g_app.selectedIndex < g_app.report.entryCount) {
        entry = &g_app.report.entries[g_app.selectedIndex];
        oric_extract_payload(g_app.rawBuffer, g_app.rawSize, &g_app.report,
                             (size_t)g_app.selectedIndex, &payload, &payloadSize);
    } else if (mode == 0 && g_app.report.mediaType == ORIC_MEDIA_ROM) {
        payload = (uint8_t *)malloc(g_app.rawSize);
        if (payload) {
            memcpy(payload, g_app.rawBuffer, g_app.rawSize);
            payloadSize = g_app.rawSize;
        }
    }

    if (!payload || payloadSize == 0) {
        free(payload);
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_NO_PAYLOAD),
                    oric_tr(g_app.lang, STR_ERR_MEM_TITLE), MB_OK | MB_ICONINFORMATION);
        return;
    }

    uint8_t *finalData = payload;
    size_t finalSize = payloadSize;

    if (mode == 1 || mode == 2) {
        uint8_t *tapBuf = NULL;
        size_t tapSize = 0;
        if (!oric_build_tap_buffer(entry, payload, payloadSize, &tapBuf, &tapSize)) {
            free(payload);
            return;
        }
        free(payload);
        if (mode == 1) {
            finalData = tapBuf;
            finalSize = tapSize;
        } else {
            uint8_t *wavBuf = NULL;
            size_t wavSize = 0;
            bool ok = oric_build_wav_from_tap(tapBuf, tapSize, &wavBuf, &wavSize);
            free(tapBuf);
            if (!ok) return;
            finalData = wavBuf;
            finalSize = wavSize;
        }
    }

    const wchar_t *defExt = (mode == 0) ? L"bin" : ((mode == 1) ? L"tap" : L"wav");
    const wchar_t *dotExt = (mode == 0) ? L".bin" : ((mode == 1) ? L".tap" : L".wav");
    OricStrId titleId = (mode == 0) ? STR_DLG_SAVE_BIN_TITLE
                                    : ((mode == 1) ? STR_DLG_SAVE_TAP_TITLE : STR_DLG_SAVE_WAV_TITLE);
    OricStrId filtId  = (mode == 0) ? STR_DLG_FILTER_BIN
                                    : ((mode == 1) ? STR_DLG_FILTER_TAP : STR_DLG_FILTER_WAV);
    const wchar_t *pat = (mode == 0) ? L"*.bin" : ((mode == 1) ? L"*.tap" : L"*.wav");

    wchar_t savePath[MAX_PATH] = {0};
    SanitizeSuggestedName(entry ? entry->name : g_app.report.fileName, dotExt, savePath, MAX_PATH);

    wchar_t filterBuf[256] = {0};
    size_t fp = append_filter_pair(filterBuf, 0, 256, oric_tr(g_app.lang, filtId), pat);
    append_filter_pair(filterBuf, fp, 256, oric_tr(g_app.lang, STR_DLG_FILTER_ALL), L"*.*");

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filterBuf;
    ofn.lpstrFile = savePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = oric_tr(g_app.lang, titleId);

    if (GetSaveFileNameW(&ofn)) {
        if (WriteBytesToFile(hwnd, savePath, finalData, finalSize)) {
            const wchar_t *slash = wcsrchr(savePath, L'\\');
            const wchar_t *base = slash ? (slash + 1) : savePath;
            wchar_t st[256];
            swprintf(st, 256, oric_tr(g_app.lang, STR_STATUS_EXTRACTED_FMT), base, finalSize);
            SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0, (LPARAM)st);
        }
    }

    free(finalData);
}

static void ExportVisualAsPng(HWND hwnd) {
    uint32_t *tmpPixels = NULL;
    const uint32_t *srcPixels = NULL;
    int imgW = 0;
    int imgH = 0;
    int selIdx = -1;
    const OricMediaReport *rep = GetActiveMediaReport(NULL, NULL, &selIdx);
    const wchar_t *baseName = rep ? rep->fileName : L"oric_export";

    if (g_app.screenPixels && g_app.screenW > 0 && g_app.screenH > 0) {
        srcPixels = g_app.screenPixels;
        imgW = g_app.screenW;
        imgH = g_app.screenH;
        if (!g_app.inExplorerMode && rep && selIdx >= 0 && (size_t)selIdx < rep->entryCount) {
            baseName = rep->entries[selIdx].name;
        }
    } else if (g_app.hasDiskMap && rep && rep->mediaType == ORIC_MEDIA_DSK) {
        RenderDiskMapToBgra(&tmpPixels, &imgW, &imgH);
        srcPixels = tmpPixels;
    }

    if (!srcPixels || imgW <= 0 || imgH <= 0) {
        free(tmpPixels);
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_NO_IMAGE),
                    oric_tr(g_app.lang, STR_ERR_MEM_TITLE), MB_OK | MB_ICONINFORMATION);
        return;
    }

    uint8_t *pngBuf = NULL;
    size_t pngSize = 0;
    bool built = oric_build_png_from_bgra(srcPixels, imgW, imgH, &pngBuf, &pngSize);
    free(tmpPixels);

    if (!built || !pngBuf || pngSize == 0) {
        free(pngBuf);
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_MEM_MSG),
                    oric_tr(g_app.lang, STR_ERR_MEM_TITLE), MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t savePath[MAX_PATH] = {0};
    SanitizeSuggestedName(baseName, L".png", savePath, MAX_PATH);

    wchar_t filterBuf[256] = {0};
    size_t fp = append_filter_pair(filterBuf, 0, 256, oric_tr(g_app.lang, STR_DLG_FILTER_PNG), L"*.png");
    append_filter_pair(filterBuf, fp, 256, oric_tr(g_app.lang, STR_DLG_FILTER_ALL), L"*.*");

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filterBuf;
    ofn.lpstrFile = savePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = oric_tr(g_app.lang, STR_DLG_SAVE_PNG_TITLE);

    if (GetSaveFileNameW(&ofn)) {
        if (WriteBytesToFile(hwnd, savePath, pngBuf, pngSize)) {
            const wchar_t *slash = wcsrchr(savePath, L'\\');
            const wchar_t *base = slash ? (slash + 1) : savePath;
            wchar_t st[256];
            swprintf(st, 256, oric_tr(g_app.lang, STR_STATUS_EXTRACTED_FMT), base, pngSize);
            SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0, (LPARAM)st);
        }
    }

    free(pngBuf);
}

/* -------------------- Emulator Configuration Modal & Launcher -------------------- */

static bool BrowseForExe(HWND hwndOwner, wchar_t *inOutPath, size_t maxChars) {
    wchar_t exePath[MAX_PATH] = {0};
    if (inOutPath && inOutPath[0] != L'\0') {
        wcsncpy(exePath, inOutPath, MAX_PATH - 1);
    }

    wchar_t filterBuf[256] = {0};
    size_t fp = append_filter_pair(filterBuf, 0, 256, oric_tr(g_app.lang, STR_DLG_FILTER_EXE), L"*.exe");
    append_filter_pair(filterBuf, fp, 256, oric_tr(g_app.lang, STR_DLG_FILTER_ALL), L"*.*");

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFilter = filterBuf;
    ofn.lpstrFile = exePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = oric_tr(g_app.lang, STR_DLG_EXE_TITLE);

    if (GetOpenFileNameW(&ofn)) {
        wcsncpy(inOutPath, exePath, maxChars - 1);
        inOutPath[maxChars - 1] = L'\0';
        return true;
    }
    return false;
}

static LRESULT CALLBACK EmuDlgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    EmuDlgState *st = (EmuDlgState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        st = (EmuDlgState *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);
        st->hwndDlg = hwnd;

        HINSTANCE hInst = GetModuleHandleW(NULL);
        int pad = ScaleDpi(hwnd, 14);
        int w = ScaleDpi(hwnd, 520);
        int fullW = w - 2 * pad;
        int browseW = ScaleDpi(hwnd, 96);
        int editPathW = fullW - browseW - ScaleDpi(hwnd, 8);
        int lblArgsW = ScaleDpi(hwnd, 145);
        int rowH = ScaleDpi(hwnd, 24);
        int lblH = ScaleDpi(hwnd, 18);
        int y = pad;

        /* .TAP section */
        HWND hLblTap = CreateWindowExW(0, L"STATIC", oric_tr(g_app.lang, STR_EMU_TAP_LABEL),
            WS_CHILD | WS_VISIBLE | SS_LEFT, pad, y, fullW, lblH, hwnd, NULL, hInst, NULL);
        SendMessageW(hLblTap, WM_SETFONT, (WPARAM)g_app.hFontUiBold, TRUE);
        y += lblH + ScaleDpi(hwnd, 4);

        st->hwndTapEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_app.tapEmuPath,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            pad, y, editPathW, rowH, hwnd, (HMENU)(UINT_PTR)IDC_EMU_TAP_EDIT, hInst, NULL);
        SendMessageW(st->hwndTapEdit, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        HWND hBtnTap = CreateWindowExW(0, L"BUTTON", oric_tr(g_app.lang, STR_EMU_BROWSE),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            pad + editPathW + ScaleDpi(hwnd, 8), y, browseW, rowH, hwnd,
            (HMENU)(UINT_PTR)IDC_EMU_TAP_BROWSE, hInst, NULL);
        SendMessageW(hBtnTap, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);
        y += rowH + ScaleDpi(hwnd, 6);

        HWND hLblTapArgs = CreateWindowExW(0, L"STATIC", oric_tr(g_app.lang, STR_EMU_ARGS_LABEL),
            WS_CHILD | WS_VISIBLE | SS_LEFT, pad, y + ScaleDpi(hwnd, 3), lblArgsW, lblH, hwnd, NULL, hInst, NULL);
        SendMessageW(hLblTapArgs, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        st->hwndTapArgs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_app.tapEmuArgs,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            pad + lblArgsW + ScaleDpi(hwnd, 6), y, fullW - lblArgsW - ScaleDpi(hwnd, 6), rowH,
            hwnd, (HMENU)(UINT_PTR)IDC_EMU_TAP_ARGS, hInst, NULL);
        SendMessageW(st->hwndTapArgs, WM_SETFONT, (WPARAM)g_app.hFontMono, TRUE);
        y += rowH + ScaleDpi(hwnd, 14);

        /* .DSK section */
        HWND hLblDsk = CreateWindowExW(0, L"STATIC", oric_tr(g_app.lang, STR_EMU_DSK_LABEL),
            WS_CHILD | WS_VISIBLE | SS_LEFT, pad, y, fullW, lblH, hwnd, NULL, hInst, NULL);
        SendMessageW(hLblDsk, WM_SETFONT, (WPARAM)g_app.hFontUiBold, TRUE);
        y += lblH + ScaleDpi(hwnd, 4);

        st->hwndDskEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_app.dskEmuPath,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            pad, y, editPathW, rowH, hwnd, (HMENU)(UINT_PTR)IDC_EMU_DSK_EDIT, hInst, NULL);
        SendMessageW(st->hwndDskEdit, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        HWND hBtnDsk = CreateWindowExW(0, L"BUTTON", oric_tr(g_app.lang, STR_EMU_BROWSE),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            pad + editPathW + ScaleDpi(hwnd, 8), y, browseW, rowH, hwnd,
            (HMENU)(UINT_PTR)IDC_EMU_DSK_BROWSE, hInst, NULL);
        SendMessageW(hBtnDsk, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);
        y += rowH + ScaleDpi(hwnd, 6);

        HWND hLblDskArgs = CreateWindowExW(0, L"STATIC", oric_tr(g_app.lang, STR_EMU_ARGS_LABEL),
            WS_CHILD | WS_VISIBLE | SS_LEFT, pad, y + ScaleDpi(hwnd, 3), lblArgsW, lblH, hwnd, NULL, hInst, NULL);
        SendMessageW(hLblDskArgs, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        st->hwndDskArgs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_app.dskEmuArgs,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            pad + lblArgsW + ScaleDpi(hwnd, 6), y, fullW - lblArgsW - ScaleDpi(hwnd, 6), rowH,
            hwnd, (HMENU)(UINT_PTR)IDC_EMU_DSK_ARGS, hInst, NULL);
        SendMessageW(st->hwndDskArgs, WM_SETFONT, (WPARAM)g_app.hFontMono, TRUE);
        y += rowH + ScaleDpi(hwnd, 12);

        HWND hHint = CreateWindowExW(0, L"STATIC", oric_tr(g_app.lang, STR_EMU_HINT),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            pad, y, fullW, ScaleDpi(hwnd, 34), hwnd, NULL, hInst, NULL);
        SendMessageW(hHint, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);
        y += ScaleDpi(hwnd, 40);

        int btnW = ScaleDpi(hwnd, 110);
        int btnH = ScaleDpi(hwnd, 30);
        HWND hOk = CreateWindowExW(0, L"BUTTON", oric_tr(g_app.lang, STR_EMU_OK),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            w - pad - 2 * btnW - ScaleDpi(hwnd, 8), y, btnW, btnH,
            hwnd, (HMENU)(UINT_PTR)IDC_EMU_OK, hInst, NULL);
        SendMessageW(hOk, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        HWND hCancel = CreateWindowExW(0, L"BUTTON", oric_tr(g_app.lang, STR_EMU_CANCEL),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            w - pad - btnW, y, btnW, btnH,
            hwnd, (HMENU)(UINT_PTR)IDC_EMU_CANCEL, hInst, NULL);
        SendMessageW(hCancel, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);
        return 0;
    }

    case WM_COMMAND:
        if (!st) break;
        switch (LOWORD(wParam)) {
        case IDC_EMU_TAP_BROWSE: {
            wchar_t tmp[MAX_PATH] = {0};
            GetWindowTextW(st->hwndTapEdit, tmp, MAX_PATH);
            if (BrowseForExe(hwnd, tmp, MAX_PATH)) {
                SetWindowTextW(st->hwndTapEdit, tmp);
            }
            return 0;
        }
        case IDC_EMU_DSK_BROWSE: {
            wchar_t tmp[MAX_PATH] = {0};
            GetWindowTextW(st->hwndDskEdit, tmp, MAX_PATH);
            if (BrowseForExe(hwnd, tmp, MAX_PATH)) {
                SetWindowTextW(st->hwndDskEdit, tmp);
            }
            return 0;
        }
        case IDC_EMU_OK:
            GetWindowTextW(st->hwndTapEdit, g_app.tapEmuPath, MAX_PATH);
            GetWindowTextW(st->hwndTapArgs, g_app.tapEmuArgs, MAX_PATH);
            GetWindowTextW(st->hwndDskEdit, g_app.dskEmuPath, MAX_PATH);
            GetWindowTextW(st->hwndDskArgs, g_app.dskEmuArgs, MAX_PATH);
            if (g_app.tapEmuArgs[0] == L'\0') wcscpy(g_app.tapEmuArgs, L"\"%s\"");
            if (g_app.dskEmuArgs[0] == L'\0') wcscpy(g_app.dskEmuArgs, L"\"%s\"");
            SaveEmulatorSettingsToIni();
            st->saved = true;
            st->done = true;
            return 0;
        case IDC_EMU_CANCEL:
            st->saved = false;
            st->done = true;
            return 0;
        }
        break;

    case WM_CLOSE:
        if (st) {
            st->saved = false;
            st->done = true;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool OpenEmulatorConfigDialog(HWND hwndParent) {
    HINSTANCE hInst = GetModuleHandleW(NULL);
    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = EmuDlgWndProc;
        wc.hInstance = hInst;
        wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = EMU_DLG_CLASS_NAME;
        RegisterClassExW(&wc);
        s_registered = true;
    }

    int clientW = ScaleDpi(hwndParent, 520);
    int clientH = ScaleDpi(hwndParent, 290);
    RECT adj = { 0, 0, clientW, clientH };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&adj, style, FALSE);
    int winW = adj.right - adj.left;
    int winH = adj.bottom - adj.top;

    RECT prc;
    GetWindowRect(hwndParent, &prc);
    int posX = prc.left + ((prc.right - prc.left) - winW) / 2;
    int posY = prc.top + ((prc.bottom - prc.top) - winH) / 2;

    EmuDlgState st;
    memset(&st, 0, sizeof(st));
    st.hwndParent = hwndParent;

    EnableWindow(hwndParent, FALSE);
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        EMU_DLG_CLASS_NAME,
        oric_tr(g_app.lang, STR_EMU_DLG_TITLE),
        style,
        posX, posY, winW, winH,
        hwndParent, NULL, hInst, &st);

    if (!hDlg) {
        EnableWindow(hwndParent, TRUE);
        return false;
    }

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    SetFocus(st.hwndTapEdit);

    MSG msg;
    while (!st.done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                st.saved = false;
                st.done = true;
                break;
            }
            if (msg.wParam == VK_RETURN) {
                HWND focus = GetFocus();
                if (focus != GetDlgItem(hDlg, IDC_EMU_TAP_BROWSE) &&
                    focus != GetDlgItem(hDlg, IDC_EMU_DSK_BROWSE) &&
                    focus != GetDlgItem(hDlg, IDC_EMU_CANCEL)) {
                    SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDC_EMU_OK, BN_CLICKED), 0);
                    break;
                }
            }
        }
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(hwndParent, TRUE);
    SetForegroundWindow(hwndParent);
    DestroyWindow(hDlg);

    if (st.saved && g_app.hwndStatus) {
        SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0,
                     (LPARAM)oric_tr(g_app.lang, STR_STATUS_EMU_SAVED));
    }
    return st.saved;
}

static void ExpandEmulatorArgs(const wchar_t *argsTemplate, const wchar_t *mediaPath,
                               wchar_t *outArgs, size_t outChars) {
    const wchar_t *tpl = (argsTemplate && argsTemplate[0] != L'\0') ? argsTemplate : L"\"%s\"";
    const wchar_t *ph = wcsstr(tpl, L"%s");
    if (!ph) {
        swprintf(outArgs, outChars, L"%ls \"%ls\"", tpl, mediaPath);
        return;
    }

    size_t pos = 0;
    size_t prefixLen = (size_t)(ph - tpl);
    for (size_t i = 0; i < prefixLen && pos + 1 < outChars; i++) {
        outArgs[pos++] = tpl[i];
    }
    size_t mLen = wcslen(mediaPath);
    for (size_t i = 0; i < mLen && pos + 1 < outChars; i++) {
        outArgs[pos++] = mediaPath[i];
    }
    const wchar_t *suffix = ph + 2;
    while (*suffix != L'\0' && pos + 1 < outChars) {
        outArgs[pos++] = *suffix++;
    }
    outArgs[pos] = L'\0';
}

static void LaunchEmulatorForCurrentMedia(HWND hwnd) {
    wchar_t targetPath[MAX_PATH] = {0};
    bool isTap = false;

    if (!g_app.inExplorerMode && g_app.loadedPath[0] != L'\0') {
        wcsncpy(targetPath, g_app.loadedPath, MAX_PATH - 1);
        isTap = (g_app.report.mediaType == ORIC_MEDIA_TAP);
    } else if (HasSelectedExplorerMediaFile(targetPath, MAX_PATH)) {
        const wchar_t *dot = wcsrchr(targetPath, L'.');
        isTap = (dot && _wcsicmp(dot, L".tap") == 0);
    } else {
        return;
    }

    const wchar_t *dot = wcsrchr(targetPath, L'.');
    bool isDsk = (!isTap && dot && _wcsicmp(dot, L".dsk") == 0);

    const wchar_t *emuExe = NULL;
    const wchar_t *emuArgs = NULL;

    if (isTap) {
        emuExe = g_app.tapEmuPath;
        emuArgs = g_app.tapEmuArgs;
    } else if (isDsk) {
        emuExe = g_app.dskEmuPath;
        emuArgs = g_app.dskEmuArgs;
    } else {
        /* Fallback for ROMs: prefer DSK emulator else TAP emulator */
        if (g_app.dskEmuPath[0] != L'\0') {
            emuExe = g_app.dskEmuPath;
            emuArgs = g_app.dskEmuArgs;
        } else {
            emuExe = g_app.tapEmuPath;
            emuArgs = g_app.tapEmuArgs;
        }
    }

    DWORD attrs = (emuExe && emuExe[0] != L'\0') ? GetFileAttributesW(emuExe) : INVALID_FILE_ATTRIBUTES;
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_EMU_MISSING),
                    oric_tr(g_app.lang, STR_ERR_EMU_TITLE), MB_OK | MB_ICONINFORMATION);
        if (!OpenEmulatorConfigDialog(hwnd)) return;

        emuExe = isTap ? g_app.tapEmuPath : (isDsk ? g_app.dskEmuPath :
                 ((g_app.dskEmuPath[0] != L'\0') ? g_app.dskEmuPath : g_app.tapEmuPath));
        emuArgs = isTap ? g_app.tapEmuArgs : (isDsk ? g_app.dskEmuArgs :
                  ((g_app.dskEmuPath[0] != L'\0') ? g_app.dskEmuArgs : g_app.tapEmuArgs));
        attrs = (emuExe && emuExe[0] != L'\0') ? GetFileAttributesW(emuExe) : INVALID_FILE_ATTRIBUTES;
        if (attrs == INVALID_FILE_ATTRIBUTES) return;
    }

    wchar_t params[MAX_PATH * 2 + 16];
    ExpandEmulatorArgs(emuArgs, targetPath, params, MAX_PATH * 2 + 16);

    wchar_t workDir[MAX_PATH];
    wcsncpy(workDir, emuExe, MAX_PATH - 1);
    workDir[MAX_PATH - 1] = L'\0';
    wchar_t *slash = wcsrchr(workDir, L'\\');
    if (slash) *slash = L'\0';

    HINSTANCE res = ShellExecuteW(hwnd, L"open", emuExe, params,
                                  slash ? workDir : NULL, SW_SHOWNORMAL);
    if ((INT_PTR)res <= 32) {
        MessageBoxW(hwnd, oric_tr(g_app.lang, STR_ERR_EMU_LAUNCH),
                    oric_tr(g_app.lang, STR_ERR_EMU_TITLE), MB_OK | MB_ICONERROR);
    } else {
        SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0,
                     (LPARAM)oric_tr(g_app.lang, STR_STATUS_EMU_OK));
    }
}

static void CopyActiveTextToClipboard(HWND hwnd) {
    if (!g_app.inExplorerMode && g_app.report.mediaType == ORIC_MEDIA_NONE) return;

    HWND targetEdit = (g_app.activeTab == 1) ? g_app.hwndCodeEdit : g_app.hwndSideEdit;
    int lenChars = GetWindowTextLengthW(targetEdit);
    if (lenChars <= 0) return;

    wchar_t *buf = (wchar_t *)malloc(((size_t)lenChars + 1u) * sizeof(wchar_t));
    if (!buf) return;
    GetWindowTextW(targetEdit, buf, lenChars + 1);

    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, ((size_t)lenChars + 1u) * sizeof(wchar_t));
        if (hMem) {
            wchar_t *locked = (wchar_t *)GlobalLock(hMem);
            if (locked) {
                memcpy(locked, buf, ((size_t)lenChars + 1u) * sizeof(wchar_t));
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            } else {
                GlobalFree(hMem);
            }
        }
        CloseClipboard();
        SendMessageW(g_app.hwndStatus, SB_SETTEXTW, 0,
                     (LPARAM)oric_tr(g_app.lang, STR_STATUS_COPIED));
    }
    free(buf);
}

/* -------------------- Layout & Context Menu -------------------- */

static void LayoutChildren(HWND hwnd) {
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    SendMessageW(g_app.hwndStatus, WM_SIZE, 0, 0);
    RECT rcStatus;
    GetWindowRect(g_app.hwndStatus, &rcStatus);
    int statusH = rcStatus.bottom - rcStatus.top;

    int totalW = rcClient.right - rcClient.left;
    int parts[3] = { (int)(totalW * 0.50), (int)(totalW * 0.76), -1 };
    SendMessageW(g_app.hwndStatus, SB_SETPARTS, 3, (LPARAM)parts);

    int margin = ScaleDpi(hwnd, 8);
    int gap    = ScaleDpi(hwnd, 10);
    int titleH = ScaleDpi(hwnd, 22);
    int rowH   = ScaleDpi(hwnd, 24);
    int tabsH  = ScaleDpi(hwnd, 28);
    int btnH   = ScaleDpi(hwnd, 30);

    int availW = totalW - 2 * margin;
    int availH = (rcClient.bottom - rcClient.top) - statusH - 2 * margin;
    if (availW < 120 || availH < 120) return;

    double ratio = g_app.splitRatio;
    if (ratio < 0.15 || ratio > 0.80) ratio = 0.40;
    int sideW = (int)(availW * ratio);
    int minSideW = ScaleDpi(hwnd, 260);
    int minLeftW = ScaleDpi(hwnd, 260);
    if (sideW < minSideW) sideW = minSideW;
    if (sideW > availW - gap - minLeftW) sideW = availW - gap - minLeftW;
    if (sideW < 100) sideW = availW / 2;

    int listW = availW - sideW - gap;
    int sideX = margin + listW + gap;

    g_app.splitRect.left   = margin + listW;
    g_app.splitRect.top    = margin;
    g_app.splitRect.right  = sideX;
    g_app.splitRect.bottom = margin + availH;

    if (g_app.inExplorerMode) {
        /* Top bar 1: Drive combo + Path edit + Up button */
        int drvLblW = ScaleDpi(hwnd, 56);
        int drvCmbW = ScaleDpi(hwnd, 66);
        int pathLblW = ScaleDpi(hwnd, 54);
        int upBtnW = ScaleDpi(hwnd, 76);
        int padS = ScaleDpi(hwnd, 4);
        int curX = margin;

        MoveWindow(g_app.hwndExpDriveLbl, curX, margin + ScaleDpi(hwnd, 3), drvLblW, rowH - 4, TRUE);
        curX += drvLblW + padS;
        MoveWindow(g_app.hwndExpDrive, curX, margin, drvCmbW, ScaleDpi(hwnd, 180), TRUE);
        curX += drvCmbW + ScaleDpi(hwnd, 8);
        MoveWindow(g_app.hwndExpPathLbl, curX, margin + ScaleDpi(hwnd, 3), pathLblW, rowH - 4, TRUE);
        curX += pathLblW + padS;
        int pathEditW = (margin + listW) - curX - upBtnW - padS;
        if (pathEditW < 60) pathEditW = 60;
        MoveWindow(g_app.hwndExpPath, curX, margin, pathEditW, rowH, TRUE);
        MoveWindow(g_app.hwndExpUp, curX + pathEditW + padS, margin, upBtnW, rowH, TRUE);

        /* Top bar 2: Filter bar */
        int bar2Y = margin + rowH + ScaleDpi(hwnd, 6);
        int filtLblW = ScaleDpi(hwnd, 56);
        MoveWindow(g_app.hwndFilterLabel, margin, bar2Y + ScaleDpi(hwnd, 3), filtLblW, rowH - 4, TRUE);
        MoveWindow(g_app.hwndFilterEdit, margin + filtLblW + padS, bar2Y,
                   listW - filtLblW - padS, rowH, TRUE);

        int listY = bar2Y + rowH + ScaleDpi(hwnd, 6);
        MoveWindow(g_app.hwndExpList, margin, listY, listW, (margin + availH) - listY, TRUE);
    } else {
        /* Analyzer top bar: [< Explorer (Esc)] + Filter: + Edit */
        int backBtnW = ScaleDpi(hwnd, 132);
        int filtLblW = ScaleDpi(hwnd, 52);
        int padS = ScaleDpi(hwnd, 6);

        MoveWindow(g_app.hwndBackBtn, margin, margin, backBtnW, rowH, TRUE);
        int fx = margin + backBtnW + padS;
        MoveWindow(g_app.hwndFilterLabel, fx, margin + ScaleDpi(hwnd, 3), filtLblW, rowH - 4, TRUE);
        fx += filtLblW + ScaleDpi(hwnd, 4);
        MoveWindow(g_app.hwndFilterEdit, fx, margin, (margin + listW) - fx, rowH, TRUE);

        int listY = margin + rowH + ScaleDpi(hwnd, 6);
        MoveWindow(g_app.hwndList, margin, listY, listW, (margin + availH) - listY, TRUE);
    }

    /* Right pane: Title + TabControl + Content + Bottom buttons */
    MoveWindow(g_app.hwndSideTitle, sideX, margin, sideW, titleH, TRUE);

    int tabY = margin + titleH + ScaleDpi(hwnd, 4);
    MoveWindow(g_app.hwndTabs, sideX, tabY, sideW, tabsH, TRUE);

    int contentY = tabY + tabsH + ScaleDpi(hwnd, 2);
    int contentH = (margin + availH) - contentY - btnH - ScaleDpi(hwnd, 8);
    if (contentH < 40) contentH = 40;

    MoveWindow(g_app.hwndSideEdit,     sideX, contentY, sideW, contentH, TRUE);
    MoveWindow(g_app.hwndCodeEdit,     sideX, contentY, sideW, contentH, TRUE);
    MoveWindow(g_app.hwndVisualCanvas, sideX, contentY, sideW, contentH, TRUE);

    int btnY = margin + availH - btnH;
    if (g_app.activeTab == 1) {
        int halfW = (sideW - ScaleDpi(hwnd, 6)) / 2;
        MoveWindow(g_app.hwndCodeToggleBtn, sideX, btnY, halfW, btnH, TRUE);
        MoveWindow(g_app.hwndCopyBtn, sideX + halfW + ScaleDpi(hwnd, 6), btnY, sideW - halfW - ScaleDpi(hwnd, 6), btnH, TRUE);
    } else if (g_app.activeTab == 2) {
        MoveWindow(g_app.hwndSavePngBtn, sideX, btnY, sideW, btnH, TRUE);
    } else {
        MoveWindow(g_app.hwndCopyBtn, sideX, btnY, sideW, btnH, TRUE);
    }
}

static void ShowListContextMenu(HWND hwnd, POINT ptScreen) {
    if (g_app.inExplorerMode || g_app.report.mediaType == ORIC_MEDIA_NONE) return;

    BOOL hasSel = (g_app.selectedIndex >= 0 && (size_t)g_app.selectedIndex < g_app.report.entryCount);
    BOOL isMediaRow = (hasSel && (g_app.report.mediaType == ORIC_MEDIA_TAP || g_app.report.mediaType == ORIC_MEDIA_DSK));
    BOOL canExtractBin = isMediaRow || (g_app.report.mediaType == ORIC_MEDIA_ROM);
    BOOL hasImage = (g_app.screenPixels != NULL && g_app.screenW > 0 && g_app.screenH > 0) || g_app.hasDiskMap;

    HMENU hPopup = CreatePopupMenu();
    AppendMenuW(hPopup, MF_STRING, IDM_CTX_VIEW_CODE,   oric_tr(g_app.lang, STR_CTX_VIEW_CODE));
    AppendMenuW(hPopup, MF_STRING, IDM_CTX_VIEW_SCREEN, oric_tr(g_app.lang, STR_CTX_VIEW_SCREEN));
    AppendMenuW(hPopup, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hPopup, MF_STRING | (canExtractBin ? MF_ENABLED : MF_GRAYED),
                IDM_TOOLS_EXTRACT_BIN, oric_tr(g_app.lang, STR_MENU_TOOLS_EXTRACT_BIN));
    AppendMenuW(hPopup, MF_STRING | (isMediaRow ? MF_ENABLED : MF_GRAYED),
                IDM_TOOLS_EXPORT_TAP, oric_tr(g_app.lang, STR_MENU_TOOLS_EXPORT_TAP));
    AppendMenuW(hPopup, MF_STRING | (isMediaRow ? MF_ENABLED : MF_GRAYED),
                IDM_TOOLS_EXPORT_WAV, oric_tr(g_app.lang, STR_MENU_TOOLS_EXPORT_WAV));
    AppendMenuW(hPopup, MF_STRING | (hasImage ? MF_ENABLED : MF_GRAYED),
                IDM_TOOLS_EXPORT_PNG, oric_tr(g_app.lang, STR_MENU_TOOLS_EXPORT_PNG));
    AppendMenuW(hPopup, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hPopup, MF_STRING, IDM_TOOLS_LAUNCH_EMU, oric_tr(g_app.lang, STR_MENU_TOOLS_LAUNCH_EMU));

    TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, hwnd, NULL);
    DestroyMenu(hPopup);
}

static void ShowExplorerContextMenu(HWND hwnd, POINT ptScreen) {
    if (!g_app.inExplorerMode || !g_app.hwndExpList) return;

    int selRow = -1;
    const ExpItem *it = GetSelectedExplorerItem(&selRow);
    BOOL hasSel = (it != NULL);
    BOOL canModify = (it != NULL && !it->isParent);
    BOOL isMediaFile = (it != NULL && !it->isDir && !it->isParent);

    HMENU hPopup = CreatePopupMenu();
    AppendMenuW(hPopup, MF_STRING | (hasSel ? MF_ENABLED : MF_GRAYED),
                IDM_EXP_OPEN, oric_tr(g_app.lang, STR_EXP_CTX_OPEN));
    AppendMenuW(hPopup, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hPopup, MF_STRING | (canModify ? MF_ENABLED : MF_GRAYED),
                IDM_EXP_RENAME, oric_tr(g_app.lang, STR_EXP_CTX_RENAME));
    AppendMenuW(hPopup, MF_STRING | (canModify ? MF_ENABLED : MF_GRAYED),
                IDM_EXP_DELETE, oric_tr(g_app.lang, STR_EXP_CTX_DELETE));
    if (isMediaFile) {
        AppendMenuW(hPopup, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hPopup, MF_STRING, IDM_TOOLS_LAUNCH_EMU,
                    oric_tr(g_app.lang, STR_MENU_TOOLS_LAUNCH_EMU));
    }

    TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, hwnd, NULL);
    DestroyMenu(hPopup);
}

static void ShowAboutDialog(HWND hwnd) {
    MessageBoxW(hwnd,
                oric_tr(g_app.lang, STR_ABOUT_TEXT),
                oric_tr(g_app.lang, STR_ABOUT_TITLE),
                MB_OK | MB_ICONINFORMATION);
}

/* -------------------- Window Procedure -------------------- */

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_app.hwndMain = hwnd;
        oric_report_init(&g_app.report);
        oric_report_init(&g_app.expPreviewReport);
        g_app.expPreviewEntryIdx = -1;
        g_app.inExplorerMode = true;
        g_app.selectedIndex = -1;
        g_app.activeTab = 0;
        g_app.sortCol = -1;
        g_app.sortAsc = true;

        CreateAppFonts(hwnd);
        RebuildMenuBar(hwnd);

        HINSTANCE hInst = GetModuleHandleW(NULL);

        /* Explorer widgets */
        g_app.hwndExpDriveLbl = CreateWindowExW(
            0, L"STATIC", oric_tr(g_app.lang, STR_EXP_DRIVE_LABEL),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 50, 20, hwnd, (HMENU)(UINT_PTR)IDC_EXP_DRIVE_LBL, hInst, NULL);
        SendMessageW(g_app.hwndExpDriveLbl, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndExpDrive = CreateWindowExW(
            0, WC_COMBOBOXW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 66, 180, hwnd, (HMENU)(UINT_PTR)IDC_EXP_DRIVE, hInst, NULL);
        SendMessageW(g_app.hwndExpDrive, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndExpPathLbl = CreateWindowExW(
            0, L"STATIC", oric_tr(g_app.lang, STR_EXP_PATH_LABEL),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 50, 20, hwnd, (HMENU)(UINT_PTR)IDC_EXP_PATH_LBL, hInst, NULL);
        SendMessageW(g_app.hwndExpPathLbl, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndExpPath = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", g_app.browseDir,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 200, 24, hwnd, (HMENU)(UINT_PTR)IDC_EXP_PATH, hInst, NULL);
        SendMessageW(g_app.hwndExpPath, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndExpUp = CreateWindowExW(
            0, L"BUTTON", oric_tr(g_app.lang, STR_BTN_UP_DIR),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 70, 24, hwnd, (HMENU)(UINT_PTR)IDC_EXP_UP, hInst, NULL);
        SendMessageW(g_app.hwndExpUp, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndExpList = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 100, 100, hwnd, (HMENU)(UINT_PTR)IDC_EXP_LIST, hInst, NULL);
        ListView_SetExtendedListViewStyle(
            g_app.hwndExpList,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SendMessageW(g_app.hwndExpList, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);
        SetupExplorerColumns(g_app.hwndExpList, hwnd);

        /* Analyzer widgets */
        g_app.hwndBackBtn = CreateWindowExW(
            0, L"BUTTON", oric_tr(g_app.lang, STR_BTN_BACK_EXPLORER),
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 120, 24, hwnd, (HMENU)(UINT_PTR)IDC_BACK_EXPLORER, hInst, NULL);
        SendMessageW(g_app.hwndBackBtn, WM_SETFONT, (WPARAM)g_app.hFontUiBold, TRUE);

        g_app.hwndFilterLabel = CreateWindowExW(
            0, L"STATIC", oric_tr(g_app.lang, STR_FILTER_LABEL),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 50, 20, hwnd, (HMENU)(UINT_PTR)IDC_FILTER_LABEL, hInst, NULL);
        SendMessageW(g_app.hwndFilterLabel, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndFilterEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 120, 24, hwnd, (HMENU)(UINT_PTR)IDC_FILTER_EDIT, hInst, NULL);
        SendMessageW(g_app.hwndFilterEdit, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndList = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_TABSTOP |
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 100, 100, hwnd, (HMENU)(UINT_PTR)IDC_LISTVIEW, hInst, NULL);
        ListView_SetExtendedListViewStyle(
            g_app.hwndList,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        SendMessageW(g_app.hwndList, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);
        SetupListViewColumns(g_app.hwndList, hwnd);

        /* Sidebar widgets */
        g_app.hwndSideTitle = CreateWindowExW(
            0, L"STATIC", oric_tr(g_app.lang, STR_SIDEBAR_TITLE),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 100, 20, hwnd, (HMENU)(UINT_PTR)IDC_SIDEBAR_TITLE, hInst, NULL);
        SendMessageW(g_app.hwndSideTitle, WM_SETFONT, (WPARAM)g_app.hFontUiBold, TRUE);

        g_app.hwndTabs = CreateWindowExW(
            0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS,
            0, 0, 100, 28, hwnd, (HMENU)(UINT_PTR)IDC_SIDEBAR_TABS, hInst, NULL);
        SendMessageW(g_app.hwndTabs, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        static const OricStrId tabIds[3] = { STR_TAB_SUMMARY, STR_TAB_CODE, STR_TAB_VISUAL };
        for (int i = 0; i < 3; i++) {
            TCITEMW tie;
            memset(&tie, 0, sizeof(tie));
            tie.mask = TCIF_TEXT;
            tie.pszText = (LPWSTR)oric_tr(g_app.lang, tabIds[i]);
            TabCtrl_InsertItem(g_app.hwndTabs, i, &tie);
        }

        g_app.hwndSideEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 100, 100, hwnd, (HMENU)(UINT_PTR)IDC_SIDEBAR_EDIT, hInst, NULL);
        SendMessageW(g_app.hwndSideEdit, WM_SETFONT, (WPARAM)g_app.hFontMono, TRUE);
        SendMessageW(g_app.hwndSideEdit, EM_LIMITTEXT, 0, 0);

        g_app.hwndCodeEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
            0, 0, 100, 100, hwnd, (HMENU)(UINT_PTR)IDC_CODE_EDIT, hInst, NULL);
        SendMessageW(g_app.hwndCodeEdit, WM_SETFONT, (WPARAM)g_app.hFontMono, TRUE);
        SendMessageW(g_app.hwndCodeEdit, EM_LIMITTEXT, 0, 0);

        g_app.hwndVisualCanvas = CreateWindowExW(
            WS_EX_CLIENTEDGE, VISUAL_CLASS_NAME, L"",
            WS_CHILD,
            0, 0, 100, 100, hwnd, (HMENU)(UINT_PTR)IDC_VISUAL_CANVAS, hInst, NULL);

        g_app.hwndCodeToggleBtn = CreateWindowExW(
            0, L"BUTTON", oric_tr(g_app.lang, STR_BTN_MODE_AUTO_HEX),
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)(UINT_PTR)IDC_CODE_TOGGLE, hInst, NULL);
        SendMessageW(g_app.hwndCodeToggleBtn, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndCopyBtn = CreateWindowExW(
            0, L"BUTTON", oric_tr(g_app.lang, STR_BTN_COPY),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)(UINT_PTR)IDC_SIDEBAR_COPY, hInst, NULL);
        SendMessageW(g_app.hwndCopyBtn, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndSavePngBtn = CreateWindowExW(
            0, L"BUTTON", oric_tr(g_app.lang, STR_BTN_SAVE_PNG),
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)(UINT_PTR)IDC_SAVE_PNG, hInst, NULL);
        SendMessageW(g_app.hwndSavePngBtn, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        g_app.hwndStatus = CreateWindowExW(
            0, STATUSCLASSNAMEW, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_STATUSBAR, hInst, NULL);
        SendMessageW(g_app.hwndStatus, WM_SETFONT, (WPARAM)g_app.hFontUi, TRUE);

        DragAcceptFiles(hwnd, TRUE);
        PopulateDriveCombo();
        SyncViewModeVisibility();
        SyncTabVisibility();
        LayoutChildren(hwnd);
        PopulateExplorerList();
        UpdateSidebarAll();
        UpdateStatusBar();
        return 0;
    }

    case WM_SIZE:
        LayoutChildren(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_app.splitRect.right > g_app.splitRect.left) {
            int midX = (g_app.splitRect.left + g_app.splitRect.right) / 2;
            int midY = (g_app.splitRect.top + g_app.splitRect.bottom) / 2;
            HPEN hPenDiv = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPenDiv);
            MoveToEx(hdc, midX, g_app.splitRect.top + 4, NULL);
            LineTo(hdc, midX, g_app.splitRect.bottom - 4);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPenDiv);

            HBRUSH hDot = CreateSolidBrush(GetSysColor(COLOR_BTNTEXT));
            int dotSz = ScaleDpi(hwnd, 2);
            if (dotSz < 2) dotSz = 2;
            for (int k = -2; k <= 2; k++) {
                int dy = midY + k * ScaleDpi(hwnd, 6);
                RECT rd = { midX - dotSz / 2, dy - dotSz / 2,
                            midX - dotSz / 2 + dotSz, dy - dotSz / 2 + dotSz };
                FillRect(hdc, &rd, hDot);
            }
            DeleteObject(hDot);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETCURSOR:
        if ((HWND)wParam == hwnd && LOWORD(lParam) == HTCLIENT) {
            if (g_app.isDraggingSplit) {
                SetCursor(LoadCursorW(NULL, IDC_SIZEWE));
                return TRUE;
            }
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (PtInRect(&g_app.splitRect, pt)) {
                SetCursor(LoadCursorW(NULL, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;

    case WM_LBUTTONDOWN: {
        POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        if (PtInRect(&g_app.splitRect, pt)) {
            g_app.isDraggingSplit = true;
            SetCapture(hwnd);
            SetCursor(LoadCursorW(NULL, IDC_SIZEWE));
            return 0;
        }
        break;
    }

    case WM_LBUTTONDBLCLK: {
        POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        if (PtInRect(&g_app.splitRect, pt)) {
            RECT oldSplit = g_app.splitRect;
            g_app.splitRatio = 0.40;
            LayoutChildren(hwnd);
            InvalidateRect(hwnd, &oldSplit, TRUE);
            InvalidateRect(hwnd, &g_app.splitRect, TRUE);
            SaveIniString(INI_SEC_SETTINGS, L"SplitRatio", L"400");
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE:
        if (g_app.isDraggingSplit) {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int totalW = rcClient.right - rcClient.left;
            int margin = ScaleDpi(hwnd, 8);
            int gap = ScaleDpi(hwnd, 10);
            int availW = totalW - 2 * margin;
            if (availW > 120) {
                int mouseX = (int)(short)LOWORD(lParam);
                int minSideW = ScaleDpi(hwnd, 260);
                int minLeftW = ScaleDpi(hwnd, 260);
                int sideW = (margin + availW) - (mouseX + gap / 2);
                if (sideW < minSideW) sideW = minSideW;
                if (sideW > availW - gap - minLeftW) sideW = availW - gap - minLeftW;
                if (sideW >= 100) {
                    RECT oldSplit = g_app.splitRect;
                    g_app.splitRatio = (double)sideW / (double)availW;
                    LayoutChildren(hwnd);
                    InvalidateRect(hwnd, &oldSplit, TRUE);
                    InvalidateRect(hwnd, &g_app.splitRect, TRUE);
                }
            }
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (g_app.isDraggingSplit) {
            g_app.isDraggingSplit = false;
            ReleaseCapture();
            int permille = (int)(g_app.splitRatio * 1000.0 + 0.5);
            if (permille < 150) permille = 150;
            if (permille > 800) permille = 800;
            wchar_t ratioBuf[16];
            swprintf(ratioBuf, 16, L"%d", permille);
            SaveIniString(INI_SEC_SETTINGS, L"SplitRatio", ratioBuf);
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        if (g_app.isDraggingSplit) {
            g_app.isDraggingSplit = false;
            int permille = (int)(g_app.splitRatio * 1000.0 + 0.5);
            if (permille < 150) permille = 150;
            if (permille > 800) permille = 800;
            wchar_t ratioBuf[16];
            swprintf(ratioBuf, 16, L"%d", permille);
            SaveIniString(INI_SEC_SETTINGS, L"SplitRatio", ratioBuf);
            return 0;
        }
        break;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mmi = (MINMAXINFO *)lParam;
        mmi->ptMinTrackSize.x = ScaleDpi(hwnd, 820);
        mmi->ptMinTrackSize.y = ScaleDpi(hwnd, 500);
        return 0;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        wchar_t droppedPath[MAX_PATH];
        if (DragQueryFileW(hDrop, 0, droppedPath, MAX_PATH) > 0) {
            if (is_valid_directory(droppedPath)) {
                g_app.inExplorerMode = true;
                SyncViewModeVisibility();
                LayoutChildren(hwnd);
                NavigateExplorerTo(droppedPath);
            } else {
                LoadOricFile(droppedPath);
            }
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_CONTEXTMENU: {
        if ((HWND)wParam == g_app.hwndExpList && g_app.inExplorerMode) {
            POINT pt = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };
            if (pt.x == -1 && pt.y == -1) {
                int sel = ListView_GetNextItem(g_app.hwndExpList, -1, LVNI_SELECTED);
                RECT r;
                if (sel >= 0 && ListView_GetItemRect(g_app.hwndExpList, sel, &r, LVIR_BOUNDS)) {
                    pt.x = r.left + 20;
                    pt.y = (r.top + r.bottom) / 2;
                    ClientToScreen(g_app.hwndExpList, &pt);
                } else {
                    GetWindowRect(g_app.hwndExpList, &r);
                    pt.x = (r.left + r.right) / 2;
                    pt.y = (r.top + r.bottom) / 2;
                }
            }
            ShowExplorerContextMenu(hwnd, pt);
            return 0;
        }
        if ((HWND)wParam == g_app.hwndList && !g_app.inExplorerMode) {
            POINT pt = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };
            if (pt.x == -1 && pt.y == -1) {
                RECT r;
                GetWindowRect(g_app.hwndList, &r);
                pt.x = (r.left + r.right) / 2;
                pt.y = (r.top + r.bottom) / 2;
            }
            ShowListContextMenu(hwnd, pt);
            return 0;
        }
        break;
    }

    case WM_NOTIFY: {
        LPNMHDR nmh = (LPNMHDR)lParam;
        if (!nmh) break;

        if (nmh->idFrom == IDC_EXP_LIST) {
            if (nmh->code == LVN_ITEMCHANGED) {
                if (!g_app.populatingExpList) {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if ((pnmv->uChanged & LVIF_STATE) &&
                        ((pnmv->uNewState & LVIS_SELECTED) != (pnmv->uOldState & LVIS_SELECTED))) {
                        int sel = ListView_GetNextItem(g_app.hwndExpList, -1, LVNI_SELECTED);
                        g_app.expSelected = -1;
                        if (sel >= 0) {
                            LVITEMW lvi;
                            memset(&lvi, 0, sizeof(lvi));
                            lvi.mask = LVIF_PARAM;
                            lvi.iItem = sel;
                            if (ListView_GetItem(g_app.hwndExpList, &lvi)) {
                                size_t idx = (size_t)lvi.lParam;
                                if (idx < g_app.expCount) {
                                    g_app.expSelected = (int)idx;
                                    if (!g_app.expItems[idx].isParent) {
                                        wcsncpy(g_app.expLastPath, g_app.expItems[idx].fullPath, MAX_PATH - 1);
                                        g_app.expLastPath[MAX_PATH - 1] = L'\0';
                                    } else {
                                        g_app.expLastPath[0] = L'\0';
                                    }
                                }
                            }
                        }
                        UpdateSidebarAll();
                        UpdateStatusBar();
                    }
                }
            } else if (nmh->code == NM_DBLCLK) {
                LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
                int row = (lpnmitem && lpnmitem->iItem >= 0) ? lpnmitem->iItem : -1;
                if (row < 0) {
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(g_app.hwndExpList, &pt);
                    LVHITTESTINFO hti;
                    memset(&hti, 0, sizeof(hti));
                    hti.pt = pt;
                    int hit = ListView_HitTest(g_app.hwndExpList, &hti);
                    if (hit >= 0 && (hti.flags & LVHT_ONITEM)) {
                        row = hit;
                    }
                }
                if (row >= 0) {
                    PostMessageW(hwnd, WM_APP_EXP_ACTIVATE, (WPARAM)row, 0);
                }
            }
        } else if (nmh->idFrom == IDC_LISTVIEW) {
            if (nmh->code == LVN_ITEMCHANGED) {
                LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                if ((pnmv->uChanged & LVIF_STATE) &&
                    ((pnmv->uNewState & LVIS_SELECTED) != (pnmv->uOldState & LVIS_SELECTED))) {
                    if (pnmv->uNewState & LVIS_SELECTED) {
                        LVITEMW lvi;
                        memset(&lvi, 0, sizeof(lvi));
                        lvi.mask = LVIF_PARAM;
                        lvi.iItem = pnmv->iItem;
                        if (ListView_GetItem(g_app.hwndList, &lvi)) {
                            g_app.selectedIndex = (int)lvi.lParam;
                        }
                    } else if (ListView_GetSelectedCount(g_app.hwndList) == 0) {
                        g_app.selectedIndex = -1;
                    }
                    UpdateSidebarAll();
                }
            } else if (nmh->code == LVN_COLUMNCLICK) {
                LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                if (g_app.sortCol == pnmv->iSubItem) {
                    g_app.sortAsc = !g_app.sortAsc;
                } else {
                    g_app.sortCol = pnmv->iSubItem;
                    g_app.sortAsc = true;
                }
                ListView_SortItems(g_app.hwndList, CompareListItems, 0);
            } else if (nmh->code == NM_DBLCLK) {
                int targetTab = 1;
                if (g_app.selectedIndex >= 0 && (size_t)g_app.selectedIndex < g_app.report.entryCount &&
                    g_app.report.entries[g_app.selectedIndex].isScreen) {
                    targetTab = 2;
                }
                g_app.activeTab = targetTab;
                TabCtrl_SetCurSel(g_app.hwndTabs, targetTab);
                SyncTabVisibility();
                LayoutChildren(hwnd);
            }
        } else if (nmh->idFrom == IDC_SIDEBAR_TABS && nmh->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(g_app.hwndTabs);
            if (sel >= 0 && sel <= 2) {
                g_app.activeTab = sel;
                SyncTabVisibility();
                LayoutChildren(hwnd);
                if (sel == 2 && g_app.hwndVisualCanvas) {
                    InvalidateRect(g_app.hwndVisualCanvas, NULL, FALSE);
                }
            }
        }
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmCode = HIWORD(wParam);

        if (wmId == IDC_FILTER_EDIT && wmCode == EN_CHANGE) {
            if (g_app.suppressFilterChange) return 0;
            if (g_app.inExplorerMode) {
                g_app.expLastTopIndex = 0;
                g_app.expFallbackIndex = -1;
                PopulateExplorerList();
                UpdateSidebarAll();
                UpdateStatusBar();
            } else {
                PopulateListView();
            }
            return 0;
        }

        if (wmId == IDC_EXP_DRIVE && wmCode == CBN_SELCHANGE) {
            int idx = (int)SendMessageW(g_app.hwndExpDrive, CB_GETCURSEL, 0, 0);
            if (idx >= 0) {
                wchar_t drv[32] = {0};
                SendMessageW(g_app.hwndExpDrive, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)drv);
                if (drv[0] != L'\0') {
                    NavigateExplorerTo(drv);
                }
            }
            return 0;
        }

        if (wmId >= IDM_FILE_RECENT_BASE && wmId < IDM_FILE_RECENT_BASE + IDM_FILE_RECENT_MAX) {
            int rIdx = wmId - IDM_FILE_RECENT_BASE;
            if (rIdx >= 0 && rIdx < g_app.recentCount) {
                LoadOricFile(g_app.recentFiles[rIdx]);
            }
            return 0;
        }

        switch (wmId) {
        case IDC_EXP_UP:
            GoToParentDirectory();
            return 0;
        case IDC_BACK_EXPLORER:
        case IDM_FILE_CLOSE:
            if (!g_app.inExplorerMode) {
                ReturnToExplorer();
            }
            return 0;
        case IDM_FILE_OPEN:
            OpenFileViaDialog(hwnd);
            return 0;
        case IDM_FILE_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case IDM_EDIT_COPY: {
            HWND focus = GetFocus();
            if (focus == g_app.hwndSideEdit || focus == g_app.hwndCodeEdit || focus == g_app.hwndExpPath) {
                DWORD selStart = 0, selEnd = 0;
                SendMessageW(focus, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
                if (selStart != selEnd) {
                    SendMessageW(focus, WM_COPY, 0, 0);
                    return 0;
                }
            }
            CopyActiveTextToClipboard(hwnd);
            return 0;
        }
        case IDC_SIDEBAR_COPY:
            CopyActiveTextToClipboard(hwnd);
            return 0;
        case IDC_SAVE_PNG:
        case IDM_TOOLS_EXPORT_PNG:
            ExportVisualAsPng(hwnd);
            return 0;
        case IDC_CODE_TOGGLE:
            g_app.forceHexMode = !g_app.forceHexMode;
            SyncTabVisibility();
            RefreshCodeInspector();
            return 0;
        case IDM_CTX_VIEW_CODE:
            g_app.activeTab = 1;
            TabCtrl_SetCurSel(g_app.hwndTabs, 1);
            SyncTabVisibility();
            LayoutChildren(hwnd);
            return 0;
        case IDM_CTX_VIEW_SCREEN:
            g_app.activeTab = 2;
            TabCtrl_SetCurSel(g_app.hwndTabs, 2);
            SyncTabVisibility();
            LayoutChildren(hwnd);
            return 0;
        case IDM_TOOLS_EXTRACT_BIN:
            ExportSelectedMedia(hwnd, 0);
            return 0;
        case IDM_TOOLS_EXPORT_TAP:
            ExportSelectedMedia(hwnd, 1);
            return 0;
        case IDM_TOOLS_EXPORT_WAV:
            ExportSelectedMedia(hwnd, 2);
            return 0;
        case IDM_EXP_OPEN:
            if (g_app.inExplorerMode) {
                ActivateSelectedExplorerItem();
            }
            return 0;
        case IDM_EXP_RENAME:
            if (g_app.inExplorerMode) {
                RenameSelectedExplorerItem(hwnd);
            }
            return 0;
        case IDM_EXP_DELETE:
            if (g_app.inExplorerMode) {
                DeleteSelectedExplorerItem(hwnd);
            }
            return 0;
        case IDM_TOOLS_LAUNCH_EMU:
            LaunchEmulatorForCurrentMedia(hwnd);
            return 0;
        case IDM_TOOLS_CONFIG_EMU:
            OpenEmulatorConfigDialog(hwnd);
            return 0;
        case IDM_LANG_EN:
            RefreshUiLanguage(ORIC_LANG_EN);
            return 0;
        case IDM_LANG_FR:
            RefreshUiLanguage(ORIC_LANG_FR);
            return 0;
        case IDM_LANG_DE:
            RefreshUiLanguage(ORIC_LANG_DE);
            return 0;
        case IDM_LANG_ES:
            RefreshUiLanguage(ORIC_LANG_ES);
            return 0;
        case IDM_HELP_ABOUT:
            ShowAboutDialog(hwnd);
            return 0;
        }
        break;
    }

    case WM_APP_EXP_ACTIVATE:
        if (g_app.inExplorerMode) {
            ActivateExplorerItemAt((int)wParam);
        }
        return 0;

    case WM_DESTROY:
        FreeExplorerItems();
        FreeExplorerPreview();
        FreeLoadedBuffer();
        oric_report_free(&g_app.report);
        if (g_app.hFontUi) DeleteObject(g_app.hFontUi);
        if (g_app.hFontUiBold) DeleteObject(g_app.hFontUiBold);
        if (g_app.hFontMono) DeleteObject(g_app.hFontMono);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    InitWorkingPaths();
    LoadSettingsFromIni();

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW vc;
    memset(&vc, 0, sizeof(vc));
    vc.cbSize = sizeof(vc);
    vc.style = CS_HREDRAW | CS_VREDRAW;
    vc.lpfnWndProc = VisualCanvasProc;
    vc.hInstance = hInstance;
    vc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    vc.lpszClassName = VISUAL_CLASS_NAME;
    RegisterClassExW(&vc);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = APP_CLASS_NAME;
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL,
                    oric_tr(g_app.lang, STR_ERR_REG_CLASS),
                    oric_tr(g_app.lang, STR_ERR_FATAL_TITLE),
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        APP_CLASS_NAME,
        oric_tr(g_app.lang, STR_APP_TITLE),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1140, 700,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL,
                    oric_tr(g_app.lang, STR_ERR_CREATE_WND),
                    oric_tr(g_app.lang, STR_ERR_FATAL_TITLE),
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc >= 2 && argv[1] && argv[1][0] != L'\0') {
            if (is_valid_directory(argv[1])) {
                NavigateExplorerTo(argv[1]);
            } else {
                LoadOricFile(argv[1]);
            }
        }
        LocalFree(argv);
    }

    HACCEL hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDA_MAIN_ACCEL));
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && GetActiveWindow() == hwnd) {
            if (msg.wParam == VK_ESCAPE && !g_app.inExplorerMode) {
                ReturnToExplorer();
                continue;
            }
            if (msg.wParam == VK_RETURN && g_app.inExplorerMode) {
                HWND focus = GetFocus();
                if (focus == g_app.hwndExpPath) {
                    wchar_t typed[MAX_PATH] = {0};
                    GetWindowTextW(g_app.hwndExpPath, typed, MAX_PATH);
                    NavigateExplorerTo(typed);
                    continue;
                }
                if (focus == g_app.hwndExpList) {
                    ActivateSelectedExplorerItem();
                    continue;
                }
            }
            if (msg.wParam == VK_BACK && g_app.inExplorerMode && GetFocus() == g_app.hwndExpList) {
                GoToParentDirectory();
                continue;
            }
            if (msg.wParam == VK_F2 && g_app.inExplorerMode && GetFocus() == g_app.hwndExpList) {
                RenameSelectedExplorerItem(hwnd);
                continue;
            }
            if (msg.wParam == VK_DELETE && g_app.inExplorerMode && GetFocus() == g_app.hwndExpList) {
                DeleteSelectedExplorerItem(hwnd);
                continue;
            }
        }

        if (!hAccel || !TranslateAcceleratorW(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return (int)msg.wParam;
}
