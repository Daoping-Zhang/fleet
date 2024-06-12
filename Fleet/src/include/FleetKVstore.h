#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

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
    // 使用双层unordered_map，第一层按组ID，第二层按hashKey分类
    std::unordered_map<int, std::unordered_map<uint64_t, std::vector<DataItem>>> database;

    // 添加数据项
    void addDataItem(const DataItem& item) {
        database[item.groupId][item.hashKey].emplace_back(item);
    }

    // 设置数据项
    bool setDataItem(const DataItem& item) {
        // 直接添加或更新数据项
        database[item.groupId][item.hashKey].push_back(item);
        return true;  // 假设操作总是成功
    }

    // 删除特定键的数据项
    bool deleteDataItem(const std::string& key, uint64_t hashKey, int groupId) {
        // 查找并删除数据项
        auto& items = database[groupId][hashKey];
        auto originalSize = items.size();
        items.erase(std::remove_if(items.begin(), items.end(), [&key](const DataItem& item) {
            return item.key == key;
        }), items.end());
        return items.size() != originalSize;  // 如果大小改变了，则说明至少删除了一个元素
    }

    // 获取数据项的值
    std::string getValue(const std::string& key, uint64_t hashKey, int groupId) {
        const auto& group = database.find(groupId);
        if (group != database.end()) {
            const auto& items = group->second.find(hashKey);
            if (items != group->second.end()) {
                for (const auto& item : items->second) {
                    if (item.key == key) {
                        return item.value;
                    }
                }
            }
        }
        return "";  // 如果找不到，返回空字符串或可以抛出异常/错误码
    }  // 缺少这个闭合大括号

    // 序列化指定组的状态
    std::string serializeGroup(int groupId) const {
        json j;
        auto it = database.find(groupId);
        if (it != database.end()) {
            for (const auto& pair : it->second) {
                for (const auto& item : pair.second) {
                    json itemJson = {
                        {"groupId", item.groupId},
                        {"hashKey", item.hashKey},
                        {"key", item.key},
                        {"value", item.value},
                        {"method", item.method}
                    };
                    j.push_back(itemJson);
                }
            }
        }
        return j.dump();  // 将 json 对象转换为字符串
    }

    // 从字符串反序列化指定组的状态
    void deserializeGroup(int groupId, const std::string& serializedData) {
        auto items = json::parse(serializedData);
        for (const auto& item : items) {
            int readGroupId = item["groupId"];
            uint64_t hashKey = item["hashKey"];
            std::string key = item["key"];
            std::string value = item["value"];
            std::string method = item["method"];

            if (readGroupId == groupId) {
                DataItem newItem(key, value, method, hashKey, groupId);
                addDataItem(newItem);
            }
        }
    }
};
