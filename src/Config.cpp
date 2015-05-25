/**
 *  \file
 *  \brief  GTags config class
 *
 *  \author  Pavel Nedev <pg.nedev@gmail.com>
 *
 *  \section COPYRIGHT
 *  Copyright(C) 2015 Pavel Nedev
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


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include "Common.h"
#include "INpp.h"
#include "Config.h"
#include "GTags.h"


namespace GTags
{

const TCHAR CConfig::cDefaultParser[]   = _T("default");
const TCHAR CConfig::cCtagsParser[]     = _T("ctags");
const TCHAR CConfig::cPygmentsParser[]  = _T("pygments");

const TCHAR* CConfig::cParsers[CConfig::PARSER_LIST_END] = {
    CConfig::cDefaultParser,
    CConfig::cCtagsParser,
    CConfig::cPygmentsParser
};

const TCHAR CConfig::cParserKey[]       = _T("Parser = ");
const TCHAR CConfig::cAutoUpdateKey[]   = _T("AutoUpdate = ");
const TCHAR CConfig::cUseLibraryKey[]   = _T("UseLibrary = ");
const TCHAR CConfig::cLibraryPathKey[]  = _T("LibraryPath = ");


/**
 *  \brief
 */
void CConfig::GetDefaultCfgFile(CPath& cfgFile)
{
    TCHAR cfgDir[MAX_PATH];
    INpp::Get().GetPluginsConfDir(_countof(cfgDir), cfgDir);

    cfgFile = cfgDir;
    cfgFile += _T("\\");
    cfgFile += cPluginName;
    cfgFile += _T(".cfg");
}


/**
 *  \brief
 */
void CConfig::SetDefaults()
{
    _parserIdx = DEFAULT_PARSER;
    _autoUpdate = true;
    _useLibDb = false;
    _libDbPath.Clear();
}


/**
 *  \brief
 */
bool CConfig::LoadFromFile(const TCHAR* file)
{
    CPath cfgFile(file);
    if (file == NULL)
    {
        GetDefaultCfgFile(cfgFile);
        if (!cfgFile.Exists())
            return true;
    }

    FILE* fp = _tfopen(cfgFile.C_str(), _T("rt"));
    if (fp == NULL)
        return false;

    TCHAR line[4096];
    while (fgetws(line, _countof(line), fp))
    {
        // Comment line
        if (line[0] == _T('#'))
            continue;

        if (!_tcsncmp(line, cParserKey, _countof(cParserKey) - 1))
        {
            unsigned pos = _countof(cParserKey) - 1;
            if (!_tcsncmp(&line[pos], cCtagsParser,
                    _countof(cCtagsParser) - 1))
                _parserIdx = CTAGS_PARSER;
            else if (!_tcsncmp(&line[pos], cPygmentsParser,
                    _countof(cPygmentsParser) - 1))
                _parserIdx = PYGMENTS_PARSER;
            else
                _parserIdx = DEFAULT_PARSER;
        }
        else if (!_tcsncmp(line, cAutoUpdateKey, _countof(cAutoUpdateKey) - 1))
        {
            unsigned pos = _countof(cAutoUpdateKey) - 1;
            if (!_tcsncmp(&line[pos], _T("yes"), _countof(_T("yes")) - 1))
                _autoUpdate = true;
            else
                _autoUpdate = false;
        }
        else if (!_tcsncmp(line, cUseLibraryKey, _countof(cUseLibraryKey) - 1))
        {
            unsigned pos = _countof(cUseLibraryKey) - 1;
            if (!_tcsncmp(&line[pos], _T("yes"), _countof(_T("yes")) - 1))
                _useLibDb = true;
            else
                _useLibDb = false;
        }
        else if (!_tcsncmp(line, cLibraryPathKey,
                _countof(cLibraryPathKey) - 1))
        {
            unsigned pos = _countof(cLibraryPathKey) - 1;
            _libDbPath = &line[pos];
        }
        else
        {
            SetDefaults();
            fclose(fp);
            return false;
        }
    }

    fclose(fp);
    return true;
}


/**
 *  \brief
 */
bool CConfig::SaveToFile(const TCHAR* file) const
{
    CPath cfgFile(file);
    if (file == NULL)
        GetDefaultCfgFile(cfgFile);

    FILE* fp = _tfopen(cfgFile.C_str(), _T("wt"));
    if (fp == NULL)
        return false;

    bool success = false;
    if (_ftprintf_s(fp, _T("%s%s\n"), cParserKey, Parser()) > 0)
    if (_ftprintf_s(fp, _T("%s%s\n"), cAutoUpdateKey,
            (_autoUpdate ? _T("yes") : _T("no"))) > 0)
    if (_ftprintf_s(fp, _T("%s%s\n"), cUseLibraryKey,
            (_useLibDb ? _T("yes") : _T("no"))) > 0)
    if (_ftprintf_s(fp, _T("%s%s\n"), cLibraryPathKey, _libDbPath.C_str()) > 0)
        success = true;

    fclose(fp);
    return success;
}

} // namespace GTags
