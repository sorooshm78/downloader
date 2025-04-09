#include <iostream>
#include <string>
#include <curl/curl.h>
#include <fstream>
#include <vector>

using namespace std;

size_t WriteCallback(void* contents, size_t file_size, size_t mem_byte, void* user_data) {
    ofstream* file = static_cast<ofstream*>(user_data);
    
    if (file) {
        file->write(static_cast<char*>(contents), mem_byte);
        return mem_byte;
    }
    return 0;
}

bool DownloadFile(const string& url, const string& output_filename) {
    CURL* curl;
    CURLcode res;
    ofstream outFile(output_filename, ios::binary);
    
    if (!outFile) {
        cerr << "Failed to open output file: " << output_filename << endl;
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

long GetFileSize(const string& url) {
    CURL *curl = curl_easy_init();  

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK){
        cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
        curl_easy_cleanup(curl);
        return 0;
    }

    double file_size = 0;
    res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &file_size);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || file_size < 0) {
        cerr << "Failed to get file file_size" << endl;
        return 0;
    }
    return static_cast<long>(file_size);
}

struct DownloadPart {
    string url;
    string filename;
    long start_byte;
    long end_byte;
    bool success;
};

int main() {
    string url = "https://file-examples.com/storage/fe2465184067ef97996fb41/2017/10/file-sample_150kB.pdf";
    string output_filename = "sample.pdf";

    long file_size = GetFileSize(url);
    cout << "file_size : " << file_size << endl;

    const int num_parts = 5;
    int part_size = file_size / num_parts;
    vector<DownloadPart> download_part;

    for (int i = 0; i < num_parts; ++i) {
        long start = i * part_size;
        long end = (i == num_parts - 1) ? file_size - 1 : start + part_size - 1;
        string part_filename = output_filename + ".part" + to_string(i);
        download_part.push_back({url, part_filename, start, end, false});
    }

    // for (auto dp : download_part){
    //     cout << "part filename" << dp.filename << endl; 
    //     cout << "strat-end : " << dp.start_byte << "-" << dp.end_byte << endl; 
    // }

    // if (DownloadFile(url, output_filename)) {
    //     cout << "File downloaded successfully to: " << output_filename << endl;
    // } else {
    //     cout << "Failed to download file." << endl;
    // }
    
    return 0;
}