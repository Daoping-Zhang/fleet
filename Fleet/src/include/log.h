#ifndef LOG_H
#define LOG_H

#include <vector>
#include<set>
#include <fstream>

class Log {
public:
    void append(const std::string& content, int term) {
        entries_.push_back(content);
        latest_index_++;
        terms_.push_back(term);
        nums_.push_back({});
        // 将新条目写入文件中
        std::ofstream outfile(file_name_, std::ios_base::app);
        if (outfile.is_open()) {
            outfile <<"-------index: "<<latest_index_<<"------term:  "<<term<<"--------------------"<< std::endl;
            outfile <<content<< std::endl;
            outfile <<"-------------------------------------------------"<< std::endl;
            outfile <<"                  | |                  "<< std::endl;
            outfile <<"                  | |                  "<< std::endl;
            outfile <<"                  \\|/                  "<< std::endl;
            outfile.close();
        }
        latest_term_=term;
    }
    void erase(int start,int end)
    {
        entries_.erase(entries_.begin()+start-1,entries_.begin()+end);
        latest_index_-=(end-start+1);
        latest_term_=term_at(latest_index_);
        nums_.erase(nums_.begin()+start-1,nums_.begin()+end);
        terms_.erase(terms_.begin()+start-1,terms_.begin()+end);
    }
    bool has_entry_at(int index, int term) const {
        return index > 0 && index <= latest_index_ && index <= committed_index_ && term == term_at(index);
    }
    const std::string& entry_at(int index) const {
        return entries_[index-1];
    }
    int term_at(int index) const {
        if(index==0)return 0;
        return terms_[index-1];
    }
    void commit(int index) {
        committed_index_ = index;
    }
    int latest_index() const {
      
        return latest_index_;
    }
    int committed_index() const {
        return committed_index_;
    }
    int latest_term() const {
        return latest_term_;
    }
    void add_num(int index,int id)
    {
        nums_[index].insert(id);
    }
    int get_num(int index) const{
        return nums_[index-1].size();
    }
    std::string file_name_;
private:
    vector<string> entries_;
    vector<int> terms_;
    int latest_index_ = 0;
    int latest_term_ = 0;
    int committed_index_ = 0;
    vector<set<int> > nums_; // 新增一个支持者数量的 vector
};

/*Log log;
log.append("entry 1");
log.append("entry 2");
log.append("entry 3");
std::cout << "latest index: " << log.latest_index() << std::endl;
log.commit(2);
std::cout << "committed index: " << log.committed_index() << std::endl;
if (log.has_entry_at(2)) {
    std::cout << "entry at index 2: " << log.entry_at(2) << std::endl;
}*/

#endif
