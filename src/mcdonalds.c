//--------------------------------------------------------------------------------------------------
// Network Lab                             Spring 2024                           System Programming
//
/// @file
/// @brief Simple virtual McDonald's server for Network Lab
///
/// @author <your name>
/// @studid <your student id>
///
/// @section changelog Change Log
/// 2020/11/18 Hyunik Kim created
/// 2021/11/23 Jaume Mateu Cuadrat cleanup, add milestones
/// 2024/05/31 ARC lab add multiple orders per request
///
/// @section license_section License
/// Copyright (c) 2020-2023, Computer Systems and Platforms Laboratory, SNU
/// Copyright (c) 2024, Architecture and Code Optimization Laboratory, SNU
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
/// CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,  INDIRECT, INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSE-
/// QUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
/// LOSS OF USE, DATA,  OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED AND ON ANY THEORY OF
/// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
/// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
/// DAMAGE.
//--------------------------------------------------------------------------------------------------

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include "net.h"
#include "burger.h"

/// @name Structures
/// @{

/// @brief general node element to implement a singly-linked list
typedef struct __node {
  struct __node *next;                                      ///< pointer to next node
  unsigned int customerID;                                  ///< customer ID that requested
  enum burger_type type;                                    ///< requested burger type
  pthread_cond_t *cond;                                     ///< conditional variable
  pthread_mutex_t *cond_mutex;                              ///< mutex variable for conditional variable
  char **order_str;                                         ///< pointer of string to be made by kitchen
  unsigned int *remain_count;                               ///< number of remaining burgers
  //
  // TODO: Add more variables if needed
  //
} Node;

/// @brief order data
typedef struct __order_list {
  Node *head;                                               ///< head of order list
  Node *tail;                                               ///< tail of order list
  unsigned int count;                                       ///< number of nodes in list
} OrderList;

/// @brief structure for server context
struct mcdonalds_ctx {
  unsigned int total_customers;                             ///< number of customers served
  unsigned int total_burgers[BURGER_TYPE_MAX];              ///< number of burgers produced by types
  unsigned int total_queueing;                              ///< number of customers in queue
  OrderList list;                                           ///< starting point of list structure
  pthread_mutex_t lock;                                     ///< lock variable for server context
};

/// @}

/// @name Global variables
/// @{

int listenfd;                                               ///< listen file descriptor
struct mcdonalds_ctx server_ctx;                            ///< keeps server context
sig_atomic_t keep_running = 1;                              ///< keeps all the threads running
pthread_t kitchen_thread[NUM_KITCHEN];                      ///< thread for kitchen
pthread_mutex_t kitchen_mutex;                              ///< shared mutex for kitchen threads

/// @}


/// @brief Enqueue elements in tail of the OrderList
/// @param customerID customer ID
/// @param types list of burger types
/// @param burger_count number of burgers
/// @retval Node** of issued order Nodes
Node** issue_orders(unsigned int customerID, enum burger_type *types, unsigned int burger_count)
{
  // List of node pointers that are to be issued
  Node **node_list = (Node **)malloc(sizeof(Node *) * burger_count);

  // Initialize order string for request
  char **order_str = (char **)malloc(sizeof(char *));
  *order_str = NULL;

  // Initialize conditon variable and mutex for request
  pthread_cond_t *cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  pthread_mutex_t *cond_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
  pthread_cond_init(cond, NULL);
  pthread_mutex_init(cond_mutex, NULL);

  // Initialize remaining count
  unsigned int *remain_count = (unsigned int*)malloc(sizeof(unsigned int));
  *remain_count = burger_count;

  //
  // TODO: Initialize shared Node variables if added any
  //

  for (int i=0; i<burger_count; i++){
    // Create new Node
    Node *new_node = malloc(sizeof(Node));

    // Initialize Node variables
    new_node->customerID = customerID;
    new_node->type = types[i];
    new_node->next = NULL;
    new_node->remain_count = remain_count;
    new_node->order_str = order_str;
    new_node->cond = cond;
    new_node->cond_mutex = cond_mutex;

    //
    // TODO: Initialize other Node variables if added any
    //

    // Add Node to list
    pthread_mutex_lock(&server_ctx.lock);
    if (server_ctx.list.tail == NULL) {
      server_ctx.list.head = new_node;
      server_ctx.list.tail = new_node;
    } else {
      server_ctx.list.tail->next = new_node;
      server_ctx.list.tail = new_node;
    }
    server_ctx.list.count++;
    pthread_mutex_unlock(&server_ctx.lock);

    // Add new node to node list
    node_list[i] = new_node;
  }

  // Return node list
  return node_list;
}

/// @brief Dequeue element from the OrderList
/// @retval Node* Node from head of the list
Node* get_order(void)
{
  Node *target_node;

  if (server_ctx.list.head == NULL) return NULL;

  pthread_mutex_lock(&server_ctx.lock);

  target_node = server_ctx.list.head;

  if (server_ctx.list.head == server_ctx.list.tail) {
    server_ctx.list.head = NULL;
    server_ctx.list.tail = NULL;
  } else {
    server_ctx.list.head = server_ctx.list.head->next;
  }

  server_ctx.list.count--;

  pthread_mutex_unlock(&server_ctx.lock);

  return target_node;
}

/// @brief Returns number of element left in OrderList
/// @retval number of element(s) in OrderList
unsigned int order_left(void)
{
  int ret;

  pthread_mutex_lock(&server_ctx.lock);
  ret = server_ctx.list.count;
  pthread_mutex_unlock(&server_ctx.lock);

  return ret;
}

/// @brief "cook" burger by appending burger name to order_str of Node
/// @param order Order Node
void make_burger(Node *order)
{
  // == DO NOT MODIFY ==
  enum burger_type type = order->type;
  char *temp;

  if(*(order->order_str) != NULL){
    // If string is initialized, append burger name to order_string

    // Copy original string to temp buffer
    int ret = asprintf(&temp, "%s", *(order->order_str));
    if(ret < 0) perror("asprintf");
    free(*(order->order_str));

    // Append burger name to order_str
    ret = asprintf(order->order_str, "%s %s", temp, burger_names[type]);
    if(ret < 0) perror("asprintf");
    free(temp);
  }
  else{
    // If string is not initialized, copy burger name to order string
    int str_len = strlen(burger_names[type]);
    *(order->order_str) = (char *)malloc(sizeof(char) * str_len);
    strncpy(*(order->order_str), burger_names[type], str_len);
  }

  sleep(1);
  // ===================
}

/// @brief Kitchen task for kitchen thread
void* kitchen_task(void *dummy)
{
  Node *order;
  enum burger_type type;
  unsigned int customerID;
  pthread_t tid = pthread_self();

  printf("[Thread %lu] Kitchen thread ready\n", tid);

  while (keep_running || order_left()) {
    // Keep dequeuing if possible
    order = get_order();

    // If no order is available, sleep and try again
    if (order == NULL) {
      sleep(2);
      continue;
    }

    type = order->type;
    customerID = order->customerID;
    printf("[Thread %lu] generating %s burger for customer %u\n", tid, burger_names[type], customerID);

    // Make burger and reduce `remain_count` of request
    // - Use make_burger()
    // - Reduce `remain_count` by one
    // TODO

    printf("[Thread %lu] %s burger for customer %u is ready\n", tid, burger_names[type], customerID);

    // If every burger is made, fire signal to serving thread
    if(*(order->remain_count) == 0){
      printf("[Thread %lu] all orders done for customer %u\n", tid, customerID);
      pthread_cond_signal(order->cond);
    }

    // Increase burger count
    pthread_mutex_lock(&server_ctx.lock);
    server_ctx.total_burgers[type]++;
    pthread_mutex_unlock(&server_ctx.lock);
  }

  printf("[Thread %lu] terminated\n", tid);
  pthread_exit(NULL);
}

/// @brief error function for the serve_client
/// @param clientfd file descriptor of the client*
/// @param newsock socketid of the client as void*
/// @param newsock buffer for the messages*
void error_client(int clientfd, void *newsock,char *buffer) {
  close(clientfd);
  free(newsock);
  free(buffer);

  pthread_mutex_lock(&server_ctx.lock);
  server_ctx.total_queueing--;
  pthread_mutex_unlock(&server_ctx.lock);
}

/// @brief client task for client thread
/// @param newsock socketID of the client as void*
void* serve_client(void *newsock)
{
  ssize_t read, sent;             // size of read and sent message
  size_t msglen;                  // message buffer size
  char *message, *buffer;         // message buffers
  char *burger;                   // parsed burger string
  unsigned int customerID;        // customer ID
  enum burger_type *types;        // list of burger types
  Node **order_list = NULL;       // list of orders issued
  int ret, i, clientfd;           // misc. values
  unsigned int burger_count = 0;  // number of burgers in request
  Node *first_order;              // first order of requests

  clientfd = *(int *) newsock;
  buffer = (char *) malloc(BUF_SIZE);
  msglen = BUF_SIZE;

  // Get customer ID
  pthread_mutex_lock(&server_ctx.lock);
  customerID = server_ctx.total_customers++;
  pthread_mutex_unlock(&server_ctx.lock);

  printf("Customer #%d visited\n", customerID);

  // Generate welcome message
  ret = asprintf(&message, "Welcome to McDonald's, customer #%d\n", customerID);
  if (ret < 0) {
    perror("asprintf");
    return NULL;
  }

  // Send welcome to mcdonalds
  sent = put_line(clientfd, message, ret);
  if (sent < 0) {
    printf("Error: cannot send data to client\n");
    error_client(clientfd, newsock, buffer);
    return NULL;
  }
  free(message);

  // Receive request from the customer
  // TODO

  // Parse and split request from the customer into orders
  // - Fill in `types` variable with each parsed burger type and increase `burger_count`
  // - While parsing, if burger is not an available type, exit connection
  // - Tip: try using strtok_r() with while statement
  // TODO

  // Issue orders to kitchen and wait
  // - Tip: use pthread_cond_wait() to wait
  // - Tip2: use issue_orders() to issue request
  // - Tip3: all orders in a request share the same `cond` and `cond_mutex`,
  //         so access such variables through the first order
  // TODO

  // If request is successfully handled, hand ordered burgers and say goodbye
  // All orders share the same `remain_count`, so access it through the first order
  if (*(first_order->remain_count) == 0) {
    ret = asprintf(&message, "Your order(%s) is ready! Goodbye!\n", *(first_order->order_str));
    sent = put_line(clientfd, message, ret);
    if (sent <= 0) {
      printf("Error: cannot send data to client\n");
      error_client(clientfd, newsock, buffer);
      return NULL;
    }
    free(*(first_order->order_str));
    free(message);
  }

  pthread_cond_destroy(first_order->cond);
  pthread_mutex_destroy(first_order->cond_mutex);

  // If any, free unused variables
  // TODO

  close(clientfd);
  free(newsock);
  free(buffer);

  pthread_mutex_lock(&server_ctx.lock);
  server_ctx.total_queueing--;
  pthread_mutex_unlock(&server_ctx.lock);

  return NULL;
}

/// @brief start server listening
void start_server()
{
  int clientfd, addrlen, opt = 1, *newsock;
  struct sockaddr_in client;
  struct addrinfo *ai, *ai_it;

  // Get socket list by using getsocklist()
  // TODO

  // Iterate over addrinfos and try to bind & listen
  // TODO

  printf("Listening...\n");

  // Keep listening and accepting clients
  // Check if max number of customers is not exceeded after accepting
  // Create a serve_client thread for the client
  // TODO
}

/// @brief prints overall statistics
void print_statistics(void)
{
  int i;

  printf("\n====== Statistics ======\n");
  printf("Number of customers visited: %u\n", server_ctx.total_customers);
  for (i = 0; i < BURGER_TYPE_MAX; i++) {
    printf("Number of %s burger made: %u\n", burger_names[i], server_ctx.total_burgers[i]);
  }
  printf("\n");
}

/// @brief exit function
void exit_mcdonalds(void)
{
  pthread_mutex_destroy(&server_ctx.lock);
  close(listenfd);
  print_statistics();
}

/// @brief Second SIGINT handler function
/// @param sig signal number
void sigint_handler2(int sig)
{
  exit_mcdonalds();
  exit(EXIT_SUCCESS);
}

/// @brief First SIGINT handler function
/// @param sig signal number
void sigint_handler(int sig)
{
  signal(SIGINT, sigint_handler2);
  printf("****** I'm tired, closing McDonald's ******\n");
  keep_running = 0;
  sleep(3);
  exit(EXIT_SUCCESS);
}

/// @brief init function initializes necessary variables and sets SIGINT handler
void init_mcdonalds(void)
{
  int i;

  printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  printf("@@@@@@@@@@@@@@@@@(,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,(@@@@@@@@@@@@@@@@@\n");
  printf("@@@@@@@@@@@@@@@,,,,,,,,,,,,,,,,,@@@@@@@@@@@@@@@@@,,,,,,,,,,,,,,,,,@@@@@@@@@@@@@@@\n");
  printf("@@@@@@@@@@@@@,,,,,,,@@@@@@,,,,,,,@@@@@@@@@@@@@@(,,,,,,@@@@@@@,,,,,,,@@@@@@@@@@@@@\n");
  printf("@@@@@@@@@@@@,,,,,,@@@@@@@@@@,,,,,,,@@@@@@@@@@@,,,,,,,@@@@@@@@@*,,,,,,@@@@@@@@@@@@\n");
  printf("@@@@@@@@@@.,,,,,,@@@@@@@@@@@@,,,,,,,@@@@@@@@@,,,,,,,@@@@@@@@@@@@,,,,,,/@@@@@@@@@@\n");
  printf("@@@@@@@@@,,,,,,,,@@@@@@@@@@@@@,,,,,,,@@@@@@@,,,,,,,@@@@@@@@@@@@@,,,,,,,,@@@@@@@@@\n");
  printf("@@@@@@@@,,,,,,,,@@@@@@@@@@@@@@@,,,,,,,@@@@@,,,,,,,@@@@@@@@@@@@@@@,,,,,,,,@@@@@@@@\n");
  printf("@@@@@@@@,,,,,,,@@@@@@@@@@@@@@@@,,,,,,,,@@@,,,,,,,,@@@@@@@@@@@@@@@@,,,,,,,@@@@@@@@\n");
  printf("@@@@@@@,,,,,,,,@@@@@@@@@@@@@@@@@,,,,,,,,@,,,,,,,,@@@@@@@@@@@@@@@@@,,,,,,,,@@@@@@@\n");
  printf("@@@@@@,,,,,,,,@@@@@@@@@@@@@@@@@@,,,,,,,,,,,,,,,,,@@@@@@@@@@@@@@@@@@,,,,,,,,@@@@@@\n");
  printf("@@@@@,,,,,,,,,@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@,,,,,,,,,@@@@@\n");
  printf("@@@@@,,,,,,,,@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@,,,,,,,,@@@@@\n");
  printf("@@@@,,,,,,,,,@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,@@@@\n");
  printf("@@@@,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,@@@@\n");
  printf("@@@,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,@@@\n");
  printf("@@@,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,@@@\n");
  printf("@@@,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,@@@\n");
  printf("@@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@@\n");
  printf("@@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@@\n");
  printf("@@,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,@@\n");
  printf("@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@\n");
  printf("@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@\n");
  printf("@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@\n");
  printf("@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@\n");
  printf("@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@\n");
  printf("@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@\n");
  printf("@,,,,,,,,,,@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@,,,,,,,,,,@\n");

  printf("\n\n                          I'm lovin it! McDonald's\n\n");

  signal(SIGINT, sigint_handler);
  pthread_mutex_init(&server_ctx.lock, NULL);

  server_ctx.total_customers = 0;
  server_ctx.total_queueing = 0;
  for (i = 0; i < BURGER_TYPE_MAX; i++) {
    server_ctx.total_burgers[i] = 0;
  }

  pthread_mutex_init(&kitchen_mutex, NULL);

  for (i = 0; i < NUM_KITCHEN; i++) {
    pthread_create(&kitchen_thread[i], NULL, kitchen_task, NULL);
    pthread_detach(kitchen_thread[i]);
  }
}

/// @brief program entry point
int main(int argc, char *argv[])
{
  init_mcdonalds();
  start_server();
  exit_mcdonalds();

  return 0;
}
