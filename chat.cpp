#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

string encryptDecrypt(string toProcess, char key) {
    string output = toProcess;
    for (int i = 0; i < toProcess.length(); i++) {
        output[i] = toProcess[i] ^ key; // XOR logic
    }
    return output;
}

// --- SHARED GLOBAL VARIABLES ---
SOCKET activeSocket = INVALID_SOCKET;
bool isRunning{true};

void receivefunction(SOCKET activesocket){
    char receiveBuffer[200];
    while (true) {
        // This call will block, but only inside THIS thread
        int byteCount = recv(activesocket, receiveBuffer, 200, 0);

    if (byteCount > 0) {
            receiveBuffer[byteCount] = '\0'; 
            string decryptedMsg = encryptDecrypt(receiveBuffer, 'K'); 

            if (decryptedMsg == "exit" || decryptedMsg == "Exit") {
                cout << "\n[System] Peer has left. Exiting..." << endl;
                exit(0); 
            }
            
            // --- CLEAN UI LOGIC ---
            // \r moves the cursor to the start of the line to overwrite the old "Me: "
            std::cout << "\r" << "Message received: " << decryptedMsg << std::endl;
            // Reprint the prompt so the user knows they can still type
            std::cout << "Me: " << std::flush; 
        }
        
        else if (byteCount == 0) {
            std::cout << "\nPeer disconnected." << std::endl;
            break; 
        } 
        else {
            // Only print error if we didn't intentionally close the socket
            if(isRunning) {
                std::cout << "\nReceive error: " << WSAGetLastError() << std::endl;
            }
            break;
        }
        memset(receiveBuffer, 0, 200);
    }
}

void server(int port){
    // Create a Socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cout << "Socket creation failed!" << endl;
        WSACleanup();
        exit(1); // Stop execution
    }

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_port = htons(port); 
    service.sin_addr.s_addr = INADDR_ANY; 

    // Creating a bind
    if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        cout << "bind() failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        exit(1); // Stop execution
    }

    // Put the socket in Listening mode
    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        cout << "listen(): Error listening on socket " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        exit(1); // Stop execution
    }

    cout << "Waiting for a connection..." << endl;
    activeSocket = accept(serverSocket, NULL, NULL);

    if (activeSocket == INVALID_SOCKET) {
        std::cout << "Accept failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        exit(1); // Stop execution
    } 

    std::cout << "Connection established." << std::endl;
    closesocket(serverSocket); // Close the listener as we only need activeSocket now
}

void client(int port){
    // Create a Socket
    activeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (activeSocket == INVALID_SOCKET) {
        std::cout << "Socket creation failed!" << std::endl;
        WSACleanup();
        exit(1); // Stop execution
    }

    // Get VALID IP from user
    std::string ipInput;
    IN_ADDR binAddr; 
    while (true) {
        std::cout << "Enter the Target IP Address (e.g., 127.0.0.1): ";
        std::cin >> ipInput;

        if (inet_pton(AF_INET, ipInput.c_str(), &binAddr) == 1) {
            break; // Success!
        }
        std::cout << "Invalid IP format. Try again." << std::endl;
    }

    // Assigning address to our socket
    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(port);
    clientService.sin_addr = binAddr; 

    // Connect to the Server
    std::cout << "Connecting to " << ipInput << " on port " << port << "..." << std::endl;

    if (connect(activeSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        std::cout << "Client: connect() failed with error: " << WSAGetLastError() << std::endl;
        closesocket(activeSocket);
        WSACleanup();
        
        std::cout << "Press Enter to exit...";
        std::cin.ignore(1000, '\n');
        std::cin.get();
        exit(1); // Stop execution
    } else {
        std::cout << "Successfully connected" << std::endl;
        std::cin.ignore(1000, '\n');
    }
}

int main(){
    char username[50];
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int choice;
    bool validChoice = false;

    while(!validChoice) {
        cout << "1. Host a Chat (Server)\n2. Join a Chat (Client)\nChoice: ";
        cin >> choice;
        cin.ignore(); // Clear newline
        cout << "Enter Username : ";
        cin >> username;
        cin.ignore();
        if (choice == 1) {
            server(5500);
            validChoice = true;
        }
        else if(choice == 2) {
            client(5500);
            validChoice = true;
        }
        else {
            cout << "Invalid input. Please enter 1 or 2." << endl;
        }
    }

    cout << "[System] Connected! Type 'exit' to quit." << endl;
    
    // Start the receiver thread
    thread worker(receivefunction, activeSocket);
    worker.detach();

    while (isRunning) {
        char msgbuffer[200] = {0}; 
        cout << username << " : ";
        std::cin.getline(msgbuffer, 200);
        
        string originalMsg = msgbuffer;

        // Skip empty messages to prevent double "Me:" prompts
        if (originalMsg.empty()) {
            continue;
        }

        // 1. One encryption call
        string encryptedMsg = encryptDecrypt(originalMsg, 'K');

        // 2. One send call
        send(activeSocket, encryptedMsg.c_str(), (int)encryptedMsg.length() + 1, 0);

        // 3. One exit check
        if (originalMsg == "exit" || originalMsg == "Exit") {
            isRunning = false;
            break;
        }
    }

    // Clean shutdown
    shutdown(activeSocket, SD_BOTH);
    closesocket(activeSocket);
    WSACleanup();
    return 0;
}