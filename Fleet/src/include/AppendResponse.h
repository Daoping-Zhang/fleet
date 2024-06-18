#include <vector>
#include <cstring> // For std::memcpy

class AppendResponse {
public:
    std::vector<int> follow_ids;
    int log_index;
    int follower_commit;
    int term;
    bool success;
    bool identify;

public:
    // 默认构造函数
    AppendResponse() : log_index(0), follower_commit(0), term(0), success(false), identify(false) {}

    // 带参数的构造函数
    AppendResponse(const std::vector<int>& followIds, int logIndex, int followerCommit, int termValue, bool isSuccess, bool doesIdentify)
        : follow_ids(followIds), log_index(logIndex), follower_commit(followerCommit), term(termValue), success(isSuccess), identify(doesIdentify) {}

    // 序列化方法
    std::string serialize() const {
        json j;
        j["follow_ids"] = follow_ids;
        j["log_index"] = log_index;
        j["follower_commit"] = follower_commit;
        j["term"] = term;
        j["success"] = success;
        j["identify"] = identify;
        return j.dump();
    }

    // 反序列化方法
    static AppendResponse deserialize(const std::string& data) {
        json j = json::parse(data);
        AppendResponse response;
        response.follow_ids = j["follow_ids"].get<std::vector<int>>();
        response.log_index = j["log_index"].get<int>();
        response.follower_commit = j["follower_commit"].get<int>();
        response.term = j["term"].get<int>();
        response.success = j["success"].get<bool>();
        response.identify = j["identify"].get<bool>();
        return response;
    }

    // Getter 和 Setter 方法
    const std::vector<int>& getFollowIds() const { return follow_ids; }
    void setFollowIds(const std::vector<int>& ids) { follow_ids = ids; }
    // 其他的 getter 和 setter 方法...
};
