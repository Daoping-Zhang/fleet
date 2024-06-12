#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class FleetGroupSendData {
public:
    int group_id;
    uint64_t latestIndex;
    uint64_t commitIndex;
    std::string entriesSerialized;

    // 默认构造函数
    FleetGroupSendData() : group_id(-1), latestIndex(0), commitIndex(0), entriesSerialized("") {}

    FleetGroupSendData(int groupId, uint64_t lIndex,  uint64_t cIndex, const std::string& serializedEntries)
        : group_id(groupId), latestIndex(lIndex), commitIndex(cIndex), entriesSerialized(serializedEntries) {}

    // 序列化整个对象到 JSON 字符串
    std::string serialize() const {
        json j = {
            {"group_id", group_id},
            {"latestIndex", latestIndex},
            {"commitIndex", commitIndex},
            {"entriesSerialized", entriesSerialized}
        };
        return j.dump();
    }

    // 从 JSON 字符串反序列化到 FleetGroupSendData 对象
    static FleetGroupSendData deserialize(const std::string& data) {
        json j = json::parse(data);
        int groupId = j["group_id"];
        uint64_t latestIndex = j["latestIndex"];
        uint64_t commitIndex = j["commitIndex"];
        std::string entries = j["entriesSerialized"];

        return FleetGroupSendData(groupId, latestIndex, commitIndex ,entries);
    }
};
