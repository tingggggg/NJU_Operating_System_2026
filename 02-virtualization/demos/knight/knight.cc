#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <regex>
#include <array>
#include <cstdio>
#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
using std::string, std::to_string;

struct Game {
    string name; // Name of the traced process
    int pid;     // Pid of the traced process
    int fd;      // Memory file of the traced process

    std::vector<uintptr_t> remain; // Watched addresses
    size_t value_size; // Size of values being searched (2 for 16-bit, 4 for 32-bit)

public:
    Game(string proc_name):
        name(proc_name),
        pid(stoi(run("pidof " + proc_name))),
        value_size(4) { // Default to 32-bit

        // See: proc(5)
        string memfile = "/proc/" + to_string(pid) + "/mem";

        // We need root permission to open this file;
        // otherwise it would be too dangerous.
        fd = open(memfile.c_str(), O_RDWR);
        if (fd < 0) {
            perror(memfile.c_str());
            exit(1);
        }
    }

    ~Game() {
        close(fd);
    }

    void search_for(uint32_t val, size_t size = 4) {
        value_size = size; // Store the current search size

        if (remain.size() == 0) {
            // No match. Start a new round of search.

            string maps = run("pmap -x " + to_string(pid));

            std::regex r(
                R"(^([0-9a-f]+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\S+)\s+(.*))"
            );

            std::istringstream iss(maps);
            for (string line; std::getline(iss, line); ) {
                std::smatch match;
                if (std::regex_search(line, match, r)) {
                    string mode = match[5].str();
                    string mapping = match[6].str();

                    // Skip shared regions or regions with "lib" in name
                    if (mode.find('s') != string::npos ||
                        mapping.find("lib") != string::npos) {
                        continue;
                    }

                    // Only scan rw- regions
                    if (mode.substr(0, 2) != "rw") {
                        continue;
                    }

                    uintptr_t start = stoll(match[1].str(), nullptr, 16);
                    uintptr_t size_bytes = stoll(match[2], nullptr, 10) * 1024;
                    printf("Scanning %lx--%lx\n", start, start + size_bytes);

                    // Copy process memory to local
                    std::unique_ptr<char[]> mem(new char [size_bytes]);
                    lseek(fd, start, SEEK_SET);
                    size_t read_bytes = read(fd, mem.get(), size_bytes);

                    if (value_size == 4) {
                        // Search for 32-bit values
                        for (uintptr_t off = 0; off <= read_bytes - sizeof(uint32_t); off += 4) {
                            uint32_t *ptr = reinterpret_cast<uint32_t*>(mem.get() + off);
                            if (*ptr == val) {
                                // Found a match!
                                remain.push_back(start + off);
                            }
                        }
                    } else if (value_size == 2) {
                        // Search for 16-bit values
                        for (uintptr_t off = 0; off <= read_bytes - sizeof(uint16_t); off += 2) {
                            uint16_t *ptr = reinterpret_cast<uint16_t*>(mem.get() + off);
                            if (*ptr == static_cast<uint16_t>(val)) {
                                // Found a match!
                                remain.push_back(start + off);
                            }
                        }
                    }
                }
            }
        } else {
            // Search in the watched values.

            std::erase_if(remain, [this, val](uintptr_t addr) {
                if (value_size == 4) {
                    return load32(addr) != val;
                } else if (value_size == 2) {
                    return load16(addr) != static_cast<uint16_t>(val);
                }
                return true;
            });
        }
        printf("There are %ld match(es).\n", remain.size());
    }

    void reset() {
        remain.clear();
        value_size = 4; // Reset to default 32-bit
    }

    void overwrite(uint32_t val) {
        int nwrite = 0;
        for (uintptr_t addr : remain) {
            if (value_size == 4) {
                store32(addr, val);
            } else if (value_size == 2) {
                store16(addr, static_cast<uint16_t>(val));
            }
            nwrite++;
        }
        printf("%d value(s) written.\n", nwrite);
    }

private:
    uint32_t load32(uintptr_t addr) {
        // Load 32-bit value from another address space
        uint32_t val;
        lseek(fd, addr, SEEK_SET);
        read(fd, &val, sizeof(val));
        return val;
    }

    void store32(uintptr_t addr, uint32_t val) {
        // Store 32-bit value to another address space
        lseek(fd, addr, SEEK_SET);
        write(fd, &val, sizeof(val));
    }

    uint16_t load16(uintptr_t addr) {
        // Load 16-bit value from another address space
        uint16_t val;
        lseek(fd, addr, SEEK_SET);
        read(fd, &val, sizeof(val));
        return val;
    }

    void store16(uintptr_t addr, uint16_t val) {
        // Store 16-bit value to another address space
        lseek(fd, addr, SEEK_SET);
        write(fd, &val, sizeof(val));
    }

    static string run(const string &cmd) {
        std::array<char,128> buf;
        string result;

        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            perror(cmd.c_str());
            exit(1);
        }

        while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
            result += buf.data();
        }

        pclose(pipe);
        return result;
    }
};


int main(int argc, char *argv[]) {
    Game g(argv[1]);
    uint32_t val;
    char buf[64];

    printf(
        "Usage:\n"
        "  - s 100: search for 32-bit value\n"
        "  - s2 100: search for 16-bit value\n"
        "  - w 99999: overwrite value (for search matches)\n"
        "  - r: reset search\n\n"
    );

    while (!feof(stdin)) {
        printf("(%s %d) ", g.name.c_str(), g.pid);
        scanf("%s", buf);

        switch (buf[0]) {
            case 'q': return 0;
            case 's':
                if (buf[1] == '2') {
                    // Search for 16-bit value
                    scanf("%d", &val);
                    g.search_for(val, 2);
                } else {
                    // Search for 32-bit value
                    scanf("%d", &val);
                    g.search_for(val, 4);
                }
                break;
            case 'w': scanf("%d", &val); g.overwrite(val); break;
            case 'r': g.reset(); break;
        }
    }
}
