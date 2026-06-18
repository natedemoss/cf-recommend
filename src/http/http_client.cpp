#include "http/http_client.hpp"

#include <curl/curl.h>

#include <stdexcept>

namespace cfr {

namespace {
size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}
}  // namespace

HttpClient::HttpClient() {
    static bool global_init = [] {
        return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    }();
    if (!global_init) throw std::runtime_error("failed to initialise libcurl");

    curl_ = curl_easy_init();
    if (!curl_) throw std::runtime_error("failed to create curl handle");
}

HttpClient::~HttpClient() {
    if (curl_) curl_easy_cleanup(static_cast<CURL*>(curl_));
}

HttpResponse HttpClient::get(const std::string& url) {
    auto* curl = static_cast<CURL*>(curl_);
    std::string body;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "cf-recommend/0.1 (+https://github.com)");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // accept gzip/deflate
#ifdef CURLSSLOPT_NATIVE_CA
    // Verify HTTPS against the OS certificate store. This keeps a statically
    // linked binary portable — it doesn't need to ship or locate a CA bundle.
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS,
                     static_cast<long>(CURLSSLOPT_NATIVE_CA));
#endif

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        throw std::runtime_error(std::string("network error: ") +
                                 curl_easy_strerror(rc));
    }

    HttpResponse resp;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
    resp.body = std::move(body);
    return resp;
}

}  // namespace cfr
