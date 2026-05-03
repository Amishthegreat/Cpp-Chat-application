#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <cctype> 

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// SHARED GLOBAL VARIABLES
SOCKET activeSocket = INVALID_SOCKET;
bool isRunning{true}; 
string localUsername;
string peerUsername = "Peer"; // Fallback name, gets updated during the handshake

// XOR is symmetric! Encrypting and decrypting are the exact same mathematical operation.
// Passing the encrypted string through this again with the same key restores the original text.
string encryptDecrypt(string toProcess, char key) {
    string output = toProcess;
    for (int i = 0; i < toProcess.length(); i++) {
        output[i] = toProcess[i] ^ key; 
    }
    return output;
}

// Keep the username clean so it doesn't break our UI formatting later
bool isValidUsername(const string& uname) {
    if (uname.length() < 3 || uname.length() > 15) {
        cout << "-> Username must be between 3 and 15 characters.\n";
        return false;
    }
    
    // No spaces or symbols allowed
    for (char c : uname) {
        if (!isalnum(c)) {
            cout << "-> Username can only contain letters and numbers.\n";
            return false;
        }
    }
    return true;
}

// This runs on a separate background thread so we can always listen for messages
// even while we are stuck waiting for the user to type something in the main thread.
void receivefunction(SOCKET activesocket){
    char receiveBuffer[200];
    
    while (isRunning) {
        // recv() is a blocking call. The thread stops here 
        // until a message arrives from the network.
        int byteCount = recv(activesocket, receiveBuffer, 200, 0);

        if (byteCount > 0) {
            // We only receive raw bytes, so we manually add the null-terminator 
            receiveBuffer[byteCount] = '\0'; 
            string decryptedMsg = encryptDecrypt(receiveBuffer, 'K'); 

            if (decryptedMsg == "exit" || decryptedMsg == "Exit") {
                cout << "\n[System] " << peerUsername << " has left. Exiting..." << endl;
                exit(0); 
            }
            
            //  \r pulls the cursor back to the start of the line, overwriting our typing prompt
            cout << "\r" << peerUsername << " : " << decryptedMsg << "\n";
            // Now safely reprint our typing prompt on the next line so we can keep typing
            cout << localUsername << " : " << flush; 
        }
        else if (byteCount == 0) {
            // A return of 0 means the other side closed the connection
            cout << "\n[System] " << peerUsername << " disconnected." << endl;
            break; 
        } 
        else {
            // Negative return means the connection crashed or dropped
            if(isRunning) {
                cout << "\nReceive error: " << WSAGetLastError() << endl;
            }
            break;
        }
        
        // cleaning up receiveBuffer
        memset(receiveBuffer, 0, 200);
    }
}

void server(int port){
    // Creating socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cout << "Socket creation failed!" << endl;
        WSACleanup();
        exit(1);
    }

    // Set up the blueprint for our IPv4 address
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_port = htons(port); // Convert port to network byte order so routers understand it
    service.sin_addr.s_addr = INADDR_ANY; // Listen on any available network interface

    // Binding the information
    if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        cout << "bind() failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        exit(1);
    }

    // listening for connections
    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        cout << "listen(): Error listening on socket " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        exit(1);
    }

    cout << "Waiting for a connection..." << endl;
    
    
    // accepting connection
    activeSocket = accept(serverSocket, NULL, NULL);

    if (activeSocket == INVALID_SOCKET) {
        cout << "Accept failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        exit(1);
    } 

    cout << "Connection established." << endl;
    
    // closing the server socket
    closesocket(serverSocket); 

    // SERVER HANDSHAKE 
    char nameBuf[50] = {0};
    
    recv(activeSocket, nameBuf, 50, 0);
    peerUsername = encryptDecrypt(string(nameBuf), 'K');
    
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

        //  to translate the text IP into a raw binary network IP
        if (inet_pton(AF_INET, ipInput.c_str(), &binAddr) == 1) {
            break;
        }
        cout << "Invalid IP format. Try again." << endl;
    }

    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(port);
    clientService.sin_addr = binAddr; // Use the binary IP we just validated

    cout << "Connecting to " << ipInput << " on port " << port << "..." << endl;

    // to connect to the server
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
        cin.ignore(1000, '\n'); // Clear the newline character left in the input buffer by cin
    }

    // CLIENT HANDSHAKE 

    string encryptedName = encryptDecrypt(localUsername, 'K');
    send(activeSocket, encryptedName.c_str(), encryptedName.length() + 1, 0);
    
    char nameBuf[50] = {0};
    recv(activeSocket, nameBuf, 50, 0);
    peerUsername = encryptDecrypt(string(nameBuf), 'K');
}

int main(){
    // Mandatory Windows initialization. Without this, no networking functions will work.
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int choice;
    bool validChoice = false;

    // Force the user to pick a valid name before doing anything else
    while (true) {
        cout << "Enter Username: ";
        getline(cin, localUsername);
        
        if (isValidUsername(localUsername)) {
            break; 
        }
    }

    // Role selection menu
    while(!validChoice) {
        cout << "1. Host a Chat (Server)\n2. Join a Chat (Client)\nChoice: ";
        cin >> choice;
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

    cout << "\n[System] Connected with " << peerUsername << "! Type 'exit' to quit." << endl;
    
    
    // detatch() tells the OS to let it run independently of our main loop.
    thread worker(receivefunction, activeSocket);
    worker.detach();

    // Main Chat Loop (The typing thread)
    while (isRunning) {
        char msgbuffer[200] = {0}; 
        cout << localUsername << " : ";
        cin.getline(msgbuffer, 200);
        
        string originalMsg = msgbuffer;

        // So that empty lines dont get printed
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

    // Cleanup process to avoid memory leak
    shutdown(activeSocket, SD_BOTH);
    closesocket(activeSocket);
    WSACleanup();
    return 0;
}