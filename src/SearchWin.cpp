/**
 *  \file
 *  \brief  Search input window
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


#pragma comment (lib, "comctl32")


#define WIN32_LEAN_AND_MEAN
#include "SearchWin.h"
#include <windowsx.h>
#include <commctrl.h>
#include <richedit.h>
#include "INpp.h"
#include "CmdEngine.h"


enum HotKeys_t
{
    HK_ACCEPT = 1,
    HK_DECLINE
};


namespace GTags
{

const TCHAR SearchWin::cClassName[]     = _T("SearchWin");
const int SearchWin::cBackgroundColor   = COLOR_WINDOW;
const TCHAR SearchWin::cBtnFont[]       = _T("Tahoma");
const int SearchWin::cWidth             = 400;


SearchWin* SearchWin::SW = NULL;


/**
 *  \brief
 */
void SearchWin::Register()
{
    WNDCLASS wc         = {0};
    wc.style            = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc      = wndProc;
    wc.hInstance        = HMod;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName    = cClassName;

    RegisterClass(&wc);

    INITCOMMONCONTROLSEX icex   = {0};
    icex.dwSize                 = sizeof(icex);
    icex.dwICC                  = ICC_STANDARD_CLASSES;

    InitCommonControlsEx(&icex);
    LoadLibrary(_T("Riched20.dll"));
}


/**
 *  \brief
 */
void SearchWin::Unregister()
{
    UnregisterClass(cClassName, HMod);
    HMODULE hLib = GetModuleHandle(_T("Riched20.dll"));
    if (hLib)
        FreeLibrary(hLib);
}


/**
 *  \brief
 */
void SearchWin::Show(const TCHAR* header, SearchData& sd, bool enRE, bool enMC)
{
    if (SW)
    {
        SetFocus(SW->_hWnd);
        return;
    }

    HWND hOwner = INpp::Get().GetHandle();

    SW = new SearchWin;
    if (SW->composeWindow(hOwner, cWidth, header, sd, enRE, enMC) == NULL)
    {
        delete SW;
        SW = NULL;
    }
}


/**
 *  \brief
 */
RECT SearchWin::adjustSizeAndPos(HWND hOwner, DWORD styleEx, DWORD style,
        int width, int height)
{
    RECT maxWin;
    GetWindowRect(GetDesktopWindow(), &maxWin);

    POINT center;
    if (hOwner)
    {
        RECT biasWin;
        GetWindowRect(hOwner, &biasWin);
        center.x = (biasWin.right + biasWin.left) / 2;
        center.y = (biasWin.bottom + biasWin.top) / 2;
    }
    else
    {
        center.x = (maxWin.right + maxWin.left) / 2;
        center.y = (maxWin.bottom + maxWin.top) / 2;
    }

    RECT win = {0};
    win.right = width;
    win.bottom = height;

    AdjustWindowRectEx(&win, style, FALSE, styleEx);

    width = win.right - win.left;
    height = win.bottom - win.top;

    if (width < maxWin.right - maxWin.left)
    {
        win.left = center.x - width / 2;
        if (win.left < maxWin.left) win.left = maxWin.left;
        win.right = win.left + width;
    }
    else
    {
        win.left = maxWin.left;
        win.right = maxWin.right;
    }

    if (height < maxWin.bottom - maxWin.top)
    {
        win.top = center.y - height / 2;
        if (win.top < maxWin.top) win.top = maxWin.top;
        win.bottom = win.top + height;
    }
    else
    {
        win.top = maxWin.top;
        win.bottom = maxWin.bottom;
    }

    return win;
}


/**
 *  \brief
 */
SearchWin::~SearchWin()
{
    if (_hWnd)
        UnregisterHotKey(_hWnd, HK_ACCEPT);
        UnregisterHotKey(_hWnd, HK_DECLINE);

    if (_hBtnFont)
        DeleteObject(_hBtnFont);
    if (_hTxtFont)
        DeleteObject(_hTxtFont);
}


/**
 *  \brief
 */
HWND SearchWin::composeWindow(HWND hOwner, int width, const TCHAR* header,
        SearchData& sd, bool enMC, bool enRE)
{
    TEXTMETRIC tm;
    HDC hdc = GetWindowDC(hOwner);
    GetTextMetrics(hdc, &tm);
    int txtHeight =
            MulDiv(UIFontSize + 1, GetDeviceCaps(hdc, LOGPIXELSY), 72) +
                tm.tmInternalLeading;
    int btnHeight = MulDiv(UIFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72) +
            tm.tmInternalLeading;
    _hTxtFont = CreateFont(
            -MulDiv(UIFontSize + 1, GetDeviceCaps(hdc, LOGPIXELSY), 72),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            FF_DONTCARE | DEFAULT_PITCH, UIFontName);
    _hBtnFont = CreateFont(
            -MulDiv(UIFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            FF_DONTCARE | DEFAULT_PITCH, cBtnFont);
    ReleaseDC(hOwner, hdc);

    DWORD styleEx = WS_EX_OVERLAPPEDWINDOW | WS_EX_TOOLWINDOW;
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;

    RECT win = adjustSizeAndPos(hOwner, styleEx, style, width,
            txtHeight + btnHeight + 6);
    width = win.right - win.left;

    _hWnd = CreateWindowEx(styleEx, cClassName, header,
            style, win.left, win.top, width, win.bottom - win.top,
            hOwner, NULL, HMod, (LPVOID) this);
    if (_hWnd == NULL)
        return NULL;

    GetClientRect(_hWnd, &win);
    width = win.right - win.left;

    _hEdit = CreateWindowEx(0, RICHEDIT_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NOOLEDRAGDROP,
            0, 0, width, txtHeight,
            _hWnd, NULL, HMod, NULL);

    width = (width - 12) / 3;

    _hRE = CreateWindowEx(0, _T("BUTTON"), _T("RegExp"),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            3, txtHeight + 3, width, btnHeight,
            _hWnd, NULL, HMod, NULL);

    _hMC = CreateWindowEx(0, _T("BUTTON"), _T("MatchCase"),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            width + 6, txtHeight + 3, width, btnHeight,
            _hWnd, NULL, HMod, NULL);

    _hOK = CreateWindowEx(0, _T("BUTTON"), _T("OK"),
            WS_CHILD | WS_VISIBLE | BS_TEXT | BS_DEFPUSHBUTTON,
            2 * width + 9, txtHeight + 3, width, btnHeight,
            _hWnd, NULL, HMod, NULL);

    SendMessage(_hEdit, EM_SETBKGNDCOLOR, 0,
            (LPARAM)GetSysColor(cBackgroundColor));

    CHARFORMAT fmt  = {0};
    fmt.cbSize      = sizeof(fmt);
    fmt.dwMask      = CFM_FACE | CFM_BOLD | CFM_ITALIC | CFM_SIZE;
    fmt.dwEffects   = CFE_AUTOCOLOR;
    fmt.yHeight     = UIFontSize * 20;
    _tcscpy_s(fmt.szFaceName, _countof(fmt.szFaceName), UIFontName);
    SendMessage(_hEdit, EM_SETCHARFORMAT, (WPARAM)SCF_ALL, (LPARAM)&fmt);

    if (_hTxtFont)
        SendMessage(_hEdit, WM_SETFONT, (WPARAM)_hTxtFont, (LPARAM)TRUE);

    SendMessage(_hEdit, EM_EXLIMITTEXT, 0, (LPARAM)(cMaxTagLen - 1));
    SendMessage(_hEdit, EM_SETEVENTMASK, 0, 0);

    int len = _tcslen(sd._str);
    if (len)
    {
        Edit_SetText(_hEdit, sd._str);
        Edit_SetSel(_hEdit, 0, len);
    }

    if (_hBtnFont)
    {
        SendMessage(_hRE, WM_SETFONT, (WPARAM)_hBtnFont, (LPARAM)TRUE);
        SendMessage(_hMC, WM_SETFONT, (WPARAM)_hBtnFont, (LPARAM)TRUE);
    }

    Button_SetCheck(_hRE, sd._regExp ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(_hMC, sd._matchCase ? BST_CHECKED : BST_UNCHECKED);

    if (!enMC)
        EnableWindow(_hMC, FALSE);
    if (!enRE)
        EnableWindow(_hRE, FALSE);

    RegisterHotKey(_hWnd, HK_ACCEPT, 0, VK_ESCAPE);
    RegisterHotKey(_hWnd, HK_DECLINE, 0, VK_RETURN);

    ShowWindow(_hWnd, SW_SHOWNORMAL);
    UpdateWindow(_hWnd);

    return _hWnd;
}


/**
 *  \brief
 */
void SearchWin::onOK()
{
    int len = Edit_GetTextLength(_hEdit);
    if (len)
    {
        TCHAR tag[cMaxTagLen];
        Edit_GetText(_hEdit, tag, len + 1);
        bool re = (Button_GetCheck(_hRE) == BST_CHECKED) ? true : false;
        bool mc = (Button_GetCheck(_hMC) == BST_CHECKED) ? true : false;

        std::shared_ptr<Cmd>
                cmd(new Cmd(FIND_FILE, cFindFile, db, tag, re, mc));
        CmdEngine::Run(cmd, showResult);
    }

    SendMessage(_hWnd, WM_CLOSE, 0, 0);
}


/**
 *  \brief
 */
LRESULT APIENTRY SearchWin::wndProc(HWND hwnd, UINT umsg,
        WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
        case WM_CREATE:
        return 0;

        case WM_SETFOCUS:
            SetFocus(SW->_hEdit);
        return 0;

        case WM_HOTKEY:
            if (HIWORD(lparam) == VK_ESCAPE)
            {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (HIWORD(lparam) == VK_RETURN)
            {
                SW->onOK();
                return 0;
            }
        break;

        case WM_COMMAND:
            if (HIWORD(wparam) == EN_KILLFOCUS)
            {
                DestroyCaret();
                return 0;
            }
            if (HIWORD(wparam) == BN_CLICKED)
            {
                if ((HWND)lparam == SW->_hOK)
                {
                    SW->onOK();
                    return 0;
                }
            }
        break;

        case WM_DESTROY:
        {
            DestroyCaret();
            delete SW;
            SW = NULL;
        }
        return 0;
    }

    return DefWindowProc(hwnd, umsg, wparam, lparam);
}

} // namespace GTags
