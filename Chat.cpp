#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

u_short portNumber = 0;

int client(std::string ip, int prt);

////SERVER

const int MAX_CLIENTS = 10;
bool SERVER_RUNNING = true;
const std::string PASSWORD = "PASSWORD123LOL";

void AddClientToList(std::string host, u_short service, std::string* connectedSocketsIp, u_short connectedSocketPorts[], int index) {
	connectedSocketsIp[index - 1] = host;
	connectedSocketPorts[index - 1] = service;
}

void RemoveClientFromList(SOCKET client, std::string* connectedSocketsIp, u_short connectedSocketPorts[], fd_set* master) {

	int index = 0;
	for (int i = 0; i < master->fd_count; i++) {
		if (client == master->fd_array[i]) {
			index = i;
		}
	}
	if (index == 0) {
		std::cout << "Couldnt find client to remove in fd list\n";
	}
	else {

		for (int i = index; i < master->fd_count - 1; i++) {
			connectedSocketsIp[i] = connectedSocketsIp[i + 1];
			connectedSocketPorts[i] = connectedSocketPorts[i + 1];
		}
		connectedSocketsIp[master->fd_count - 1] = "";
		connectedSocketPorts[master->fd_count - 1] = 0;

		char buf[4096] = "You have been kicked off the server.\n";

		send(client, buf, sizeof(buf) + 1, 0);

		FD_CLR(client, master);
		std::cout << "Successfully disconnected client\n";
	}
}

void GetConnectedClients(char *buf, const std::string*  connectedSocketsIp, const int size) {
	std::string s = "";
	std::stringstream ss;
	for (int i = 0; i < size - 1; i++) {
		int index = i + 1;
		ss << "\t" << index << " : " << connectedSocketsIp[i + 1] << "\n";
	}
	s = ss.str();
	std::strcpy(buf, s.c_str());
}

void GetClientIP(char *buf, SOCKET client, const fd_set* master, const std::string*  const connectedSocketsIp, const int size) {
	std::string s = "Cant find client.\n";
	for (int i = 0; i < size; i++) {
		SOCKET sockInArray = master->fd_array[i];
		if (client == sockInArray) {
			s = "Your ip is : " + connectedSocketsIp[i] + "\n";
			break;
		}
	}
	std::strcpy(buf, s.c_str());
}

void GetClientPort(char *buf, const SOCKET client, const fd_set* master, const u_short connectedSocketsPort[], const int size) {
	std::string s = "Cant find client.\n";
	for (int i = 0; i < size; i++) {
		SOCKET sockInArray = master->fd_array[i];
		if (client == sockInArray) {
			s = "Your port is : " + std::to_string(connectedSocketsPort[i]) + "\n";
			break;
		}
	}
	std::strcpy(buf, s.c_str());
}

void SendSomeoneAMessage(char buf[], const SOCKET sender, const std::string* connectedSocketIps, const fd_set* master) {

	std::string id = "";
	std::string message = "";
	int idValue = 0;

	int bufIndex = 0;

	while (buf[bufIndex] != ' ') {
		bufIndex++;
	}
	bufIndex++;
	int idIndex = 0;
	while (buf[bufIndex] != ' ') {
		id[idIndex] = buf[bufIndex];
		bufIndex++;
	}

	idValue = std::stoi(id);

	if (idValue == 0 || idValue >= master->fd_count) {
		message = "Error sending your message. Make sure you typed in the command correctly.\n";
		send(sender, message.c_str(), message.length() + 1, 0);
		return;
	}

	bufIndex++;
	std::ostringstream ss;
	ss << "Message from " << connectedSocketIps[idValue] << " : ";
	while (buf[bufIndex] != '\0') {
		ss << buf[bufIndex];
		bufIndex++;
	}
	ss << '\n';
	message = ss.str();
	send(master->fd_array[idValue], message.c_str(), message.length() + 1, 0);
}

void SendMessageToEveryone(char* buf, const SOCKET client, const SOCKET listeningSocket, const fd_set* master) {
	std::string message = buf;
	for (int i = 0; i < master->fd_count; i++) {
		SOCKET sock = master->fd_array[i];
		if (sock != client /*&& sock != listeningSocket*/) {
			send(sock, message.c_str(), message.length(), 0);
		}
	}
}

void SystemShutDown(char* buf, SOCKET client, std::string* connectedSocketIps, u_short connectedSocketsPort[], SOCKET listeningSocket, fd_set* master) {
	std::string message(buf);
	std::string password = " ";
	std::ostringstream ss;

	int index = 0;

	while (buf[index] != ' ') {
		index++;
	}
	index++;

	while (index < message.length()) {
		ss << buf[index];
		index++;
	}

	password = ss.str();

	if (password == PASSWORD) {
		std::cout << "Sytem shutting down\n";
		char msg[] = "Server is shutting down.\n";

		SendMessageToEveryone(msg, client, listeningSocket, master);
		send(client, message.c_str(), message.length() + 1, 0);

		for (int i = 0; i < master->fd_count; i++) {
			connectedSocketIps[i] = "";
			connectedSocketsPort[i] = 0;
		}

		for (int i = master->fd_count; i > 0; i--) {
			closesocket(master->fd_array[i - 1]);
			FD_CLR(master->fd_array[i - 1], master);
		}
		SERVER_RUNNING = false;
	}
	else {
		message = "Wrong password\n";
		send(client, message.c_str(), message.length() + 1, 0);
	}

}

void server()
{
	std::string connectedSocketsIP[MAX_CLIENTS];
	u_short connectedSocketsPort[MAX_CLIENTS]{ 0 };

	char host[NI_MAXHOST];
	char service[NI_MAXSERV];

	//CREATE THE SOCKET TO BIND TO
	SOCKET listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
	//    testing

	//BIND AN IP ADDRESS AND THE PORT TO THE SOCKET
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(portNumber); //big endian little endian
	hint.sin_addr.S_un.S_addr = INADDR_ANY;

	bind(listeningSocket, (sockaddr*)&hint, sizeof(hint));

	//TELL WINSOCK THAT THE SOCKET IS FOR LISTENING
	listen(listeningSocket, SOMAXCONN);//marks it as listening socket - max connection

	fd_set master; //file descriptor set
	FD_ZERO(&master);

	FD_SET(listeningSocket, &master);


	while (SERVER_RUNNING) {
		fd_set copyOfMasterSet = master;

		int socketCount = select(0, &copyOfMasterSet, nullptr, nullptr, nullptr);

		for (int i = 0; i < socketCount; i++)
		{
			SOCKET sock = copyOfMasterSet.fd_array[i];
			if (sock == listeningSocket) {
				//accept new connection

				sockaddr_in socketAddr;
				int socketAddrSize = sizeof(socketAddr);

				SOCKET client = accept(listeningSocket, (sockaddr*)&socketAddr, &socketAddrSize);

				if (master.fd_count < MAX_CLIENTS) {
					FD_SET(client, &master);

					ZeroMemory(host, NI_MAXHOST);
					ZeroMemory(service, NI_MAXSERV);

					inet_ntop(AF_INET, &socketAddr.sin_addr, host, NI_MAXHOST);


					inet_ntop(AF_INET, &socketAddr.sin_addr, host, NI_MAXHOST);
					AddClientToList(host, (u_short)ntohs(socketAddr.sin_port), connectedSocketsIP, connectedSocketsPort, master.fd_count);

					std::cout << "A new client connected. \n";
				}
				else {
					std::string wMessage = "Sorry the server is full!\r\n";
					send(client, wMessage.c_str(), wMessage.size() + 1, 0);
					closesocket(client);
				}


			}
			else {
				char buf[4096];
				ZeroMemory(buf, 4096);

				int bytesIn = recv(sock, buf, 4096, 0);

				if (bytesIn <= 0) {
					RemoveClientFromList(sock, connectedSocketsIP, connectedSocketsPort, &master);
					closesocket(sock);
					FD_CLR(sock, &master);
					std::cout << "A client has disconnected.";
				}
				else {
					//HANDLE MESSAGE
					char tempBuf[4096];
					ZeroMemory(tempBuf, 4096);

					for (int i = 0; i < 4096; i++) {
						if (buf[i] != ' ') tempBuf[i] = buf[i];
					}

					std::string command = tempBuf;

					if (command == "help") {
						//std::cout << "Client asked for help\n";
						std::string msg = "\nconnect <ip> <port> connects you to another peer\nlist : lists all connected ips\nmyip : gives you your ip \nmyport : gives you your connected port\nsend : lets you send a message to the ips listed - do list to get the ips and do 'send <id> <message>\nexit : kicks you out of the server\n";
						std::strcpy(buf, msg.c_str());
						send(sock, buf, sizeof(buf) + 1, 0);
					}
					else if (command == "list") {
						//std::cout << "Client asked for list\n";
						GetConnectedClients(buf, connectedSocketsIP, master.fd_count);
						send(sock, buf, sizeof(buf) + 1, 0);
					}
					else if (command == "myip") {
						//std::cout << "Client asked for ip\n";
						GetClientIP(buf, sock, &master, connectedSocketsIP, master.fd_count);
						send(sock, buf, sizeof(buf) + 1, 0);
					}
					else if (command == "myport") {
						//std::cout << "Client asked for port\n";
						GetClientPort(buf, sock, &master, connectedSocketsPort, master.fd_count);
						send(sock, buf, sizeof(buf) + 1, 0);
					}
					else if (command == "connect") {
						//std::cout << "Client asked for help\n";
						//deal in client
					}
					else if (command == "exit") {
						//std::cout << "Client asked to exit\n";
						RemoveClientFromList(sock, connectedSocketsIP, connectedSocketsPort, &master);
					}
					else if (command == "send") {
						//std::cout << "Client asked to send a message to someone\n";
						SendSomeoneAMessage(buf, sock, connectedSocketsIP, &master);
					}
					else if (command == "systemshutdown") {
						//std::cout << "Client asked to shutdown system\n";
						SystemShutDown(buf, sock, connectedSocketsIP, connectedSocketsPort, listeningSocket, &master);
					}
					else {
						//std::cout << "Client asked to send a message to everyone\n";
						//std::cout << buf << std::endl;
						SendMessageToEveryone(buf, sock, listeningSocket, &master);
					}
				}
			}
		}
	}

	//CLOSE THE WINSOCK - SHUTDOWN WINSOCK
	WSACleanup();
}

// END SERVER

//CLIENT

void GetUserInput(SOCKET sock) {
	std::string input;
	bool loop = true;
	while (loop) {
		//std::cout << "getuserinput\n";
		std::cout << " > ";
		getline(std::cin, input);

		if (input.size() > 0)
		{
			//read the input
			char tempInput[4096];
			ZeroMemory(tempInput, 4096);
			int inputIndex = 0;
			while (input[inputIndex] != ' ' && input[inputIndex] != '\0') {
				tempInput[inputIndex] = input[inputIndex];
				inputIndex++;
			}
			std::string temp(tempInput);
			if (temp == "connect" && SERVER_RUNNING) {
				inputIndex++;
				char ip[30];
				char port[30];
				ZeroMemory(ip, 30);
				ZeroMemory(port, 30);
				int i = 0;
				while (input[inputIndex] != ' ' && input[inputIndex] != '\0') {
					ip[i] = input[inputIndex];
					inputIndex++;
					i++;
				}
				inputIndex++;
				i = 0;
				while (input[inputIndex] != ' ' && input[inputIndex] != '\0') {
					port[i] = input[inputIndex];
					inputIndex++;
					i++;
				}

				std::string tempIP(ip);
				u_short tempPort = std::stoi(port);

				SERVER_RUNNING = false;
				loop = false;
				client(tempIP, tempPort);
				//serverThread.join();
			} // end if(temp == connect)
			else {
				int sendResult = send(sock, input.c_str(), input.size() + 1, 0);
			}
		} // end if(userinput.size() > 0)
	}//end while(true)

}

void GetServerMessage(SOCKET sock) {
	char buf[4096];
	ZeroMemory(buf, 4096);
	while (true) {
		//std::cout << "getservermessage\n";
		ZeroMemory(buf, 4096);
		int bytesReceived = recv(sock, buf, 4096, 0);
		if (bytesReceived > 0)
		{
			// Echo response to consoles
			//std::cout << "Message from server: " << buf << std::endl;
			std::cout << std::string(buf, 0, bytesReceived) << std::endl;
			std::cout << " > ";
		}
	}
}

int client(std::string ip, int prt)
{
	std::string ipAddress = ip;			// IP Address of the server
	int port = prt;						// Listening port # on the server

	// Create socket
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
		std::cerr << "Can't create socket, Err #" << WSAGetLastError() << std::endl;
		WSACleanup();
		return 0;
	}

	// Fill in a hint structure
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(port);
	inet_pton(AF_INET, ipAddress.c_str(), &hint.sin_addr);

	// Connect to server
	int connResult = connect(sock, (sockaddr*)&hint, sizeof(hint));
	if (connResult == SOCKET_ERROR)
	{
		std::cerr << "Can't connect to server, Err #" << WSAGetLastError() << std::endl;
		closesocket(sock);
		WSACleanup();
		return 0;
	}

	// Do-while loop to send and receive data
	char buf[4096];
	std::string userInput;

	do
	{
		std::thread userInputThread(GetUserInput, sock);
		std::thread serverMessageThread(GetServerMessage, sock);

		userInputThread.join();
		serverMessageThread.join();

	} while (true);

	// Gracefully close down everything
	closesocket(sock);
	WSACleanup();
	return 0;
}

// END CLIENT


//MAIN


std::string getHostName()
{
	char ac[80];
	if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
		std::cerr << "Error " << WSAGetLastError() <<
			" when getting local host name." << std::endl;
		return "Error";
	}
	std::cout << "Local host is " << ac << "." << std::endl;

	struct hostent *phe = gethostbyname(ac);
	if (phe == 0) {
		std::cerr << "Bad host lookup." << std::endl;
		return "Error";
	}

	struct in_addr addr;
	memcpy(&addr, phe->h_addr_list[0], sizeof(struct in_addr));
	//std::cout << "Address " << i << ": " << inet_ntoa(addr) << std::endl;

	return inet_ntoa(addr);
}


int main(int argc, char* argv[]) {

	WSAData data;
	WORD ver = MAKEWORD(2, 2);
	int wsResult = WSAStartup(ver, &data);
	if (wsResult != 0)
	{
		std::cerr << "Can't start Winsock, Err #" << wsResult << std::endl;
		return 0;
	}

	std::string localHostIp = getHostName();

	if (localHostIp == "Error") {
		std::cout << "Error - closing application.\n";
		return 1;
	}


	std::cout << "================================================\n";
	std::cout << "WELCOME!\n";
	std::cout << "type '!help' for more commands\n";
	std::cout << "================================================\n";

	if (argc <= 1) {
		std::cout << "Enter the listening port: ";
		std::cin >> portNumber;
	}
	else {
		portNumber = (u_short)std::strtoul(argv[1], nullptr, 0);
	}

	std::thread serverThread(server);

	//connect to the server?
	client(localHostIp, portNumber);

	std::thread clientThread;
	bool loop = true;
	do {
		//std::string input;
		std::string input;
		std::getline(std::cin, input);

		char tempInput[4096];
		ZeroMemory(tempInput, 4096);
		int inputIndex = 0;
		while (input[inputIndex] != ' ' && input[inputIndex] != '\0') {
			tempInput[inputIndex] = input[inputIndex];
			inputIndex++;
		}
		std::string temp(tempInput);
		if (temp == "connect") {
			inputIndex++;
			char ip[30];
			char port[30];
			ZeroMemory(ip, 30);
			ZeroMemory(port, 30);
			int i = 0;
			while (input[inputIndex] != ' ' && input[inputIndex] != '\0') {
				ip[i] = input[inputIndex];
				inputIndex++;
				i++;
			}
			inputIndex++;
			i = 0;
			while (input[inputIndex] != ' ' && input[inputIndex] != '\0') {
				port[i] = input[inputIndex];
				inputIndex++;
				i++;
			}

			std::string tempIP(ip);
			u_short tempPort = std::stoi(port);

			SERVER_RUNNING = false;
			loop = false;
			std::thread clientThread(client, tempIP, tempPort);
			serverThread.join();
		}
	} while (loop);



	clientThread.join();


	system("PAUSE");
	return 0;

}





