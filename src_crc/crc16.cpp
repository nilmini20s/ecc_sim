/*
 * export BOOSTINC=/home/nilmini/programs/boost_1_53_0/
 * export BOOSTLIB=/home/nilmini/programs/boost_1_53_0/stage/lib
 * export LD_LIBRARY_PATH=/home/nilmini/programs/boost_1_53_0/stage/lib/
 *
 * Compile: g++ -c -Wall -std=c++0x -O2 -g -I/home/nilmini/programs/boost_1_53_0/ crc32.cpp -o crc32.o
 * Link: g++ crc32.o -L/home/nilmini/programs/boost_1_53_0/stage/lib -lboost_program_options -o crc32.exe
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <algorithm> // to get std::for_each
#include <boost/crc.hpp> //for boost::crc_basic, boost::crc_optimal

using namespace std;

int main()
{
    // the message
    unsigned char const mx[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const mx_len = sizeof (mx) / sizeof (mx[0]);

    // generator polynomial for CRC-16-CCITT
    // x^16 + x^12 + x^5 + 1 (Bluetooth, etc)
    unsigned const gx = 0x1021;

    // Basic version
    boost::crc_basic<16> mycrc_basic(gx, 0xFFFF, 0, false, false);
    mycrc_basic.process_bytes(mx, mx_len);
    printf("the checksum 0x%x\n", mycrc_basic.checksum());

    // Optimal version
    boost::crc_optimal<16, gx, 0xFFFF, 0, false, false> mycrc_optimal;
    mycrc_optimal = std::for_each(mx, mx+mx_len, mycrc_optimal);
    printf("the checksum 0x%x\n", mycrc_optimal.checksum());

    unsigned char const encoded_mx[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x29, 0xb1 };
    std::size_t const encoded_mx_len = sizeof (encoded_mx) / sizeof (encoded_mx[0]);
    boost::crc_optimal<16, gx, 0xFFFF, 0, false, false> decoder;
    decoder = std::for_each(encoded_mx, encoded_mx+encoded_mx_len, decoder);
    printf("the decoded checksum 0x%x\n", decoder.checksum());

    return 0;
}
