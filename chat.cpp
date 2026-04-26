#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <cctype> // Required for isalnum()

#pragma comment(lib, "ws2_32.lib")

using namespace std;

//  SHARED GLOBAL VARIABLES 
SOCKET activeSocket = INVALID_SOCKET;
bool isRunning{true};
string localUsername;
string peerUsername = "Peer"; // Default, gets updated during handshake

string encryptDecrypt(string toProcess, char key) {
    string output = toProcess;
    for (int i = 0; i < toProcess.length(); i++) {
        output[i] = toProcess[i] ^ key; // XOR logic
    }
    return output;
}

//  USERNAME VALIDATION 
bool isValidUsername(const string& uname) {
    // Constraint 1: Length between 3 and 15 characters
    if (uname.length() < 3 || uname.length() > 15) {
        cout << "-> Username must be between 3 and 15 characters.\n";
        return false;
    }
    // Constraint 2: Alphanumeric only (no spaces, no special characters)
    for (char c : uname) {
        if (!isalnum(c)) {
            cout << "-> Username can only contain letters and numbers.\n";
            return false;
        }
    }
    return true;
}

void displayLocalIPs() {
    char hostName[256];
    
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR) {
        cout << "[System] Could not retrieve IP addresses." << endl;
        return;
    }

    addrinfo hints = {0};
    hints.ai_family = AF_INET;       
    hints.ai_socktype = SOCK_STREAM; 

    addrinfo* addressList = nullptr;

    if (getaddrinfo(hostName, NULL, &hints, &addressList) != 0) {
        cout << " Could not retrieve IP addresses." << endl;
        return;
    }

    cout << " Share this IP with the client " << endl;

    for (addrinfo* ptr = addressList; ptr != nullptr; ptr = ptr->ai_next) {
        sockaddr_in* sockaddr_ipv4 = (sockaddr_in*)ptr->ai_addr;
        char ipString[INET_ADDRSTRLEN];
        
        inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipString, INET_ADDRSTRLEN);
        

        if (string(ipString) != "127.0.0.1") {
            cout << " -> " << ipString << endl;
        }
    }
    
    freeaddrinfo(addressList);
}

void receivefunction(SOCKET activesocket){
    char receiveBuffer[200];
    while (isRunning) {
        int byteCount = recv(activesocket, receiveBuffer, 200, 0);

        if (byteCount > 0) {
            receiveBuffer[byteCount] = '\0'; 
            string decryptedMsg = encryptDecrypt(receiveBuffer, 'K'); 

            if (decryptedMsg == "exit" || decryptedMsg == "Exit") {
                cout << "\n[System] " << peerUsername << " has left. Exiting..." << endl;
                exit(0); 
            }
            
            // \r overwrites the local prompt
            cout << "\r" << peerUsername << " : " << decryptedMsg << "\n";
            // Reprint the local prompt
            cout << localUsername << " : " << flush; 
        }
        else if (byteCount == 0) {
            cout << "\n[System] " << peerUsername << " disconnected." << endl;
            break; 
        } 
        else {
            if(isRunning) {
                cout << "\nReceive error: " << WSAGetLastError() << endl;
            }
            break;
        }
        memset(receiveBuffer, 0, 200);
    }
}

void server(int port){
    displayLocalIPs();
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cout << "Socket creation failed!" << endl;
        WSACleanup();
        exit(1);
    }

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_port = htons(port); 
    service.sin_addr.s_addr = INADDR_ANY; 

    if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        cout << "bind() failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        exit(1);
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        cout << "listen(): Error listening on socket " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        exit(1);
    }

    cout << "Waiting for a connection..." << endl;
    activeSocket = accept(serverSocket, NULL, NULL);

    if (activeSocket == INVALID_SOCKET) {
        cout << "Accept failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        exit(1);
    } 

    cout << "Connection established." << endl;
    closesocket(serverSocket); 

    //  SERVER HANDSHAKE 
    char nameBuf[50] = {0};
    //  Receive client's username
    recv(activeSocket, nameBuf, 50, 0);
    peerUsername = encryptDecrypt(string(nameBuf), 'K');
    //  Send our username
    string encryptedName = encryptDecrypt(localUsername, 'K');
    send(activeSocket, encryptedName.c_str(), encryptedName.length() + 1, 0);
}

void client(int port){
    activeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (activeSocket == INVALID_SOCKET) {
        cout << "Socket creation failed!" << endl;
        WSACleanup();
        exit(1);
    }

    string ipInput;
    IN_ADDR binAddr; 
    while (true) {
        cout << "Enter the Target IP Address (e.g., 127.0.0.1): ";
        cin >> ipInput;

        if (inet_pton(AF_INET, ipInput.c_str(), &binAddr) == 1) {
            break; 
        }
        cout << "Invalid IP format. Try again." << endl;
    }

    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(port);
    clientService.sin_addr = binAddr; 

    cout << "Connecting to " << ipInput << " on port " << port << "..." << endl;

    if (connect(activeSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        cout << "Client: connect() failed with error: " << WSAGetLastError() << endl;
        closesocket(activeSocket);
        WSACleanup();
        
        cout << "Press Enter to exit...";
        cin.ignore(1000, '\n');
        cin.get();
        exit(1);
    } else {
        cout << "Successfully connected" << endl;
        cin.ignore(1000, '\n');
    }

    //  CLIENT HANDSHAKE 
    // Send our username
    string encryptedName = encryptDecrypt(localUsername, 'K');
    send(activeSocket, encryptedName.c_str(), encryptedName.length() + 1, 0);
    // Receive server's username
    char nameBuf[50] = {0};
    recv(activeSocket, nameBuf, 50, 0);
    peerUsername = encryptDecrypt(string(nameBuf), 'K');
}

int main(){
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int choice;
    bool validChoice = false;

    //  Get and Validate Username first
    while (true) {
        cout << "Enter Username: ";
        getline(cin, localUsername);
        
        if (isValidUsername(localUsername)) {
            break; // Valid name, break the loop
        }
    }

    //  Get Host/Join Choice
    while(!validChoice) {
        cout << "1. Host a Chat (Server)\n2. Join a Chat (Client)\nChoice: ";
        cin >> choice;
        cin.ignore(); // Clear newline left by cin
        
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

    cout << "\n[System] Connected with " << peerUsername << "! Type 'exit' to quit." << endl;
    
    // Start the receiver thread
    thread worker(receivefunction, activeSocket);
    worker.detach();

    // Give the receiver thread a tiny fraction of a second to print if needed, 
    // though the UI synchronization usually handles this fine.
    this_thread::sleep_for(chrono::milliseconds(100));

    // Chat Loop
    while (isRunning) {
        char msgbuffer[200] = {0}; 
        cout << localUsername << " : ";
        cin.getline(msgbuffer, 200);
        
        string originalMsg = msgbuffer;

        if (originalMsg.empty()) {
            continue;
        }

        string encryptedMsg = encryptDecrypt(originalMsg, 'K');
        send(activeSocket, encryptedMsg.c_str(), (int)encryptedMsg.length() + 1, 0);

        if (originalMsg == "exit" || originalMsg == "Exit") {
            isRunning = false;
            break;
        }
    }

    shutdown(activeSocket, SD_BOTH);
    closesocket(activeSocket);
    WSACleanup();
    return 0;
}