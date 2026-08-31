#include "huffman.h"
#include <map>
#include <queue>

std::map<uint8_t, int> build_frequency_table(const std::vector<uint8_t>& data){
    std::map<uint8_t, int> frequency_table;
    for(size_t i=0; i<data.size(); i++){
        frequency_table[data[i]]++;
    }
    return frequency_table;
}

struct CompareNodes{
    bool operator()(const std::shared_ptr<HuffmanNode>&a, const std::shared_ptr<HuffmanNode>& b) const{
        return a->frequency > b->frequency;
    };
}