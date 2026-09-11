#include "huffman.h"
#include <map>
#include <queue>
void code_table_helper(const std::shared_ptr<HuffmanNode>& curr_node, std::map<uint8_t, std::pair<uint32_t, int>>& code_table, uint32_t code, int depth);

std::map<uint8_t, int> build_frequency_table(const std::vector<uint8_t>& data){
    std::map<uint8_t, int> frequency_table;
    for(size_t i=0; i<data.size(); i++){
        frequency_table[data[i]]++;
    }
    return frequency_table;
}
struct CompareNodes{
    bool operator()(const std::shared_ptr<HuffmanNode>& a, const std::shared_ptr<HuffmanNode>& b) const{
        return a-> frequency > b->frequency;
    }
};
std::shared_ptr<HuffmanNode> build_tree(const std::map<uint8_t, int>& freq_table){
    // Init priority queue
    std::priority_queue<std::shared_ptr<HuffmanNode>,std::vector<std::shared_ptr<HuffmanNode>>, CompareNodes > minPQ;

    // Populate pq with data from freq table
    for(const auto& [byte, frequency] : freq_table){
        minPQ.push(std::make_shared<HuffmanNode>(byte,frequency));
    }
    // Merge/Pop loop
    while(minPQ.size()>1){
        // store vals for left & right nodes then pop to create parent node
            std::shared_ptr<HuffmanNode> left = minPQ.top();
            minPQ.pop();
            std::shared_ptr<HuffmanNode> right = minPQ.top();
            minPQ.pop();
            // Create parent node with merged vals from left & right
            std::shared_ptr<HuffmanNode> parent = std::make_shared<HuffmanNode>(0, left->frequency + right->frequency, left, right);
            minPQ.push(parent);
    }
    return  minPQ.top();
}

// Need to build code table, need to store a section of data along with that sections information. 
// Map holds the information(code,depth) of a given byte
std::map<uint8_t, std::pair<uint32_t, int>> build_code_table(const std::shared_ptr<HuffmanNode>& root){
    std::map<uint8_t,std::pair<uint32_t,int>> code_table;
    uint32_t code=0;
    int depth=0;
    if(root->is_leaf()){
        depth=1;
        code_table.insert(std::make_pair(root->byte, std::make_pair(code,depth)));
        return code_table;
    }else{
        code_table_helper(root,code_table, code, depth);
    }
    return code_table;
}


void code_table_helper(const std::shared_ptr<HuffmanNode>& curr_node, std::map<uint8_t, std::pair<uint32_t, int>>& code_table, uint32_t code, int depth){
//recursive dfs since we're curious to find what the code is for a given value
    if(curr_node -> is_leaf()){
           code_table.insert(std::make_pair(curr_node->byte, std::make_pair(code,depth)));
           return;
    }
    code_table_helper(curr_node->left, code_table, code<<1 | 0, depth+1);
    code_table_helper(curr_node->right, code_table, code<<1 | 1, depth+1);
}






