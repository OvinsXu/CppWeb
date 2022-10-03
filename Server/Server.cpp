#include"Server.hpp"

int main()
{
	Server server;
	server.initSocket();
	server.Bind(nullptr, 4567);
	server.Listen(5);

	while (server.IsRun())
	{
		server.OnRun();
	}
	server.Close();

	return 0;
}
