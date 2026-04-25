#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#pragma comment(lib, "ws2_32.lib")

string encryptDecrypt(string toProcess, char key) {
    string output = toProcess;
    for (int i = 0; i < toProcess.length(); i++) {
        output[i] = toProcess[i] ^ key; // XOR logic
    }
    return output;
}

using namespace std;

// --- SHARED GLOBAL VARIABLES ---
SOCKET activeSocket = INVALID_SOCKET;
bool isRunning{true};

void receivefunction(SOCKET activesocket){
    char receiveBuffer[200];
    while (true) {
        // This call will block, but only inside THIS thread
        int byteCount = recv(activesocket, receiveBuffer, 200, 0);

        if (byteCount > 0) {
            receiveBuffer[byteCount] = '\0'; // Null-terminate to prevent garbage text

            // DECRYPTION STEP
            string encryptedData = receiveBuffer;
            string decryptedMsg = encryptDecrypt(encryptedData, 'K'); // Must use the same key 'K'
            
            std::cout << "\nMessage received: " << receiveBuffer << std::endl;
            std::cout << "Enter your message: "; // Re-prompt so the UI looks clean
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
    }
}

int main(){
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int choice;
    bool validChoice = false;

    while(!validChoice) {
        cout << "1. Host a Chat (Server)\n2. Join a Chat (Client)\nChoice: ";
        cin >> choice;
        cin.ignore(); // Clear newline

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

    // chat logic
    cout << "[System] Connected! Type 'exit' to quit." << endl;
    thread worker(receivefunction, activeSocket);
    worker.detach();

while (isRunning) {

        char msgbuffer[200] = {0}; 
        cout << "Me: ";
        std::cin.getline(msgbuffer, 200);
        
        string originalMsg = msgbuffer;
        if (originalMsg == "exit" || originalMsg == "Exit") {
            isRunning = false;
            break;
        }
        
        if (originalMsg.length() > 0) {
            // ENCRYPTION STEP
            // This applies the XOR logic
            string encryptedMsg = encryptDecrypt(originalMsg, 'K'); 
    
            // Send the encrypted string instead of the raw buffer
            send(activeSocket, encryptedMsg.c_str(), (int)encryptedMsg.length() + 1, 0);
        }
    }

    shutdown(activeSocket, SD_BOTH);
    closesocket(activeSocket);
    WSACleanup();
    return 0;
}