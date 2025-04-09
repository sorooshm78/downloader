#include <iostream>
#include <string>
#include <curl/curl.h>
#include <fstream>

using namespace std;

static size_t WriteCallback(void* contents, size_t size, size_t mem_byte, void* user_data) {
    ofstream* file = static_cast<ofstream*>(user_data);
    
    if (file) {
        file->write(static_cast<char*>(contents), mem_byte);
        return mem_byte;
    }
    return 0;
}

bool DownloadFile(const string& url, const string& outputFilename) {
    CURL* curl;
    CURLcode res;
    ofstream outFile(outputFilename, ios::binary);
    
    if (!outFile) {
        cerr << "Failed to open output file: " << outputFilename << endl;
        return false;
    }

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
            curl_easy_cleanup(curl);
            outFile.close();
            return false;
        }
        
        curl_easy_cleanup(curl);
    } else {
        cerr << "Failed to initialize libcurl" << endl;
        outFile.close();
        return false;
    }
    
    outFile.close();
    return true;
}

int main() {
    string url = "https://file-examples.com/storage/fe2465184067ef97996fb41/2017/10/file-sample_150kB.pdf";
    string outputFilename = "sample.pdf";

    if (DownloadFile(url, outputFilename)) {
        cout << "File downloaded successfully to: " << outputFilename << endl;
    } else {
        cout << "Failed to download file." << endl;
    }
    
    return 0;
}