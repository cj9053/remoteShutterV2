#include "bitstream.h"
#include <cmath>

// TODO: implement BitWriter and BitReader here.
//
// Reminder of the contract (see bitstream.h and the design doc):
//   - MSB-first packing, consistently in both writer and reader
//   - finish() must flush a partial trailing byte, zero-padded
//   - BitReader needs an exact bit_length so it doesn't decode padding
//     as real data

//need to append one bit to the stream
void BitWriter::write_bit(int bit){
    if(bit!=0 && bit !=1){
        return;
    }
    
    int position = bytePosition -bits_in_current;
    //shift 1 into the right position
    if(bit==1){
        curr_byte_ |= (1<<position);    
    }
    bits_in_current++;
    if(bits_in_current==8){
        bytes_.push_back(curr_byte_);
        curr_byte_=0;
        bits_in_current=0;
    }
}

void BitWriter::write_bits(uint32_t value, int width){
    for (int i=width-1; i>= 0; i--){
        int current_bit = (value >> i) & 1;
        write_bit(current_bit);
    }
}

std::vector<uint8_t> BitWriter::finish(){
    if(bits_in_current>0){
        bytes_.push_back(curr_byte_);
    }
    return bytes_;
}

BitReader::BitReader(const std::vector<uint8_t>& data, size_t bit_length){
    byte_buffer = data;
    bit_length_ = bit_length;
}

int BitReader::read_bit(){
    uint8_t current_byte = byte_buffer[byteIndex];
    int position = bytePosition - bitIndex;
    bitIndex++;
    if(bitIndex==8 ){
        bitIndex=0;
        byteIndex++;
    }
    return (current_byte >> position) & 1;

}

 bool BitReader::at_end() const {
    if(byteIndex*8+bitIndex==bit_length_){
        return true;
    }
    return false;
}

uint32_t BitReader::read_bits(int width){
    uint32_t assembledBits =0;
    for(int i=width-1; i>=0; i--){
       assembledBits = (assembledBits<<1) | read_bit();
    }
    return assembledBits;
}