///////////////////////////////////////////////////////////////////////////////
// INetGet - Lightweight command-line front-end to WinINet API
// Copyright (C) 2015 LoRd_MuldeR <MuldeR2@GMX.de>
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

//Internal
#include "Compat.h"
#include "Utils.h"
#include "Version.h"
#include "URL.h"
#include "Params.h"
#include "Client_FTP.h"
#include "Client_HTTP.h"
#include "Sink_File.h"
#include "Sink_StdOut.h"
#include "Sink_Null.h"
#include "Timer.h"
#include "Average.h"
#include "Thread.h"

//Win32
#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <WinINet.h>

//CRT
#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <memory>
#include <sstream>
#include <algorithm>

//Globals
static std::unique_ptr<AbstractClient> g_client;
static std::unique_ptr<AbstractSink>   g_sink;

//Externals
namespace Zero
{
	extern Sync::Signal g_sigUserAbort;
}

//=============================================================================
// HELPER MACROS
//=============================================================================

#define CHECK_USER_ABORT() do \
{ \
	if(Zero::g_sigUserAbort.get()) \
	{ \
		std::wcerr << L"\n\nSIGINT: Operation aborted by the user !!!" << std::endl; \
		exit(EXIT_FAILURE); \
	} \
} \
while(0)

#define TRIGGER_SYSTEM_SOUND(X,Y) do \
{ \
	if((X))  \
	{ \
		Utils::trigger_system_sound((Y)); \
	} \
} \
while(0)

//=============================================================================
// INTERNAL FUNCTIONS
//=============================================================================

static const wchar_t *const UPDATE_INFO = L"Check http://muldersoft.com/ or https://github.com/lordmulder/ for updates!\n";

static std::string stdin_get_line(void)
{
	std::string line;
	std::getline(std::cin, line);
	return line;
}

static std::wstring build_version_string(void)
{
	std::wostringstream str;
	if(VERSION_PATCH > 0)
	{
		str << std::setw(0) << VERSION_MAJOR << L'.' << std::setfill(L'0') << std::setw(2) << VERSION_MINOR << L'-' << std::setw(0) << VERSION_PATCH;
	}
	else
	{
		str << std::setw(0) << VERSION_MAJOR << L'.' << std::setfill(L'0') << std::setw(2) << VERSION_MINOR;
	}
	return str.str();
}

static void print_logo(void)
{
	const std::ios::fmtflags stateBackup(std::wcout.flags());
	std::wcerr << L"\nINetGet v" << build_version_string() << L" - Lightweight command-line front-end to WinINet\n"
		<< L"Copyright (c) " << &BUILD_DATE[7] << L" LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.\n"
		<< L"Built on " << BUILD_DATE << L" at " << BUILD_TIME << L", " << BUILD_COMP << L", Win-" << BUILD_ARCH << L", " << BUILD_CONF << L'\n' << std::endl;
	std::wcout.flags(stateBackup);

	const time_t build_time = Utils::decode_date_str(BUILD_DATE);
	if(build_time > 0)
	{
		const time_t now = time(NULL);
		if((now > build_time) && ((now - build_time) >= 31556926i64))
		{
			std::wcerr << L"NOTE: This binary is more than a year old, there probably is a new version.\n" << UPDATE_INFO << std::endl;
		}
	}
}

static void print_help_screen(void)
{
	const std::ios::fmtflags stateBackup(std::wcout.flags());
	std::wcerr << UPDATE_INFO
		<< L'\n'
		<< L"Usage:\n"
		<< L"  INetGet.exe [options] <source_addr> <output_file>\n"
		<< L'\n'
		<< L"Required:\n"
		<< L"  <source_addr> : Specifies the source internet address (URL)\n"
		<< L"  <output_file> : Specifies the output file, you can specify \"-\" for STDOUT\n"
		<< L'\n'
		<< L"Optional:\n"
		<< L"  --verb=<verb> : Specify the HTTP method (verb) to be used, default is GET\n"
		<< L"  --data=<data> : Append data to request, in 'x-www-form-urlencoded' format\n"
		<< L"  --no-proxy    : Don't use proxy server for address resolution\n"
		<< L"  --agent=<str> : Overwrite the default 'user agent' string used by INetGet\n"
		<< L"  --no-redir    : Disable automatic redirection, enabled by default\n"
		<< L"  --insecure    : Don't fail, if server certificate is invalid (HTTPS only)\n"
		<< L"  --refer=<url> : Include the given 'referrer' address in the request\n"
		<< L"  --notify      : Trigger a system sound when the download completed/failed\n"
		<< L"  --time-cn=<n> : Specifies the connection timeout, in seconds\n"
		<< L"  --time-rc=<n> : Specifies the receive timeout, in seconds\n"
		<< L"  --timeout=<n> : Specifies the connection & receive timeouts, in seconds\n"
		<< L"  --retry=<n>   : Specifies the max. number of connection attempts\n"
		<< L"  --no-retry    : Do not retry, if the connection failed (i.e. '--retry=0')\n"
		<< L"  --force-crl   : Make the connection fail, if CRL could *not* be retrieved\n"
		<< L"  --set-ftime   : Set the file's Creation/LastWrite time to 'Last-Modified'\n"
		<< L"  --update      : Update (replace) local file, iff server has newer version\n"
		<< L"  --keep-failed : Keep the incomplete output file, when download has failed\n"
		<< L"  --config=<cf> : Read INetGet options from specified configuration file(s)\n"
		<< L"  --help        : Show this help screen\n"
		<< L"  --slunk       : Enable slunk mode, this is intended for kendo master only\n"
		<< L"  --verbose     : Enable detailed diagnostic output (for debugging)\n"
		<< L'\n'
		<< L"Examples:\n"
		<< L"  INetGet.exe http://www.warr.org/buckethead.html output.html\n"
		<< L"  INetGet.exe --verb=POST --data=\"foo=bar\" http://localhost/form.php result\n"
		<< std::endl;
	std::wcout.flags(stateBackup);
}

static bool create_client(std::unique_ptr<AbstractClient> &client, AbstractListener &listener, const int16_t scheme_id, const Params &params)
{
	switch(scheme_id)
	{
	case INTERNET_SCHEME_FTP:
		client.reset(new FtpClient(Zero::g_sigUserAbort, params.getDisableProxy(), params.getUserAgent(), params.getVerboseMode()));
		break;
	case INTERNET_SCHEME_HTTP:
	case INTERNET_SCHEME_HTTPS:
		client.reset(new HttpClient(Zero::g_sigUserAbort, params.getDisableProxy(), params.getUserAgent(), params.getDisableRedir(), params.getInsecure(), params.getForceCrl(), params.getTimeoutCon(), params.getTimeoutRcv(), params.getRetryCount(), params.getVerboseMode()));
		break;
	default:
		client.reset();
		break;
	}

	if(client)
	{
		client->add_listener(listener);
		return true;
	}

	return false;
}

static bool create_sink(std::unique_ptr<AbstractSink> &sink, const std::wstring fileName, const uint64_t &timestamp, const bool &keep_failed)
{
	if(_wcsicmp(fileName.c_str(), L"-") == 0)
	{
		sink.reset(new StdOutSink());
	}
	else if(_wcsicmp(fileName.c_str(), L"NUL") == 0)
	{
		sink.reset(new NullSink());
	}
	else
	{
		sink.reset(new FileSink(fileName, timestamp, keep_failed));
	}

	return sink ? sink->open() : false;
}

static void print_response_info(const uint32_t &status_code, const uint64_t &file_size,	const uint64_t &time_stamp, const std::wstring &content_type, const std::wstring &content_encd)
{
	static const wchar_t *const UNSPECIFIED = L"<N/A>";

	std::wcerr << L"--> HTTP Status code : " << status_code << L" [" << Utils::status_to_string(status_code) << "]\n";
	std::wcerr << L"--> Content type     : " << (content_type.empty() ? UNSPECIFIED : content_type) << L'\n';
	std::wcerr << L"--> Content encoding : " << (content_encd.empty() ? UNSPECIFIED : content_encd) << L'\n';
	std::wcerr << L"--> Content length   : " << ((file_size == AbstractClient::SIZE_UNKNOWN) ? UNSPECIFIED : std::to_wstring(file_size)) << L" Byte\n";
	std::wcerr << L"--> Last modified TS : " << ((time_stamp == AbstractClient::TIME_UNKNOWN) ? UNSPECIFIED : Utils::timestamp_to_str(time_stamp)) << L'\n';
	std::wcerr << std::endl;
}

static inline void print_progress(const std::wstring url_string, uint64_t &total_bytes, const uint64_t &file_size, Timer &timer_update, const double &current_rate, uint8_t &index, const bool &force = false)
{
	static const wchar_t SPINNER[4] = { L'-', L'\\', L'/', L'-' };

	if(force || (timer_update.query() > 0.2))
	{
		const std::ios::fmtflags stateBackup(std::wcout.flags());
		std::wcerr << std::setprecision(1) << std::fixed << std::setw(0) << L"\r[" << SPINNER[(index++) & 3] << L"] ";

		if(file_size != AbstractClient::SIZE_UNKNOWN)
		{
			const double percent = (file_size > 0.0) ? (100.0 * std::min(1.0, double(total_bytes) / double(file_size))) : 100.0;

			if(!ISNAN(current_rate))
			{
				if(current_rate > 0.0)
				{
					const double time_left = (total_bytes < file_size) ? (double(file_size - total_bytes) / current_rate) : 0.0;
					if(time_left > 3)
					{
						std::wcerr << percent << L"% of " << Utils::nbytes_to_string(double(file_size)) << L" received, " << Utils::nbytes_to_string(current_rate) << L"/s, " << Utils::second_to_string(time_left) << L" remaining...";
					}
					else
					{
						std::wcerr << percent << L"% of " << Utils::nbytes_to_string(double(file_size)) << L" received, " << Utils::nbytes_to_string(current_rate) << L"/s, almost finished...";
					}
				}
				else
				{
					std::wcerr << percent << L"% of " << Utils::nbytes_to_string(double(file_size)) << L" received, " << Utils::nbytes_to_string(current_rate) << L"/s, please stand by...";
				}
			}
			else
			{
				std::wcerr << percent << L"% of " << Utils::nbytes_to_string(double(file_size)) << L" received, please stand by...";
			}

			std::wostringstream title;
			title << std::setprecision(1) << std::fixed << std::setw(0) << L"INetGet [" << percent << L"% of " << Utils::nbytes_to_string(double(file_size)) << L"] - " << url_string;
			Utils::set_console_title(title.str());
		}
		else
		{
			if(!ISNAN(current_rate))
			{
				std::wcerr << Utils::nbytes_to_string(double(total_bytes)) << L" received, " << Utils::nbytes_to_string(current_rate) << L"/s, please stand by...";
			}
			else
			{
				std::wcerr << Utils::nbytes_to_string(double(total_bytes)) << L" received, please stand by...";
			}

			std::wostringstream title;
			title << L"INetGet [" << Utils::nbytes_to_string(double(total_bytes)) << L"] - " << url_string;
			Utils::set_console_title(title.str());
		}

		std::wcerr << L"    " << std::flush;
		std::wcout.flags(stateBackup);
		timer_update.reset();
	}
}

//=============================================================================
// STATUS LISTENER
//=============================================================================

class StatusListener : public AbstractListener
{
protected:
	virtual void onMessage(const std::wstring message)
	{
		Sync::Locker locker(m_mutex);
		if(!Zero::g_sigUserAbort.get())
		{
			std::wcerr << L"--> " << message << std::endl;
		}
	}
private:
	Sync::Mutex m_mutex;
};

//=============================================================================
// THREAD TEST
//=============================================================================

class MyThread : public Thread
{
protected:
	virtual uint32_t main(void)
	{
		while(!is_stopped())
		{
			std::wcerr << L"TEST" << std::endl;
			Sleep(500);
		}

		return 0;
	}
};

//=============================================================================
// PROCESS
//=============================================================================

static int transfer_file(const std::wstring &url_string, const uint64_t &file_size, const uint64_t &timestamp, const std::wstring &outFileName, const bool &alert, const bool &keep_failed)
{
	//Open output file
	if(!create_sink(g_sink, outFileName, timestamp, keep_failed))
	{
		TRIGGER_SYSTEM_SOUND(alert, false);
		std::wcerr << L"ERROR: Failed to open the sink, unable to download file!\n" << std::endl;
		return EXIT_FAILURE;
	}

	//Allocate buffer
	static const size_t BUFF_SIZE = 16384;
	std::unique_ptr<uint8_t[]> buffer(new uint8_t[BUFF_SIZE]);

	//Initialize local variables
	Timer timer_start, timer_transfer, timer_update;
	uint64_t total_bytes = 0ui64, transferred_bytes = 0ui64;
	uint8_t index = 0;
	Average rate_estimate(125);
	bool eof_flag = false;
	double current_rate = std::numeric_limits<double>::quiet_NaN();;

	//Print progress
	std::wcerr << L"Download in progress:" << std::endl;
	print_progress(url_string, total_bytes, file_size, timer_update, current_rate, index, true);

	//Download the file now!
	while(!eof_flag)
	{
		size_t bytes_read = 0;

		CHECK_USER_ABORT();
		if(!g_client->read_data(buffer.get(), BUFF_SIZE, bytes_read, eof_flag))
		{
			std::wcerr << L"\b\b\bfailed\n\n" << g_client->get_error_text() << L'\n' << std::endl;
			std::wcerr << L"ERROR: Failed to receive incoming data, download has been aborted!\n" << std::endl;
			g_sink->close(false);
			TRIGGER_SYSTEM_SOUND(alert, false);
			return EXIT_FAILURE;
		}

		if(bytes_read > 0)
		{
			CHECK_USER_ABORT();
			transferred_bytes += bytes_read;
			total_bytes += bytes_read;

			const double interval = timer_transfer.query();
			if(interval >= 0.5)
			{
				current_rate = rate_estimate.update(double(transferred_bytes) / interval);
				timer_transfer.reset();
				transferred_bytes = 0ui64;
			}

			if(!g_sink->write(buffer.get(), bytes_read))
			{
				std::wcerr << L"\b\b\bfailed\n\n" << std::endl;
				std::wcerr << L"ERROR: Failed to write data to sink, download has been aborted!\n" << std::endl;
				g_sink->close(false);
				TRIGGER_SYSTEM_SOUND(alert, false);
				return EXIT_FAILURE;
			}
		}

		CHECK_USER_ABORT();
		print_progress(url_string, total_bytes, file_size, timer_update, current_rate, index);
	}

	//Finalize progress
	print_progress(url_string, total_bytes, file_size, timer_update, current_rate, index, true);
	const double total_time = timer_start.query(), average_rate = total_bytes / total_time;

	//Flush and close the sink
	std::wcerr << L"\b\b\bdone\n\nFlushing output buffers... " << std::flush;
	g_sink->close(true);

	//Report total time and average download rate
	TRIGGER_SYSTEM_SOUND(alert, true);
	std::wcerr << L"done\n\nDownload completed in " << ((total_time >= 1.0) ? Utils::second_to_string(total_time) : L"no time") << L" (avg. rate: " << Utils::nbytes_to_string(average_rate) << L"/s).\n" << std::endl;

	//Done
	return EXIT_SUCCESS;
}

static int retrieve_url(const std::wstring &url_string, const http_verb_t &http_verb, const URL &url, const std::wstring &post_data, const std::wstring &referrer, const std::wstring &outFileName, const bool &set_ftime, const bool &update_mode, const bool &alert, const bool &keep_failed)
{
	//Initialize the post data string
	const std::string post_data_encoded = post_data.empty() ? std::string() : ((post_data.compare(L"-") != 0) ? URL::urlEncode(Utils::wide_str_to_utf8(post_data)) : URL::urlEncode(stdin_get_line()));

	//Detect filestamp of existing file
	const uint64_t timestamp_existing = update_mode ? Utils::get_file_time(outFileName) : AbstractClient::TIME_UNKNOWN;
	if(update_mode && (timestamp_existing == AbstractClient::TIME_UNKNOWN))
	{
		std::wcerr << L"WARNING: Local file does not exist yet, going to download unconditionally!\n" << std::endl;
	}

	//Create the HTTPS connection/request
	std::wcerr << L"Connecting to " << url.getHostName() << L':' << url.getPortNo() << L", please wait..." << std::endl;
	if(!g_client->open(http_verb, url, post_data_encoded, referrer, timestamp_existing))
	{
		TRIGGER_SYSTEM_SOUND(alert, false);
		std::wcerr << L'\n' << g_client->get_error_text() << L'\n' << std::endl;
		std::wcerr << "ERROR: Connection could not be established!\n" << std::endl;
		return EXIT_FAILURE;
	}

	//Add extra space
	std::wcerr << std::endl;

	//Initialize local variables
	bool success;
	uint32_t status_code;
	std::wstring content_type, content_encd;
	uint64_t file_size, timestamp;

	//Query result information
	CHECK_USER_ABORT();
	if(!g_client->result(success, status_code, file_size, timestamp, content_type, content_encd))
	{
		TRIGGER_SYSTEM_SOUND(alert, false);
		std::wcerr << g_client->get_error_text() << L'\n' << std::endl;
		std::wcerr << "ERROR: Failed to query the response status!\n" << std::endl;
		return EXIT_FAILURE;
	}

	//Skip download this time?
	if(update_mode && (status_code == 304))
	{
		TRIGGER_SYSTEM_SOUND(alert, true);
		std::wcerr << L"SKIPPED: Server currently does *not* provide a newer version of the file." << std::endl;
		std::wcerr << L"         Version created at '" << Utils::timestamp_to_str(timestamp_existing) << L"' was retained.\n" << std::endl;
		return EXIT_SUCCESS;
	}

	//Print some status information
	std::wcerr << L"HTTP response successfully received from server:\n";
	print_response_info(status_code, file_size, timestamp, content_type, content_encd);

	//Request successful?
	if(!success)
	{
		TRIGGER_SYSTEM_SOUND(alert, false);
		std::wcerr << "ERROR: The server failed to handle this request! [Status " << status_code << "]\n" << std::endl;
		return EXIT_FAILURE;
	}

	CHECK_USER_ABORT();
	return transfer_file(url_string, file_size, (set_ftime ? timestamp : 0), outFileName, alert, keep_failed);
}

//=============================================================================
// MAIN
//=============================================================================

int inetget_main(const int argc, const wchar_t *const argv[])
{
	//Print application info
	print_logo();

	//Initialize parameters
	Params params;

	//Load configuration file, if it exists
	const std::wstring config_file = Utils::exe_path(L".cfg");
	if(Utils::file_exists(config_file))
	{
		if(!params.load_conf_file(config_file))
		{
			std::wcerr << "Invalid configuration file, refer to the documentation for details!\n" << std::endl;
			return EXIT_FAILURE;
		}
	}

	//Parse command-line parameters
	if(!params.parse_cli_args(argc, argv))
	{
		std::wcerr << "Invalid command-line arguments, type \"INetGet.exe --help\" for details!\n" << std::endl;
		return EXIT_FAILURE;
	}

	//Show help screen, if it was requested
	if(params.getShowHelp())
	{
		print_help_screen();
		return EXIT_SUCCESS;
	}

	//Parse the specified source URL
	const std::wstring source = (params.getSource().compare(L"-") == 0) ? Utils::utf8_to_wide_str(stdin_get_line()) : params.getSource();
	URL url(source);
	if(!url.isComplete())
	{
		std::wcerr << "The specified URL is incomplete or unsupported:\n" << source << L'\n' << std::endl;
		return EXIT_FAILURE;
	}

	//Print request URL
	const std::wstring url_string = url.toString();
	std::wcerr << L"Request address:\n" << url.toString() << L'\n' << std::endl;
	Utils::set_console_title(std::wstring(L"INetGet - ").append(url_string));

	//Create the HTTP(S) client
	StatusListener listener;
	if(!create_client(g_client, listener, url.getScheme(), params))
	{
		std::wcerr << "Specified protocol is unsupported! Only HTTP(S) and FTP are allowed.\n" << std::endl;
		return EXIT_FAILURE;
	}

	//Retrieve the URL
	return retrieve_url(url_string, params.getHttpVerb(), url, params.getPostData(), params.getReferrer(), params.getOutput(), params.getSetTimestamp(), params.getUpdateMode(), params.getEnableAlert(), params.getKeepFailed());
}
