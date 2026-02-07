#include "audio.h"

#include <iostream>
#include <string>
#include <thread>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// Default Buffer Size - 1024 Bytes
constexpr unsigned int DEFAULT_BUFFER_SIZE = 1024;

// Store isRunning & selectedUsername as atomic to prevent race conditions
std::atomic<bool> isRunning = true;
std::atomic<bool> selectedUsername = false;

static void Receive(SOCKET client_socket) {
	while (true) {
		// Receive the reversed sentence from the server
		char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
		int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
		
		if (bytes_received > 0) {
			buffer[bytes_received] = '\0'; // Null-terminate the received data
			std::cout << '\r' << "                       " << '\r';
			std::cout << buffer << std::endl;
			std::cout << "Send message to server: ";
			std::cout.flush();
		}
		else if (bytes_received == 0) {
			std::cout << "Connection closed by server." << std::endl;
		}
		else if (!isRunning) {
			return; 
		}
		else {
			std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
		}
	}
}

static void client() {
	const char* host = "127.0.0.1";  // Server IP Address
	unsigned int port = 65432;
	std::string message = "Hello, server!";

	// Step 1: Initialise WinSock Library
	// - Version Requested as a Word: 2.2
	// - Pointer to a WSADATA structure
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
		return;
	}

	// Step 2: Create a socket
	// - Address Family: IPv4 (AF_INET)
	// - Socket Type: TCP (SOCK_STREAM)
	// - Protocol: TCP (IPPROTO_TCP)
	SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client_socket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}

	// Step 3: Convert an IP address from string to binary
	// - Address Family: IPv4 (AF_INET)
	// - Source IP String: host ("127.0.0.1")
	// - Destination Pointer: Sturcture holding binary representation
	sockaddr_in server_address = {};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);
	if (inet_pton(AF_INET, host, &server_address.sin_addr) <= 0) {
		std::cerr << "Invalid address/Address not supported" << std::endl;
		closesocket(client_socket);
		WSACleanup();
		return;
	}

	// Step 4: Establishing the connection
	// - s: The socket descriptor
	// - name: Pointer to a sockaddr structure (containing the server's address and port)
	// - namelen: Size of the sockaddr structure
	if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
		std::cerr << "Connection failed with error: " << WSAGetLastError() << std::endl;
		closesocket(client_socket);
		WSACleanup();
		return;
	}
	std::cout << "Connected to the server!" << std::endl;

	// Loop before launching the thread; to see if two users try to enter same name
	// If unique - break; if same - request entering name again until success...
	while (!selectedUsername) {
		// Enter username before connecting
		std::cout << "Enter Username: ";
		std::getline(std::cin, message);

		// Step 5: Sending data to the server
		// - s: The socket descriptor
		// - buf: Pointer to the data buffer
		// - len: Length of the data to send
		// - flags: Default behaviour (0)
		if (send(client_socket, message.c_str(), static_cast<int>(message.size()), 0) == SOCKET_ERROR) {
			std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
			closesocket(client_socket);
			WSACleanup();
			return;
		}
		// std::cout << "Sent: \"" << message << "\" to the server!" << std::endl;

		// Receive the "UNIQUE" or "NOT_UNIQUE" from the server
		char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
		int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
		if (bytes_received > 0) {
			buffer[bytes_received] = '\0'; // Null-terminate the received data
			std::cout << "Received from server: " << buffer << std::endl;
			if (strcmp(buffer, "UNIQUE") == 0) selectedUsername = true;
			else if (strcmp(buffer, "NOT_UNIQUE") == 0) std::cout << "Username already taken, try again!" << std::endl;
		}
		else if (bytes_received == 0) std::cout << "Connection closed by server." << std::endl;
		else std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
	}

	// Username taken, now we can launch the thread
	std::thread t(Receive, client_socket);
	t.detach();

	// Connection Loop
	while (isRunning) {
		std::cout << "Send message to server: ";
		std::getline(std::cin, message);

		// /exit --> Exits the chatroom
		if (message == "/exit") isRunning = false;

		// Step 5: Sending data to the server
		// - s: The socket descriptor
		// - buf: Pointer to the data buffer
		// - len: Length of the data to send
		// - flags: Default behaviour (0)
		if (send(client_socket, message.c_str(), static_cast<int>(message.size()), 0) == SOCKET_ERROR) {
			std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
			closesocket(client_socket);
			WSACleanup();
			return;
		}
		// std::cout << "Sent: \"" << message << "\" to the server!" << std::endl;
	}
	std::cout << "Closing the connection!" << std::endl;

	// Step 7: Cleanup
	closesocket(client_socket);
	WSACleanup();
}

int main(int argc, char** argv) {
	client();
	return 0;
}