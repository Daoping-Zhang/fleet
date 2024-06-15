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
    std::unordered_map<int, std::unordered_map<uint64_t, int>> vote_count; // 每个组中每个日志条目的投票计数
    std::unordered_map<int, std::unordered_map<uint64_t, std::unordered_map<int, uint64_t>>> latest_indexes_by_node; // 记录每个节点的最新索引

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

    std::vector<LogEntry> getUncommittedEntries(int groupId) {
        std::vector<LogEntry> uncommittedEntries;
        uint64_t commitIndex = committed_index[groupId];
        uint64_t latestIndex = latest_index[groupId];

        for (uint64_t i = commitIndex; i < latestIndex; ++i) {
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

    // 增加投票数
    void addVote(int groupId, uint64_t index, int nodeId) {

        if(latest_indexes_by_node[groupId][index][nodeId] < index)
        {
            vote_count[groupId][index]++;
            latest_indexes_by_node[groupId][index][nodeId] = index;
            printf("成功投票，当前票数%d\n", vote_count[groupId][index]);
        }

        else
        {
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
};
