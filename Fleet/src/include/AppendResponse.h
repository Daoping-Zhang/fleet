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
    std::vector<char> serialize() const {
        std::vector<char> data;
        size_t vectorSize = follow_ids.size();
        data.insert(data.end(), reinterpret_cast<const char*>(&vectorSize), reinterpret_cast<const char*>(&vectorSize + 1));
        for (int id : follow_ids) {
            data.insert(data.end(), reinterpret_cast<const char*>(&id), reinterpret_cast<const char*>(&id + 1));
        }
        data.insert(data.end(), reinterpret_cast<const char*>(&log_index), reinterpret_cast<const char*>(&log_index + 1));
        data.insert(data.end(), reinterpret_cast<const char*>(&follower_commit), reinterpret_cast<const char*>(&follower_commit + 1));
        data.insert(data.end(), reinterpret_cast<const char*>(&term), reinterpret_cast<const char*>(&term + 1));
        data.insert(data.end(), reinterpret_cast<const char*>(&success), reinterpret_cast<const char*>(&success + 1));
        data.insert(data.end(), reinterpret_cast<const char*>(&identify), reinterpret_cast<const char*>(&identify + 1));
        return data;
    }

    // 反序列化方法
    static AppendResponse deserialize(const char* data, size_t size) {
        (void)size; // 若未使用，可防止编译警告
        AppendResponse response;
        size_t offset = 0;
        size_t vectorSize;
        std::memcpy(&vectorSize, data + offset, sizeof(vectorSize));
        offset += sizeof(vectorSize);
        response.follow_ids.resize(vectorSize);
        for (size_t i = 0; i < vectorSize; ++i) {
            std::memcpy(&response.follow_ids[i], data + offset, sizeof(int));
            offset += sizeof(int);
        }
        std::memcpy(&response.log_index, data + offset, sizeof(response.log_index));
        offset += sizeof(response.log_index);
        std::memcpy(&response.follower_commit, data + offset, sizeof(response.follower_commit));
        offset += sizeof(response.follower_commit);
        std::memcpy(&response.term, data + offset, sizeof(response.term));
        offset += sizeof(response.term);
        std::memcpy(&response.success, data + offset, sizeof(response.success));
        offset += sizeof(response.success);
        std::memcpy(&response.identify, data + offset, sizeof(response.identify));
        return response;
    }

    // Getter 和 Setter 方法
    const std::vector<int>& getFollowIds() const { return follow_ids; }
    void setFollowIds(const std::vector<int>& ids) { follow_ids = ids; }
    // 其他的 getter 和 setter 方法...
};
