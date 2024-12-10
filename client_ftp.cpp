//client_ftp.cpp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                #include <iostream>
#include <string>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <regex>
#include <netdb.h>
#include <sstream>
#include <cstring>

#define BUFFER_SIZE 1024

void sendCommand(int socket, const std::string& command) {
    int n = send(socket, command.c_str(), command.length(), 0);
    if (n < 0) {
        std::cerr << "Error sending command.\n";
    }
}

std::string receiveResponse(int socket) {
    char buffer[BUFFER_SIZE];
    int bytesReceived = recv(socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesReceived <= 0) {
        return "Connection closed or error.";
    }
    buffer[bytesReceived] = '\0';
    return std::string(buffer);
}

void handleLIST(int controlSocket) {
    sendCommand(controlSocket, "PASV\r\n");
    std::string pasvResponse = receiveResponse(controlSocket);
    std::cout << "Server: " << pasvResponse << std::endl;

    std::regex pasvRegex(R"(\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\))");
    std::smatch match;
    if (!std::regex_search(pasvResponse, match, pasvRegex)) {
        std::cerr << "Failed to parse PASV response.\n";
        return;
    }

    std::string ipAddress = match[1].str() + "." + match[2].str() + "." + match[3].str() + "." + match[4].str();
    int port = (std::stoi(match[5].str()) << 8) + std::stoi(match[6].str());

    int dataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSocket < 0) {
        perror("Data socket creation failed");
        return;
    }

    sockaddr_in dataAddr{};
    dataAddr.sin_family = AF_INET;
    dataAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ipAddress.c_str(), &dataAddr.sin_addr) <= 0) {
        perror("Invalid address for data connection");
        close(dataSocket);
        return;
    }

    if (connect(dataSocket, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0) {
        perror("Failed to connect to data socket");
        close(dataSocket);
        return;
    }

    sendCommand(controlSocket, "LIST\r\n");
    std::cout << "Server: " << receiveResponse(controlSocket) << std::endl;

    char buffer[BUFFER_SIZE];
    int bytesRead;
    while ((bytesRead = recv(dataSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        std::cout << buffer;
    }

    close(dataSocket);
    std::cout << "\nServer: " << receiveResponse(controlSocket) << std::endl;
    std::cout << "\nData connection closed.\n";
}

void handleSTOR(int controlSocket, const std::string& fileName) {
    std::ifstream file(fileName, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for upload: " << fileName << std::endl;
        return;
    }

    sendCommand(controlSocket, "PASV\r\n");
    std::string pasvResponse = receiveResponse(controlSocket);
    std::cout << "Server: " << pasvResponse << std::endl;

    std::regex pasvRegex(R"(\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\))");
    std::smatch match;
    if (!std::regex_search(pasvResponse, match, pasvRegex)) {
        std::cerr << "Failed to parse PASV response.\n";
        return;
    }

    std::string ipAddress = match[1].str() + "." + match[2].str() + "." + match[3].str() + "." + match[4].str();
    int port = (std::stoi(match[5].str()) * 256) + std::stoi(match[6].str());

    int dataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSocket < 0) {
        perror("Data socket creation failed");
        return;
    }

    sockaddr_in dataAddr{};
    dataAddr.sin_family = AF_INET;
    dataAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ipAddress.c_str(), &dataAddr.sin_addr) <= 0) {
        perror("Invalid address for data connection");
        close(dataSocket);
        return;
    }

    if (connect(dataSocket, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0) {
        perror("Failed to connect to data socket");
        close(dataSocket);
        return;
    }

    sendCommand(controlSocket, "STOR " + fileName + "\r\n");
    std::cout << "Server: " << receiveResponse(controlSocket) << std::endl;

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        int bytesToSend = file.gcount();
        if (send(dataSocket, buffer, bytesToSend, 0) != bytesToSend) {
            perror("Failed to send file data");
            break;
        }
    }

    file.close();
    close(dataSocket);

    std::cout << "Server: " << receiveResponse(controlSocket) << std::endl;

    std::cout << "File uploaded successfully.\n";
}

void handleRETR(int controlSocket, const std::string& fileName) {
    sendCommand(controlSocket, "PASV\r\n");
    std::string pasvResponse = receiveResponse(controlSocket);
    std::cout << "Server PASV: " << pasvResponse << std::endl;

    if (pasvResponse.substr(0, 3) != "227") {
        std::cerr << "Error: PASV command failed.\n";
        return;
    }

    std::regex pasvRegex(R"(\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\))");
    std::smatch match;
    if (!std::regex_search(pasvResponse, match, pasvRegex)) {
        std::cerr << "Error: Failed to parse PASV response.\n";
        return;
    }

    std::string ipAddress = match[1].str() + "." + match[2].str() + "." + match[3].str() + "." + match[4].str();
    int port = (std::stoi(match[5].str()) << 8) + std::stoi(match[6].str());

    std::cout << "Connecting to data port " << port << " on IP " << ipAddress << std::endl;

    int dataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSocket < 0) {
        perror("Error creating data socket");
        return;
    }

    sockaddr_in dataAddr{};
    dataAddr.sin_family = AF_INET;
    dataAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ipAddress.c_str(), &dataAddr.sin_addr) <= 0) {
        perror("Invalid address for data connection");
        close(dataSocket);
        return;
    }

    if (connect(dataSocket, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0) {
        perror("Failed to connect to data socket");
        close(dataSocket);
        return;
    }
    std::cout << "Data connection established.\n";

    sendCommand(controlSocket, "RETR " + fileName + "\r\n");
    std::string retrResponse = receiveResponse(controlSocket);
    std::cout << "Server RETR: " << retrResponse << std::endl;

    if (retrResponse.substr(0, 3) != "150") {
        std::cerr << "Error: Server did not initiate file transfer.\n";
        close(dataSocket);
        return;
    }

    std::ofstream file(fileName, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open local file for writing: " << fileName << "\n";
        close(dataSocket);
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytesReceived;
    int totalBytes = 0;
    while ((bytesReceived = recv(dataSocket, buffer, sizeof(buffer), 0)) > 0) {
        file.write(buffer, bytesReceived);
        totalBytes += bytesReceived;
    }

    std::cout << "Received " << totalBytes << " bytes.\n";

    file.close();
    close(dataSocket);

    std::cout << "Server: " << receiveResponse(controlSocket) << std::endl;

    std::cout << "File downloaded successfully.\n";
}


void handlePWD(int socket) {
    sendCommand(socket, "PWD\r\n");

    std::string response = receiveResponse(socket);
    std::cout << "Server: " << response << std::endl;
}

void handleMDTM(int controlSocket, const std::string& fileName) {
    sendCommand(controlSocket, "MDTM " + fileName + "\r\n");

    std::string response = receiveResponse(controlSocket);
    std::cout << "Server: " << response << std::endl;

    if (response.substr(0, 3) == "213") {
        std::string dateTime = response.substr(4, 14);  // Extrage 14 caractere pentru data și ora

        if (std::regex_match(dateTime, std::regex(R"(\d{14})"))) {
            std::cout << "File modification date and time: " << dateTime << std::endl;

            struct tm tm;
            std::string year = dateTime.substr(0, 4);
            std::string month = dateTime.substr(4, 2);
            std::string day = dateTime.substr(6, 2);
            std::string hour = dateTime.substr(8, 2);
            std::string minute = dateTime.substr(10, 2);
            std::string second = dateTime.substr(12, 2);

            std::string dateStr = year + "-" + month + "-" + day + " " + hour + ":" + minute + ":" + second;

            std::cout << "Formatted date and time: " << dateStr << std::endl;
        } else {
            std::cerr << "Invalid date-time format received from server.\n";
        }
    } else {
        std::cerr << "Server responded with an error: " << response << std::endl;
    }
}

std::string createPORTCommand(const std::string& ip, int port) {
    std::stringstream ipStream(ip);
    std::string segment;
    int ipParts[4];
    for (int i = 0; i < 4; i++) {
        std::getline(ipStream, segment, '.');
        ipParts[i] = std::stoi(segment);
    }

    // Calculăm high byte și low byte pentru port
    int highByte = port / 256;
    int lowByte = port % 256;

    // Construiește comanda PORT
    std::ostringstream commandStream;
    commandStream << "PORT " << ipParts[0] << ","
                  << ipParts[1] << ","
                  << ipParts[2] << ","
                  << ipParts[3] << ","
                  << highByte << ","
                  << lowByte << "\r\n";
    return commandStream.str();
}


void handlePORT(int controlSocket, const std::string& ip, int port) {
    std::string portCommand = createPORTCommand(ip, port);

    sendCommand(controlSocket, portCommand);

    std::string response = receiveResponse(controlSocket);
    std::cout << "Server: " << response << std::endl;

    if (response.substr(0, 3) == "200") {
        std::cout << "PORT command successful, waiting for data connection...\n";
    } else {
        std::cerr << "Failed to establish data connection. Server response: " << response << std::endl;
    }
}


int main() {
    const char* serverIP = "127.0.0.1"; // Adresa IP a serverului
    //const char* serverIP = "192.168.1.132";
    //const char* serverIP = "172.29.140.129";
    const int port = 2121;               // Portul serverului FTP
    int sock;

    // Creare socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    std::cout << "Connected to FTP server at " << serverIP << ":" << port << std::endl;

    std::cout << "Server: " << receiveResponse(sock) << std::endl;

    while (true) {
        std::cout << "ftp> ";
        std::string command;
        std::getline(std::cin, command);

        if (command == "quit") {
            sendCommand(sock, "QUIT\r\n");
            std::cout << "Server: " << receiveResponse(sock) << std::endl;
            break;
        } else if (command.substr(0, 4) == "STOR") {
            std::string fileName = command.substr(5);
            handleSTOR(sock, fileName);
        } else if (command.substr(0, 4) == "RETR") {
            std::string fileName = command.substr(5);
            handleRETR(sock, fileName);
        } else if (command == "LIST") {
           handleLIST(sock);
        } else if (command == "MDMT") {
           std::string fileName = command.substr(5);
           handleMDTM(sock, fileName);
        } else if (command.substr(0, 4) == "PORT") {
           std::string ip = "127.0.0.1";  // Adresa IP a serverului
           int port = 2121;  // Portul pentru conexiunea de date (modifică-l dacă este necesar)
           handlePORT(sock, ip, port);
        } else {
            command += "\r\n";
            sendCommand(sock, command);
            std::cout << "Server: " << receiveResponse(sock) << std::endl;
        }
    }

    close(sock);
    return 0;
}

