
#include <getopt.h>
#include <cstdio>
#include <string>
#include <boost/asio/signal_set.hpp>
#include <boost/bind/bind.hpp>
#include "globaldef.h"
#include "daemon.h"
#include "GLog.h"
#include "Parameter.h"
#include "EnvMonitor.h"
#include "SQM.h"

using namespace std;

#ifdef NDEBUG
GLog _gLog(stdout);
#else
GLog _gLog(LOG_DIR, LOG_PREFIX);
#endif

void PrintUsage();



int main(int argc, char** argv) {
	// 解析命令行参数
	struct option longopts[] = {
		{ "help",    no_argument,       NULL, 'h' },
		{ "default", no_argument,       NULL, 'd' },
		{ "config",  required_argument, NULL, 'c' },
		{ "sqm",     no_argument,       NULL, 'f' },
		{ NULL,      0,                 NULL,  0  }
	};
	char optstr[] = "hdc:f";
	int ch, optndx;
	string pathConfig = CONFIG_PATH;

	/* 解析命令行参数 */
	while ((ch = getopt_long(argc, argv, optstr, longopts, &optndx)) != -1) {
		if (ch == 'h') {
			PrintUsage();
			return -1;
		}
		else if (ch == 'd')		{
			printf ("generating default configuration file here\n");
			Parameter param;
			param.Init(CONFIG_NAME);
			return -2;
		}
		else if (ch == 'c') pathConfig = optarg;
	}

	// 启动服务
	boost::asio::io_service ios;
	boost::asio::signal_set signals(ios, SIGINT, SIGTERM);  // interrupt signal
	signals.async_wait(boost::bind(&boost::asio::io_service::stop, &ios));

#ifndef NDEBUG
	if (!MakeItDaemon(ios)) return 1;
	if (!isProcSingleton(DAEMON_PID))
	{
		_gLog.Write("%s is already running or failed to access PID file", DAEMON_NAME);
		return -4;
	}
#endif

	_gLog.Write("Try to launch %s %s %s as daemon", DAEMON_NAME, DAEMON_VERSION, DAEMON_AUTHORITY);
	// 主程序入口
	Parameter param;
	if (!param.Load(pathConfig.c_str())) return -5;

	EnvMonitor wemon(&param);
	if (wemon.Start()) {
		_gLog.Write("Daemon goes running");
		ios.run();
		wemon.Stop();
		_gLog.Write("Daemon stopped");
	}
	else {
		_gLog.Write(LOG_FAULT, NULL, "Fail to launch %s", DAEMON_NAME);
	}

	return 0;
}

void PrintUsage() {
	printf (
		"Usage:\n"
		"\t %s [options]\n"
		"Option:\n"
		"\t -h / --help    : print this help message\n"
		"\t -d / --default : generate default configuration file here\n"
		"\t -c / --config  : configuration file path\n"
		"\t -f / --sqm     : find SQM IP address\n",
		DAEMON_NAME
	);
}
