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

//Internal
#include "Version.h"
#include "URL.h"
#include "Utils.h"
#include "Params.h"
#include "Client_Abstract.h"

//Win32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <WinInet.h>

//CRT
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <io.h>
#include <fcntl.h>

#if (defined(NDEBUG) && defined(_DEBUG)) || ((!defined(NDEBUG)) && (!defined(_DEBUG)))
#error Inconsistent DEBUG flags!
#endif

//=============================================================================
// INTERNAL FUNCTIONS
//=============================================================================

static void printLogo(void)
{
	const std::ios::fmtflags stateBackup(std::wcout.flags());
	std::wcerr << L"\nINetGet v"
		<< std::setw(0) << VER_INETGET_MAJOR << L'.' << std::setfill(L'0') << std::setw(2) << VER_INETGET_MINOR << L'_' << std::setw(0) << VER_INETGET_PATCH
		<< " - Lightweight command-line front-end to WinInet\n"
		<< L"Copyright (c) " << &BUILD_DATE[7] << L" LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.\n"
		<< L"Built on " << BUILD_DATE << " at " << BUILD_TIME << " with " << BUILD_COMP << " (" << BUILD_ARCH << ")\n" << std::endl;
	std::wcout.flags(stateBackup);
}

//=============================================================================
// HELP SCREEN
//=============================================================================

void print_help_screen(void)
{
	const std::ios::fmtflags stateBackup(std::wcout.flags());
	std::wcerr << L"Usage:\n"
		<< L"  INetGet.exe [options] <source_url> <output_file>\n"
		<< std::endl;
	std::wcout.flags(stateBackup);
}

//=============================================================================
// MAIN
//=============================================================================

static int inetget_main(const int argc, const wchar_t *const argv[])
{
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_U8TEXT);

	printLogo();

	Params params;
	if(!params.initialize(argc, argv))
	{
		std::wcerr << "Invalid command-line arguments, type \"INetGet.exe --help\" for details!\n" << std::endl;
		return EXIT_FAILURE;
	}

	if(params.getShowHelp())
	{
		print_help_screen();
		return EXIT_SUCCESS;
	}

	URL url(params.getSource());
	if(!url.isComplete())
	{
		std::wcerr << "The specified URL is incomplete or unsupported:\n" << params.getSource() << L'\n' << std::endl;
		return EXIT_FAILURE;
	}
	if((url.getScheme() != INTERNET_SCHEME_FTP) && (url.getScheme() != INTERNET_SCHEME_HTTP) && (url.getScheme() != INTERNET_SCHEME_HTTPS))
	{
		std::wcerr << "Specified protocol is unsupported! Only FTP, HTTP and HTTPS are allowed.\n" << std::endl;
		return EXIT_FAILURE;
	}

	AbstractClient client;
	if(!client.init_client())
	{
		std::wcerr << "FATAL ERROR: Failed to initialize WinInet API!\n" << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

//=============================================================================
// ERROR HANDLING
//=============================================================================

static void my_invalid_param_handler(const wchar_t* exp, const wchar_t* fun, const wchar_t* fil, unsigned int, uintptr_t)
{
	std::wcerr << "\n\nGURU MEDITATION: Invalid parameter handler invoked, application will exit!\n" << std::endl;
	_exit(EXIT_FAILURE);
}

static LONG WINAPI my_exception_handler(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	std::wcerr << "\n\nGURU MEDITATION: Unhandeled exception handler invoked, application will exit!\n" << std::endl;
	_exit(EXIT_FAILURE);
	return EXCEPTION_EXECUTE_HANDLER;
}

static void setup_error_handlers(void)
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
	SetUnhandledExceptionFilter(my_exception_handler);
	_set_invalid_parameter_handler(my_invalid_param_handler);
	SetDllDirectoryW(L""); /*don'tload DLL from "current" directory*/
}

//=============================================================================
// ENTRY POINT
//=============================================================================

static int wmain_ex(const int argc, const wchar_t *const argv[])
{
	int ret = -1;
	try
	{
		ret = inetget_main(argc, argv);
	}
	catch(std::exception &err)
	{
		std::wcerr << "\n\nUNHANDELED EXCEPTION: " << err.what() << '\n' << std::endl;
		_exit(EXIT_FAILURE);
	}
	catch(...)
	{
		std::wcerr << "\n\nUNHANDELED EXCEPTION: Unknown C++ exception error!\n" << std::endl;
		_exit(EXIT_FAILURE);
	}
	return ret;
}

int wmain(int argc, wchar_t* argv[])
{
	int ret = -1;
#ifdef NDEBUG
	__try
	{
		setup_error_handlers();
		ret = wmain_ex(argc, argv);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		std::wcerr << "\n\nGURU MEDITATION: Unhandeled exception error, application will exit!\n" << std::endl;
	}
#else
	ret =  inetget_main(argc, argv);
#endif
	return ret;
}
