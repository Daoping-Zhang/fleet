#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class InitializeData {
public:
    int m_id;
    int group_id;
    uint64_t latestIndex;
    uint64_t commitIndex;
    std::string serializedData;

    // 构造函数
    InitializeData(): m_id(0), group_id(0) , latestIndex(0), commitIndex(0), serializedData("") {}
    InitializeData(int id, int gid, uint64_t latest, uint64_t commit, const std::string& data)
        : m_id(id),group_id(gid), latestIndex(latest), commitIndex(commit), serializedData(data) {}

    // 序列化函数
    std::string serialize() const {
        json j;
        j["m_id"] = m_id;
        j["group_id"] = group_id;
        j["latestIndex"] = latestIndex;
        j["commitIndex"] = commitIndex;
        j["serializedData"] = serializedData;
        return j.dump();  // 将 json 对象转换为字符串
    }

    // 反序列化函数
    static InitializeData deserialize(const std::string& data) {
        auto j = json::parse(data);  // 解析 json 字符串
        int id = j["m_id"];
        int gid = j["group_id"];
        uint64_t latest = j["latestIndex"];
        uint64_t commit = j["commitIndex"];
        std::string serialized = j["serializedData"];
        return InitializeData(id,gid, latest, commit, serialized);
    }
};
