#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <assert.h>
#include <math.h>
#include <algorithm> // to get std::for_each
#include <boost/random/mersenne_twister.hpp>

#include <ezpwd/rs>
#include <ezpwd/output>

// 64-bit random values
typedef boost::mt19937_64   ENG; //Mersenne Twister
ENG eng; // define the generator once. 

#define MAX_THREADS 32 // max parallel threads
#define MAX_MX_LEN 72 // maximum message size is 512b or 64B + parity symbols
#define DRAM_1BIT 0
#define DRAM_1WORD 1
#define DRAM_1RANK 2
#define DEBUG 0

using namespace std;

void printMessage_charArray(unsigned char message_buffer[])
{
    printf("mx = 0x");
    for (int i = 0; i < MAX_MX_LEN; i++)
    {
        if (message_buffer[i] == 0) printf("00");
        else if (message_buffer[i] < 0x10) printf("0%x", message_buffer[i]);
        else printf("%x", message_buffer[i]);
    }
    printf("\n");
}

void printMessage_vector(std::vector<unsigned char> &message_buffer)
{
    int len = (int)message_buffer.size();
    printf("mx = 0x");
    for (int i = 0; i < len; i++)
    {
        if (message_buffer[i] == 0) printf("00");
        else if (message_buffer[i] < 0x10) printf("0%x", message_buffer[i]);
        else printf("%x", message_buffer[i]);
    }
    printf("\n");
}

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
        unsigned char before[m_len];
        memcpy(before, message_buffer, m_len);
        // rank failure, mess up all the bytes (i.e. symbols)
        for (int i = 0; i < m_len; i++)
            message_buffer[(random+i) % m_len] <<= 0x1;
        definite_difference = memcmp(before, message_buffer, m_len) ? true:false;
    }

    return definite_difference;
}

int oneSim()
{
    // generate the message
    unsigned char orig[MAX_MX_LEN] = {0};
    unsigned char mx[MAX_MX_LEN];
    size_t mx_len = 64; 
    generateMessage(orig, mx_len);
    memcpy(mx, orig, MAX_MX_LEN);

    // convert the unsigned char* into a vector (b/c RS doesn't accpet chars)
    std::vector<unsigned char> mx_vec(mx, mx+mx_len);
    if (DEBUG) { printf("Vector    : "); printMessage_vector(mx_vec); }

    // initialize the RS algorithm 255 symbol codeword, max 250 data (i.e. 5 parity symbols)
    ezpwd::RS<255,250> rs;

    // encode the message
    rs.encode(mx_vec);

    // convert the vector back into unsigned char*
    std::copy(mx_vec.begin(), mx_vec.end(), mx);
    mx_len = mx_vec.size();
    if (DEBUG) { printf("Encoded   : "); printMessage_charArray(mx); }

    // inject faults
    bool faulty = createFaultyBits(mx, mx_len, DRAM_1RANK);
    if (DEBUG) { printf("Corrupted : "); printMessage_charArray(mx); }

    // convert the unsigned char* into a vector
    mx_vec.assign(mx, mx+mx_len);
    if (DEBUG) { printf("Vector    : "); printMessage_vector(mx_vec); }

    // decode the codeword.
    int count = rs.decode(mx_vec); // Correct any symbols possible
    if (DEBUG) { printf("Restored  : "); printMessage_vector(mx_vec); }

    // discard the parity symbols
    mx_vec.resize(mx_vec.size() - rs.nroots());

    // convert the vector back into unsigned char*
    std::copy(mx_vec.begin(), mx_vec.end(), mx);
    mx_len = mx_vec.size();

    // compare the restored value with the original value
    bool isequal = memcmp(orig, mx, mx_len);
    if (DEBUG) { printf("faulty %d, count %2d, isequal %d\n", faulty, count, !isequal); }

    // if count == -1, an error was detected, but not corrected
    // if count > 0, that many symbols were corrected
    // if count == 0, no symbols were found to be corrupted
    if ((count < 0) || !faulty)
        return 1;
    if ((count > 0) && !isequal)
        return 1;

    return 0;
}

int main()
{
    long int total_sims = 1e7;
    int num_successes[MAX_THREADS] = {0}; 
    int num_fails[MAX_THREADS] = {0};

    // initialize random seed
    srand (time(NULL));

    // run Monte Carlo simulations
#pragma omp parallel
{
    int id=omp_get_thread_num();
#pragma omp for
    for (long int i = 0; i < total_sims; i++)
    {
        //printf("sim: %ld\n", i);
        if (oneSim() == 1)
            num_successes[id]++;
        else
            num_fails[id]++;
    } 
}

    int total_successes=0, total_fails=0;
    for (int i=0; i < MAX_THREADS; i++) total_successes+=num_successes[i];
    for (int i=0; i < MAX_THREADS; i++) total_fails+=num_fails[i];

    printf("Total simulations: %ld\n", total_sims);
    printf("Num successfully detected faults: %d\n", total_successes);
    printf("Num faults failed to detect: %d\n", total_fails);

    return 0;
}
