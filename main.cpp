#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <URL> <output_file>" << std::endl;
        return 1;
    }

    std::string url = argv[1];
    std::string outputFile = argv[2];

    // Parse URL (assumes format: http://host/path)
    size_t hostStart = url.find("://");
    if (hostStart == std::string::npos) {
        std::cerr << "Invalid URL: Missing protocol" << std::endl;
        return 1;
    }
    hostStart += 3; // Skip "://"

    size_t pathStart = url.find('/', hostStart);
    std::string host = (pathStart == std::string::npos) ? url.substr(hostStart) : url.substr(hostStart, pathStart - hostStart);
    std::string path = (pathStart == std::string::npos) ? "/" : url.substr(pathStart);

    std::cout << "Host: " << host << std::endl;
    std::cout << "Path: " << path << std::endl;

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }
    
    // Resolve hostname
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        std::cerr << "Failed to resolve hostname" << std::endl;
        close(sock);
        return 1;
    }

    // Set up server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(80); // HTTP port
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Connect to server
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to connect to server" << std::endl;
        close(sock);
        return 1;
    }

    // Send HTTP GET request
    std::string request = "GET " + path + " HTTP/1.1\r\n"
                          "Host: " + host + "\r\n"
                          "Connection: close\r\n\r\n";
    send(sock, request.c_str(), request.size(), 0);

    // Open output file
    FILE* file = fopen(outputFile.c_str(), "wb");
    if (!file) {
        std::cerr << "Failed to open output file" << std::endl;
        close(sock);
        return 1;
    }

    // Read response and save to file
    char buffer[4096];
    bool headerEnded = false;
    while (true) {
        int bytesRead = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) break;

        if (!headerEnded) {
            // Find end of HTTP header (double CRLF)
            std::string data(buffer, bytesRead);
            size_t headerEnd = data.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                // Write content after header
                fwrite(buffer + headerEnd + 4, 1, bytesRead - (headerEnd + 4), file);
                headerEnded = true;
            }
        } else {
            fwrite(buffer, 1, bytesRead, file);
        }
    }

    fclose(file);
    close(sock);
    std::cout << "File downloaded: " << outputFile << std::endl;

    return 0;
}