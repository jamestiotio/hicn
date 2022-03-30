/*
 * Copyright (c) 2021 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * \file connection_table.c
 * \brief Implementation of hICN connection table
 */

#include <hicn/util/log.h>

#include "connection.h"
#include "connection_table.h"

/* This is only used as a hint for first allocation, as the table is resizeable
 */
#define DEFAULT_CONNECTION_TABLE_SIZE 64

connection_table_t *_connection_table_create(size_t init_size,
                                             size_t max_size) {
  if (init_size == 0) init_size = DEFAULT_CONNECTION_TABLE_SIZE;

  connection_table_t *table = malloc(sizeof(connection_table_t));
  if (!table) return NULL;

  table->max_size = max_size;

  /* Initialize indices */
  table->id_by_pair = kh_init_ct_pair();
  table->id_by_name = kh_init_ct_name();

  /*
   * We start by allocating a reasonably-sized pool, as this will eventually
   * be resized if needed.
   */
  pool_init(table->connections, init_size, 0);

  return table;
}

void connection_table_free(connection_table_t *table) {
  const char *k_name;
  const address_pair_t *k_pair;
  unsigned v_conn_id;

  connection_t *connection;
  const char *name;
  kh_foreach(table->id_by_name, k_name, v_conn_id, {
    connection = connection_table_get_by_id(table, v_conn_id);
    name = connection_get_name(connection);
    INFO("Removing connection %s [%d]", name, connection->fd);
    connection_finalize(connection);
  });

  (void)v_conn_id;
  kh_foreach(table->id_by_name, k_name, v_conn_id, { free((char *)k_name); });
  kh_foreach(table->id_by_pair, k_pair, v_conn_id,
             { free((address_pair_t *)k_pair); });

  kh_destroy_ct_pair(table->id_by_pair);
  kh_destroy_ct_name(table->id_by_name);
  pool_free(table->connections);
  free(table);
}

connection_t *connection_table_get_by_pair(const connection_table_t *table,
                                           const address_pair_t *pair) {
  khiter_t k = kh_get_ct_pair(table->id_by_pair, pair);
  if (k == kh_end(table->id_by_pair)) return NULL;
  return table->connections + kh_val(table->id_by_pair, k);
}

off_t connection_table_get_id_by_name(const connection_table_t *table,
                                      const char *name) {
  khiter_t k = kh_get_ct_name(table->id_by_name, name);
  if (k == kh_end(table->id_by_name)) return CONNECTION_ID_UNDEFINED;
  return kh_val(table->id_by_name, k);
}

connection_t *connection_table_get_by_name(const connection_table_t *table,
                                           const char *name) {
  unsigned conn_id = connection_table_get_id_by_name(table, name);
  if (!connection_id_is_valid(conn_id)) return NULL;
  return connection_table_at(table, conn_id);
}

void connection_table_remove_by_id(connection_table_t *table, off_t id) {
  connection_t *connection = connection_table_at(table, id);
  INFO("Removing connection %d (%s)", id, connection_get_name(connection));

  connection_table_deallocate(table, connection);
}

void connection_table_print_by_pair(const connection_table_t *table) {
  const address_pair_t *k;
  unsigned v;

  char local_addr_str[NI_MAXHOST], remote_addr_str[NI_MAXHOST];
  int local_port, remote_port;
  connection_t *connection;
  const char *name;

  INFO("*** Connection table ***");
  kh_foreach(table->id_by_pair, k, v, {
    address_to_string(&(k->local), local_addr_str, &local_port);
    address_to_string(&(k->remote), remote_addr_str, &remote_port);
    connection = connection_table_get_by_id(table, v);
    name = connection_get_name(connection);
    INFO("(%s:%d - %s:%d)\t\t\t(%u, %s)", local_addr_str, local_port,
         remote_addr_str, remote_port, v, name);
  })
}

void connection_table_print_by_name(const connection_table_t *table) {
  const char *k;
  unsigned v;

  connection_t *connection;
  const char *name;

  INFO("*** Connection table ***");
  kh_foreach(table->id_by_name, k, v, {
    connection = connection_table_get_by_id(table, v);
    name = connection_get_name(connection);
    INFO("(%s)\t\t\t(%u, %s)", k, v, name);
  })
}

connection_t *_connection_table_get_by_id(connection_table_t *table, off_t id) {
  return connection_table_get_by_id(table, id);
}

#define RANDBYTE() (u8)(rand() & 0xFF)

char *connection_table_get_random_name(const connection_table_t *table) {
  char *connection_name = malloc(SYMBOLIC_NAME_LEN * sizeof(char));
  u8 rand_num;

  /* Generate a random connection name */
  while (1) {
    rand_num = RANDBYTE();
    int rc = snprintf(connection_name, SYMBOLIC_NAME_LEN, "conn%u", rand_num);
    _ASSERT(rc < SYMBOLIC_NAME_LEN);

    // Return if connection name does not already exist
    khiter_t k = kh_get_ct_name(table->id_by_name, connection_name);
    if (k == kh_end(table->id_by_name)) break;
  }

  return connection_name;
}