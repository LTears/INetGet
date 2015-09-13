///////////////////////////////////////////////////////////////////////////////
// INetGet - Lightweight command-line front-end to WinInet API
// Copyright (C) 2015 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version, but always including the *additional*
// restrictions defined in the "License.txt" file.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Client_Abstract.h"

class HttpClient : public AbstractClient
{
public:
	//Constructor & destructor
	HttpClient(const bool &disableProxy = false, const std::wstring &userAgentStr = std::wstring(), const bool &verbose = false);
	virtual ~HttpClient(void);

	//Connection handling
	virtual bool open(const http_verb_t &verb, const bool &secure, const std::wstring &hostName, const uint16_t &portNo, const std::wstring &userName, const std::wstring &password, const std::wstring &path);
	virtual bool close(void);

	//Fetch result
	virtual bool result(bool &success, uint32_t &status_code, uint32_t &file_size, std::wstring &content_type, std::wstring &content_encd);

private:
	//Create connection/request
	bool connect(const std::wstring &hostName, const uint16_t &portNo, const std::wstring &userName, const std::wstring &password);
	bool create_request(const bool &secure, const http_verb_t &verb, const std::wstring &path);

	//Status handler
	virtual void update_status(const uint32_t &status, const uintptr_t &information);

	//Utilities
	static const wchar_t *get_verb(const http_verb_t &verb);
	static bool get_header_int(void *const request, const uint32_t type, uint32_t &value);
	static bool get_header_str(void *const request, const uint32_t type, std::wstring &value);

	//Handles
	void *m_hConnection;
	void *m_hRequest;

	//Current status
	uint32_t m_current_status;
};

