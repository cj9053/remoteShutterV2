#include "lzss.h"

//   - lzss_decode: replay tokens into a growing output buffer. Match
//     tokens must copy byte by byte from the *output*, not the input --
//     watch the overlapping-copy case (offset < length).

//return longest match from scanning backwards form pos. needs to be within WINDOW_SIZE
// longest match starts at pos, length is capped by MAX_MATCH
Match find_longest_match(const std::vector<uint8_t>& data, size_t pos){
    Match match_;
    //off set determines how far the search window 
    // goes from this point backwards
    match_.offset = pos;
    match_.length=0;

    //current max length

    size_t search_buffer_start=0; 
    //start from idx 0 if window hasnt exceeded max match 
    if(pos>WINDOW_SIZE){
        search_buffer_start = std::max(size_t(0),(pos-WINDOW_SIZE));
    }
    //iterate through candidate matches
    //sliding window
    //outer loop acts as the search buffer
    for(size_t i=search_buffer_start; i<pos; i++){
        size_t j=0; 
        size_t curr_length=0;
        //inner loop looks ahead to see if there are any candidate matches, if true, start checking 
        //whether element @ index j+i == element @ pos+j 
        while(pos+j<data.size()&&j<MAX_MATCH){
            if(data[j+i]==data[pos+j]){
                curr_length++;
                if(curr_length>match_.length){
                    match_.length=curr_length;
                    match_.offset=pos-i;
                }
            }else{
                break;
            }
            j++;
        }
    }
    if(match_.length>=MIN_MATCH){
        return match_;
    }
    match_.length=0;
    return match_;
    
    
}

std::vector<Token> lzss_encode(const std::vector<uint8_t>& data){
    
    std::vector<Token> tokenized;
    
    for(size_t i=0; i<data.size();){
        Match result = find_longest_match(data,i);
        Token token_;

        if(result.length==0){
            token_.is_literal=true;
            token_.literal=data[i];
            tokenized.push_back(token_);
            i++;
        }else{
            token_.offset=result.offset;
            token_.length=result.length;
            tokenized.push_back(token_);
            i+=result.length;
        }
    }
    return tokenized;
}

std::vector<uint8_t> lzss_decode(const std::vector<Token>& tokens){
    std::vector<uint8_t> decodedBytes;
    for(size_t i=0; i<tokens.size(); i++){
        size_t arrSize=decodedBytes.size();
        if(tokens[i].is_literal){
            decodedBytes.push_back(tokens[i].literal);
        }else{
            size_t readIdx=arrSize-tokens[i].offset;

            size_t copiedByteCount=0;
            while(copiedByteCount<tokens[i].length){
                decodedBytes.push_back(decodedBytes[readIdx]);
                readIdx++;
                copiedByteCount++;
            }
        }
    }
    return decodedBytes;

}