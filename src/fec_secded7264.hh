/*
 * Copyright (c) 2017 Sultan Qasim Khan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef FEC7264_H
#define FEC7264_H

/* fec_secded7264_encode
 *
 * Inputs:
 * dec_msg_len  size of original data
 * msg_dec      original message data
 *
 * Outputs:
 * msg_enc      encoded message data gets put here
 *
 * Returns:
 * Size of encoded message in bytes
 */
unsigned int fec_secded7264_encode(unsigned int dec_msg_len,
                                   const unsigned char *msg_dec,
                                   unsigned char *msg_enc);

/* fec_secded7264_decode
 *
 * Inputs:
 * enc_msg_len  size of encoded data
 * msg_enc      encoded message data
 *
 * Outputs:
 * msg_dec      decoded original message data gets put here
 * num_errors   number of unrecoverable errors encountered in decoding
 *              (can be NULL)
 *
 * Returns:
 * Size of decoded message in bytes
 */
unsigned int fec_secded7264_decode(unsigned int enc_msg_len,
                                   const unsigned char *msg_enc,
                                   unsigned char *msg_dec,
                                   unsigned int *num_errors);

/* fec_secded7264_decode_lazy
 *
 * Inputs:
 * enc_msg_len  size of encoded data
 * msg_enc      encoded message data
 *
 * Outputs:
 * msg_dec      decoded original message data gets put here
 *              this function is lazy and doesn't actually do anything
 *              to fix corrupt data
 *
 * Returns:
 * Size of decoded message in bytes
 */
unsigned int fec_secded7264_decode_lazy(unsigned int enc_msg_len,
                                        const unsigned char *msg_enc,
                                        unsigned char *msg_dec);

#endif
