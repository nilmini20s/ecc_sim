#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <algorithm> // to get std::for_each
#include <boost/crc.hpp> //for boost::crc_basic, boost::crc_optimal
#include <boost/random/mersenne_twister.hpp>

// 64-bit random values
typedef boost::mt19937_64   ENG; //Mersenne Twister
ENG eng; // define the generator once. 
int crc_type;

#define MAX_MX_LEN 64 // maximum message size is 512b or 64B
#define CRC16 0
#define CRC32 1
#define DRAM_1BIT 0
#define DRAM_1WORD 1
#define DRAM_1RANK 2

using namespace std;

void generateMessage(unsigned char message_buffer[], int m_len)
{
    // figure out the length of the message, only accept discrete lengths
    int num_iter;
    if (m_len == 8) // 64-bit message
        num_iter = 1;
    else if (m_len == 16) // 128-bit message
        num_iter = 2;
    else if (m_len == 32) // 256-bit message
        num_iter = 4;
    else if (m_len == 64) // 512-bit message
        num_iter = 8;
    else
        assert(0);

    // create the message
    int j = 0;
    for (int iter = 0; iter < num_iter; iter++)
    {
        // generate a random 64-bit value
        uint64_t random = eng();
        for (int i = sizeof(random)-1; i >= 0; i--, j++)
            message_buffer[j] = (random >> (8*i)) & 0xff;
    }
}

bool createFaultyBits(unsigned char message_buffer[], int m_len, int fail_type)
{
    int random = rand();
    bool definite_difference = true;

    //printf("mx+rx  = ");
    //for (int i = 0; i < m_len; i++)
    //{
    //    if (message_buffer[i] == 0) printf("00");
    //    else if (message_buffer[i] < 0x10) printf("0%x", message_buffer[i]);
    //    else printf("%x", message_buffer[i]);
    //}
    //printf("\n");

    if (fail_type == DRAM_1BIT) {
        int before = message_buffer[random % m_len];
        // bit failure, flip a single bit
        message_buffer[random % m_len] ^= (0x1 << random % 8);
        definite_difference = (before == message_buffer[random % m_len]) ? false:true;
    } else if (fail_type == DRAM_1WORD) {
        int before = message_buffer[random % m_len];
        // word failure, pick one byte, mess up that byte (by shifting up by 1)
        message_buffer[random % m_len] <<= 0x1;
        definite_difference = (before == message_buffer[random % m_len]) ? false:true;
    } else if (fail_type == DRAM_1RANK) {
        // rank failure, mess up all the bits (by shifting all the bits up by 1)
        for (int i = 0; i < m_len; i++)
            message_buffer[i] <<= 0x1;
    }

    //printf("mx+rx' = ");
    //for (int i = 0; i < m_len; i++)
    //{
    //    if (message_buffer[i] == 0) printf("00");
    //    else if (message_buffer[i] < 0x10) printf("0%x", message_buffer[i]);
    //    else printf("%x", message_buffer[i]);
    //}
    //printf("\n");

    return definite_difference;
}

int crc16(unsigned char mx[], int mx_len, int &checksum_sz)
{
    // generator polynomial for CRC-16-CCITT
    // x^16 + x^12 + x^5 + 1 (Bluetooth, etc)
    unsigned const gx = 0x1021;

    // Optimal version
    boost::crc_optimal<16, gx, 0xFFFF, 0, false, false> mycrc_optimal;
    mycrc_optimal = std::for_each(mx, mx+mx_len, mycrc_optimal);

    checksum_sz = sizeof(mycrc_optimal.checksum());
    return mycrc_optimal.checksum();
}

int crc32(unsigned char mx[], int mx_len, int &checksum_sz)
{
    // generator polynomial for CRC-16-CCITT
    // [32 26 23 22 16 12 11 10 8 7 5 4 2 1 0] (Ethernet, WiFi, POSIX chksum, etc)
    unsigned const gx = 0x04C11DB7;

    // Optimal version
    boost::crc_optimal<32, gx, 0xFFFFFFFF, 0, false, false> mycrc_optimal;
    mycrc_optimal = std::for_each(mx, mx+mx_len, mycrc_optimal);

    checksum_sz = sizeof(mycrc_optimal.checksum());
    return mycrc_optimal.checksum();
}

int crc_encode(unsigned char mx[], int mx_len, int &checksum_sz)
{
    if (crc_type == CRC32)
        return crc32(mx, mx_len, checksum_sz);
    else
        return crc16(mx, mx_len, checksum_sz);
}

int oneSim()
{
    // generate the message
    unsigned char mx[MAX_MX_LEN];
    size_t mx_len = 32; 
    generateMessage(mx, mx_len);

    // calculate the checksum on raw data
    int checksum_sz;
    int checksum = crc_encode(mx, mx_len, checksum_sz);
    //printf("the checksum 0x%x of size %d bytes\n", checksum, checksum_sz);

    // break the checksum into bytes and
    // attach the checksum to the end of the message
    for(int i = checksum_sz-1; i >= 0; i--)
       mx[mx_len++] = (checksum >> (8*i)) & 0xff; 
    //printf("mx_len %lu\n", mx_len);

    // inject faults
    bool faulty = createFaultyBits(mx, mx_len, DRAM_1RANK);

    // decode the codeword. 
    // if there is no error, the checksum should be 0!
    int decoded_checksum = crc_encode(mx, mx_len, checksum_sz);
    //printf("the decoded checksum 0x%x of size %d bytes\n", decoded_checksum, checksum_sz);

    if ((decoded_checksum != 0x0) || !faulty)
        return 1;

    return 0;
}

int main()
{
    long int total_sims = 1e6;
    int num_successes = 0; 
    int num_fails = 0;
    crc_type = CRC16;

    // initialize random seed
    srand (time(NULL));

    // run Monte Carlo simulations
    for (long int i = 0; i < total_sims; i++)
    {
        if (oneSim() == 1)
            num_successes++;
        else
            num_fails++;
    }

    printf("Total simulations: %ld\n", total_sims);
    printf("Num successfully detected faults: %d\n", num_successes);
    printf("Num faults failed to detect: %d\n", num_fails);

    return 0;
}
