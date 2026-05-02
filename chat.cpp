#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <limits>
#include <cctype>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// --- SHARED GLOBAL VARIABLES ---
SOCKET activeSocket = INVALID_SOCKET;
bool isRunning{true};
string localUsername;
string peerUsername = "Peer";
vector<string> chatHistory;

// XOR Encryption/Decryption
string encryptDecrypt(string toProcess, char key) {
    string output = toProcess;
    for (int i = 0; i < toProcess.length(); i++) {
        output[i] = toProcess[i] ^ key;
    }
    return output;
}

// Username Validation
bool isValidUsername(const string& uname) {
    if (uname.length() < 3 || uname.length() > 15) return false;
    for (char c : uname) if (!isalnum(c)) return false;
    return true;
}

// Display IPs for the Host
void displayLocalIPs() {
    char hostName[256];
    gethostname(hostName, sizeof(hostName));
    addrinfo hints = {0}, *addressList = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo(hostName, NULL, &hints, &addressList);

    cout << "[System] Share one of these IPs with your friend:" << endl;
    for (addrinfo* ptr = addressList; ptr != nullptr; ptr = ptr->ai_next) {
        sockaddr_in* sockaddr_ipv4 = (sockaddr_in*)ptr->ai_addr;
        char ipString[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipString, INET_ADDRSTRLEN);
        if (string(ipString) != "127.0.0.1") cout << " -> " << ipString << endl;
    }
    freeaddrinfo(addressList);
}

// Receiver Thread Logic
void receiveFunction(SOCKET activesocket) {
    char receiveBuffer[201]; // Extra byte for null terminator
    while (isRunning) {
        int byteCount = recv(activesocket, receiveBuffer, 200, 0);

        if (byteCount > 0) {
            receiveBuffer[byteCount] = '\0';
            string decryptedMsg = encryptDecrypt(string(receiveBuffer), 'K');

            // Handle Protocol Signals
            if (decryptedMsg == "TYPING:1") {
                cout << "\r[" << peerUsername << " is typing...] " << flush;
                continue;
            } 
            if (decryptedMsg == "TYPING:0") {
                // Clear the typing line using spaces and return cursor
                cout << "\r                                     \r" << localUsername << " : " << flush;
                continue;
            }
            if (decryptedMsg == "exit" || decryptedMsg == "Exit") {
                cout << "\n[System] " << peerUsername << " disconnected. Press Enter to quit." << endl;
                isRunning = false;
                break;
            }

            // Normal Message Handling
            chatHistory.push_back(peerUsername + " : " + decryptedMsg);
            cout << "\r" << peerUsername << " : " << decryptedMsg << "\n";
            cout << localUsername << " : " << flush;
        } 
        else if (byteCount <= 0) break;
    }
}

// SERVER setup
void server(int port) {
    displayLocalIPs();
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in service = {AF_INET, htons(port)};
    service.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (SOCKADDR*)&service, sizeof(service));
    listen(serverSocket, 1);
    
    cout << "Waiting for a connection..." << endl;
    activeSocket = accept(serverSocket, NULL, NULL);
    cout << "Connection established." << endl;
    closesocket(serverSocket);

    // Handshake
    char nameBuf[50] = {0};
    recv(activeSocket, nameBuf, 50, 0);
    peerUsername = encryptDecrypt(string(nameBuf), 'K');
    string encryptedName = encryptDecrypt(localUsername, 'K');
    send(activeSocket, encryptedName.c_str(), (int)encryptedName.length() + 1, 0);
}

// CLIENT setup
void client(int port) {
    activeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    string ipInput;
    cout << "Enter Target IP: ";
    cin >> ipInput;

    sockaddr_in clientService = {AF_INET, htons(port)};
    inet_pton(AF_INET, ipInput.c_str(), &clientService.sin_addr);

    if (connect(activeSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        cout << "Connect failed." << endl; exit(1);
    }
    
    // Handshake
    string encryptedName = encryptDecrypt(localUsername, 'K');
    send(activeSocket, encryptedName.c_str(), (int)encryptedName.length() + 1, 0);
    char nameBuf[50] = {0};
    recv(activeSocket, nameBuf, 50, 0);
    peerUsername = encryptDecrypt(string(nameBuf), 'K');
    cin.ignore(1000, '\n');
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    while (true) {
        cout << "Enter Username: ";
        getline(cin, localUsername);
        if (isValidUsername(localUsername)) break;
        cout << "Invalid Username (3-15 chars, alphanumeric only)." << endl;
    }

    cout << "1. Host\n2. Join\nChoice: ";
    int choice; cin >> choice;
    cin.ignore();

    if (choice == 1) server(5500); else client(5500);

    cout << "\n[System] Connected! Type 'history' to see logs or 'exit' to quit.\n" << endl;
    
    thread worker(receiveFunction, activeSocket);
    worker.detach();

    while (isRunning) {
        char msgbuffer[200] = {0};
        cout << localUsername << " : " << flush;

        // SIGNAL: Typing Started (Encrypted)
        string typingStart = encryptDecrypt("TYPING:1", 'K');
        send(activeSocket, typingStart.c_str(), (int)typingStart.length() + 1, 0);

        cin.getline(msgbuffer, 200);

        // SIGNAL: Typing Finished (Encrypted)
        string typingStop = encryptDecrypt("TYPING:0", 'K');
        send(activeSocket, typingStop.c_str(), (int)typingStop.length() + 1, 0);

        string input = msgbuffer;
        if (input.empty()) continue;

        if (input == "history") {
            cout << "\n--- Chat History ---" << endl;
            for (const auto& log : chatHistory) cout << log << endl;
            cout << "--------------------" << endl;
            continue;
        }

        string encryptedMsg = encryptDecrypt(input, 'K');
        send(activeSocket, encryptedMsg.c_str(), (int)encryptedMsg.length() + 1, 0);
        chatHistory.push_back("Me : " + input);

        if (input == "exit") { isRunning = false; break; }
    }

    closesocket(activeSocket);
    WSACleanup();
    return 0;
}