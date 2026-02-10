#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// Default Buffer Size - 1024 Bytes
constexpr unsigned int BUFFER_SIZE = 1024;

// Store active clients in a map; paired as <ClientName, ClientSocket>
std::map<std::string, SOCKET> active_clients;

// Mutex for thread safety
std::mutex mtx_server;

// Store isRunning as atomic to prevent race conditions
std::atomic<bool> isRunning = true;

static void communicateClient(SOCKET client_socket, int connection)
{
	// Username Loop
	std::string client_name;
	while (true)
	{
		// Check user join or not...
		char buffer[BUFFER_SIZE] = { 0 };
		int receivedBytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

		if (receivedBytes > 0)
		{
			buffer[receivedBytes] = '\0';
			client_name = buffer;
			std::cout << "Received Name: " << buffer << std::endl;

			auto iter = active_clients.find(client_name);
			if (iter == active_clients.end())
			{
				std::lock_guard<std::mutex> lock(mtx_server);
				active_clients.emplace(client_name, client_socket);
				std::cout << client_name << " joined the chat..." << std::endl;
				std::string finalMessage = "UNIQUE";
				send(client_socket, finalMessage.c_str(), static_cast<int>(finalMessage.size()), 0);
				break;
			}
			else
			{
				std::string finalMessage = "NOT_UNIQUE";
				send(client_socket, finalMessage.c_str(), static_cast<int>(finalMessage.size()), 0);
			}
		}
	}

	// Send previous user connected message to the current client (except the client themselves)
	for (auto const& client : active_clients)
	{
		if (strcmp(client_name.c_str(), client.first.c_str()) == 0) continue;
		std::string finalMessage = "[SERVER] " + client.first + " joined the chat";
		send(client_socket, finalMessage.c_str(), static_cast<int>(finalMessage.size()), 0);
	}
	std::cout << "Current User Join Message sent." << std::endl;

	// Send user connected message to every client (except the client themselves)
	for (auto const& client : active_clients)
	{
		if (strcmp(client_name.c_str(), client.first.c_str()) == 0) continue;
		std::string finalMessage = "[SERVER] " + client_name + " joined the chat";
		send(client.second, finalMessage.c_str(), static_cast<int>(finalMessage.size()), 0);
	}
	std::cout << "Current User Join Message sent." << std::endl;

	// Connection Loop
	while (isRunning)
	{
		// Step 6: Communicate with the client
		char buffer[BUFFER_SIZE] = { 0 };
		int receivedBytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
		if (receivedBytes > 0)
		{
			buffer[receivedBytes] = '\0';
			std::cout << "Received: " << buffer << std::endl;
			std::string response(buffer);

			// /exit --> Exits the chatroom
			if (response == "/exit") break; //isRunning = false;

			// Example -- [Command] [UserName] [MessageBody]
			// /dm [UserName] [MessageBody] --> Sends the [MessageBody] to [UserName] as [DirectMessage]
			std::stringstream ss(response);
			std::string command;
			ss >> command;
			std::cout << command << std::endl;
			if (command == "/dm")
			{
				std::string user;
				ss >> user >> std::ws;
				std::cout << user << std::endl;

				std::string message;
				std::getline(ss, message);
				std::cout << message << std::endl;
				
				std::lock_guard<std::mutex> lock(mtx_server);
				auto iter = active_clients.find(user);
				if (iter != active_clients.end())  // Target client is active
				{
					SOCKET target = iter->second;
					std::string finalMessage = "[DirectMessage] " + client_name + " " + iter->first + " " + message;
					send(target, finalMessage.c_str(), static_cast<int>(finalMessage.size()), 0);
					std::cout << "Direct Message sent from client " << client_name << " to client " << iter->first << "." << std::endl;
				}
				else  // Target client is not connected
				{
					std::cout << "User not found..." << std::endl;
					std::string errorMessage = "[SERVER] User \"" + user + "\" is not connected!";
					send(client_socket, errorMessage.c_str(), static_cast<int>(errorMessage.size()), 0);
					std::cout << "Error Message sent to client " << client_name << "." << std::endl;
				}
			}
			else  // Broadcast message
			{
				std::lock_guard<std::mutex> lock(mtx_server);
				for (auto const& client : active_clients)
				{
					std::string finalMessage = "[BroadcastMessage] " + client_name + " " + response;
					send(client.second, finalMessage.c_str(), static_cast<int>(finalMessage.size()), 0);
					std::cout << "Broadcast Message sent from client " << client_name << "." << std::endl;
				}
			}
		}
	}
	std::cout << "Closing the connection to client" << connection << "!" << std::endl;

	// Step 7: Cleanup
	std::lock_guard<std::mutex> lock(mtx_server);
	active_clients.erase(client_name);

	// Send the disconnect message to every client
	for (auto const& client : active_clients)
	{
		// std::string finalMessage = "[SERVER] : " + client_name + " has left the chat";
		std::string finalMessage = "[SERVER] " + client_name + " left the chat";
		send(client.second, finalMessage.c_str(), static_cast<int>(finalMessage.size()), 0);
		std::cout << "Leave Message sent." << std::endl;
	}

	closesocket(client_socket);
}

static int server()
{
	// Server Port
	unsigned int port = 65432;

	// Step 1: Initialise WinSock Library
	// - Version Requested as a Word: 2.2
	// - Pointer to a WSADATA structure
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
		return 1;
	}

	// Step 2: Create a socket
	// - Address Family: IPv4 (AF_INET)
	// - Socket Type: TCP (SOCK_STREAM)
	// - Protocol: TCP (IPPROTO_TCP)
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket == INVALID_SOCKET)
	{
		std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}

	// Step 3: Bind the socket
	// Returns a socket descriptor or INVALID_SOCKET on error
	// - s: The socket descriptor
	// - name: Pointer to a sockaddr structure (containing the address and port)
	// - namelen : Size of the sockaddr structure
	sockaddr_in server_address = {};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);		  // Server Port
	server_address.sin_addr.s_addr = INADDR_ANY;  // Accept any IP connections
	if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR)
	{
		std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		return 1;
	}

	// Step 4: Listen for incoming connections
	// - s: Socket descriptor
	// - backlog: Max number of pending connections in the queue (SOMAXCONN: system-defined maximum queue length)
	if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		return 1;
	}
	std::cout << "Server is listening on the port " << port << "..." << std::endl;

	// Multithreaded Server
	unsigned int connection = 0;

	while (true)
	{
		// Step 5: Accept Connection
		// Blocks until a connection is received; unless in non-blocking mode
		// Returns a new socket for communication with the client
		// - s: The listening socket descriptor
		// - addr: Pointer to a sockaddr structure (to store the client's address)
		// - addrlen: Size of the sockaddr structure
		sockaddr_in client_address = {};
		int client_address_len = sizeof(client_address);
		SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
		if (client_socket == INVALID_SOCKET)
		{
			std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
			closesocket(server_socket);
			WSACleanup();
			return 1;
		}

		// Convert IP Address from binary to string
		// - Address Family: IPv4 (AF_INET)
		// - Source IP: Pointer to the binary representation of the IP address (sockaddr_in.sin_addr)
		// - Destination: Buffer to store the string representation of the IP address
		// - Size: Size of the buffer
		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
		
		std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;
		std::cout << "Connection ID = " << ++connection << std::endl;

		// Step 6: Communicate with the client (Multithreaded)
		std::thread* t = new std::thread(communicateClient, client_socket, connection);
		t->detach();
	}
	std::cout << "Server has been shutdown!" << std::endl;

	// Step 7: Cleanup
	closesocket(server_socket);
	WSACleanup();

	// Terminate successfully
	return 0;
}

int main(int argc, char** argv)
{
	server();
	return 0;
}