///////////////////////////////////////////////////////////////////////////////
// INetGet - Lightweight command-line front-end to WinINet API
// Copyright (C) 2015-2018 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// See https://www.gnu.org/licenses/gpl-2.0-standalone.html for details!
///////////////////////////////////////////////////////////////////////////////

#include "Sink_StdOut.h"

//Internal
#include "Utils.h"

//CRT
#include <cstdio>
#include <iostream>

//=============================================================================
// CONSTRUCTOR / DESTRUCTOR
//=============================================================================

StdOutSink::StdOutSink(void)
:
	m_isOpen(false)
{
}

StdOutSink::~StdOutSink(void)
{
	close(false);
}

//=============================================================================
// OPEN / CLOSE
//=============================================================================

bool StdOutSink::open(void)
{
	Sync::Locker locker(m_mutex);
	if(!ferror(stdout))
	{
		m_isOpen  = true;
		return true;
	}
	return false;
}

bool StdOutSink::close(const bool& /*success*/)
{
	Sync::Locker locker(m_mutex);
	fflush(stdout);
	m_isOpen  = false;
	return true;
}

//=============================================================================
// WRITE
//=============================================================================

bool StdOutSink::write(uint8_t *const buffer, const size_t &count)
{
	Sync::Locker locker(m_mutex);
	if(m_isOpen)
	{
		if(count > 0)
		{
			const size_t bytesWritten = fwrite(buffer, sizeof(uint8_t), count, stdout);
			if(bytesWritten < count)
			{
				const int error_code = errno;
				std::wcerr << L"An I/O error occurred while trying to write to STDOUT:\n" << Utils::crt_error_string(error_code) << L'\n' << std::endl;
				return false;
			}
		}
		return true;
	}
	return false;
}
