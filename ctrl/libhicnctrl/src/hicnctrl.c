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
 * \file cli.c
 * \brief Command line interface
 */
#include <ctype.h>  // isalpha isalnum
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>  // getopt

#include <hicn/ctrl.h>
#include <hicn/util/ip_address.h>
#include <hicn/util/token.h>
#include <hicn/validation.h>

#define die(LABEL, MESSAGE) \
  do {                      \
    printf(MESSAGE "\n");   \
    rc = -1;                \
    goto ERR_##LABEL;       \
  } while (0)

const char HICNLIGHT_PARAM[] = "hicnlight";
const char HICNLIGHT_NG_PARAM[] = "hicnlightng";
const char VPP_PARAM[] = "vpp";

void usage_header() { fprintf(stderr, "Usage:\n"); }

void usage_face_create(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr,
          "%s -f TYPE LOCAL_ADDRESS LOCAL_PORT REMOTE_ADDRESS REMOTE_PORT "
          "[INTERFACE_NAME]\n",
          prog);
  if (verbose)
    fprintf(stderr, "    Create a face on specified address and port.\n");
}

void usage_face_delete(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -df ID\n", prog);
  // fprintf(stderr, "%s -df NAME\n", prog);
  fprintf(stderr,
          "%s -df TYPE LOCAL_ADDRESS LOCAL_PORT REMOTE_ADDRESS REMOTE_PORT "
          "[INTERFACE_NAME]\n",
          prog);
  if (verbose) fprintf(stderr, "    Delete a face...\n");
}

void usage_face_list(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -F\n", prog);
  if (verbose) fprintf(stderr, "    List all faces.\n");
}

void usage_face(const char *prog, bool header, bool verbose) {
  usage_face_create(prog, header, verbose);
  usage_face_delete(prog, header, verbose);
  usage_face_list(prog, header, verbose);
}

void usage_route_create(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -r FACE_ID PREFIX [COST]\n", prog);
  // fprintf(stderr, "%s -r [FACE_ID|NAME] PREFIX [COST]\n", prog);
  if (verbose) fprintf(stderr, "    Create a route...\n");
}

void usage_route_delete(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -dr FACE_ID PREFIX\n", prog);
  // fprintf(stderr, "%s -dr [FACE_ID|NAME] PREFIX\n", prog);
  if (verbose) fprintf(stderr, "    Delete a route...\n");
}

void usage_route_list(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -R\n", prog);
  if (verbose) fprintf(stderr, "    List all routes.\n");
}

void usage_route(const char *prog, bool header, bool verbose) {
  usage_route_create(prog, header, verbose);
  usage_route_delete(prog, header, verbose);
  usage_route_list(prog, header, verbose);
}

void usage_forwarding_strategy_create(const char *prog, bool header,
                                      bool verbose) {
  if (header) usage_header();
}
void usage_forwarding_strategy_delete(const char *prog, bool header,
                                      bool verbose) {
  if (header) usage_header();
}

void usage_forwarding_strategy_list(const char *prog, bool header,
                                    bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -S\n", prog);
  if (verbose)
    fprintf(stderr, "    List all availble forwarding strategies.\n");
}

void usage_forwarding_strategy(const char *prog, bool header, bool verbose) {
  usage_forwarding_strategy_create(prog, header, verbose);
  usage_forwarding_strategy_delete(prog, header, verbose);
  usage_forwarding_strategy_list(prog, header, verbose);
}

void usage_listener_create(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -l NAME TYPE LOCAL_ADDRESS LOCAL_PORT [INTERFACE_NAME]\n",
          prog);
  if (verbose)
    fprintf(stderr, "    Create a listener on specified address and port.\n");
}

void usage_listener_delete(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -dl ID\n", prog);
  fprintf(stderr, "%s -dl NAME\n", prog);
  fprintf(stderr, "%s -dl TYPE LOCAL_ADDRESS LOCAL_PORT [INTERFACE_NAME]\n",
          prog);
  if (verbose) fprintf(stderr, "    Delete a listener...\n");
}

void usage_listener_list(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -L\n", prog);
  if (verbose) fprintf(stderr, "    List all listeners.\n");
}

void usage_listener(const char *prog, bool header, bool verbose) {
  usage_listener_create(prog, header, verbose);
  usage_listener_delete(prog, header, verbose);
  usage_listener_list(prog, header, verbose);
}
void usage_connection_create(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr,
          "%s -c NAME TYPE LOCAL_ADDRESS LOCAL_PORT REMOTE_ADDRESS REMOTE_PORT "
          "[INTERFACE_NAME]\n",
          prog);
  if (verbose)
    fprintf(stderr, "    Create a connection on specified address and port.\n");
}

void usage_connection_delete(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -dc ID\n", prog);
  fprintf(stderr, "%s -dc NAME\n", prog);
  fprintf(stderr,
          "%s -dc TYPE LOCAL_ADDRESS LOCAL_PORT REMOTE_ADDRESS REMOTE_PORT "
          "[INTERFACE_NAME]\n",
          prog);
  if (verbose) fprintf(stderr, "    Delete a connection...\n");
}

void usage_connection_list(const char *prog, bool header, bool verbose) {
  if (header) usage_header();
  fprintf(stderr, "%s -C\n", prog);
  if (verbose) fprintf(stderr, "    List all connections.\n");
}

void usage_connection(const char *prog, bool header, bool verbose) {
  usage_connection_create(prog, header, verbose);
  usage_connection_delete(prog, header, verbose);
  usage_connection_list(prog, header, verbose);
}

void usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s [ -z forwarder (hicnlight | vpp) ] [ [-d] [-f|-l|-c|-r] "
          "PARAMETERS | [-F|-L|-C|-R] ]\n",
          prog);
  fprintf(stderr, "\n");
  fprintf(stderr, "High-level commands\n");
  fprintf(stderr, "\n");
  usage_face(prog, false, true);
  usage_route(prog, false, true);
  usage_forwarding_strategy(prog, false, true);
  fprintf(stderr, "\n");
  fprintf(stderr, "Low level commands (hicn-light specific)\n");
  fprintf(stderr, "\n");
  usage_listener(prog, false, true);
  usage_connection(prog, false, true);
}

#if 0
typedef struct {
    hc_action_t action;
    hc_object_t object;
    union {
        hc_face_t face;
        hc_route_t route;
        hc_connection_t connection;
        hc_listener_t listener;
    };
} hc_command_t;
#endif

int parse_options(int argc, char *argv[], hc_command_t *command,
                  forwarder_type_t *forwarder) {
  command->object.type = OBJECT_UNDEFINED;
  command->action = ACTION_CREATE;
  int opt;
  int family;

  while ((opt = getopt(argc, argv, "dflcrFLCRShz:")) != -1) {
    switch (opt) {
      case 'z':
        if (strncmp(optarg, VPP_PARAM, strlen(VPP_PARAM)) == 0) {
          *forwarder = VPP;
        } else if (strncmp(optarg, HICNLIGHT_PARAM, strlen(HICNLIGHT_PARAM))) {
          *forwarder = HICNLIGHT;
        } else if (strncmp(optarg, HICNLIGHT_NG_PARAM,
                           strlen(HICNLIGHT_NG_PARAM))) {
          *forwarder = HICNLIGHT_NG;
        } else {
          usage(argv[0]);
          exit(EXIT_FAILURE);
        }
        break;
      case 'd':
        command->action = ACTION_DELETE;
        break;
      case 'f':
        command->object.type = OBJECT_FACE;
        break;
      case 'c':
        command->object.type = OBJECT_CONNECTION;
        break;
      case 'F':
        command->action = ACTION_LIST;
        command->object.type = OBJECT_FACE;
        break;
      case 'L':
        command->action = ACTION_LIST;
        command->object.type = OBJECT_LISTENER;
        break;
      case 'C':
        command->action = ACTION_LIST;
        command->object.type = OBJECT_CONNECTION;
        break;
      case 'R':
        command->action = ACTION_LIST;
        command->object.type = OBJECT_ROUTE;
        break;
      case 'S':
        command->action = ACTION_LIST;
        command->object.type = OBJECT_STRATEGY;
        break;
      default: /* "h" */
        usage(argv[0]);
        exit(EXIT_SUCCESS);
    }
  }

  if (command->object.type == OBJECT_UNDEFINED) {
    fprintf(stderr,
            "Missing object specification: connection | listener | route\n");
    return -1;
  }

  /* Parse and validate parameters for add/delete */
  switch (command->object.type) {
    case OBJECT_FACE:
      switch (command->action) {
        case ACTION_CREATE:
          if ((argc - optind != 5) && (argc - optind != 6)) {
            usage_face_create(argv[0], true, false);
            goto ERR_PARAM;
          }
          /* NAME will be autogenerated (and currently not used) */
          // snprintf(command->face.name, SYMBOLIC_NAME_LEN, "%s",
          // argv[optind++]);
          command->object.face.face.type = face_type_from_str(argv[optind++]);
          if (command->object.face.face.type == FACE_TYPE_UNDEFINED)
            goto ERR_PARAM;
          command->object.face.face.family =
              ip_address_get_family(argv[optind]);
          if (!IS_VALID_FAMILY(command->object.face.face.family))
            goto ERR_PARAM;
          if (ip_address_pton(argv[optind++],
                              &command->object.face.face.local_addr) < 0)
            goto ERR_PARAM;
          command->object.face.face.local_port = atoi(argv[optind++]);
          family = ip_address_get_family(argv[optind]);
          if (!IS_VALID_FAMILY(family) ||
              (command->object.face.face.family != family))
            goto ERR_PARAM;
          if (ip_address_pton(argv[optind++],
                              &command->object.face.face.remote_addr) < 0)
            goto ERR_PARAM;
          command->object.face.face.remote_port = atoi(argv[optind++]);
          if (argc != optind) {
            // netdevice_set_name(&command->object.face.face.netdevice,
            // argv[optind++]);
            command->object.face.face.netdevice.index = atoi(argv[optind++]);
          }

          break;
        case ACTION_DELETE:
          if ((argc - optind != 1) && (argc - optind != 5) &&
              (argc - optind != 6)) {
            usage_face_delete(argv[0], true, false);
            goto ERR_PARAM;
          }

          if (argc - optind == 1) {
            /* Id or name */
            if (is_number(argv[optind], SYMBOLIC_NAME_LEN)) {
              command->object.face.id = atoi(argv[optind++]);
              snprintf(command->object.face.name, SYMBOLIC_NAME_LEN, "%s",
                       argv[optind++]);
              //} else if (is_symbolic_name(argv[optind])) {
              //    snprintf(command->object.face.name, SYMBOLIC_NAME_LEN, "%s",
              //    argv[optind++]);
            } else {
              fprintf(stderr, "Invalid argument\n");
              goto ERR_PARAM;
            }
          } else {
            command->object.face.face.type = face_type_from_str(argv[optind++]);
            if (command->object.face.face.type == FACE_TYPE_UNDEFINED)
              goto ERR_PARAM;
            command->object.face.face.family =
                ip_address_get_family(argv[optind]);
            if (!IS_VALID_FAMILY(command->object.face.face.family))
              goto ERR_PARAM;
            if (ip_address_pton(argv[optind++],
                                &command->object.face.face.local_addr) < 0)
              goto ERR_PARAM;
            command->object.face.face.local_port = atoi(argv[optind++]);
            family = ip_address_get_family(argv[optind]);
            if (!IS_VALID_FAMILY(family) ||
                (command->object.face.face.family != family))
              goto ERR_PARAM;
            if (ip_address_pton(argv[optind++],
                                &command->object.face.face.remote_addr) < 0)
              goto ERR_PARAM;
            command->object.face.face.remote_port = atoi(argv[optind++]);
            if (argc != optind) {
              command->object.face.face.netdevice.index = atoi(argv[optind++]);
              // netdevice_set_name(&command->object.face.face.netdevice,
              // argv[optind++]);
            }
          }
          break;

        case ACTION_LIST:
          if (argc - optind != 0) {
            usage_face_list(argv[0], true, false);
            goto ERR_PARAM;
          }
          break;

        default:
          goto ERR_COMMAND;
      }
      break;

    case OBJECT_ROUTE:
      switch (command->action) {
        case ACTION_CREATE:
          if ((argc - optind != 2) && (argc - optind != 3)) {
            usage_route_create(argv[0], true, false);
            goto ERR_PARAM;
          }

          command->object.route.face_id = atoi(argv[optind++]);

          {
            ip_prefix_t prefix;
            ip_prefix_pton(argv[optind++], &prefix);
            command->object.route.family = prefix.family;
            command->object.route.remote_addr = prefix.address;
            command->object.route.len = prefix.len;
          }

          if (argc != optind) {
            printf("parse cost\n");
            command->object.route.cost = atoi(argv[optind++]);
          }
          break;

        case ACTION_DELETE:
          if (argc - optind != 2) {
            usage_route_delete(argv[0], true, false);
            goto ERR_PARAM;
          }

          command->object.route.face_id = atoi(argv[optind++]);

          {
            ip_prefix_t prefix;
            ip_prefix_pton(argv[optind++], &prefix);
            command->object.route.family = prefix.family;
            command->object.route.remote_addr = prefix.address;
            command->object.route.len = prefix.len;
          }
          break;

        case ACTION_LIST:
          if (argc - optind != 0) {
            usage_route_list(argv[0], true, false);
            goto ERR_PARAM;
          }
          break;

        default:
          goto ERR_COMMAND;
      }
      break;

    case OBJECT_STRATEGY:
      switch (command->action) {
        case ACTION_LIST:
          if (argc - optind != 0) {
            usage_forwarding_strategy_list(argv[0], true, false);
            goto ERR_PARAM;
          }
          break;
        default:
          goto ERR_COMMAND;
      }
      break;

    case OBJECT_LISTENER:
      switch (command->action) {
        case ACTION_CREATE:
          if ((argc - optind != 4) && (argc - optind != 5)) {
            usage_listener_create(argv[0], true, false);
            goto ERR_PARAM;
          }

          snprintf(command->object.listener.name, SYMBOLIC_NAME_LEN, "%s",
                   argv[optind++]);
          command->object.listener.type = face_type_from_str(argv[optind++]);
          if (command->object.listener.type == FACE_TYPE_UNDEFINED)
            goto ERR_PARAM;
          command->object.listener.family = ip_address_get_family(argv[optind]);
          if (!IS_VALID_FAMILY(command->object.listener.family)) goto ERR_PARAM;
          if (ip_address_pton(argv[optind++],
                              &command->object.listener.local_addr) < 0)
            goto ERR_PARAM;
          command->object.listener.local_port = atoi(argv[optind++]);
          if (argc != optind) {
            snprintf(command->object.listener.interface_name, INTERFACE_LEN,
                     "%s", argv[optind++]);
          }
          break;

        case ACTION_DELETE:
          if ((argc - optind != 1) && (argc - optind != 3) &&
              (argc - optind != 4)) {
            usage_listener_delete(argv[0], true, false);
            goto ERR_PARAM;
          }

          if (argc - optind == 1) {
            /* Id or name */
            if (is_number(argv[optind], SYMBOLIC_NAME_LEN)) {
              command->object.listener.id = atoi(argv[optind++]);
              snprintf(command->object.listener.name, SYMBOLIC_NAME_LEN, "%s",
                       argv[optind++]);
            } else if (is_symbolic_name(argv[optind], SYMBOLIC_NAME_LEN)) {
              snprintf(command->object.listener.name, SYMBOLIC_NAME_LEN, "%s",
                       argv[optind++]);
            } else {
              fprintf(stderr, "Invalid argument\n");
              goto ERR_PARAM;
            }
          } else {
            command->object.listener.type = face_type_from_str(argv[optind++]);
            if (command->object.listener.type == FACE_TYPE_UNDEFINED)
              goto ERR_PARAM;
            command->object.listener.family =
                ip_address_get_family(argv[optind]);
            if (!IS_VALID_FAMILY(command->object.listener.family))
              goto ERR_PARAM;
            if (ip_address_pton(argv[optind++],
                                &command->object.listener.local_addr) < 0)
              goto ERR_PARAM;
            command->object.listener.local_port = atoi(argv[optind++]);
            if (argc != optind) {
              snprintf(command->object.listener.interface_name, INTERFACE_LEN,
                       "%s", argv[optind++]);
            }
          }
          break;

        case ACTION_LIST:
          if (argc - optind != 0) {
            usage_listener_list(argv[0], true, false);
            goto ERR_PARAM;
          }
          break;

        default:
          goto ERR_COMMAND;
      }
      break;

    case OBJECT_CONNECTION:
      switch (command->action) {
        case ACTION_CREATE:
          /* NAME TYPE LOCAL_ADDRESS LOCAL_PORT REMOTE_ADDRESS REMOTE_PORT */
          if ((argc - optind != 6) && (argc - optind != 7)) {
            usage_connection_create(argv[0], true, false);
            goto ERR_PARAM;
          }
          snprintf(command->object.connection.name, SYMBOLIC_NAME_LEN, "%s",
                   argv[optind++]);
          command->object.connection.type = face_type_from_str(argv[optind++]);
          if (command->object.connection.type == FACE_TYPE_UNDEFINED)
            goto ERR_PARAM;
          command->object.connection.family =
              ip_address_get_family(argv[optind]);
          if (!IS_VALID_FAMILY(command->object.connection.family))
            goto ERR_PARAM;
          if (ip_address_pton(argv[optind++],
                              &command->object.connection.local_addr) < 0)
            goto ERR_PARAM;
          command->object.connection.local_port = atoi(argv[optind++]);
          family = ip_address_get_family(argv[optind]);
          if (!IS_VALID_FAMILY(family) ||
              (command->object.connection.family != family))
            goto ERR_PARAM;
          if (ip_address_pton(argv[optind++],
                              &command->object.connection.remote_addr) < 0)
            goto ERR_PARAM;
          command->object.connection.remote_port = atoi(argv[optind++]);

          break;

        case ACTION_DELETE:
          if ((argc - optind != 1) && (argc - optind != 5) &&
              (argc - optind != 6)) {
            usage_connection_delete(argv[0], true, false);
            goto ERR_PARAM;
          }

          if (argc - optind == 1) {
            /* Id or name */
            if (is_number(argv[optind], SYMBOLIC_NAME_LEN)) {
              command->object.connection.id = atoi(argv[optind++]);
              snprintf(command->object.connection.name, SYMBOLIC_NAME_LEN, "%s",
                       argv[optind++]);
            } else if (is_symbolic_name(argv[optind], SYMBOLIC_NAME_LEN)) {
              snprintf(command->object.connection.name, SYMBOLIC_NAME_LEN, "%s",
                       argv[optind++]);
            } else {
              fprintf(stderr, "Invalid argument\n");
              goto ERR_PARAM;
            }
          } else {
            command->object.connection.type =
                face_type_from_str(argv[optind++]);
            if (command->object.connection.type == FACE_TYPE_UNDEFINED)
              goto ERR_PARAM;
            command->object.connection.family =
                ip_address_get_family(argv[optind]);
            if (!IS_VALID_FAMILY(command->object.connection.family))
              goto ERR_PARAM;
            if (ip_address_pton(argv[optind++],
                                &command->object.connection.local_addr) < 0)
              goto ERR_PARAM;
            command->object.connection.local_port = atoi(argv[optind++]);
            family = ip_address_get_family(argv[optind]);
            if (!IS_VALID_FAMILY(family) ||
                (command->object.connection.family != family))
              goto ERR_PARAM;
            if (ip_address_pton(argv[optind++],
                                &command->object.connection.remote_addr) < 0)
              goto ERR_PARAM;
            command->object.connection.remote_port = atoi(argv[optind++]);
          }
          break;

        case ACTION_LIST:
          if (argc - optind != 0) {
            usage_connection_list(argv[0], true, false);
            goto ERR_PARAM;
          }
          break;

        default:
          goto ERR_COMMAND;
      }
      break;

    default:
      goto ERR_COMMAND;
  }

  return 0;

ERR_PARAM:
ERR_COMMAND:
  return -1;
}

int main(int argc, char *argv[]) {
  hc_data_t *data;
  int rc = 1;
  hc_command_t command = {0};
  char buf_listener[MAXSZ_HC_LISTENER];
  char buf_connection[MAXSZ_HC_CONNECTION];
  char buf_route[MAXSZ_HC_ROUTE];
  char buf_strategy[MAXSZ_HC_STRATEGY];

  forwarder_type_t forwarder = HICNLIGHT;

  if (parse_options(argc, argv, &command, &forwarder) < 0)
    die(OPTIONS, "Bad arguments");

  hc_sock_t *s = hc_sock_create_forwarder(forwarder);
  if (!s) die(SOCKET, "Error creating socket.");

  if (hc_sock_connect(s) < 0)
    die(CONNECT, "Error connecting to the forwarder.");

  switch (command.object.type) {
    case OBJECT_FACE:
      switch (command.action) {
        case ACTION_CREATE:
          if (hc_face_create(s, &command.object.face) < 0)
            die(COMMAND, "Error creating face");
          printf("OK\n");
          break;

        case ACTION_DELETE:
          if (hc_face_delete(s, &command.object.face, 1) < 0)
            die(COMMAND, "Error creating face");
          printf("OK\n");
          break;

        case ACTION_LIST:
          if (hc_face_list(s, &data) < 0)
            die(COMMAND, "Error getting connections.");

          printf("Faces:\n");
          foreach_face(f, data) {
            if (hc_face_snprintf(buf_connection, MAXSZ_HC_FACE, f) >=
                MAXSZ_HC_FACE)
              die(COMMAND, "Display error");
            printf("[%s] %s\n", f->name, buf_connection);
          }

          hc_data_free(data);
          break;
        default:
          die(COMMAND, "Unsupported command for connection");
          break;
      }
      break;

    case OBJECT_ROUTE:
      switch (command.action) {
        case ACTION_CREATE:
          if (hc_route_create(s, &command.object.route) < 0)
            die(COMMAND, "Error creating route");
          printf("OK\n");
          break;

        case ACTION_DELETE:
          if (hc_route_delete(s, &command.object.route) < 0)
            die(COMMAND, "Error creating route");
          printf("OK\n");
          break;

        case ACTION_LIST:
          if (hc_route_list(s, &data) < 0)
            die(COMMAND, "Error getting routes.");

          printf("Routes:\n");
          foreach_route(r, data) {
            if (hc_route_snprintf(buf_route, MAXSZ_HC_ROUTE, r) >=
                MAXSZ_HC_ROUTE)
              die(COMMAND, "Display error");
            printf("%s\n", buf_route);
          }

          hc_data_free(data);
          break;
        default:
          die(COMMAND, "Unsupported command for route");
          break;
      }
      break;

    case OBJECT_STRATEGY:
      switch (command.action) {
        case ACTION_LIST:
          if (hc_strategy_list(s, &data) < 0)
            die(COMMAND, "Error getting routes.");

          printf("Forwarding strategies:\n");
          foreach_strategy(st, data) {
            if (hc_strategy_snprintf(buf_strategy, MAXSZ_HC_STRATEGY, st) >=
                MAXSZ_HC_STRATEGY)
              die(COMMAND, "Display error");
            printf("%s\n", buf_strategy);
          }

          hc_data_free(data);
          break;
        default:
          die(COMMAND, "Unsupported command for strategy");
          break;
      }
      break;

    case OBJECT_LISTENER:
      switch (command.action) {
        case ACTION_CREATE:
          if (hc_listener_create(s, &command.object.listener) < 0)
            die(COMMAND, "Error creating listener");
          printf("OK\n");
          break;
        case ACTION_DELETE:
          if (hc_listener_delete(s, &command.object.listener) < 0)
            die(COMMAND, "Error deleting listener");
          printf("OK\n");
          break;
        case ACTION_LIST:
          if (hc_listener_list(s, &data) < 0)
            die(COMMAND, "Error getting listeners.");

          printf("Listeners:\n");
          foreach_listener(l, data) {
            if (hc_listener_snprintf(buf_listener, MAXSZ_HC_LISTENER + 17, l) >=
                MAXSZ_HC_LISTENER)
              die(COMMAND, "Display error");
            printf("[%d] %s\n", l->id, buf_listener);
          }

          hc_data_free(data);
          break;
        default:
          die(COMMAND, "Unsupported command for listener");
          break;
      }
      break;

    case OBJECT_CONNECTION:
      switch (command.action) {
        case ACTION_CREATE:
          if (hc_connection_create(s, &command.object.connection) < 0)
            die(COMMAND, "Error creating connection");
          printf("OK\n");
          break;
        case ACTION_DELETE:
          if (hc_connection_delete(s, &command.object.connection) < 0)
            die(COMMAND, "Error creating connection");
          printf("OK\n");
          break;
        case ACTION_LIST:
          if (hc_connection_list(s, &data) < 0)
            die(COMMAND, "Error getting connections.");

          printf("Connections:\n");
          foreach_connection(c, data) {
            if (hc_connection_snprintf(buf_connection, MAXSZ_HC_CONNECTION,
                                       c) >= MAXSZ_HC_CONNECTION)
              die(COMMAND, "Display error");
            printf("[%s] %s\n", c->name, buf_connection);
          }

          hc_data_free(data);
          break;
        default:
          die(COMMAND, "Unsupported command for connection");
          break;
      }
      break;

    default:
      die(COMMAND, "Unsupported object");
      break;
  }

ERR_COMMAND:
ERR_CONNECT:
  hc_sock_free(s);
ERR_SOCKET:
ERR_OPTIONS:
  return (rc < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
