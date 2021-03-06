/**
 *  \file
 *  \brief  GTags plugin main routines
 *
 *  \author  Pavel Nedev <pg.nedev@gmail.com>
 *
 *  \section COPYRIGHT
 *  Copyright(C) 2014-2015 Pavel Nedev
 *
 *  \section LICENSE
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <windows.h>
#include <tchar.h>
#include <shlobj.h>
#include <list>
#include "AutoLock.h"
#include "Common.h"
#include "INpp.h"
#include "Config.h"
#include "DbManager.h"
#include "CmdEngine.h"
#include "DocLocation.h"
#include "SearchWin.h"
#include "ActivityWin.h"
#include "AutoCompleteWin.h"
#include "ResultWin.h"
#include "ConfigWin.h"
#include "AboutWin.h"
#include "GTags.h"


#define LINUX_WINE_WORKAROUNDS


namespace
{

using namespace GTags;


const TCHAR cCreateDatabase[]   = _T("Create Database");
const TCHAR cUpdateSingle[]     = _T("Database Single File Update");
const TCHAR cAutoCompl[]        = _T("AutoComplete");
const TCHAR cAutoComplFile[]    = _T("AutoComplete Filename");
const TCHAR cFindFile[]         = _T("Find File");
const TCHAR cFindDefinition[]   = _T("Find Definition");
const TCHAR cFindReference[]    = _T("Find Reference");
const TCHAR cFindSymbol[]       = _T("Find Symbol");
const TCHAR cSearch[]           = _T("Search");
const TCHAR cVersion[]          = _T("About");


std::list<CPath> UpdateList;
Mutex UpdateLock;


/**
*  \brief
*/
inline void releaseKeys()
{
#ifdef LINUX_WINE_WORKAROUNDS
    Tools::ReleaseKey(VK_SHIFT, false);
    Tools::ReleaseKey(VK_CONTROL, false);
    Tools::ReleaseKey(VK_MENU, false);
#endif // LINUX_WINE_WORKAROUNDS
}


/**
 *  \brief
 */
bool checkForGTagsBinaries(CPath& dllPath)
{
    dllPath.StripFilename();
    dllPath += cBinsDir;
    dllPath += _T("\\global.exe");

    bool gtagsBinsFound = dllPath.FileExists();
    if (gtagsBinsFound)
    {
        dllPath.StripFilename();
        dllPath += _T("gtags.exe");
        gtagsBinsFound = dllPath.FileExists();
    }

    if (!gtagsBinsFound)
    {
        dllPath.StripFilename();

        CText msg(_T("GTags binaries not found in\n\""));
        msg += dllPath.C_str();
        msg += _T("\"\n");
        msg += cPluginName;
        msg += _T(" plugin will not be loaded!");

        MessageBox(NULL, msg.C_str(), cPluginName, MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}


/**
 *  \brief
 */
CText getSelection(bool autoSelectWord = false, bool skipPreSelect = false)
{
    INpp& npp = INpp::Get();

    npp.ReadSciHandle();
    if (npp.IsSelectionVertical())
        return CText();

    CTextA tagA;

    if (!skipPreSelect)
        npp.GetSelection(tagA);

    if (skipPreSelect || (tagA.IsEmpty() && autoSelectWord))
        npp.GetWord(tagA, true);

    if (tagA.IsEmpty())
        return CText();

    return CText(tagA.C_str());
}


/**
 *  \brief
 */
DbHandle getDatabase(bool writeEn = false)
{
    INpp& npp = INpp::Get();
    bool success;
    CPath currentFile;
    npp.GetFilePath(currentFile);

    DbHandle db = DbManager::Get().GetDb(currentFile, writeEn, &success);
    if (!db)
    {
        MessageBox(npp.GetHandle(), _T("GTags database not found"), cPluginName, MB_OK | MB_ICONINFORMATION);
    }
    else if (!success)
    {
        MessageBox(npp.GetHandle(), _T("GTags database is currently in use"), cPluginName, MB_OK | MB_ICONINFORMATION);
        db = NULL;
    }

    return db;
}


/**
 *  \brief
 */
int CALLBACK browseFolderCB(HWND hWnd, UINT uMsg, LPARAM, LPARAM lpData)
{
    if (uMsg == BFFM_INITIALIZED)
        SendMessage(hWnd, BFFM_SETSELECTION, TRUE, lpData);
    return 0;
}


/**
 *  \brief
 */
void sheduleForUpdate(const CPath& file)
{
    AUTOLOCK(UpdateLock);

    std::list<CPath>::reverse_iterator iFile;
    for (iFile = UpdateList.rbegin(); iFile != UpdateList.rend(); ++iFile)
        if (*iFile == file)
            return;

    UpdateList.push_back(file);
}


/**
 *  \brief
 */
bool runSheduledUpdate(const TCHAR* dbPath)
{
    CPath file;
    {
        AUTOLOCK(UpdateLock);

        if (UpdateList.empty())
            return false;

        std::list<CPath>::iterator iFile;
        for (iFile = UpdateList.begin(); iFile != UpdateList.end(); ++iFile)
            if (iFile->IsSubpathOf(dbPath))
                break;

        if (iFile == UpdateList.end())
            return false;

        file = *iFile;
        UpdateList.erase(iFile);
    }

    if (!UpdateSingleFile(file))
        return runSheduledUpdate(dbPath);

    return true;
}


/**
 *  \brief
 */
void dbWriteReady(const std::shared_ptr<Cmd>& cmd)
{
    if (cmd->Status() != OK && cmd->Id() == CREATE_DATABASE)
        DbManager::Get().UnregisterDb(cmd->Db());
    else
        DbManager::Get().PutDb(cmd->Db());

    runSheduledUpdate(cmd->DbPath());

    if (cmd->Status() == RUN_ERROR)
    {
        MessageBox(INpp::Get().GetHandle(), _T("Running GTags failed"), cmd->Name(), MB_OK | MB_ICONERROR);
    }
    else if (cmd->Status() == FAILED || cmd->Result())
    {
        CText msg(cmd->Result());
        MessageBox(INpp::Get().GetHandle(), msg.C_str(), cmd->Name(), MB_OK | MB_ICONEXCLAMATION);
    }
}


/**
 *  \brief
 */
void autoComplReady(const std::shared_ptr<Cmd>& cmd)
{
    DbManager::Get().PutDb(cmd->Db());

    runSheduledUpdate(cmd->DbPath());

    if (cmd->Status() == OK)
    {
        if (cmd->Result())
            AutoCompleteWin::Show(cmd);
        else
            INpp::Get().ClearSelection();
    }
    else if (cmd->Status() == FAILED)
    {
        releaseKeys();
        INpp::Get().ClearSelection();

        CText msg(cmd->Result());
        msg += _T("\nTry re-creating database.");
        MessageBox(INpp::Get().GetHandle(), msg.C_str(), cmd->Name(), MB_OK | MB_ICONERROR);
    }
    else if (cmd->Status() == RUN_ERROR)
    {
        releaseKeys();
        INpp::Get().ClearSelection();

        MessageBox(INpp::Get().GetHandle(), _T("Running GTags failed"), cmd->Name(), MB_OK | MB_ICONERROR);
    }
    else
    {
        INpp::Get().ClearSelection();
    }
}


/**
 *  \brief
 */
void showResult(const std::shared_ptr<Cmd>& cmd)
{
    DbManager::Get().PutDb(cmd->Db());

    runSheduledUpdate(cmd->DbPath());

    releaseKeys();

    if (cmd->Status() == OK)
    {
        if (cmd->Result())
        {
            ResultWin::Show(cmd);
        }
        else
        {
            CText msg(_T("\""));
            msg += cmd->Tag();
            msg += _T("\" not found");
            MessageBox(INpp::Get().GetHandle(), msg.C_str(), cmd->Name(), MB_OK | MB_ICONINFORMATION);
        }
    }
    else if (cmd->Status() == FAILED)
    {
        CText msg(cmd->Result());
        msg += _T("\nTry re-creating database.");
        MessageBox(INpp::Get().GetHandle(), msg.C_str(), cmd->Name(), MB_OK | MB_ICONERROR);
    }
    else if (cmd->Status() == RUN_ERROR)
    {
        MessageBox(INpp::Get().GetHandle(), _T("Running GTags failed"), cmd->Name(), MB_OK | MB_ICONERROR);
    }
}


/**
 *  \brief
 */
void findReady(const std::shared_ptr<Cmd>& cmd)
{
    if (cmd->Status() == OK && cmd->Result() == NULL)
    {
        cmd->Id(FIND_SYMBOL);
        cmd->Name(cFindSymbol);

        CmdEngine::Run(cmd, showResult);
    }
    else
    {
        showResult(cmd);
    }
}


/**
 *  \brief
 */
/*
void EnablePluginMenuItem(int itemIdx, bool enable)
{
    static HMENU HMenu = NULL;

    if (HMenu == NULL)
    {
        TCHAR buf[PLUGIN_ITEM_SIZE];
        HMENU hMenu = INpp::Get().GetPluginMenu();

        MENUITEMINFO mi = {0};
        mi.cbSize       = sizeof(mi);
        mi.fMask        = MIIM_STRING;

        int idx;
        int itemsCount = GetMenuItemCount(hMenu);
        for (idx = 0; idx < itemsCount; ++idx)
        {
            mi.dwTypeData = NULL;
            GetMenuItemInfo(hMenu, idx, TRUE, &mi);
            mi.dwTypeData = buf;
            ++mi.cch;
            GetMenuItemInfo(hMenu, idx, TRUE, &mi);
            mi.dwTypeData[mi.cch - 1] = 0;
            if (!_tcscmp(cPluginName, mi.dwTypeData))
                break;
        }
        if (idx == itemsCount)
            return;

        HMenu = GetSubMenu(hMenu, idx);
    }

    UINT flags = MF_BYPOSITION;
    flags |= enable ? MF_ENABLED : MF_GRAYED;

    EnableMenuItem(HMenu, itemIdx, flags);
}
*/


/**
 *  \brief
 */
void AutoComplete()
{
    CText tag = getSelection(true, true);
    if (tag.IsEmpty())
        return;

    DbHandle db = getDatabase();
    if (!db)
    {
        INpp::Get().ClearSelection();
        return;
    }

    std::shared_ptr<Cmd> cmd(new Cmd(AUTOCOMPLETE, cAutoCompl, db, tag.C_str()));

    if (CmdEngine::Run(cmd))
    {
        cmd->Id(AUTOCOMPLETE_SYMBOL);
        CmdEngine::Run(cmd);
    }

    autoComplReady(cmd);
}


/**
 *  \brief
 */
void AutoCompleteFile()
{
    CText tag = getSelection(true, true);
    if (tag.IsEmpty())
        return;

    tag.Insert(0, _T('/'));

    DbHandle db = getDatabase();
    if (!db)
    {
        INpp::Get().ClearSelection();
        return;
    }

    std::shared_ptr<Cmd> cmd(new Cmd(AUTOCOMPLETE_FILE, cAutoComplFile, db, tag.C_str()));

    CmdEngine::Run(cmd);
    autoComplReady(cmd);
}


/**
 *  \brief
 */
void FindFile()
{
    SearchWin::Close();

    DbHandle db = getDatabase();
    if (!db)
        return;

    std::shared_ptr<Cmd> cmd(new Cmd(FIND_FILE, cFindFile, db));

    CText tag = getSelection();
    if (tag.IsEmpty())
    {
        CPath fileName;
        INpp::Get().GetFileNamePart(fileName);
        cmd->Tag(fileName.C_str());

        SearchWin::Show(cmd, showResult);
    }
    else
    {
        cmd->Tag(tag.C_str());

        CmdEngine::Run(cmd, showResult);
    }
}


/**
 *  \brief
 */
void FindDefinition()
{
    SearchWin::Close();

    DbHandle db = getDatabase();
    if (!db)
        return;

    std::shared_ptr<Cmd> cmd(new Cmd(FIND_DEFINITION, cFindDefinition, db));

    CText tag = getSelection(true);
    if (tag.IsEmpty())
    {
        SearchWin::Show(cmd, findReady, false);
    }
    else
    {
        cmd->Tag(tag.C_str());

        CmdEngine::Run(cmd, findReady);
    }
}


/**
 *  \brief
 */
void FindReference()
{
    SearchWin::Close();

    if (Config._parserIdx == CConfig::CTAGS_PARSER)
    {
        MessageBox(INpp::Get().GetHandle(), _T("Ctags parser doesn't support reference search"), cPluginName,
                MB_OK | MB_ICONINFORMATION);
        return;
    }

    DbHandle db = getDatabase();
    if (!db)
        return;

    std::shared_ptr<Cmd> cmd(new Cmd(FIND_REFERENCE, cFindReference, db));

    CText tag = getSelection(true);
    if (tag.IsEmpty())
    {
        SearchWin::Show(cmd, findReady, false);
    }
    else
    {
        cmd->Tag(tag.C_str());

        CmdEngine::Run(cmd, findReady);
    }
}


/**
 *  \brief
 */
void Search()
{
    SearchWin::Close();

    DbHandle db = getDatabase();
    if (!db)
        return;

    std::shared_ptr<Cmd> cmd(new Cmd(GREP, cSearch, db));

    CText tag = getSelection(true);
    if (tag.IsEmpty())
    {
        SearchWin::Show(cmd, showResult);
    }
    else
    {
        cmd->Tag(tag.C_str());

        CmdEngine::Run(cmd, showResult);
    }
}


/**
 *  \brief
 */
void GoBack()
{
    DocLocation::Get().Back();
}


/**
 *  \brief
 */
void GoForward()
{
    DocLocation::Get().Forward();
}


/**
 *  \brief
 */
void CreateDatabase()
{
    SearchWin::Close();

    CPath currentFile;

    bool success;
    INpp& npp = INpp::Get();
    npp.GetFilePath(currentFile);

    DbHandle db = DbManager::Get().GetDb(currentFile, true, &success);
    if (db)
    {
        if (!success)
        {
            MessageBox(npp.GetHandle(), _T("GTags database is currently in use"), cPluginName,
                    MB_OK | MB_ICONINFORMATION);
            return;
        }

        CText msg(_T("Database at\n\""));
        msg += db->C_str();
        msg += _T("\" exists.\nRe-create?");
        int choice = MessageBox(npp.GetHandle(), msg.C_str(), cPluginName, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);
        if (choice != IDYES)
        {
            DbManager::Get().PutDb(db);
            return;
        }
    }
    else
    {
        TCHAR path[MAX_PATH];

        currentFile.StripFilename();

        BROWSEINFO bi       = {0};
        bi.hwndOwner        = npp.GetHandle();
        bi.pszDisplayName   = path;
        bi.lpszTitle        = _T("Select the project root");
        bi.ulFlags          = BIF_RETURNONLYFSDIRS;
        bi.lpfn             = browseFolderCB;
        bi.lParam           = (DWORD)currentFile.C_str();

        LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
        if (!pidl)
            return;

        SHGetPathFromIDList(pidl, path);

        IMalloc* imalloc = NULL;
        if (SUCCEEDED(SHGetMalloc(&imalloc)))
        {
            imalloc->Free(pidl);
            imalloc->Release();
        }

        currentFile = path;
        currentFile += _T("\\");
        db = DbManager::Get().RegisterDb(currentFile);
    }

    std::shared_ptr<Cmd> cmd(new Cmd(CREATE_DATABASE, cCreateDatabase, db));
    releaseKeys();
    CmdEngine::Run(cmd, dbWriteReady);
}


/**
 *  \brief
 */
void DeleteDatabase()
{
    SearchWin::Close();

    DbHandle db = getDatabase(true);
    if (!db)
        return;

    INpp& npp = INpp::Get();
    TCHAR buf[512];
    _sntprintf_s(buf, _countof(buf), _TRUNCATE, _T("Delete database from\n\"%s\"?"), db->C_str());
    int choice = MessageBox(npp.GetHandle(), buf, cPluginName, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);
    if (choice != IDYES)
    {
        DbManager::Get().PutDb(db);
        return;
    }

    if (DbManager::Get().UnregisterDb(db))
        MessageBox(npp.GetHandle(), _T("GTags database deleted"), cPluginName, MB_OK | MB_ICONINFORMATION);
    else
        MessageBox(npp.GetHandle(), _T("Deleting database failed, is it read-only?"), cPluginName,
                MB_OK | MB_ICONERROR);
}


/**
 *  \brief
 */
void ToggleResultWinFocus()
{
    releaseKeys();
    ResultWin::Show();
}


/**
 *  \brief
 */
void SettingsCfg()
{
    ConfigWin::Show(&Config);
}


/**
 *  \brief
 */
void About()
{
    std::shared_ptr<Cmd> cmd(new Cmd(VERSION, cVersion));
    CmdEngine::Run(cmd);

    CText msg;

    if (cmd->Status() == OK)
        msg = cmd->Result();
    else
        msg = _T("VERSION READ FAILED\n");

    AboutWin::Show(msg.C_str());
}

} // anonymous namespace


namespace GTags
{

FuncItem Menu[18] = {
    /* 0 */  FuncItem(cAutoCompl, AutoComplete),
    /* 1 */  FuncItem(cAutoComplFile, AutoCompleteFile),
    /* 2 */  FuncItem(cFindFile, FindFile),
    /* 3 */  FuncItem(cFindDefinition, FindDefinition),
    /* 4 */  FuncItem(cFindReference, FindReference),
    /* 5 */  FuncItem(cSearch, Search),
    /* 6 */  FuncItem(),
    /* 7 */  FuncItem(_T("Go Back"), GoBack),
    /* 8 */  FuncItem(_T("Go Forward"), GoForward),
    /* 9 */  FuncItem(),
    /* 10 */ FuncItem(cCreateDatabase, CreateDatabase),
    /* 11 */ FuncItem(_T("Delete Database"), DeleteDatabase),
    /* 12 */ FuncItem(),
    /* 13 */ FuncItem(_T("Toggle Results Window Focus"), ToggleResultWinFocus),
    /* 14 */ FuncItem(),
    /* 15 */ FuncItem(_T("Settings"), SettingsCfg),
    /* 16 */ FuncItem(),
    /* 17 */ FuncItem(cVersion, About)
};

HINSTANCE HMod = NULL;
CPath DllPath;

CText UIFontName;
unsigned UIFontSize;

CConfig Config;


/**
 *  \brief
 */
BOOL PluginLoad(HINSTANCE hMod)
{
    CPath moduleFileName(MAX_PATH);
    GetModuleFileName((HMODULE)hMod, moduleFileName.C_str(), MAX_PATH);
    moduleFileName.AutoFit();

    DllPath = moduleFileName;

    if (!checkForGTagsBinaries(moduleFileName))
        return FALSE;

    HMod = hMod;

    return TRUE;
}


/**
 *  \brief
 */
void PluginInit()
{
    INpp& npp = INpp::Get();
    char font[32];

    npp.GetFontName(STYLE_DEFAULT, font);
    UIFontName = font;
    UIFontSize = (unsigned)npp.GetFontSize(STYLE_DEFAULT);

    ActivityWin::Register();
    SearchWin::Register();
    AutoCompleteWin::Register();

    if (ResultWin::Register())
        MessageBox(npp.GetHandle(), _T("Results Window init failed, plugin will not be operational"), cPluginName,
                MB_OK | MB_ICONERROR);
    else if(!Config.LoadFromFile())
        MessageBox(npp.GetHandle(), _T("Bad config file, default settings will be used"), cPluginName,
                MB_OK | MB_ICONEXCLAMATION);
}


/**
 *  \brief
 */
void PluginDeInit()
{
    ActivityWin::Unregister();
    SearchWin::Unregister();
    AutoCompleteWin::Unregister();
    ResultWin::Unregister();

    HMod = NULL;
}


/**
 *  \brief
 */
bool UpdateSingleFile(const CPath& file)
{
    bool success;
    DbHandle db = DbManager::Get().GetDb(file, true, &success);
    if (!db)
        return false;

    if (!success)
    {
        sheduleForUpdate(file);
        return true;
    }

    std::shared_ptr<Cmd> cmd(new Cmd(UPDATE_SINGLE, cUpdateSingle, db, file.C_str()));

    releaseKeys();
    if (!CmdEngine::Run(cmd, dbWriteReady))
        return false;

    return true;
}


/**
 *  \brief
 */
const CPath CreateLibraryDatabase(HWND hWnd)
{
    TCHAR path[MAX_PATH];
    CPath libraryPath;

    BROWSEINFO bi       = {0};
    bi.hwndOwner        = hWnd;
    bi.pszDisplayName   = path;
    bi.lpszTitle        = _T("Select the library root");
    bi.ulFlags          = BIF_RETURNONLYFSDIRS;
    bi.lpfn             = browseFolderCB;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (!pidl)
        return libraryPath;

    SHGetPathFromIDList(pidl, path);

    IMalloc* imalloc = NULL;
    if (SUCCEEDED(SHGetMalloc(&imalloc)))
    {
        imalloc->Free(pidl);
        imalloc->Release();
    }

    libraryPath = path;
    libraryPath += _T("\\");

    DbHandle db;

    if (DbManager::Get().DbExistsInFolder(libraryPath))
    {
        CText msg(_T("Database at\n\""));
        msg += libraryPath;
        msg += _T("\" exists.\nRe-create?");
        int choice = MessageBox(hWnd, msg.C_str(), cPluginName, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (choice != IDYES)
            return libraryPath;

        bool success;
        db = DbManager::Get().GetDb(libraryPath, true, &success);

        if (!success)
        {
            MessageBox(hWnd, _T("GTags database is currently in use"), cPluginName, MB_OK | MB_ICONINFORMATION);
            libraryPath.Clear();

            return libraryPath;
        }
    }
    else
    {
        db = DbManager::Get().RegisterDb(libraryPath);
    }

    std::shared_ptr<Cmd> cmd(new Cmd(CREATE_DATABASE, cCreateDatabase, db));
    releaseKeys();
    CmdEngine::Run(cmd);

    dbWriteReady(cmd);
    if (cmd->Status() != OK)
        libraryPath.Clear();

    return libraryPath;
}

} // namespace GTags
