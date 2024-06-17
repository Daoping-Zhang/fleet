#include <unordered_map>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;




struct LogEntry {
    std::string key;
    std::string value;
    std::string method;
    uint64_t hashKey;
    int term;

    LogEntry(std::string k, std::string v, std::string m, uint64_t h, int t)
        : key(std::move(k)), value(std::move(v)), method(std::move(m)), hashKey(h), term(t) {}
};

struct UncommittedEntriesResult {
    std::vector<LogEntry> entries;
    int processedCount;
};

class FleetLog {
public:
    std::unordered_map<int, std::vector<LogEntry>> logs;               // 每个组的日志条目
    std::unordered_map<int, uint64_t> latest_index;                    // 每个组的最新日志索引
    std::unordered_map<int, uint64_t> committed_index;                 // 每个组的已提交日志索引
    std::unordered_map<int, std::unordered_map<uint64_t, int>> vote_count; // 每个组中每个日志条目的投票计数
    std::unordered_map<int, std::unordered_map<int, uint64_t>> latest_indexes_by_node;

    void append(int groupId, const LogEntry& entry) {
        logs[groupId].push_back(entry);
        latest_index[groupId] = logs[groupId].size();  // 更新最新索引（从1开始计数）
        vote_count[groupId][latest_index[groupId]] = 0; // 初始化投票计数
    }

    void commit(int groupId, uint64_t index) {
        if (index <= latest_index[groupId]) {
            committed_index[groupId] = index;  // 确认提交到指定索引
        }
    }

    uint64_t getLatestIndex(int groupId) {
        return latest_index[groupId];
    }

    uint64_t getCommittedIndex(int groupId) {
        return committed_index[groupId];
    }

    UncommittedEntriesResult getUncommittedEntries(int groupId, int nodeId, int maxEntries) {

    UncommittedEntriesResult result;
    uint64_t nodeLatestIndex = 0;

    // 查找节点的最新索引
    if (latest_indexes_by_node[groupId].find(nodeId) != latest_indexes_by_node[groupId].end()) {
        nodeLatestIndex = latest_indexes_by_node[groupId][nodeId];
    }
    std::cout << "Node " << nodeId << " latest index: " << nodeLatestIndex << std::endl;

    uint64_t latestIndex = latest_index[groupId];
    std::cout << "Latest index for group " << groupId << ": " << latestIndex << std::endl;

    for (uint64_t i = nodeLatestIndex; i < latestIndex && result.processedCount < maxEntries; ++i) {
        if (i < logs[groupId].size()) {
            
            result.entries.push_back(logs[groupId][i]);

            result.processedCount++;

            const LogEntry& entry = logs[groupId][i];
            std::cout << "Uncommitted entry at index " << i << ": {"
                      << "key: " << entry.key << ", "
                      << "value: " << entry.value << ", "
                      << "method: " << entry.method << ", "
                      << "hashKey: " << entry.hashKey << ", "
                      << "term: " << entry.term << "}" << std::endl;
        }else
        {
            std::cout<< "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        }
    }

    return result;
}



    std::string serializeLogEntries(const std::vector<LogEntry>& entries) {
        json j = json::array();
        for (const auto& entry : entries) {
            j.push_back({
                {"key", entry.key},
                {"value", entry.value},
                {"method", entry.method},
                {"hashKey", entry.hashKey},
                {"term", entry.term}
            });
        }
        return j.dump();
    }

    std::vector<LogEntry> deserializeLogEntries(const std::string& data) {
        json j = json::parse(data);
        std::vector<LogEntry> entries;
        for (const auto& item : j) {
            entries.emplace_back(item["key"], item["value"], item["method"], item["hashKey"], item["term"]);
        }
        return entries;
    }

    // 增加投票数
    void addVote(int groupId, uint64_t index, int nodeId) {
        if (latest_indexes_by_node[groupId][nodeId] < index){
            vote_count[groupId][index]++;
            latest_indexes_by_node[groupId][nodeId] = index;
            printf("成功投票，当前票数%d\n", vote_count[groupId][index]);
        } else {
            printf("重复投票不计算\n");
        }
    }

    // 检查是否有未提交的日志条目并发送
    std::vector<LogEntry> getEntriesToSend(int groupId, uint64_t fromIndex) {
        std::vector<LogEntry> entriesToSend;
        uint64_t latestIndex = latest_index[groupId];

        for (uint64_t i = fromIndex; i <= latestIndex; ++i) {
            entriesToSend.push_back(logs[groupId][i]);
        }

        return entriesToSend;
    }
std::string serializeGroupLogs(int groupId) {
    json j;
    j["logs"] = json::array();

    if (logs.find(groupId) != logs.end()) {
        for (const auto& entry : logs[groupId]) {
            j["logs"].push_back({
                {"key", entry.key},
                {"value", entry.value},
                {"method", entry.method},
                {"hashKey", entry.hashKey},
                {"term", entry.term}
            });
        }
    }

    j["latest_index"] = latest_index[groupId];
    j["committed_index"] = committed_index[groupId];
    j["vote_count"] = vote_count[groupId];

    return j.dump();
}

void deserializeGroupLogs(int groupId, const std::string& data) {
    try {
        json j = json::parse(data);

        logs[groupId].clear();
        for (const auto& item : j["logs"]) {
            if (!item.is_object()) {
                throw std::runtime_error("Log entry is not an object.");
            }
            logs[groupId].emplace_back(
                item["key"].get<std::string>(),
                item["value"].get<std::string>(),
                item["method"].get<std::string>(),
                item["hashKey"].get<uint64_t>(),
                item["term"].get<int>()
            );
        }

        latest_index[groupId] = j["latest_index"].get<uint64_t>();
        committed_index[groupId] = j["committed_index"].get<uint64_t>();

        vote_count[groupId].clear();
        for (const auto& vote : j["vote_count"].items()) {
            vote_count[groupId][std::stoull(vote.key())] = vote.value().get<int>();
        }
    } catch (const json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error deserializing group logs: " << e.what() << std::endl;
    }
}
// 序列化函数
json serializeLatestIndexesByNode(int groupId) {
    json j;
    if (latest_indexes_by_node.find(groupId) != latest_indexes_by_node.end()) {
        for (const auto& entry : latest_indexes_by_node[groupId]) {
            int nodeId = entry.first;
            uint64_t index = entry.second;
            j[std::to_string(nodeId)] = index;
        }
    }
    return j;
}

// 反序列化函数
void deserializeLatestIndexesByNode(int groupId, const json& j) {
    latest_indexes_by_node[groupId].clear();
    for (auto& entry : j.items()) {
        int nodeId = std::stoi(entry.key());
        uint64_t index = entry.value().get<uint64_t>();
        latest_indexes_by_node[groupId][nodeId] = index;
    }
}

};