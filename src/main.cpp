#include <cstdio>
#include <string>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <future>
#include <cerrno>
#include <atomic>

// Our thread engine.
#include "ThreadEngine.h"
#include "EventDispatcher.h"
#include "MySQL.h"
#include "Config.h"
#include "Request.h"

std::atomic_bool quit;

ThreadHandler *threads;
MySQL *ms;
Config *c;
EventDispatcher OnRequest;


void OpenListener(int sock_fd)
{
	printf("Listener spawned for socket %d in thread %d\n", sock_fd, ThreadHandler::GetThreadID());
	FCGX_Request request;
	memset(&request, 0, sizeof(FCGX_Request));

	FCGX_InitRequest(&request, sock_fd, 0);

	// Idle infinitely and accept requests.
	while(FCGX_Accept_r(&request) == 0)
	{
		printf("Thread %d handling request\n", ThreadHandler::GetThreadID());
		Request r(&request);

		// Set the status to 200 OK
		r.SetStatus(200);

		// Call an event, this will later be used by plugins
		// once they register with the handler.
		//OnRequest.CallVoidEvent("REQUEST", r, r.GetParam("SCRIPT_NAME"));

		// Form the HTTP header enough, nginx will fill in the rest.
		r.Write("Content-Type: text/html\r\n\r\n");

		// Send our message

		r.Write("<html><head>");
		r.Write("<title>Example Test thingy</title>");
		r.Write("</head>");

		r.Write("<h2>%s thread %d</h2>", r.GetParam("QUERY_STRING"), ThreadHandler::GetThreadID());

		r.Write("<p>%s<br/>", r.GetParam("REMOTE_ADDR"));
		r.Write("%s<br/>", r.GetParam("REQUEST_URI"));
		r.Write("%s<br/>", r.GetParam("SERVER_PROTOCOL"));
		r.Write("%s<br/>", r.GetParam("REQUEST_METHOD"));
		r.Write("%s<br/>", r.GetParam("REMOTE_PORT"));
		r.Write("%s<br/>", r.GetParam("SCRIPT_NAME"));
		r.Write("</p>");
		r.Write("<br /><br /><br /><h2>MySQL Query:</h2><br />");

		printf("Running MySQL Query...\n");
		// Test Query
		try {
			MySQL_Result mr = ms->Query("SELECT * from " + ms->Escape("testtbl"));
			// Dump fields first
			for (auto field : mr[0])
				r.Write("%s ", field.first);

			// Now dump rows.
			for (auto it : mr)
			{
				for (auto it2 : it)
					r.Write("%s ", it2.second.empty() ? "(NULL)" : it2.second);

				r.Write("<br/>");
			}
			printf("%lu rows with %lu columns\n", mr.size(), mr[0].size());
		}
		catch(const MySQLException &e)
		{
			printf("MySQL error: %s\n", e.what());
			r.Write("<p>MySQL error: %s</p><br/>", e.what());
		}

		r.Write("</html>");

		FCGX_Finish_r(&request);
	}

	printf("Exiting listener thread %d\n", ThreadHandler::GetThreadID());

	FCGX_Free(&request, sock_fd);
}

int main(int argc, char **argv)
{
	std::vector<std::string> args(argv, argv+argc);
	quit = false;

	// Parse our config before anything
	Config conf("adkit.conf");
	c = &conf;

	printf("Config:\n");
	c->Parse();

	ThreadHandler th;
	th.Initialize();
	threads = &th;

	FCGX_Init();
	// Formulate the string from the config.
	std::stringstream val;
	val << c->bind << ":" << c->port;
	// Initialize a new FastCGI socket.
	std::cout << "Opening FastCGI socket: " << val.str() << std::endl;
	int sock_fd = FCGX_OpenSocket(val.str().c_str(), 1024);
	printf("Opened socket fd: %d\n", sock_fd);

	// Initialize MySQL
	MySQL m(c->hostname, c->username, c->password, c->database, c->mysqlport);
	ms = &m;

	for (unsigned int i = 0; i < (th.totalConcurrentThreads * 2) / 2; ++i)
		th.AddQueue(OpenListener, sock_fd);

	printf("Submitting jobs...\n");
	th.Submit();


	printf("Idling main thread.\n");
	while(!quit)
	{
		sleep(5);
		ms->CheckConnection();
	}

	printf("Shutting down.\n");
	th.Shutdown();

	return EXIT_SUCCESS;
}
