//--------------------------------------------------------------------------------------------------
// Network Lab                             Spring 2024                           System Programming
//
/// @file  net.h
/// @brief helper functions for network programming
///
/// @author Bernhard Egger <bernhard@csap.snu.ac.kr>
/// @section changelog Change Log
/// 2016/10/14 Bernhard Egger created
/// 2017/11/24 Bernhard Egger added put/get_line functions
/// 2017/12/06 Bernhard Egger added getsocklist() & cleanup
/// 2020/11/25 Bernhard Egger cleanup & minor bugfixes
///
/// @section license_section License
/// Copyright (c) 2016-2023, Computer Systems and Platforms Laboratory, SNU
/// All rights reserved.
///
/// Redistribution and use in source and binary forms, with or without modification, are permitted
/// provided that the following conditions are met:
///
/// - Redistributions of source code must retain the above copyright notice, this list of condi-
///   tions and the following disclaimer.
/// - Redistributions in binary form must reproduce the above copyright notice, this list of condi-
///   tions and the following disclaimer in the documentation and/or other materials provided with
///   the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
/// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED  TO, THE IMPLIED  WARRANTIES OF MERCHANTABILITY
/// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
/// CONTRIBUTORS BE  LIABLE FOR ANY DIRECT,  INDIRECT, INCIDENTAL,  SPECIAL, EXEMPLARY,  OR CONSE-
/// QUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
/// LOSS OF USE, DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
/// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)
/// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
/// DAMAGE.
//--------------------------------------------------------------------------------------------------

#ifndef __NET_H__

#include <sys/socket.h>

/// @name network helper functions
/// @{

/// @brief wrapper for getaddrinfo(). Make sure to free the returned structure with freeaddrinfo().
/// @param host   host string (URL or IP in decimal dotted notation)
/// @param port   port
/// @param family network family (AF_INET: IPv4, AF_INET6: IPv6, AF_UNSPEC: IPv4/6)
/// @param type   socket type (SOCK_STREAM: TCP, SOCK_DGRAM: UDP)
/// @param listening >0 for listening sockets, ==0 for connecting sockets
/// @param res    if not NULL, holds result of getaddrinfo() call.
/// @retval addrinfo linked list of matching sockets
/// @retval NULL  in case of error. Use gai_strerror() with the @a res parameter to print the
///               error in human-readable form.
struct addrinfo *getsocklist(const char *host, unsigned short port, int family, int type, 
                             int listening, int *res);

/// @brief dump a sockaddr structure to stdout in human readable form.
/// @param sa pointer to sockaddr struct
void dump_sockaddr(struct sockaddr *sa);

/// @}

/// @name sending/receiving of unstructured data
/// @{

/// @brief read @a len bytes from @a sock into @a buf. Blocks until @a len bytes have been read,
///        and survives interrupts caused by signals.
/// @param sock socket to read from
/// @param buf pointer to data buffer
/// @param len number of bytes to read
/// @retval >0 number of bytes read
/// @retval == 0 nothing read (socket closed by peer)
/// @retval -1 error, errno contains error code
/// @retval -2 invalid arguments
int get_data(int sock, char *buf, size_t len);

/// @brief write @a len bytes from @a buf to @a sock. Blocks until @a len bytes have been written,
///        and survives interrupts caused by signals.
/// @param sock socket to write to
/// @param buf pointer to data buffer
/// @param len number of bytes to write
/// @retval >0 number of bytes sent
/// @retval == 0 nothing sent (socket closed by peer)
/// @retval -1 error, errno contains error code
/// @retval -2 invalid arguments
int put_data(int sock, char *buf, size_t len);

/// @}

/// @name sending/receiving of '\n'-terminated strings
/// @{

/// @brief read a '\\n'-terminated line from @a sock into @a buf. @a Buf is reallocated if neces-
///        sary. Blocks until one line has been read, and survives interrupts caused by signals.
/// @param sock socket to read from
/// @param buf data buffer. In/out parameter.
/// @param cur_len length of data buffer. In/out parameter.
/// @retval >0 number of bytes read (including terminating newline)
/// @retval == 0 nothing read (socket closed by peer)
/// @retval -1 error, errno contains error code
/// @retval -2 invalid arguments
int get_line(int sock, char **buf, size_t *cur_len);

/// @brief write one or several '\\n'-terminated lines from @a buf to @a sock. If @a buf is not
///        '\\n'-terminated, an extra newline character is sent automatically. Blocks until all
///        lines have been sent, and survives interrupts caused by signals.
/// @param sock socket to write to
/// @param buf pointer to line buffer
/// @param len (max.) number of bytes to write (termination at first '\0')
/// @retval >0 number of bytes sent (including terminating newline)
/// @retval == 0 nothing sent (socket closed by peer)
/// @retval -1 error, errno contains error code
/// @retval -2 invalid arguments
int put_line(int sock, char *buf, size_t len);

/// @}


#endif // __NET_H__
