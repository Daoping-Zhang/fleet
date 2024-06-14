#include"confdeal.h"
#include <sstream>


std::vector<std::vector<std::string>> parse_config(const std::string& filename)
{
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return {};
    }

    std::string line;
    std::vector<std::vector<std::string>> node_info;

    while (std::getline(infile, line)) {
        if (line.find("follower_info") != std::string::npos) {
            // 解析节点信息
            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;

            while (iss >> token) {
                tokens.push_back(token);
            }

            if (tokens.size() == 3) {
                std::string addr = tokens[1];
                std::string ip = addr.substr(0, addr.find(":"));
                std::string port = addr.substr(addr.find(":") + 1);
                std::string id = tokens[2];
                node_info.push_back({ip, port, id}); // 将节点信息插入向量
            }
        }
    }

    if (node_info.empty()) {
        std::cerr << "Error: No follower_info found in config file" << std::endl;
    }

    return node_info;
}