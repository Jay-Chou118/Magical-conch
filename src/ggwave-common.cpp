#include "ggwave-common.h"

#if !defined(_WIN32)
#include <dlfcn.h>
#include <unistd.h>
#endif

#include <cstring>
#include <fstream>
#include <iterator>

namespace {
void dummy() {}
}

std::map<std::string, std::string> parseCmdArguments(int argc, char ** argv) {
    int last = argc;
    std::map<std::string, std::string> res;
    for (int i = 1; i < last; ++i) {
        if (argv[i][0] == '-') {
            if (strlen(argv[i]) > 1) {
                char key = argv[i][1];
                std::string value = "";
                
                if (strlen(argv[i]) > 2) {
                    value = argv[i] + 2;
                } else if (i + 1 < last && argv[i + 1][0] != '-') {
                    value = argv[i + 1];
                    i++;
                }
                
                res[std::string(1, key)] = value;
            }
        }
    }

    return res;
}

std::vector<char> readFile(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open() || !file.good()) return {};

    file.unsetf(std::ios::skipws);
    std::streampos fileSize;

    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> vec;
    vec.reserve(fileSize);

    vec.insert(vec.begin(),
               std::istream_iterator<char>(file),
               std::istream_iterator<char>());

    return vec;
}

std::string getBinaryPath() {
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
    return "";
#else
    std::string result;
    void* p = reinterpret_cast<void*>(dummy);

    Dl_info info;
    dladdr(p, &info);

    if (*info.dli_fname == '/') {
        result = info.dli_fname;
    } else {
        char buff[2048];
        auto len = readlink("/proc/self/exe", buff, sizeof(buff) - 1);
        if (len > 0) {
            buff[len] = 0;
            result = buff;
        }
    }

    auto slash = result.rfind('/');
    if (slash != std::string::npos) {
        result.erase(slash + 1);
    }

    return result;
#endif
}
