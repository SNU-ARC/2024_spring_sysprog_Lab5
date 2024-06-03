//--------------------------------------------------------------------------------------------------
// Network Lab                             Spring 2024                           System Programming
//
/// @file  net.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include "net.h"

struct addrinfo *getsocklist(const char *host, unsigned short port, int family, int type, 
                             int listening, int *res)
{
  char portstr[6];
  struct addrinfo hints, *ai;
  int r;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = family;
  hints.ai_socktype = type;
  hints.ai_flags    = AI_ADDRCONFIG;
  if (listening) hints.ai_flags |= AI_PASSIVE;
  hints.ai_protocol = 0;

  snprintf(portstr, sizeof(portstr), "%d", port);
  r = getaddrinfo(listening ? NULL : host, portstr, &hints, &ai);

  if (res) *res = r;
  if (r != 0) return NULL;
  else return ai;
}

void dump_sockaddr(struct sockaddr *sa)
{
  char adrstr[40];

  if (sa->sa_family == AF_INET) {
    // IPv4
    struct sockaddr_in *sa4 = (struct sockaddr_in*)sa;
    if (inet_ntop(sa4->sin_family, &sa4->sin_addr, adrstr, sizeof(adrstr)) == NULL) {
      perror("inet_pton");
    } else {
      printf("%s:%d (IPv4)", adrstr, ntohs(sa4->sin_port));
    }
  } else if (sa->sa_family == AF_INET6) {
    // IPv6
    struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)sa;
    if (inet_ntop(sa6->sin6_family, &sa6->sin6_addr, adrstr, sizeof(adrstr)) == NULL) {
      perror("inet_pton");
    } else {
      printf("%s:%d (IPv6)", adrstr, ntohs(sa6->sin6_port));
    }
  } else {
    // unsupported
    printf("unknown protocol family (neither IPv4 nor IPv6)\n");
  }
  fflush(stdout);
}

/// @internal
#define NET_RECV 0
#define NET_SEND 1

static int transfer_data(int mode, int sock, char *buf, size_t len)
{
  if (!((mode == NET_RECV) || (mode == NET_SEND)) || (buf == NULL)) return -2;

  int res = 0;

  while (len > 0) {
    int r;
    if (mode == NET_RECV) r = recv(sock, buf, len, 0);
    else r = send(sock, buf, len, 0);

    if (r > 0) {
      // success: read r bytes
      buf += r;   // point to next position in buf
      len -= r;   // compute remaining number of bytes
      res += r;   // update total number of read bytes
    } else if (r == 0) {
      // EOF: no bytes read
      break;
    } else {
      // error: return -1, error in errno
      // interrupted by signal; continue
      if (errno == EINTR) continue;
      // unrecoverable error: abort and report back
      res = -1;
      break;
    }
  }

  return res;
}
/// @endinternal

int get_data(int sock, char *buf, size_t len)
{
  return transfer_data(NET_RECV, sock, buf, len);
}

int put_data(int sock, char *buf, size_t len)
{
  return transfer_data(NET_SEND, sock, buf, len);
}

int get_line(int sock, char **buf, size_t *cur_len)
{
  if (*cur_len == 0) return -2;

  char c;
  int res = 0;
  size_t pos = 0;

  // read to first newline ('\n') or transmission error
  do {
    res = get_data(sock, &c, 1);
    if (res == 1) {
      (*buf)[pos++] = c;

      // allocate more memory for buf if necessary
      if (pos == *cur_len) {
        *cur_len <<= 1;
        *buf = (char *)realloc(*buf, *cur_len);
      }
    }
  } while ((res == 1) && (c != '\n'));

  // null-terminate string
  (*buf)[pos] = '\0';

  // return number of characters read (excluding \0) or error
  if (c == '\n') return (int)pos; // we assume pos < MAX_INT
  else return res;
}

int put_line(int sock, char *buf, size_t len)
{
  if (len == 0) return -2;

  char c;
  int res = 1, res2;
  size_t pos = 0;

  // find end of string (terminating '\0')
  while ((pos < len) && (buf[pos] != '\0')) pos++;

  // there was some data (not just a '\0'), send it (exclude terminating '\0')
  if (pos > 0) {
    res = put_data(sock, buf, pos);
  }

  // send '\n' if string wasn't ended by it
  if ((res > 0) && (buf[pos-1] != '\n')) {
    c = '\n';
    res2 = put_data(sock, &c, 1);
    if (res2 < 0) res = res2;
    else res += res2;
  }

  // return  0 for success, <0 on error
  return res;
}

