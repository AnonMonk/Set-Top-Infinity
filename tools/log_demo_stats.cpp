#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#endif

namespace fs = std::filesystem;

static constexpr double DEFAULT_INTERVAL = 0.5;

static void printUsage()
{
	std::cout
		<< "Log CPU/RAM while the demo runs into a CSV under tools/log/.\n"
		<< "External side process — does not change demo code.\n"
		<< "\n"
		<< "Usage:\n"
		<< "  ./tools/log_demo_stats <demo_pid> [interval_sec] [logfile]\n"
		<< "  ./tools/log_demo_stats --find-demo\n"
		<< "\n"
		<< "CSV columns:\n"
		<< "  time_iso, elapsed_s, demo_pid, demo_cpu_pct, demo_rss_mb,\n"
		<< "  load_1m, load_5m, load_15m, ncpu,\n"
		<< "  core0_pct, core1_pct, ... (system cores)\n";
}

static bool pidAlive(pid_t pid)
{
	if (pid <= 0)
		return false;
	if (kill(pid, 0) == 0)
		return true;
	return errno != ESRCH;
}

static fs::path toolsDirFromArgv0(const char* argv0)
{
	fs::path p(argv0);
	std::error_code ec;
	fs::path canon = fs::weakly_canonical(p, ec);
	if (!ec)
		p = canon;
	else if (p.has_parent_path())
		p = fs::absolute(p);
	return p.parent_path();
}

static fs::path defaultLogPath(const fs::path& toolsDir)
{
	const auto now = std::chrono::system_clock::now();
	const std::time_t t = std::chrono::system_clock::to_time_t(now);
	std::tm local {};
#if defined(_WIN32)
	localtime_s(&local, &t);
#else
	localtime_r(&t, &local);
#endif
	char stamp[32];
	std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &local);

	fs::path logDir = toolsDir / "log";
	fs::create_directories(logDir);
	return logDir / (std::string("demo_cpu_") + stamp + ".csv");
}

static bool readProcess(pid_t pid, double* outCpuPct, double* outRssMb)
{
	char cmd[128];
	std::snprintf(cmd, sizeof(cmd), "ps -p %d -o %%cpu= -o rss= 2>/dev/null", (int)pid);

	FILE* fp = popen(cmd, "r");
	if (!fp)
		return false;

	char line[256];
	if (!std::fgets(line, sizeof(line), fp)) {
		pclose(fp);
		return false;
	}
	pclose(fp);

	for (char* c = line; *c; ++c) {
		if (*c == ',')
			*c = '.';
	}

	double cpu = 0.0;
	double rssKb = 0.0;
	if (std::sscanf(line, "%lf %lf", &cpu, &rssKb) != 2)
		return false;

	*outCpuPct = cpu;
	*outRssMb = rssKb / 1024.0;
	return true;
}

static pid_t findDemoPid()
{
	FILE* fp = popen("pgrep -x demo 2>/dev/null", "r");
	if (!fp)
		return -1;

	pid_t last = -1;
	char line[64];
	while (std::fgets(line, sizeof(line), fp)) {
		char* end = nullptr;
		long v = std::strtol(line, &end, 10);
		if (end != line && v > 0)
			last = (pid_t)v;
	}
	pclose(fp);
	return last;
}

struct CoreTicks {
	unsigned long long user = 0;
	unsigned long long system = 0;
	unsigned long long idle = 0;
	unsigned long long nice = 0;
};

static bool readCoreTicks(std::vector<CoreTicks>& out)
{
	out.clear();

#if defined(__APPLE__)
	natural_t cpuCount = 0;
	processor_info_array_t infoArray = nullptr;
	mach_msg_type_number_t infoCount = 0;

	kern_return_t kr = host_processor_info(
		mach_host_self(),
		PROCESSOR_CPU_LOAD_INFO,
		&cpuCount,
		&infoArray,
		&infoCount
	);
	if (kr != KERN_SUCCESS || infoArray == nullptr || cpuCount == 0)
		return false;

	auto* load = reinterpret_cast<processor_cpu_load_info_t>(infoArray);
	out.resize(cpuCount);
	for (natural_t i = 0; i < cpuCount; ++i) {
		out[i].user = load[i].cpu_ticks[CPU_STATE_USER];
		out[i].system = load[i].cpu_ticks[CPU_STATE_SYSTEM];
		out[i].idle = load[i].cpu_ticks[CPU_STATE_IDLE];
		out[i].nice = load[i].cpu_ticks[CPU_STATE_NICE];
	}

	const vm_size_t bytes = (vm_size_t)infoCount * sizeof(integer_t);
	vm_deallocate(mach_task_self(), (vm_address_t)infoArray, bytes);
	return true;

#elif defined(__linux__)
	FILE* fp = std::fopen("/proc/stat", "r");
	if (!fp)
		return false;

	char line[512];
	while (std::fgets(line, sizeof(line), fp)) {
		if (std::strncmp(line, "cpu", 3) != 0)
			break;
		if (line[3] == ' ' || line[3] == '\t')
			continue;
		if (line[3] < '0' || line[3] > '9')
			continue;

		unsigned long long user = 0, nice = 0, system = 0, idle = 0;
		unsigned long long iowait = 0, irq = 0, softirq = 0, steal = 0;
		if (std::sscanf(
				line,
				"%*s %llu %llu %llu %llu %llu %llu %llu %llu",
				&user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal
			) < 4) {
			continue;
		}
		CoreTicks t;
		t.user = user;
		t.nice = nice;
		t.system = system + irq + softirq + steal;
		t.idle = idle + iowait;
		out.push_back(t);
	}
	std::fclose(fp);
	return !out.empty();
#else
	(void)out;
	return false;
#endif
}

static bool coreUsagePct(
	const std::vector<CoreTicks>& prev,
	const std::vector<CoreTicks>& cur,
	std::vector<double>& outPct)
{
	outPct.clear();
	if (prev.empty() || cur.empty() || prev.size() != cur.size())
		return false;

	outPct.resize(cur.size());
	for (size_t i = 0; i < cur.size(); ++i) {
		const unsigned long long du = cur[i].user - prev[i].user;
		const unsigned long long ds = cur[i].system - prev[i].system;
		const unsigned long long di = cur[i].idle - prev[i].idle;
		const unsigned long long dn = cur[i].nice - prev[i].nice;
		const unsigned long long total = du + ds + di + dn;
		if (total == 0)
			outPct[i] = 0.0;
		else {
			const unsigned long long busy = du + ds + dn;
			outPct[i] = 100.0 * (double)busy / (double)total;
		}
	}
	return true;
}

static void readLoadavg(double* a1, double* a5, double* a15)
{
	double la[3] = {0.0, 0.0, 0.0};
	if (getloadavg(la, 3) == 3) {
		*a1 = la[0];
		*a5 = la[1];
		*a15 = la[2];
	} else {
		*a1 = *a5 = *a15 = 0.0;
	}
}

static int hardwareCpuCount()
{
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	if (n < 1)
		n = 1;
	return (int)n;
}

static std::string formatTimeIsoMs()
{
	using namespace std::chrono;
	const auto now = system_clock::now();
	const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
	const std::time_t t = system_clock::to_time_t(now);

	std::tm local {};
#if defined(_WIN32)
	localtime_s(&local, &t);
#else
	localtime_r(&t, &local);
#endif

	char buf[64];
	char base[32];
	std::strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &local);

#if defined(__APPLE__) || defined(__linux__)
	char zone[16];
	std::strftime(zone, sizeof(zone), "%z", &local);
	char zoneFmt[8] = {0};
	if (std::strlen(zone) == 5) {
		zoneFmt[0] = zone[0];
		zoneFmt[1] = zone[1];
		zoneFmt[2] = zone[2];
		zoneFmt[3] = ':';
		zoneFmt[4] = zone[3];
		zoneFmt[5] = zone[4];
	} else {
		std::snprintf(zoneFmt, sizeof(zoneFmt), "%s", zone);
	}
	std::snprintf(buf, sizeof(buf), "%s.%03d%s", base, (int)ms.count(), zoneFmt);
#else
	std::snprintf(buf, sizeof(buf), "%s.%03d", base, (int)ms.count());
#endif
	return buf;
}

static void sleepSeconds(double sec)
{
	if (sec <= 0.0)
		return;
	std::this_thread::sleep_for(
		std::chrono::duration<double>(sec)
	);
}

int main(int argc, char** argv)
{
	if (argc < 2 ||
		std::strcmp(argv[1], "-h") == 0 ||
		std::strcmp(argv[1], "--help") == 0) {
		printUsage();
		return 0;
	}

	const fs::path toolsDir = toolsDirFromArgv0(argv[0]);

	pid_t pid = -1;
	double interval = DEFAULT_INTERVAL;
	fs::path logPath;
	int argi = 1;

	if (std::strcmp(argv[argi], "--find-demo") == 0) {
		std::cout << "Waiting for process 'demo' …" << std::endl;
		for (int i = 0; i < 600; ++i) {
			pid = findDemoPid();
			if (pid > 0)
				break;
			sleepSeconds(0.1);
		}
		if (pid <= 0) {
			std::cerr << "No demo process found." << std::endl;
			return 1;
		}
		++argi;
	} else {
		char* end = nullptr;
		long v = std::strtol(argv[argi], &end, 10);
		if (end == argv[argi] || *end != '\0' || v <= 0) {
			std::cerr << "First argument: PID or --find-demo" << std::endl;
			return 1;
		}
		pid = (pid_t)v;
		++argi;
	}

	if (argi < argc) {
		char* end = nullptr;
		double iv = std::strtod(argv[argi], &end);
		if (end != argv[argi] && *end == '\0' && iv > 0.0) {
			interval = iv;
			++argi;
		}
	}

	if (argi < argc)
		logPath = argv[argi];
	else
		logPath = defaultLogPath(toolsDir);

	if (logPath.has_parent_path())
		fs::create_directories(logPath.parent_path());

	const int ncpu = hardwareCpuCount();

	std::vector<CoreTicks> prevTicks;
	readCoreTicks(prevTicks);
	sleepSeconds(0.15);

	const auto start = std::chrono::steady_clock::now();

	std::ofstream out(logPath.string(), std::ios::out | std::ios::trunc);
	if (!out) {
		std::cerr << "Cannot open log file: " << logPath << std::endl;
		return 1;
	}

	out << "time_iso,elapsed_s,demo_pid,demo_cpu_pct,demo_rss_mb,"
		<< "load_1m,load_5m,load_15m,ncpu";
	for (int i = 0; i < ncpu; ++i)
		out << ",core" << i << "_pct";
	out << "\n";
	out.flush();

	std::cout << "Log: " << logPath << std::endl;
	std::cout << "PID: " << pid
			  << "  interval: " << interval
			  << "s  ncpu: " << ncpu << std::endl;

	while (true) {
		if (!pidAlive(pid))
			break;

		double demoCpu = 0.0;
		double demoRss = 0.0;
		if (!readProcess(pid, &demoCpu, &demoRss))
			break;

		std::vector<CoreTicks> curTicks;
		std::vector<double> cores;
		if (readCoreTicks(curTicks)) {
			coreUsagePct(prevTicks, curTicks, cores);
			if (!curTicks.empty())
				prevTicks = std::move(curTicks);
		}

		double load1 = 0.0, load5 = 0.0, load15 = 0.0;
		readLoadavg(&load1, &load5, &load15);

		const auto now = std::chrono::steady_clock::now();
		const double elapsed = std::chrono::duration<double>(now - start).count();

		out << formatTimeIsoMs() << ","
			<< std::fixed;
		out.precision(2);
		out << elapsed << ","
			<< pid << ",";
		out.precision(1);
		out << demoCpu << ","
			<< demoRss << ",";
		out.precision(2);
		out << load1 << ","
			<< load5 << ","
			<< load15 << ","
			<< ncpu;

		out.precision(1);
		for (int i = 0; i < ncpu; ++i) {
			out << ",";
			if (i < (int)cores.size())
				out << cores[(size_t)i];
		}
		out << "\n";
		out.flush();

		out.unsetf(std::ios::floatfield);

		sleepSeconds(interval);
	}

	std::cout << "Log finished: " << logPath << std::endl;
	return 0;
}
