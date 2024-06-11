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

class FleetLog {
public:
    std::unordered_map<int, std::vector<LogEntry>> logs;               // 每个组的日志条目
    std::unordered_map<int, uint64_t> latest_index;                    // 每个组的最新日志索引
    std::unordered_map<int, uint64_t> committed_index;                 // 每个组的已提交日志索引

    void append(int groupId, const LogEntry& entry) {
        logs[groupId].push_back(entry);
        latest_index[groupId] = logs[groupId].size() - 1;  // 更新最新索引（从0开始计数）
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

    std::vector<LogEntry> getUncommittedEntries(int groupId) {
        std::vector<LogEntry> uncommittedEntries;
        uint64_t commitIndex = committed_index[groupId];
        uint64_t latestIndex = latest_index[groupId];

        for (uint64_t i = commitIndex + 1; i <= latestIndex; ++i) {
            uncommittedEntries.push_back(logs[groupId][i]);
        }

        return uncommittedEntries;
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




};
