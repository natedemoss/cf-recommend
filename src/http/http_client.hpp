// Thin synchronous HTTP GET wrapper over libcurl.
#pragma once

#include <string>

namespace cfr {

struct HttpResponse {
    long status_code = 0;
    std::string body;
    bool ok() const { return status_code == 200; }
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Performs a blocking GET. Throws std::runtime_error on transport failure
    // (DNS, connection, timeout); HTTP error statuses are returned in the
    // response so callers can inspect the body.
    HttpResponse get(const std::string& url);

private:
    void* curl_ = nullptr;  // CURL*
};

}  // namespace cfr
