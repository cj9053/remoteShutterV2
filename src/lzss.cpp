#include "lzss.h"

// TODO: implement each function declared in lzss.h.
//
// Reminder of the contract (see lzss.h and the design doc):
//   - find_longest_match: scan backward from pos, within WINDOW_SIZE,
//     for the longest run that also occurs starting at pos. Cap length
//     at MAX_MATCH and at the bytes actually remaining in data.
//   - lzss_encode: walk the buffer; at each position try
//     find_longest_match. If it found >= MIN_MATCH, emit a match token
//     and advance by the match length; otherwise emit one literal token
//     and advance by 1.
//   - lzss_decode: replay tokens into a growing output buffer. Match
//     tokens must copy byte by byte from the *output*, not the input --
//     watch the overlapping-copy case (offset < length).

//return longest match from scanning backwards form pos. needs to be within WINDOW_SIZE
// longest match starts at pos, length is capped by MAX_MATCH
Match find_longest_match(const std::vector<uint8_t>& data, size_t pos){
    Match match_;
    match_.offset = pos;
    match_.length=0;

        //current max length
    size_t search_buffer_start=0;
    if(pos>WINDOW_SIZE){
        search_buffer_start = std::max(size_t(0),(pos-WINDOW_SIZE));
    }
    //iterate through candidate matches
    for(size_t i=search_buffer_start; i<pos; i++){
        size_t j=0;
        size_t curr_length=0;

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