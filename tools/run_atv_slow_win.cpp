#ifdef _WIN32

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
	std::string quoteArgument(const std::string& argument)
	{
		if (argument.find_first_of(" \t\"") == std::string::npos)
			return argument;

		std::string quoted = "\"";
		int backslashes = 0;
		for (size_t i = 0; i < argument.size(); ++i)
		{
			const char ch = argument[i];
			if (ch == '\\')
			{
				++backslashes;
				continue;
			}
			if (ch == '"')
			{
				quoted.append((size_t)backslashes * 2u + 1u, '\\');
				quoted += '"';
				backslashes = 0;
				continue;
			}
			quoted.append((size_t)backslashes, '\\');
			backslashes = 0;
			quoted += ch;
		}
		quoted.append((size_t)backslashes * 2u, '\\');
		quoted += '"';
		return quoted;
	}

	void printWindowsError(const char* operation)
	{
		std::fprintf(
			stderr,
			"%s fehlgeschlagen (Windows-Fehler %lu).\n",
			operation,
			(unsigned long)GetLastError()
		);
	}

	bool readApproximateHostMHz(DWORD* outMHz)
	{
		DWORD value = 0;
		DWORD bytes = sizeof(value);
		const LSTATUS status = RegGetValueA(
			HKEY_LOCAL_MACHINE,
			"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
			"~MHz",
			RRF_RT_REG_DWORD,
			0,
			&value,
			&bytes
		);
		if (status != ERROR_SUCCESS || value == 0)
			return false;
		*outMHz = value;
		return true;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::printf(
			"Usage:\n"
			"  run_atv_slow_win.exe <1..100 %% eines Kerns> [demo-argumente]\n"
			"  run_atv_slow_win.exe --mhz <Zieltakt> [demo-argumente]\n"
			"Beispiele:\n"
			"  tools\\run_atv_slow_win.exe 10 julia\n"
			"  tools\\run_atv_slow_win.exe --mhz 1000 julia\n"
		);
		return 1;
	}

	double requestedPercent = 0.0;
	int firstDemoArgument = 2;
	bool clockApproximation = false;
	double requestedMHz = 0.0;
	DWORD hostMHz = 0;
	if (std::strcmp(argv[1], "--mhz") == 0)
	{
		if (argc < 3 || !readApproximateHostMHz(&hostMHz))
		{
			std::fprintf(
				stderr,
				"Zieltakt fehlt oder Host-Takt konnte nicht gelesen werden.\n"
			);
			return 1;
		}
		char* end = 0;
		requestedMHz = std::strtod(argv[2], &end);
		if (!end || *end != '\0' || requestedMHz <= 0.0)
		{
			std::fprintf(stderr, "Zieltakt muss eine positive MHz-Zahl sein.\n");
			return 1;
		}
		requestedPercent = requestedMHz * 100.0 / (double)hostMHz;
		if (requestedPercent > 100.0) requestedPercent = 100.0;
		firstDemoArgument = 3;
		clockApproximation = true;
	}
	else
	{
		char* end = 0;
		requestedPercent = std::strtod(argv[1], &end);
		if (!end || *end != '\0'
			|| requestedPercent < 1.0 || requestedPercent > 100.0)
		{
			std::fprintf(stderr, "CPU-Limit muss 1..100 Prozent eines Kerns sein.\n");
			return 1;
		}
	}

	char modulePath[MAX_PATH];
	const DWORD moduleLength = GetModuleFileNameA(0, modulePath, MAX_PATH);
	if (moduleLength == 0 || moduleLength >= MAX_PATH)
	{
		printWindowsError("GetModuleFileName");
		return 1;
	}

	const std::filesystem::path toolPath(modulePath);
	const std::filesystem::path root = toolPath.parent_path().parent_path();
	const std::filesystem::path demoPath = root / "demo.exe";
	if (!std::filesystem::exists(demoPath))
	{
		std::fprintf(
			stderr,
			"Demo nicht gefunden: %s\nZuerst bauen: mingw32-make win\n",
			demoPath.string().c_str()
		);
		return 1;
	}

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	const DWORD processorCount =
		systemInfo.dwNumberOfProcessors > 0 ? systemInfo.dwNumberOfProcessors : 1;

	/* CpuRate gilt fuer das ganze System. Auf einen Kern normieren. */
	DWORD cpuRate = (DWORD)std::lround(
		requestedPercent * 100.0 / (double)processorCount
	);
	if (cpuRate < 1) cpuRate = 1;
	if (cpuRate > 10000) cpuRate = 10000;
	const double effectiveOneCorePercent =
		(double)cpuRate * (double)processorCount / 100.0;

	HANDLE job = CreateJobObjectA(0, 0);
	if (!job)
	{
		printWindowsError("CreateJobObject");
		return 1;
	}

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!SetInformationJobObject(
			job,
			JobObjectExtendedLimitInformation,
			&limits,
			(DWORD)sizeof(limits)))
	{
		printWindowsError("SetInformationJobObject(Kill-on-close)");
		CloseHandle(job);
		return 1;
	}

	JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpuControl = {};
	cpuControl.ControlFlags =
		JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
	cpuControl.CpuRate = cpuRate;
	if (!SetInformationJobObject(
			job,
			JobObjectCpuRateControlInformation,
			&cpuControl,
			(DWORD)sizeof(cpuControl)))
	{
		printWindowsError("SetInformationJobObject(CPU hard cap)");
		CloseHandle(job);
		return 1;
	}

	std::string commandLine = quoteArgument(demoPath.string());
	for (int i = firstDemoArgument; i < argc; ++i)
	{
		commandLine += ' ';
		commandLine += quoteArgument(argv[i]);
	}
	std::vector<char> mutableCommand(commandLine.begin(), commandLine.end());
	mutableCommand.push_back('\0');

	STARTUPINFOA startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	const std::string rootString = root.string();
	if (!CreateProcessA(
			demoPath.string().c_str(),
			mutableCommand.data(),
			0,
			0,
			TRUE,
			CREATE_SUSPENDED,
			0,
			rootString.c_str(),
			&startup,
			&process))
	{
		printWindowsError("CreateProcess");
		CloseHandle(job);
		return 1;
	}

	if (!AssignProcessToJobObject(job, process.hProcess))
	{
		printWindowsError("AssignProcessToJobObject");
		TerminateProcess(process.hProcess, 1);
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		CloseHandle(job);
		return 1;
	}

	/* Die Demo ist CPU-seitig weitgehend single-threaded. */
	if (!SetProcessAffinityMask(process.hProcess, (DWORD_PTR)1))
		printWindowsError("SetProcessAffinityMask (Warnung)");

	std::printf(
		"ATV1-CPU-Naeherung: %.2f %% eines Kerns "
		"(%lu logische Prozessoren, Job-CpuRate=%lu)\n",
		effectiveOneCorePercent,
		(unsigned long)processorCount,
		(unsigned long)cpuRate
	);
	if (clockApproximation)
	{
		std::printf(
			"Takt-Naeherung: %.0f MHz Ziel / %lu MHz Host. "
			"IPC, Cache und Turboverhalten werden nicht emuliert.\n",
			requestedMHz,
			(unsigned long)hostMHz
		);
	}
	std::printf("GPU wird nicht gedrosselt. Beenden: ESC in der Demo.\n\n");
	std::fflush(stdout);

	ResumeThread(process.hThread);
	CloseHandle(process.hThread);
	WaitForSingleObject(process.hProcess, INFINITE);

	DWORD exitCode = 1;
	GetExitCodeProcess(process.hProcess, &exitCode);
	CloseHandle(process.hProcess);
	CloseHandle(job);
	return (int)exitCode;
}

#else

#include <cstdio>
int main()
{
	std::fprintf(stderr, "Dieses Tool ist nur fuer Windows.\n");
	return 1;
}

#endif
