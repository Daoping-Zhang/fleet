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
    json response = json::array();




    // 设置数据项
    bool setDataItem(const DataItem& item) {
        auto& items = database[item.groupId][item.hashKey];
        for (auto& existingItem : items) {
            if (existingItem.key == item.key) {
                existingItem.value = item.value; // 更新现有的值
                return true; // 成功更新
            }
        }
        // 如果没有找到，就添加新的条目
        items.emplace_back(item);
        return true; // 假设操作总是成功
    }


    // 删除特定键的数据项
    int deleteDataItems(const std::string& keys, uint64_t hashKey, int groupId) {
        int deletedCount = 0;
        auto& items = database[groupId][hashKey];
        auto originalSize = items.size();
        std::vector<std::string> keyList = split(keys, ' ');  // 在方法内部进行拆分

        items.erase(std::remove_if(items.begin(), items.end(), 
            [&keyList](const DataItem& item) {
                return std::find(keyList.begin(), keyList.end(), item.key) != keyList.end();
            }), items.end());

        deletedCount = originalSize - items.size();  // 计算删除了多少条目
    
        return deletedCount;
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

    json handleRequest(const json& request) {
        std::string keys = request["key"];
        std::string method = request["method"];
        uint64_t hashKey = request["hashKey"];
        int groupId = request["groupId"];

        json response;

        if (method == "SET") {
            std::string value = request.value("value", "");
            DataItem newItem(keys, value, method, hashKey, groupId);
            bool success = setDataItem(newItem);
            response = {
                {"code", success ? 1 : 0},
                {"value", ""}
            };
        } else if (method == "GET") {
            std::string foundValue = getValue(keys, hashKey, groupId);
            response = {
                {"code", !foundValue.empty() ? 1 : 0},
                {"value", foundValue}
            };
        } else if (method == "DEL") {
            int countDeleted = deleteDataItems(keys, hashKey, groupId);  // 直接传入字符串
            response = {
                {"code", countDeleted},
                {"value", ""}
            };
        } else {
            response = {
                {"code", -1},
                {"value", ""}
            };
        }

        return response;
    }

    std::vector<std::string> split(const std::string& str, char delim) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delim)) {
            tokens.push_back(token);
        }
        return tokens;
    }


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
                setDataItem(newItem);
            }
        }
    }
};
