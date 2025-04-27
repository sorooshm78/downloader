#include <iostream>
#include <string>
#include <curl/curl.h>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <fmt/core.h>
#include <fmt/color.h>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std;

struct DownloadPart {
    string url;
    string filename;
    long downloaded_byte;
    long total_byte;
    long start_byte;
    long end_byte;
    bool success;
};

class Downloader
{
private:
    string url_;
    string filename_;
    int num_parts_;
    vector<DownloadPart> download_parts_;
    vector<thread> threads_;
    atomic<bool> running_{true};
    
    static size_t WriteCallback(void* contents, size_t file_size, size_t mem_byte, void* user_data) {
        ofstream* file = static_cast<ofstream*>(user_data);
        
        if (file) {
            file->write(static_cast<char*>(contents), mem_byte);
            return mem_byte;
        }
        return 0;
    }

    static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
        DownloadPart* part = static_cast<DownloadPart*>(clientp);
        if (dltotal > 0) {
            part->downloaded_byte = dlnow;
        }
        return 0;
    }

    bool DownloadFile(DownloadPart& download_part) {
        CURL* curl;
        CURLcode res;
        ofstream outFile(download_part.filename, ios::binary);

        if (!outFile) {
            cerr << "Failed to open output file: " << download_part.filename << endl;
            return false;
        }
    
        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, download_part.url.c_str());
            string range = to_string(download_part.start_byte) + "-" + (download_part.end_byte > 0 ? to_string(download_part.end_byte) : "");
            curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &download_part);

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
        download_part.success = true;
        return true;
    }

public:
    bool MergeParts()
    {
        ofstream final_file(filename_, ios::binary | ios::trunc);
        if (!final_file)
        {
            cerr << "Cannot create final file: " << filename_ << '\n';
            return false;
        }

        for (int i = 0; i < num_parts_; ++i)
        {
            const string part_name = filename_ + ".part" + to_string(i + 1);
            ifstream part_file(part_name, ios::binary);

            if (!part_file)
            {
                cerr << "Missing part file " << part_name << '\n';
                return false;
            }

            final_file << part_file.rdbuf();
            part_file.close();
        }
        final_file.close();

        for (int i = 0; i < num_parts_; ++i)
        {
            const string part_name = filename_ + ".part" + to_string(i + 1);
            fs::remove(part_name);
        }
        return true;
    }

    long GetFileSize()
    {
        CURL *curl = curl_easy_init();  

        curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
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
    
    void CompleteDisplayProgress() {
        fmt::print("\x1B[2J\x1B[H"); 

        for(int i=0; i<download_parts_.size(); i++) {
            int bar_width = 50;
            char fill_char = '=';
            
            const string bar = fmt::format(
                "[{0}]",
                string(bar_width,  fill_char)
            );
            
            fmt::print(fmt::fg(fmt::color::spring_green), "{} {:.2f}% \n", bar, 100.00);
        }
        fflush(stdout);
    }

    void DisplayProgress() {
        while (running_) {
            fmt::print("\x1B[2J\x1B[H"); 

            for(int i=0; i<download_parts_.size(); i++) {
                int bar_width = 50;
                double  percent = 100.0 * static_cast<double>(download_parts_[i].downloaded_byte) / download_parts_[i].total_byte;
                const double fraction = percent / 100.0;
                const int filled      = static_cast<int>(fraction * bar_width);
                char   fill_char = '=';
                
                const string bar = fmt::format(
                    "[{0}{1}]",
                    string(filled,  fill_char),                 // filled section
                    string(bar_width - filled, ' ')       // empty section
                );
                
                fmt::print(fmt::fg(fmt::color::spring_green), "{} {:.2f}% \n", bar, percent);
            }
            fflush(stdout);
            this_thread::sleep_for(chrono::seconds(5));
        }
    }

    void Start()
    {
        // Start progress display thread
        thread progress_thread(&Downloader::DisplayProgress, this);

        // Start download threads
        threads_.reserve(num_parts_);
        for (DownloadPart& download_part : download_parts_){
            threads_.emplace_back([this, &download_part]() {
                this->DownloadFile(download_part);
            });
        }
    
        for (auto &thread : threads_) {
            thread.join();
        }

        running_ = false;
        progress_thread.join();
        CompleteDisplayProgress();
        MergeParts();
    }
    
    Downloader(string url, string filename, int num_parts): url_(url), filename_(filename), num_parts_(num_parts)
    {
        long file_size = GetFileSize();
        if (file_size == 0){
            throw runtime_error("lenght not exist");
        }
        int part_size = file_size / num_parts;
        
        for (int i = 0; i < num_parts; ++i) {
            long start = i * part_size;
            long end = (i == num_parts - 1) ? file_size - 1 : start + part_size - 1;
            string part_filename = filename_ + ".part" + to_string(i + 1);
            download_parts_.push_back({url, part_filename, 0, part_size, start, end, false});
        }
    }
};

int main() {
    const string url = "https://pdn.sharezilla.ir/d/game/Marvels.Spider-Man.2-RUNE_p30download.com.part01.rar";
    const string filename = "sample.pdf";
    const int num_parts = 5;

    Downloader downloader(url, filename, num_parts);
    downloader.Start();
    return 0;
}