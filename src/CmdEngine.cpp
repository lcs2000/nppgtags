/**
 *  \file
 *  \brief  GTags command execution and result classes
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
#include <process.h>
#include "Common.h"
#include "INpp.h"
#include "Config.h"
#include "GTags.h"
#include "ActivityWin.h"
#include "ReadPipe.h"
#include "CmdEngine.h"


namespace GTags
{

const TCHAR CmdEngine::cCreateDatabaseCmd[] = _T("\"%s\\gtags.exe\" -c --skip-unreadable");
const TCHAR CmdEngine::cUpdateSingleCmd[]   = _T("\"%s\\gtags.exe\" -c --skip-unreadable --single-update \"%s\"");
const TCHAR CmdEngine::cAutoComplCmd[]      = _T("\"%s\\global.exe\" -cT \"%s\"");
const TCHAR CmdEngine::cAutoComplSymCmd[]   = _T("\"%s\\global.exe\" -cs \"%s\"");
const TCHAR CmdEngine::cAutoComplFileCmd[]  = _T("\"%s\\global.exe\" -cP --match-part=all \"%s\"");
const TCHAR CmdEngine::cFindFileCmd[]       = _T("\"%s\\global.exe\" -P \"%s\"");
const TCHAR CmdEngine::cFindDefinitionCmd[] = _T("\"%s\\global.exe\" -dT --result=grep \"%s\"");
const TCHAR CmdEngine::cFindReferenceCmd[]  = _T("\"%s\\global.exe\" -r --result=grep \"%s\"");
const TCHAR CmdEngine::cFindSymbolCmd[]     = _T("\"%s\\global.exe\" -s --result=grep \"%s\"");
const TCHAR CmdEngine::cGrepCmd[]           = _T("\"%s\\global.exe\" -g --result=grep \"%s\"");
const TCHAR CmdEngine::cVersionCmd[]        = _T("\"%s\\global.exe\" --version");


/**
 *  \brief
 */
Cmd::Cmd(CmdId_t id, const TCHAR* name, DbHandle db, const TCHAR* tag, bool regExp, bool matchCase) :
        _id(id), _db(db), _regExp(regExp), _matchCase(matchCase), _status(CANCELLED)
{
    if (db)
        _dbPath = *db;

    if (name)
        _name = name;

    if (tag)
        _tag = tag;
}


/**
 *  \brief
 */
bool CmdEngine::Run(const std::shared_ptr<Cmd>& cmd, CompletionCB complCB)
{
    CmdEngine* engine = new CmdEngine(cmd, complCB);
    cmd->Status(RUN_ERROR);

    engine->_hThread = (HANDLE)_beginthreadex(NULL, 0, threadFunc, engine, 0, NULL);
    if (engine->_hThread == NULL)
    {
        delete engine;
        return false;
    }

    if (complCB)
        return true;

    // If no callback is given then wait until command is ready
    // Since this blocks the UI thread we need a message pump to
    // handle N++ window messages
    while (MsgWaitForMultipleObjects(1, &engine->_hThread, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
    {
        MSG msg;

        // Flush all N++ user input
        while (PeekMessage(&msg, NULL, 0, 0, PM_QS_INPUT | PM_REMOVE));

        // Handle all other messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // N++ thread quit?!?
        if (msg.message == WM_QUIT)
            break;
    }

    delete engine;

    return (cmd->Status() == OK ? true : false);
}


/**
 *  \brief
 */
CmdEngine::~CmdEngine()
{
    if (_complCB)
        _complCB(_cmd);

    if (_hThread)
        CloseHandle(_hThread);
}


/**
 *  \brief
 */
unsigned __stdcall CmdEngine::threadFunc(void* data)
{
    CmdEngine* engine = static_cast<CmdEngine*>(data);
    unsigned r = engine->runProcess();

    if (engine->_complCB)
        delete engine;

    return r;
}


/**
 *  \brief
 */
const TCHAR* CmdEngine::getCmdLine() const
{
    switch (_cmd->_id)
    {
        case CREATE_DATABASE:
            return cCreateDatabaseCmd;
        case UPDATE_SINGLE:
            return cUpdateSingleCmd;
        case AUTOCOMPLETE:
            return cAutoComplCmd;
        case AUTOCOMPLETE_SYMBOL:
            return cAutoComplSymCmd;
        case AUTOCOMPLETE_FILE:
            return cAutoComplFileCmd;
        case FIND_FILE:
            return cFindFileCmd;
        case FIND_DEFINITION:
            return cFindDefinitionCmd;
        case FIND_REFERENCE:
            return cFindReferenceCmd;
        case FIND_SYMBOL:
            return cFindSymbolCmd;
        case GREP:
            return cGrepCmd;
        case VERSION:
            return cVersionCmd;
    }

    return NULL;
}


/**
 *  \brief
 */
void CmdEngine::composeCmd(CText& buf) const
{
    CPath path(DllPath);
    path.StripFilename();
    path += cBinsDir;

    if (_cmd->_id == CREATE_DATABASE || _cmd->_id == VERSION)
        _sntprintf_s(buf.C_str(), buf.Size(), _TRUNCATE, getCmdLine(), path.C_str());
    else
        _sntprintf_s(buf.C_str(), buf.Size(), _TRUNCATE, getCmdLine(), path.C_str(), _cmd->Tag());

    if (_cmd->_id == CREATE_DATABASE || _cmd->_id == UPDATE_SINGLE)
    {
        path += _T("\\gtags.conf");
        if (path.FileExists())
        {
            buf += _T(" --gtagsconf \"");
            buf += path.C_str();
            buf += _T("\"");
            buf += _T(" --gtagslabel=");
            buf += Config.Parser();
        }
    }
    else if (_cmd->_id != VERSION)
    {
        if (_cmd->_matchCase)
            buf += _T(" -M");
        else
            buf += _T(" -i");

        if (!_cmd->_regExp)
            buf += _T(" --literal");
    }
}


/**
 *  \brief
 */
unsigned CmdEngine::runProcess()
{
    CText buf(2048);
    composeCmd(buf);

    DWORD createFlags = NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW;
    const TCHAR* env = NULL;
    const TCHAR* currentDir = _cmd->DbPath();

    CText envVars(_T("GTAGSLIBPATH="));
    if (Config._useLibDb)
        envVars += Config._libDbPath;

    if (_cmd->_id == VERSION)
    {
        currentDir = NULL;
    }
    else if (_cmd->_id == AUTOCOMPLETE || _cmd->_id == FIND_DEFINITION)
    {
        env = envVars.C_str();
        createFlags |= CREATE_UNICODE_ENVIRONMENT;
    }

    ReadPipe errorPipe;
    ReadPipe dataPipe;

    STARTUPINFO si  = {0};
    si.cb           = sizeof(si);
    si.dwFlags      = STARTF_USESTDHANDLES;
    si.hStdError    = errorPipe.GetInputHandle();
    si.hStdOutput   = dataPipe.GetInputHandle();

    PROCESS_INFORMATION pi;
    if (!CreateProcess(NULL, buf.C_str(), NULL, NULL, TRUE, createFlags, (LPVOID)env, currentDir, &si, &pi))
    {
        _cmd->_status = RUN_ERROR;
        return 1;
    }

    SetThreadPriority(pi.hThread, THREAD_PRIORITY_NORMAL);

    if (!errorPipe.Open() || !dataPipe.Open())
    {
        endProcess(pi);
        _cmd->_status = RUN_ERROR;
        return 1;
    }

    CText header(_cmd->Name());
    header += _T(" - \"");
    if (_cmd->_id == CREATE_DATABASE)
        header += _cmd->DbPath();
    else if (_cmd->_id != VERSION)
        header += _cmd->Tag();
    header += _T('\"');

    // Display activity window and block until process is ready or user has cancelled the operation
    bool cancelled = ActivityWin::Show(pi.hProcess, 600, header.C_str(),
            (_cmd->_id == CREATE_DATABASE || _cmd->_id == UPDATE_SINGLE) ? 0 : 300);
    endProcess(pi);

    if (cancelled)
    {
        _cmd->_status = CANCELLED;
        return 1;
    }

    if (!dataPipe.GetOutput().empty())
    {
        _cmd->appendResult(dataPipe.GetOutput());
    }
    else if (!errorPipe.GetOutput().empty())
    {
        _cmd->setResult(errorPipe.GetOutput());

        if (_cmd->_id != CREATE_DATABASE)
        {
            _cmd->_status = FAILED;
            return 1;
        }
    }

    _cmd->_status = OK;

    return 0;
}


/**
 *  \brief
 */
void CmdEngine::endProcess(PROCESS_INFORMATION& pi)
{
    DWORD r;
    GetExitCodeProcess(pi.hProcess, &r);
    if (r == STILL_ACTIVE)
        TerminateProcess(pi.hProcess, 0);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

} // namespace GTags
