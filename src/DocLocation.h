/**
 *  \file
 *  \brief  Stores documents locations in a stack to be visited at later time
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


#pragma once


#include <tchar.h>
#include <vector>
#include "AutoLock.h"
#include "Common.h"


/**
 *  \class  DocLocation
 *  \brief
 */
class DocLocation
{
public:
    static DocLocation& Get() { return Instance; }

    unsigned GetDepth() const { return _maxDepth; }
    void SetDepth(unsigned depth);
    void Push();
    void Back();
    void Forward();

private:
    /**
     *  \struct  Location
     *  \brief
     */
    struct Location
    {
        CPath   _filePath;
        long    _posInFile;

        inline const Location& operator=(const Location& loc)
        {
            _posInFile = loc._posInFile;
            _filePath = loc._filePath;
            return loc;
        }

        inline bool operator==(const Location& loc) const
        {
            return ((_posInFile == loc._posInFile) && (_filePath == loc._filePath));
        }
    };

    static const unsigned cInitialDepth;

    static DocLocation Instance;

    DocLocation() : _maxDepth(cInitialDepth), _backLocIdx(-1) {}
    DocLocation(const DocLocation&);
    ~DocLocation() {}

    void swapView(Location& loc);

    unsigned                _maxDepth;
    int                     _backLocIdx;
    Mutex                   _lock;
    std::vector<Location>   _locList;
};
