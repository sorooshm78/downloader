#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

using namespace std;

class Request {
private:
    string host;
    string path;

public:
    Request(string url) {
        size_t hostStart = url.find("://");
        if (hostStart == string::npos) {
            throw invalid_argument("Invalid URL: Missing protocol");
        }
        hostStart += 3; // Skip "://"
    
        size_t pathStart = url.find('/', hostStart);
        host = (pathStart == string::npos) ? url.substr(hostStart) : url.substr(hostStart, pathStart - hostStart);
        path = (pathStart == string::npos) ? "/" : url.substr(pathStart);
    
        cout << "Host: " << host << endl;
        cout << "Path: " << path << endl;
    }

    string GetRequest() {
        string request = "GET " + path + " HTTP/1.1\r\n"
                          "Host: " + host + "\r\n"
                          "Connection: close\r\n\r\n";
        return request;
    }
    
    string GetHost() {
        return host;
    }

    string GetPath() {
        return path;
    }
};


class Connection {
private:
    int sock;

public:
    Connection(){
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            throw std::runtime_error("Failed to create socket: " + string(strerror(errno)));
        }
    }

    ~Connection(){
        close(sock);
    }

    void CreateConnection(string host){
        // Resolve hostname
        struct hostent* server = gethostbyname(host.c_str());
        if (!server) {
            close(sock);
            throw std::runtime_error("Failed to resolve hostname");
        }
        
        // Set up server address
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(80); // HTTP port
        memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
        
        // Connect to server
        if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            close(sock);
            throw std::runtime_error("Failed to connect to server");
        }
    }
    
    void Send(string request){
        send(sock, request.c_str(), request.size(), 0);
    }

    void SaveTo(ofstream& file){
        vector<char> buffer(4096);
        bool headerEnded = false;

        while (true) {
            ssize_t bytesRead = recv(sock, buffer.data(), buffer.size(), 0);
            if (bytesRead <= 0) break;

            if (!headerEnded) {
                string data(buffer.begin(), buffer.begin() + bytesRead);
                size_t headerEnd = data.find("\r\n\r\n");
                if (headerEnd != string::npos) {
                    file.write(data.c_str() + headerEnd + 4, bytesRead - (headerEnd + 4));
                    headerEnded = true;
                    continue;
                }
            } else {
                file.write(buffer.data(), bytesRead);
            }
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <URL> <output_file>" << std::endl;
        return 1;
    }

    string url = argv[1];
    string output_file_name = argv[2];

    Request request(url);
    Connection connection;
    connection.CreateConnection(request.GetHost());
    connection.Send(request.GetRequest());
    ofstream output_file(output_file_name);
    connection.SaveTo(output_file);

    return 0;
}