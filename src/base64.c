/* https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c */

#include <base64.h>
#include <stdint.h>
#include <stdlib.h>

//#define DEBUG 1
#include <debug.h>


static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static int mod_table[] = {0, 2, 1};

uint8_t decoding_table[256];

void b64_init()
{
    int i;
    for (i = 0; i < 64; i++)
        decoding_table[(unsigned char) encoding_table[i]] = i;
}

void b64_encode(const unsigned char *data, uint32_t input_length, char *encoded_data)
{
    uint32_t i, j;
    size_t output_len = 4 * ((input_length + 2) / 3);

    for (i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; (int)i < mod_table[input_length % 3]; i++)
        encoded_data[output_len - 1 - i] = '=';
    encoded_data[output_len] = '\0';
}

/* *output_len is both input max len and output actual len; return 0 on success */
int b64_decode(const char *data, uint32_t input_length, uint8_t *decoded_data, uint32_t *poutput_len)
{
    uint32_t i, j;
    uint32_t len;

    if (input_length == 0 || input_length % 4 != 0) {
        return 1;
    }

    len = input_length / 4 * 3;
    if (data[input_length - 1] == '=') len--;
    if (data[input_length - 2] == '=') len--;

    if (*poutput_len < len) {
        return 1;
    }
    *poutput_len = len;

    for (i = 0, j = 0; i < input_length;) {
        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[(int)data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[(int)data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[(int)data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[(int)data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < len) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < len) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < len) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }
    return 0;
}
