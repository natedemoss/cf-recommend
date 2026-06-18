// Minimal, dependency-free JSON parser tailored for the Codeforces API.
//
// Supports the full JSON grammar (objects, arrays, strings with escapes and
// \uXXXX surrogate pairs, numbers, booleans, null). It is intentionally small
// rather than fast; the API responses we parse are at most a few MB.
#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace json {

class Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
    Value() : type_(Type::Null) {}
    explicit Value(bool b) : type_(Type::Bool), bool_(b) {}
    explicit Value(double n) : type_(Type::Number), num_(n) {}
    explicit Value(std::string s) : type_(Type::String), str_(std::move(s)) {}
    explicit Value(Array a) : type_(Type::Array), arr_(std::move(a)) {}
    explicit Value(Object o) : type_(Type::Object), obj_(std::move(o)) {}

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool(bool def = false) const { return is_bool() ? bool_ : def; }
    double as_number(double def = 0.0) const { return is_number() ? num_ : def; }
    long long as_int(long long def = 0) const {
        return is_number() ? static_cast<long long>(num_) : def;
    }
    const std::string& as_string() const {
        static const std::string empty;
        return is_string() ? str_ : empty;
    }

    const Array& as_array() const {
        static const Array empty;
        return is_array() ? arr_ : empty;
    }
    const Object& as_object() const {
        static const Object empty;
        return is_object() ? obj_ : empty;
    }

    // Object member access. Returns a static null Value when absent so chaining
    // like value["a"]["b"].as_string() never throws.
    const Value& operator[](const std::string& key) const {
        static const Value null_value;
        if (!is_object()) return null_value;
        auto it = obj_.find(key);
        return it == obj_.end() ? null_value : it->second;
    }

    bool contains(const std::string& key) const {
        return is_object() && obj_.find(key) != obj_.end();
    }

    std::size_t size() const {
        if (is_array()) return arr_.size();
        if (is_object()) return obj_.size();
        return 0;
    }

    static Value parse(const std::string& text);

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    Array arr_;
    Object obj_;
};

namespace detail {

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    Value parse() {
        skip_ws();
        Value v = parse_value();
        skip_ws();
        if (pos_ != s_.size()) fail("trailing characters after JSON document");
        return v;
    }

private:
    const std::string& s_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(const std::string& msg) const {
        throw std::runtime_error("JSON parse error at offset " +
                                 std::to_string(pos_) + ": " + msg);
    }

    char peek() const { return pos_ < s_.size() ? s_[pos_] : '\0'; }
    char get() {
        if (pos_ >= s_.size()) fail("unexpected end of input");
        return s_[pos_++];
    }

    void skip_ws() {
        while (pos_ < s_.size()) {
            char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    Value parse_value() {
        skip_ws();
        switch (peek()) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return Value(parse_string());
            case 't': case 'f': return parse_bool();
            case 'n': return parse_null();
            default: return parse_number();
        }
    }

    Value parse_object() {
        get();  // consume '{'
        Object obj;
        skip_ws();
        if (peek() == '}') { get(); return Value(std::move(obj)); }
        while (true) {
            skip_ws();
            if (peek() != '"') fail("expected string key in object");
            std::string key = parse_string();
            skip_ws();
            if (get() != ':') fail("expected ':' after object key");
            obj[std::move(key)] = parse_value();
            skip_ws();
            char c = get();
            if (c == ',') continue;
            if (c == '}') break;
            fail("expected ',' or '}' in object");
        }
        return Value(std::move(obj));
    }

    Value parse_array() {
        get();  // consume '['
        Array arr;
        skip_ws();
        if (peek() == ']') { get(); return Value(std::move(arr)); }
        while (true) {
            arr.push_back(parse_value());
            skip_ws();
            char c = get();
            if (c == ',') continue;
            if (c == ']') break;
            fail("expected ',' or ']' in array");
        }
        return Value(std::move(arr));
    }

    Value parse_bool() {
        if (s_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value(true); }
        if (s_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value(false); }
        fail("invalid literal");
    }

    Value parse_null() {
        if (s_.compare(pos_, 4, "null") == 0) { pos_ += 4; return Value(); }
        fail("invalid literal");
    }

    Value parse_number() {
        std::size_t start = pos_;
        if (peek() == '-') get();
        while (std::isdigit(static_cast<unsigned char>(peek()))) get();
        if (peek() == '.') {
            get();
            while (std::isdigit(static_cast<unsigned char>(peek()))) get();
        }
        if (peek() == 'e' || peek() == 'E') {
            get();
            if (peek() == '+' || peek() == '-') get();
            while (std::isdigit(static_cast<unsigned char>(peek()))) get();
        }
        if (pos_ == start) fail("invalid value");
        return Value(std::stod(s_.substr(start, pos_ - start)));
    }

    static void append_utf8(std::string& out, unsigned cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    unsigned parse_hex4() {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            char c = get();
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned>(c - 'A' + 10);
            else fail("invalid \\u escape");
        }
        return value;
    }

    std::string parse_string() {
        get();  // consume opening quote
        std::string out;
        while (true) {
            char c = get();
            if (c == '"') break;
            if (c == '\\') {
                char esc = get();
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        unsigned cp = parse_hex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF) {  // high surrogate
                            if (get() != '\\' || get() != 'u')
                                fail("expected low surrogate");
                            unsigned lo = parse_hex4();
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default: fail("invalid escape sequence");
                }
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
};

}  // namespace detail

inline Value Value::parse(const std::string& text) {
    return detail::Parser(text).parse();
}

}  // namespace json
