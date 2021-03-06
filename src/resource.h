#include <tchar.h>

#define VER_VERSION         4,0,0,0
#define VER_VERSION_STR     _T("4.0.0\0")

#define VER_AUTHOR          _T("Pavel Nedev\0")
#define VER_COPYRIGHT       _T("Copyright(C) 2014-2015 Pavel Nedev\0")

#define VER_PLUGIN_NAME     _T("NppGTags\0")
#define VER_DLLFILENAME     _T("NppGTags.dll\0")

#ifndef DEVELOPMENT

#define VER_DESCRIPTION     _T("GTags plugin for Notepad++\0")

#else

#define VER_DESCRIPTION     _T("GTags plugin for Notepad++ (dev. version)\0")

#endif
