#include <iostream>
#include <unordered_map>
#include <vector>

class ActiveIDTable {
private:
    std::unordered_map<int, bool> activeTable;

public:
    // 设置 ID 的活跃状态
    void setActive(int id, bool isActive) {
        activeTable[id] = isActive;
    }

    // 检查 ID 是否活跃
    bool isActive(int id) const {
        auto it = activeTable.find(id);
        if (it != activeTable.end()) {
            return it->second;
        }
        return false; // 默认非活跃，如果 ID 未找到
    }

    // 获取所有活跃的 ID
    std::vector<int> getAllActiveIDs() const {
        std::vector<int> activeIDs;
        for (const auto& pair : activeTable) {
            if (pair.second) { // 如果状态为 true，表示活跃
                activeIDs.push_back(pair.first);
            }
        }
        return activeIDs;
    }

    // 获取所有不活跃的 ID
    std::vector<int> getAllInactiveIDs() const {
        std::vector<int> inactiveIDs;
        for (const auto& pair : activeTable) {
            if (!pair.second) { // 如果状态为 false，表示不活跃
                inactiveIDs.push_back(pair.first);
            }
        }
        return inactiveIDs;
    }

    // 设置所有 ID 为不活跃
    void setAllInactive() {
        for (auto& pair : activeTable) {
            pair.second = false;
        }
    }

    // 打印所有 ID 及其活跃状态（用于调试）
    void printActiveStates() const {
        for (const auto& pair : activeTable) {
            std::cout << "ID: " << pair.first << " - Active: " << (pair.second ? "Yes" : "No") << std::endl;
        }
    }
};