// NodeManage.cc
#include "NodeManage.h"
#include <cstring>
#include <set>
#include <algorithm> 


void NodeManage::addMapping(int id, const sockaddr_in& sockaddr) {
    m_ids.push_back(id);
    id_to_sockaddr[id] = sockaddr;
    sockaddr_to_ids[sockaddr].push_back(id);
}

sockaddr_in NodeManage::getSockaddrById(int id) {
    return id_to_sockaddr[id];
}

std::vector<int> NodeManage::getIdsBySockaddr(const sockaddr_in& sockaddr) {
    return sockaddr_to_ids[sockaddr];
}


void NodeManage::createGroup(int groupId, int leaderId) {
    groups[groupId].leaderId = leaderId;
    groups[groupId].memberIds.push_back(leaderId);
}

void NodeManage::addToGroup(int groupId, int id) {
    groups[groupId].memberIds.push_back(id);
}

std::vector<int> NodeManage::getIdsByGroup(int groupId) {
    return groups[groupId].memberIds;
}

int NodeManage::getLeaderIdByGroup(int groupId) {
    return groups[groupId].leaderId;
}

std::vector<int> NodeManage::getGroupIndicesWithMemberId(int memberId) {
    std::vector<int> indices;
    for (const auto& group : groups) {
        if (std::find(group.second.memberIds.begin(), group.second.memberIds.end(), memberId) != group.second.memberIds.end()) {
            indices.push_back(group.first);
        }
    }
    return indices;
}

std::vector<char> NodeManage::serialize(bool includeGroups) const {
        std::vector<char> data;

        // Serialize id_to_sockaddr
        size_t map_size = id_to_sockaddr.size();
        data.insert(data.end(), reinterpret_cast<const char*>(&map_size), reinterpret_cast<const char*>(&map_size + 1));
        for (const auto& pair : id_to_sockaddr) {
            int id = pair.first;
            const sockaddr_in& addr = pair.second;
            data.insert(data.end(), reinterpret_cast<const char*>(&id), reinterpret_cast<const char*>(&id + 1));
            data.insert(data.end(), reinterpret_cast<const char*>(&addr), reinterpret_cast<const char*>(&addr + 1));
        }

        // Serialize sockaddr_to_ids
        map_size = sockaddr_to_ids.size();
        data.insert(data.end(), reinterpret_cast<const char*>(&map_size), reinterpret_cast<const char*>(&map_size + 1));
        for (const auto& pair : sockaddr_to_ids) {
            const sockaddr_in& addr = pair.first;
            data.insert(data.end(), reinterpret_cast<const char*>(&addr), reinterpret_cast<const char*>(&addr + 1));

            const std::vector<int>& ids = pair.second;
            size_t vector_size = ids.size();
            data.insert(data.end(), reinterpret_cast<const char*>(&vector_size), reinterpret_cast<const char*>(&vector_size + 1));
            for (int id : ids) {
                data.insert(data.end(), reinterpret_cast<const char*>(&id), reinterpret_cast<const char*>(&id + 1));
            }
        }

        // Conditionally serialize groups
        if (includeGroups) {
            map_size = groups.size();
            data.insert(data.end(), reinterpret_cast<const char*>(&map_size), reinterpret_cast<const char*>(&map_size + 1));
            for (const auto& pair : groups) {
                int groupId = pair.first;
                const GroupInfo& info = pair.second;
                data.insert(data.end(), reinterpret_cast<const char*>(&groupId), reinterpret_cast<const char*>(&groupId + 1));

                int leaderId = info.leaderId;
                data.insert(data.end(), reinterpret_cast<const char*>(&leaderId), reinterpret_cast<const char*>(&leaderId + 1));

                const std::vector<int>& memberIds = info.memberIds;
                size_t  vector_size = memberIds.size();
                data.insert(data.end(), reinterpret_cast<const char*>(&vector_size), reinterpret_cast<const char*>(&vector_size + 1));
                for (int memberId : memberIds) {
                    data.insert(data.end(), reinterpret_cast<const char*>(&memberId), reinterpret_cast<const char*>(&memberId + 1));
                }
            }
        }

        return data;
}


NodeManage NodeManage::deserialize(const std::vector<char>& data) {
    NodeManage nodeManage;
        size_t i = 0;

        // Deserialize id_to_sockaddr
        size_t map_size;
        if (data.size() >= i + sizeof(size_t)) {
            std::memcpy(&map_size, &data[i], sizeof(map_size));
            i += sizeof(map_size);
        }

        for (size_t j = 0; j < map_size && i < data.size(); ++j) {
            int id;
            sockaddr_in addr;
            if (data.size() >= i + sizeof(int)) {
                std::memcpy(&id, &data[i], sizeof(id));
                i += sizeof(id);
            }
            if (data.size() >= i + sizeof(sockaddr_in)) {
                std::memcpy(&addr, &data[i], sizeof(addr));
                i += sizeof(addr);
            }
            nodeManage.id_to_sockaddr[id] = addr;
        }

        // Deserialize sockaddr_to_ids
        if (data.size() >= i + sizeof(size_t)) {
            std::memcpy(&map_size, &data[i], sizeof(map_size));
            i += sizeof(map_size);
        }

        for (size_t j = 0; j < map_size && i < data.size(); ++j) {
            sockaddr_in addr;
            if (data.size() >= i + sizeof(sockaddr_in)) {
                std::memcpy(&addr, &data[i], sizeof(addr));
                i += sizeof(sockaddr_in);
            }

            size_t vector_size;
            if (data.size() >= i + sizeof(size_t)) {
                std::memcpy(&vector_size, &data[i], sizeof(vector_size));
                i += sizeof(vector_size);
            }

            std::vector<int> ids;
            ids.reserve(vector_size);
            for (size_t k = 0; k < vector_size && i < data.size(); ++k) {
                int id;
                if (data.size() >= i + sizeof(int)) {
                    std::memcpy(&id, &data[i], sizeof(id));
                    i += sizeof(int);
                }
                ids.push_back(id);
            }
            nodeManage.sockaddr_to_ids[addr] = ids;
        }

        // Optional: Deserialize groups if data is still available
        if (i + sizeof(size_t) <= data.size()) {
            std::memcpy(&map_size, &data[i], sizeof(map_size));
            i += sizeof(map_size);

            for (size_t j = 0; j < map_size && i < data.size(); ++j) {
                int groupId;
                if (data.size() >= i + sizeof(int)) {
                    std::memcpy(&groupId, &data[i], sizeof(groupId));
                    i += sizeof(int);
                }

                int leaderId;
                if (data.size() >= i + sizeof(int)) {
                    std::memcpy(&leaderId, &data[i], sizeof(leaderId));
                    i += sizeof(int);
                }

                size_t member_size;
                if (data.size() >= i + sizeof(size_t)) {
                    std::memcpy(&member_size, &data[i], sizeof(member_size));
                    i += sizeof(member_size);
                }

                std::vector<int> members;
                members.reserve(member_size);
                for (size_t k = 0; k < member_size && i < data.size(); ++k) {
                    int memberId;
                    if (data.size() >= i + sizeof(int)) {
                        std::memcpy(&memberId, &data[i], sizeof(memberId));
                        i += sizeof(int);
                    }
                    members.push_back(memberId);
                }
                GroupInfo info{leaderId, members};
                nodeManage.groups[groupId] = info;
            }
        }

        return nodeManage;
}

void NodeManage::createGroupsFromIds(const std::vector<int>& m_ids) {
    if (m_ids.empty()) return;  // 如果列表为空，直接返回

    int num_ids = m_ids.size();
    for (int i = 0; i < num_ids; ++i) {
        int currentId = m_ids[i];
        int groupId = currentId;  // 使用当前 ID 作为组 ID
        createGroup(groupId, currentId);

        // 添加前一个 ID，循环到最后一个如果是第一个
        int prevId = (i == 0) ? m_ids[num_ids - 1] : m_ids[i - 1];
        addToGroup(groupId, prevId);

        // 添加后一个 ID，循环到第一个如果是最后一个
        int nextId = (i == num_ids - 1) ? m_ids[0] : m_ids[i + 1];
        addToGroup(groupId, nextId);
    }
}

