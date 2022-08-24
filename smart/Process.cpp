/// \file  Process.cpp
/// \brief	Definitions of functions for process control.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>		// Win32 API
#else
#	include <sys/time.h>
#endif

#include <vector>	// std::vector
#include <stdexcept>	// std::runtime_error

#if !defined(WIN32)
#include <dirent.h>	// struct DIR, opendir, etc.
#include <pthread.h>	// pthread_create
#endif
#include <stdio.h>	// fprintf
#include <stdio.h>	// STDIN_FILENO, etc.
#if !defined(WIN32)
#include <unistd.h>	// open, fork, execlp, pipe, read
#include <sys/types.h>
#include <sys/wait.h>		// waitpid
#endif

#include "File.h"		// File
#include "Process.h"	// ourselves.
#include "string.h"		// int_of

namespace smart {
namespace Process {

// --------------------------------------------------------------------------------------------------------------------
intptr_t create(const std::string& cmd, int* fd_output)
{
#if !defined(WIN32)
	int fildes[2] = {-1, -1};
	if (fd_output != nullptr) {
		const int r_pipe = pipe(fildes);
		if (r_pipe != 0) {
			throw std::runtime_error("Process:create: cannot create pipes!");
		}
		*fd_output = fildes[0];
	}
	const int	child_pid = fork();
#endif

	// 2. Arguments for the exec.
	// Strictly speaking, only child needs them, however, this way it is easier to debug.
	std::string			s_cmd = cmd;
	std::vector<char*>	argv;
	const bool			is_background = cmd.size()>0u && cmd[cmd.size()-1u]=='&';
	unsigned int		first_nonspace = 0;
	unsigned int		first_space;

	if (is_background) {
		s_cmd.resize(s_cmd.size()-1u);
	}

#if defined(WIN32)
	std::string	s_cmd_original = s_cmd;
#endif

	while (first_nonspace < s_cmd.size()) {
		argv.push_back(&s_cmd[first_nonspace]);
		first_space = s_cmd.size();
		for (unsigned int i = first_nonspace + 1; i<s_cmd.size(); ++i) {
			if (s_cmd[i] == ' ') {
				first_space = i;
				s_cmd[i] = 0;
				break;
			}
		}
		first_nonspace = first_space + 1;
	}
	argv.push_back(nullptr);

#if defined(WIN32)
	// NB! Without cmd.exe!
	DWORD	creation_flags = NORMAL_PRIORITY_CLASS | DETACHED_PROCESS;
	STARTUPINFO		startup_info;
	PROCESS_INFORMATION	process_info;
	memset(&startup_info, 0, sizeof(startup_info));
	memset(&process_info, 0, sizeof(process_info));
	startup_info.cb = sizeof(startup_info);
	//startup_info.dwFlags = STARTF_USESHOWWINDOW;
	//startup_info.wShowWindow = 
	int r = CreateProcess(
		argv[0],					// LPCTSTR lpApplicationName, 
		&s_cmd_original[0],			// LPTSTR lpCommandLine, -- xbuf
		0,							// LPSECURITY_ATTRIBUTES lpProcessAttributes, 
		0,							// LPSECURITY_ATTRIBUTES lpThreadAttributes, 
		FALSE,						// BOOL bInheritHandles, 
		creation_flags,				// DWORD dwCreationFlags, 
		0,							// LPVOID lpEnvironment, 
		0,							// LPCTSTR lpCurrentDirectory, 
		&startup_info,				// LPSTARTUPINFO lpStartupInfo, 
		&process_info				// LPPROCESS_INFORMATION lpProcessInformation
	);//ShellExecute
	const bool	ok = r != 0;
	if (ok && !is_background) {
		WaitForSingleObject(process_info.hProcess, INFINITE);
		//GetExitCodeProcess(process_info.hProcess,Result);
	}
	return reinterpret_cast<intptr_t>(process_info.hProcess);
#else
	if (child_pid < 0) {
		// Parent process: have to close the write end of the pipe.
		if (fd_output != nullptr) {
			close(fildes[1]);
		}
		return child_pid;
	}
	else if (child_pid == 0) {
		// Child process: have to redirect the write end of the pipe to the stdout/stderr.
		if (fd_output != nullptr) {
			dup2(fildes[1], STDOUT_FILENO);
			dup2(fildes[1], STDERR_FILENO);
		}
		// 1. Close off all file descriptors bigger than STDXX_FILENO
		DIR*	dirp = opendir("/proc/self/fd");
		if (dirp != nullptr) {
			std::vector<int>	to_be_closed;	// Files to be closed. Closing them directly would result in breaking the directory scan.
			struct dirent*	e;
			while ((e = readdir(dirp)) != nullptr) {
				int	fd = STDIN_FILENO;
				if (e->d_name[0]!='.' && int_of(e->d_name, fd)) {
					if (fd > STDERR_FILENO) {
						to_be_closed.push_back(fd);
					}
				}
			}
			closedir(dirp);
			// Close the files now, when the scan is complete.
			for (int fd : to_be_closed) {
				close(fd);
			}
		}

		// 2. Invoke the program.
		const int r_exec = execvp(s_cmd.c_str(), &argv[0]);
		_exit(r_exec);
	}
	else {
		return child_pid;
	}
#endif
}

#if !defined(WIN32)
static bool read_data_if_available(
		std::string& buffer,
		unsigned int& so_far,
		const unsigned int buffer_size,
		int fd,
		const unsigned int timeout_us)
{
	struct timeval		select_timeout = {
			static_cast<time_t>(timeout_us / 1000000u),
			static_cast<suseconds_t>(timeout_us % 1000000u) } ;
	fd_set				read_fds = {};

	FD_SET(fd, &read_fds);

	const int r_select = select(fd + 1, &read_fds, nullptr, nullptr, &select_timeout);
	bool is_data_available = r_select >= 0 && FD_ISSET(fd, &read_fds);
	if (!is_data_available) {
		return false;
	}

	buffer.resize(so_far + buffer_size);
	const int	this_round = ::read(fd, &buffer[so_far], buffer_size);
	if (this_round <= 0) {
		return false;
	}
	so_far += this_round;
	return true;
}
#endif

// --------------------------------------------------------------------------------------------------------------------
std::string checkOutput(const std::string& command)
{
#if defined(WIN32)
	throw std::runtime_error("checkOutput not supported on Win32");
#else
	std::string r;
	int fd = -1;
	intptr_t pid = create(command, &fd);
	if (pid < 0) {
		throw std::runtime_error(ssprintf("Process::checkOutput: Cannot create process '%s', errno=%d", command.c_str(), (int)(-pid)));
	}
	if (fd<0) {
		throw std::runtime_error(ssprintf("Process::checkOutput: Unable to get the  '%s', errno=%d", command.c_str(), (int)(-pid)));
	}
	File	fd_auto(fd);

	constexpr unsigned int	TIMEOUT_US = 10*1000;
	constexpr unsigned int	BUFFER_SIZE = 16384;

	unsigned int so_far = 0;
	for (;;) {
		if (read_data_if_available(r, so_far, BUFFER_SIZE, fd, TIMEOUT_US)) {
			continue;
		}
		int status = -1;
		const int r_waitpid = waitpid(pid, &status, WNOHANG);
		if (r_waitpid<0 || r_waitpid==pid) {
			break;
		}
	}
	read_data_if_available(r, so_far, BUFFER_SIZE, fd, TIMEOUT_US);
	r.resize(so_far);
	return r;
#endif
}

// --------------------------------------------------------------------------------------------------------------------
int wait(const intptr_t pid)
{
#if defined(WIN32)
	const HANDLE hpid = reinterpret_cast<HANDLE>(pid);
	if (hpid == INVALID_HANDLE_VALUE) {
		return -1;
	
	}
	DWORD exit_code = -1;
	WaitForSingleObject(hpid, INFINITE);
	GetExitCodeProcess(hpid, &exit_code);
	return exit_code;
#else
	if (pid < 0) {
		return pid;
	}
	int status = 0;
	const int	r_waitpid = waitpid(pid, &status, 0);
	if (r_waitpid == -1) {
		return r_waitpid;
	}
	return WEXITSTATUS(status);
#endif
}

// --------------------------------------------------------------------------------------------------------------------
int safeSystem(const std::string& command)
{
	const int	child_pid = create(command, nullptr);
	if (child_pid < 0) {
		return child_pid;
	}
	else {
		return wait(child_pid);
	}
}

} // namespace Process
} // namespace smart
