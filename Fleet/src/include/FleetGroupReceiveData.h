#ifndef FLEET_GROUP_RECEIVE_DATA_H
#define FLEET_GROUP_RECEIVE_DATA_H

#include <nlohmann/json.hpp>
#include <vector>
using json = nlohmann::json;

class FleetGroupReceiveData {
public:
    int vote_num;
    bool success;
    uint64_t latestIndex;
    uint64_t commitIndex;
    std::vector<int> ids;
    int group_id;  // 新增 group_id

    // 默认构造函数
    FleetGroupReceiveData() : vote_num(0), success(false), latestIndex(0), commitIndex(0), group_id(0) {}

    // 带参数的构造函数
    FleetGroupReceiveData(int v, bool s, uint64_t lIndex, uint64_t cIndex, const std::vector<int>& idsVec, int g_id) 
        : vote_num(v), success(s), latestIndex(lIndex), commitIndex(cIndex), ids(idsVec), group_id(g_id) {}

    // 序列化方法
    std::string serialize() const {
        json j;
        j["vote_num"] = vote_num;
        j["success"] = success;
        j["latestIndex"] = latestIndex;
        j["commitIndex"] = commitIndex;
        j["ids"] = ids;
        j["group_id"] = group_id;
        return j.dump();
    }

    // 反序列化方法
    static FleetGroupReceiveData deserialize(const std::string& data) {
        json j = json::parse(data);
        return FleetGroupReceiveData(
            j["vote_num"], 
            j["success"], 
            j["latestIndex"], 
            j["commitIndex"], 
            j["ids"].get<std::vector<int>>(),
            j["group_id"]
        );
    }
};

#endif // FLEET_GROUP_RECEIVE_DATA_H
