
/* https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c */

#ifndef BASE64_H_
#define BASE64_H_


#include <stdint.h>


#define BASE64_OUTPUT_LEN(len)    (4u * (((len) + 2) / 3))


void b64_init(void);

void b64_encode(const uint8_t *data,
                    uint32_t input_length,
                    char *encoded_data);

/*
 *  *poutput_len input: max len of decoded_data,
 *              output: actual len of decoded_data
 * return 0 on success
 */
int b64_decode(const char *data, uint32_t input_length, uint8_t *decoded_data, uint32_t *poutput_len);

#endif /* BASE64_H_ */
