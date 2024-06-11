#include <unordered_map>
#include <string>
#include <vector>

struct DataItem {
    std::string key;
    std::string value;
    std::string method;
    uint64_t hashKey;
    int groupId;

    DataItem(std::string k, std::string v, std::string m, uint64_t h, int g)
        : key(std::move(k)), value(std::move(v)), method(std::move(m)), hashKey(h), groupId(g) {}
};

class FleetKVStore {
public:
    std::unordered_map<uint64_t, std::unordered_map<int, std::vector<DataItem>>> database;

    void addDataItem(const DataItem& item) {
        database[item.hashKey][item.groupId].emplace_back(item);
    }
};
