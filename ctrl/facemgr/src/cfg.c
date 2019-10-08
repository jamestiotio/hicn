/**
 * \file cfg.c
 * \brief Implementation of Face manager configuration
 */

#include <assert.h>
#include <hicn/ctrl.h> // HICN_DEFAULT_PORT
#include <hicn/facemgr/cfg.h>
#include <hicn/policy.h>
#include <hicn/util/ip_address.h>
#include "util/set.h"

/* Overlay */

typedef struct {
    bool is_local_port;
    uint16_t local_port;
    bool is_local_addr;
    ip_address_t local_addr;
    bool is_remote_port;
    uint16_t remote_port;
    bool is_remote_addr;
    ip_address_t remote_addr;
} facemgr_cfg_overlay_t;

int
facemgr_cfg_overlay_initialize(facemgr_cfg_overlay_t * overlay)
{
    overlay->is_local_port = false;
    overlay->local_port = 0;
    overlay->is_local_addr = false;
    overlay->local_addr = IP_ADDRESS_EMPTY;

    overlay->is_remote_port = false;
    overlay->remote_port = 0;
    overlay->is_remote_addr = false;
    overlay->remote_addr = IP_ADDRESS_EMPTY;

    return 0;
}

int
facemgr_cfg_overlay_finalize(facemgr_cfg_overlay_t * overlay)
{
    return 0;
}

facemgr_cfg_overlay_t * facemgr_cfg_overlay_create()
{
    facemgr_cfg_overlay_t * overlay = malloc(sizeof(facemgr_cfg_overlay_t));
    if (!overlay)
        return NULL;

    int rc = facemgr_cfg_overlay_initialize(overlay);
    if (rc < 0) {
        free(overlay);
        return NULL;
    }

    return overlay;
}

void facemgr_cfg_overlay_free(facemgr_cfg_overlay_t * overlay)
{
    facemgr_cfg_overlay_finalize(overlay);
    free(overlay);
}

typedef struct {
    facemgr_cfg_overlay_t * v4;
    facemgr_cfg_overlay_t * v6;
} facemgr_cfg_overlays_t;

typedef struct {
    const char * interface_name;
    netdevice_type_t interface_type;
} facemgr_cfg_match_t;


typedef struct {
    /* Interface specific */
    bool is_face_type; // default is auto
    facemgr_face_type_t face_type;

    /* This should be defaut for the global settings */
    bool is_ignore;
    bool ignore;
    bool is_discovery;
    bool discovery;
    bool is_ipv4;
    bool ipv4;
    bool is_ipv6;
    bool ipv6;

    facemgr_cfg_overlays_t overlays; // fallback unless discovery is disabled
} facemgr_cfg_override_t;

struct facemgr_cfg_rule_s {
    facemgr_cfg_match_t match;
    facemgr_cfg_override_t override;
};

int
facemgr_cfg_override_initialize(facemgr_cfg_override_t * override)
{
    override->is_face_type = false;
    override->face_type = FACEMGR_FACE_TYPE_UNDEFINED;

    override->is_ignore = false;
    override->ignore = false;

    override->is_discovery = false;
    override->discovery = false;

    override->overlays.v4 = NULL;
    override->overlays.v6 = NULL;

    return 0;
}

int
facemgr_cfg_override_finalize(facemgr_cfg_override_t * override)
{
    if (override->overlays.v4) {
        facemgr_cfg_overlay_free(override->overlays.v4);
        override->overlays.v4 = NULL;
    }
    if (override->overlays.v6) {
        facemgr_cfg_overlay_free(override->overlays.v6);
        override->overlays.v6 = NULL;
    }

    return 0;
}


/* Rule */
facemgr_cfg_rule_t * facemgr_cfg_rule_create()
{
    facemgr_cfg_rule_t * rule = malloc(sizeof(facemgr_cfg_rule_t));
    if (!rule)
        return NULL;

    int rc = facemgr_cfg_rule_initialize(rule);
    if (rc < 0)
        return NULL;

    return rule;
}

void facemgr_cfg_rule_free(facemgr_cfg_rule_t * rule)
{
    facemgr_cfg_rule_finalize(rule);
    free(rule);
}

int
facemgr_cfg_rule_initialize(facemgr_cfg_rule_t * rule)
{
    rule->match.interface_name = NULL;
    rule->match.interface_type = NETDEVICE_TYPE_UNDEFINED;

    int rc = facemgr_cfg_override_initialize(&rule->override);
    if (rc < 0)
        return -1;

    return 0;
}

int
facemgr_cfg_rule_finalize(facemgr_cfg_rule_t * rule)
{
    if (rule->match.interface_name) {
        free((void*)rule->match.interface_name);
        rule->match.interface_name = NULL;
    }
    return facemgr_cfg_override_finalize(&rule->override);
}

void
facemgr_cfg_rule_dump(facemgr_cfg_rule_t * rule)
{
    DEBUG("  <rule>");
    DEBUG("    <match interface_name=%s interface_type=%s>",
            rule->match.interface_name,
            netdevice_type_str[rule->match.interface_type]);
    DEBUG("    <override>");
    if (rule->override.is_face_type) {
        DEBUG("      <face_type>%d</face_type>", rule->override.face_type);
    }
    if (rule->override.is_ignore) {
        DEBUG("      <ignore>%d</ignore>", rule->override.ignore);
    }
    if (rule->override.is_discovery) {
        DEBUG("      <discovery>%d</discovery>", rule->override.discovery);
    }
    if (rule->override.is_ipv4) {
        DEBUG("      <ipv4>%d</ipv4>", rule->override.ipv4);
    }
    if (rule->override.is_ipv6) {
        DEBUG("      <ipv6>%d</ipv6>", rule->override.ipv6);
    }
    DEBUG("      <overlays>");
    if (rule->override.overlays.v4) {
        DEBUG("        <ipv4>");
        if (rule->override.overlays.v4->is_local_addr) {
            char buf[MAXSZ_IP_ADDRESS];
            ip_address_snprintf(buf, MAXSZ_IP_ADDRESS,
                    &rule->override.overlays.v4->local_addr, AF_INET);
            DEBUG("          <local_addr>%s</local_addr>", buf);
        }
        if (rule->override.overlays.v4->is_local_port) {
            DEBUG("          <local_port>%d</local_port>",
                    rule->override.overlays.v4->local_port);
        }
        if (rule->override.overlays.v4->is_remote_addr) {
            char buf[MAXSZ_IP_ADDRESS];
            ip_address_snprintf(buf, MAXSZ_IP_ADDRESS,
                    &rule->override.overlays.v4->remote_addr, AF_INET);
            DEBUG("          <remote_addr>%s</remote_addr>", buf);
        }
        if (rule->override.overlays.v4->is_remote_port) {
            DEBUG("          <remote_port>%d</remote_port>",
                    rule->override.overlays.v4->remote_port);
        }
        DEBUG("        </ipv4>");
    }
    if (rule->override.overlays.v6) {
        DEBUG("        <ipv6>");
        if (rule->override.overlays.v6->is_local_addr) {
            char buf[MAXSZ_IP_ADDRESS];
            ip_address_snprintf(buf, MAXSZ_IP_ADDRESS,
                    &rule->override.overlays.v6->local_addr, AF_INET6);
            DEBUG("          <local_addr>%s</local_addr>", buf);
        }
        if (rule->override.overlays.v6->is_local_port) {
            DEBUG("          <local_port>%d</local_port>",
                    rule->override.overlays.v6->local_port);
        }
        if (rule->override.overlays.v6->is_remote_addr) {
            char buf[MAXSZ_IP_ADDRESS];
            ip_address_snprintf(buf, MAXSZ_IP_ADDRESS,
                    &rule->override.overlays.v6->remote_addr, AF_INET6);
            DEBUG("          <remote_addr>%s</remote_addr>", buf);
        }
        if (rule->override.overlays.v6->is_remote_port) {
            DEBUG("          <remote_port>%d</remote_port>",
                    rule->override.overlays.v6->remote_port);
        }
        DEBUG("        </ipv6>");
    }
    DEBUG("      </overlays>");
    DEBUG("    </override>");
    DEBUG("  </rule>");
}

int
facemgr_cfg_rule_set_match(facemgr_cfg_rule_t * rule, const char * interface_name,
        netdevice_type_t interface_type)
{
    rule->match.interface_name = interface_name ? strdup(interface_name) : NULL;
    rule->match.interface_type = interface_type;
    return 0;
}

int
facemgr_cfg_rule_set_face_type(facemgr_cfg_rule_t * rule, facemgr_face_type_t * face_type)
{
    rule->override.is_face_type = true;
    rule->override.face_type = *face_type;
    return 0;
}

int
facemgr_cfg_rule_unset_face_type(facemgr_cfg_rule_t * rule)
{
    rule->override.is_face_type = false;
    rule->override.face_type = FACEMGR_FACE_TYPE_UNDEFINED; /* optional */
    return 0;
}

int
facemgr_cfg_rule_set_discovery(facemgr_cfg_rule_t * rule, bool status)
{
    rule->override.is_discovery = true;
    rule->override.discovery = status;
    return 0;
}

int
facemgr_cfg_rule_unset_discovery(facemgr_cfg_rule_t * rule)
{
    rule->override.is_discovery = false;
    return 0;
}

int
facemgr_cfg_rule_set_ignore(facemgr_cfg_rule_t * rule, bool status)
{
    rule->override.is_ignore = true;
    rule->override.ignore = status;
    return 0;
}

int
facemgr_cfg_rule_unset_ignore(facemgr_cfg_rule_t * rule)
{
    rule->override.is_ignore = false;
    return 0;
}

int
facemgr_cfg_rule_set_ipv4(facemgr_cfg_rule_t * rule, bool status)
{
    rule->override.is_ipv4 = true;
    rule->override.ipv4 = status;
    return 0;
}

int
facemgr_cfg_rule_unset_ipv4(facemgr_cfg_rule_t * rule)
{
    rule->override.is_ipv4 = false;
    return 0;
}

int
facemgr_cfg_rule_set_ipv6(facemgr_cfg_rule_t * rule, bool status)
{
    rule->override.is_ipv6 = true;
    rule->override.ipv6 = status;
    return 0;
}

int
facemgr_cfg_rule_unset_ipv6(facemgr_cfg_rule_t * rule)
{
    rule->override.is_ipv6 = false;
    return 0;
}

int
facemgr_cfg_rule_set_overlay(facemgr_cfg_rule_t * rule, int family,
    ip_address_t * local_addr, uint16_t local_port,
    ip_address_t * remote_addr, uint16_t remote_port) {
    if ((family != AF_INET) && (family != AF_INET6))
        return -1;

    facemgr_cfg_overlay_t * overlay = facemgr_cfg_overlay_create();
    if (local_addr) {
        overlay->is_local_addr = true;
        overlay->local_addr = *local_addr;
    }
    if (IS_VALID_PORT(local_port)) {
        overlay->is_local_port = true;
        overlay->local_port = local_port;
    }
    if (remote_addr) {
        overlay->is_remote_addr = true;
        overlay->remote_addr = *remote_addr;
    }
    if (IS_VALID_PORT(remote_port)) {
        overlay->is_remote_port = true;
        overlay->remote_port = remote_port;
    }

    switch(family) {
        case AF_INET:
            rule->override.overlays.v4 = overlay;
            break;

        case AF_INET6:
            rule->override.overlays.v6 = overlay;
            break;

        default:
            return -1;
    }

    return 0;
}

int
facemgr_rule_unset_overlay(facemgr_cfg_rule_t * rule, int family)
{
    if ((family != AF_INET) && (family != AF_INET6) && (family != AF_UNSPEC))
        return -1;

    if ((family == AF_UNSPEC) || (family == AF_INET)) {
        if (rule->override.overlays.v4) {
            facemgr_cfg_overlay_free(rule->override.overlays.v4);
            rule->override.overlays.v4 = NULL;
        }
    }
    if ((family == AF_UNSPEC) || (family == AF_INET6)) {
        if (rule->override.overlays.v6) {
            facemgr_cfg_overlay_free(rule->override.overlays.v6);
            rule->override.overlays.v6 = NULL;
        }
    }
    return 0;
}

int
facemgr_cfg_rule_cmp(const facemgr_cfg_rule_t * r1, const facemgr_cfg_rule_t * r2)
{
    /*
     * We implement a lexicographic order on the tuple (interface_name,
     * interface_type)
     */

    /* We need to handle NULL cases out of strcmp */
    if (!r1->match.interface_name) {
        if (r2->match.interface_name)
            return 1;
        else
            goto BOTH_NULL;
    } else {
        if (!r2->match.interface_name)
            return -1;
    }


    /* Only if both are non-NULL, we proceed to strcmp */
    int rc = strcmp(r1->match.interface_name, r2->match.interface_name);
    if (rc != 0)
        return rc;

BOTH_NULL:
    return r1->match.interface_type - r2->match.interface_type;
}

/* General */

TYPEDEF_SET_H(facemgr_cfg_rule_set, facemgr_cfg_rule_t *);
TYPEDEF_SET(facemgr_cfg_rule_set, facemgr_cfg_rule_t *, facemgr_cfg_rule_cmp, generic_snprintf);

struct facemgr_cfg_s {
    facemgr_cfg_override_t global;
    facemgr_cfg_rule_set_t * rule_set;
    //log_cfg_t log;
};

facemgr_cfg_t * facemgr_cfg_create()
{
    facemgr_cfg_t * cfg = malloc(sizeof(facemgr_cfg_t));
    if (!cfg)
        return NULL;

    int rc = facemgr_cfg_initialize(cfg);
    if (rc < 0) {
        free(cfg);
        return NULL;
    }

    return cfg;
}

void facemgr_cfg_free(facemgr_cfg_t * cfg)
{
    facemgr_cfg_finalize(cfg);
    free(cfg);
}

int
facemgr_cfg_initialize(facemgr_cfg_t * cfg)
{
    int rc = facemgr_cfg_override_initialize(&cfg->global);
    if (rc < 0)
        goto ERR_OVERRIDE;

    cfg->rule_set = facemgr_cfg_rule_set_create();
    if (!cfg->rule_set)
        goto ERR_RULE_SET;

    return 0;

ERR_RULE_SET:
    facemgr_cfg_override_finalize(&cfg->global);
ERR_OVERRIDE:
    return -1;
}

int
facemgr_cfg_finalize(facemgr_cfg_t * cfg)
{
    /* TODO Free all rules */
    facemgr_cfg_rule_set_free(cfg->rule_set);
    return facemgr_cfg_override_finalize(&cfg->global);
}

void facemgr_cfg_dump(facemgr_cfg_t * cfg)
{
    return; /* NOT IMPLEMENTED */
}

/* General */
int
facemgr_cfg_set_face_type(facemgr_cfg_t * cfg, facemgr_face_type_t * face_type)
{
    cfg->global.is_face_type = true;
    cfg->global.face_type = *face_type;
    return 0;
}

int
facemgr_cfg_unset_face_type(facemgr_cfg_t * cfg)
{
    cfg->global.is_face_type = false;
    cfg->global.face_type = FACEMGR_FACE_TYPE_UNDEFINED; /* optional */
    return 0;
}

int
facemgr_cfg_set_discovery(facemgr_cfg_t * cfg, bool status)
{
    cfg->global.is_discovery = true;
    cfg->global.discovery = status;
    return 0;
}

int
facemgr_cfg_unset_discovery(facemgr_cfg_t * cfg)
{
    cfg->global.is_discovery = false;
    return 0;
}

int
facemgr_cfg_set_overlay(facemgr_cfg_t * cfg, int family,
    ip_address_t * local_addr, uint16_t local_port,
    ip_address_t * remote_addr, uint16_t remote_port)
{
    if ((family != AF_INET) && (family != AF_INET6))
        return -1;

    facemgr_cfg_overlay_t * overlay = facemgr_cfg_overlay_create();
    if (local_addr) {
        overlay->is_local_addr = true;
        overlay->local_addr = *local_addr;
    }
    if (IS_VALID_PORT(local_port)) {
        overlay->is_local_port = true;
        overlay->local_port = local_port;
    }
    if (remote_addr) {
        overlay->is_remote_addr = true;
        overlay->remote_addr = *remote_addr;
    }
    if (IS_VALID_PORT(remote_port)) {
        overlay->is_remote_port = true;
        overlay->remote_port = remote_port;
    }

    DEBUG("facemgr_cfg_set_overlay");

    switch(family) {
        case AF_INET:
            cfg->global.overlays.v4 = overlay;
            break;

        case AF_INET6:
            cfg->global.overlays.v6 = overlay;
            break;

        default:
            return -1;
    }

    DEBUG("<global>");
    DEBUG("  <overlay>");
    if (overlay) {
        DEBUG("    <ipv4>");
        if (overlay->is_local_addr) {
            char buf[MAXSZ_IP_ADDRESS];
            ip_address_snprintf(buf, MAXSZ_IP_ADDRESS,
                    &overlay->local_addr, AF_INET);
            DEBUG("      <local_addr>%s</local_addr>", buf);
        }
        if (overlay->is_local_port) {
            DEBUG("      <local_port>%d</local_port>",
                    overlay->local_port);
        }
        if (overlay->is_remote_addr) {
            char buf[MAXSZ_IP_ADDRESS];
            ip_address_snprintf(buf, MAXSZ_IP_ADDRESS,
                    &overlay->remote_addr, AF_INET);
            DEBUG("      <remote_addr>%s</remote_addr>", buf);
        }
        if (overlay->is_remote_port) {
            DEBUG("      <remote_port>%d</remote_port>",
                    overlay->remote_port);
        }
        DEBUG("    </ipv4>");
    }
    DEBUG("  </overlay>");
    DEBUG("</global>");

    return 0;
}

int
facemgr_cfg_unset_overlay(facemgr_cfg_t * cfg, int family)
{
    if ((family != AF_INET) && (family != AF_INET6) && (family != AF_UNSPEC))
        return -1;

    if ((family == AF_UNSPEC) || (family == AF_INET)) {
        if (cfg->global.overlays.v4) {
            facemgr_cfg_overlay_free(cfg->global.overlays.v4);
            cfg->global.overlays.v4 = NULL;
        }
    }
    if ((family == AF_UNSPEC) || (family == AF_INET6)) {
        if (cfg->global.overlays.v6) {
            facemgr_cfg_overlay_free(cfg->global.overlays.v6);
            cfg->global.overlays.v6 = NULL;
        }
    }
    return 0;
}

int
facemgr_cfg_add_rule(facemgr_cfg_t * cfg, facemgr_cfg_rule_t * rule)
{
    facemgr_cfg_rule_dump(rule);
    return facemgr_cfg_rule_set_add(cfg->rule_set, rule);
}

int
facemgr_cfg_del_rule(facemgr_cfg_t * cfg, facemgr_cfg_rule_t * rule)
{
    return facemgr_cfg_rule_set_remove(cfg->rule_set, rule, NULL);
}

int facemgr_cfg_get_rule(const facemgr_cfg_t * cfg, const char * interface_name,
        netdevice_type_t interface_type, facemgr_cfg_rule_t ** rule) {
    facemgr_cfg_rule_t rule_search = {
        .match = {
            .interface_name = interface_name,
            .interface_type = interface_type,
        },
    };
    return facemgr_cfg_rule_set_get(cfg->rule_set, &rule_search, rule);
}

/* Query API */

/*
 * Check whether there are override rules for the given netdevice
 *
 * TODO:
 *  - until we have proper indexes we loop through the whole structure
 */
int
facemgr_cfg_get_override(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        facemgr_cfg_override_t ** override)
{
    if (!netdevice) {
        *override = NULL;
        return 0;
    }

    facemgr_cfg_rule_t **rule_array;
    int rc = facemgr_cfg_rule_set_get_array(cfg->rule_set, &rule_array);
    if (rc < 0) {
        ERROR("facemgr_cfg_rule_set_get_array failed");
        return rc;
    }
    for (unsigned i = 0; i < rc; i++) {
        const char * interface_name = rule_array[i]->match.interface_name;
        /* Check match for interface name */
        if (interface_name && (strcmp(interface_name, netdevice->name) != 0))
            continue;
        /* Check match for interface type */
        if (rule_array[i]->match.interface_type != NETDEVICE_TYPE_UNDEFINED) {
#ifdef __ANDROID__
            if (netdevice_type != rule_array[i]->match.interface_type)
                continue;
#else
            ERROR("Match on interface type is currently not implemented");
            goto ERR_ARRAY;
#endif /* __ANDROID__ */
        }
        /* Found match... do we have an override for face_type */
        DEBUG("override found nd=%s, ndt=%s", rule_array[i]->match.interface_name, netdevice_type_str[rule_array[i]->match.interface_type]);
        *override = &rule_array[i]->override;
        goto FOUND;
    }

    DEBUG("override not found");
    *override = NULL;

FOUND:
    free(rule_array);
    return 0;

#ifndef __ANDROID__
ERR_ARRAY:
    free(rule_array);
    return -1;
#endif /* __ANDROID__ */
}

int
facemgr_cfg_get_face_type(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        facemgr_face_type_t * face_type)
{
    facemgr_cfg_override_t * override;
    int rc = facemgr_cfg_get_override(cfg, netdevice, netdevice_type,
            &override);
    if (rc < 0) {
        ERROR("get override failed");
        return rc;
    }

    if ((override) && (override->is_face_type)) {
        *face_type = override->face_type;
        return 0;
    }

    *face_type = cfg->global.is_face_type
        ? cfg->global.face_type
        : FACEMGR_FACE_TYPE_DEFAULT;

    return 0;
}

int
facemgr_cfg_get_discovery(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        bool * discovery)
{
    facemgr_cfg_override_t * override;
    int rc = facemgr_cfg_get_override(cfg, netdevice, netdevice_type,
            &override);
    if (rc < 0)
        return rc;

    if ((override) && (override->is_discovery)) {
        *discovery = override->discovery;
        return 0;
    }

    *discovery = cfg->global.is_discovery
        ? cfg->global.discovery
        : FACEMGR_CFG_DEFAULT_DISCOVERY;
    return 0;
}

int
facemgr_cfg_get_ipv4(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        bool * ipv4)
{
    facemgr_cfg_override_t * override;
    int rc = facemgr_cfg_get_override(cfg, netdevice, netdevice_type,
            &override);
    if (rc < 0)
        return rc;

    if ((override) && (override->is_ipv4)) {
        *ipv4 = override->ipv4;
        return 0;
    }

    *ipv4 = cfg->global.is_ipv4
        ? cfg->global.ipv4
        : FACEMGR_CFG_DEFAULT_IPV4;
    return 0;
}

int
facemgr_cfg_get_ipv6(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        bool * ipv6)
{
    facemgr_cfg_override_t * override;
    int rc = facemgr_cfg_get_override(cfg, netdevice, netdevice_type,
            &override);
    if (rc < 0)
        return rc;

    if ((override) && (override->is_ipv6)) {
        *ipv6 = override->ipv6;
        return 0;
    }

    *ipv6 = cfg->global.is_ipv6
        ? cfg->global.ipv6
        : FACEMGR_CFG_DEFAULT_IPV6;
    return 0;
}

int
facemgr_cfg_get_ignore(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        bool * ignore)
{
    facemgr_cfg_override_t * override;
    int rc = facemgr_cfg_get_override(cfg, netdevice, netdevice_type,
            &override);
    if (rc < 0)
        return rc;

    if ((override) && (override->is_ignore)) {
        *ignore = override->ignore;
        return 0;
    }

    assert (!cfg->global.is_ignore);

    *ignore = (netdevice && (netdevice->name[0] != '\0') && strcmp(netdevice->name, "lo") == 0);

    return 0;
}

int
facemgr_cfg_get_overlay_local_addr(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        int family, ip_address_t * addr)
{
    facemgr_cfg_override_t * override;
    int rc = facemgr_cfg_get_override(cfg, netdevice, netdevice_type,
            &override);
    if (rc < 0)
        return rc;

    switch (family) {
        case AF_INET:
            if ((override) && (override->overlays.v4) && (override->overlays.v4->is_local_addr)) {
                *addr = override->overlays.v4->local_addr;
                return 0;
            }
            if ((cfg->global.overlays.v4) && (cfg->global.overlays.v4->is_local_addr)) {
                *addr = cfg->global.overlays.v4->local_addr;
                return 0;
            }
            break;
        case AF_INET6:
            if ((override) && (override->overlays.v6) && (override->overlays.v6->is_local_addr)) {
                *addr = override->overlays.v6->local_addr;
                return 0;
            }
            if ((cfg->global.overlays.v6) && (cfg->global.overlays.v6->is_local_addr)) {
                *addr = cfg->global.overlays.v6->local_addr;
                return 0;
            }
            break;
        case AF_UNSPEC:
            break;
        default:
            return -1;
    }

    *addr = IP_ADDRESS_EMPTY;
    return 0;
}

int
facemgr_cfg_get_overlay_local_port(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        int family, u16 * port)
{
    facemgr_cfg_override_t * override;
    int rc = facemgr_cfg_get_override(cfg, netdevice, netdevice_type,
            &override);
    if (rc < 0)
        return rc;

    switch (family) {
        case AF_INET:
            if ((override) && (override->overlays.v4) && (override->overlays.v4->is_local_port)) {
                *port = override->overlays.v4->local_port;
                return 0;
            }
            if ((cfg->global.overlays.v4) && (cfg->global.overlays.v4->is_local_port)) {
                *port = cfg->global.overlays.v4->local_port;
                return 0;
            }
            break;
        case AF_INET6:
            if ((override) && (override->overlays.v6) && (override->overlays.v6->is_local_port)) {
                *port = override->overlays.v6->local_port;
                return 0;
            }
            if ((cfg->global.overlays.v6) && (cfg->global.overlays.v6->is_local_port)) {
                *port = cfg->global.overlays.v6->local_port;
                return 0;
            }
            break;
        case AF_UNSPEC:
            break;
        default:
            return -1;
    }

    *port = HICN_DEFAULT_PORT;
    return 0;
}

int
facemgr_cfg_get_overlay_remote_addr(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        int family, ip_address_t * addr)
{
    facemgr_cfg_override_t * override;
    int rc = facemgr_cfg_get_override(cfg, netdevice, netdevice_type,
            &override);
    if (rc < 0)
        return rc;

    switch (family) {
        case AF_INET:
            if ((override) && (override->overlays.v4) && (override->overlays.v4->is_remote_addr)) {
                DEBUG("remote addr v4 from override");
                *addr = override->overlays.v4->remote_addr;
                return 0;
            }
            if ((cfg->global.overlays.v4) && (cfg->global.overlays.v4->is_remote_addr)) {
                DEBUG("remote addr v4 from global");
                *addr = cfg->global.overlays.v4->remote_addr;
                return 0;
            }
            break;
        case AF_INET6:
            if ((override) && (override->overlays.v6) && (override->overlays.v6->is_remote_addr)) {
                DEBUG("remote addr v6 from override");
                *addr = override->overlays.v6->remote_addr;
                return 0;
            }
            if ((cfg->global.overlays.v6) && (cfg->global.overlays.v6->is_remote_addr)) {
                DEBUG("remote addr v6 from global");
                *addr = cfg->global.overlays.v6->remote_addr;
                return 0;
            }
            break;
        case AF_UNSPEC:
            break;
        default:
            return -1;
    }

    DEBUG("remote addr empty");
    *addr = IP_ADDRESS_EMPTY;
    return 0;
}

int
facemgr_cfg_get_overlay_remote_port(const facemgr_cfg_t * cfg,
        const netdevice_t * netdevice, netdevice_type_t netdevice_type,
        int family, u16 * port)
{
    facemgr_cfg_override_t * override;
    int rc = facemgr_cfg_get_override(cfg, netdevice, netdevice_type,
            &override);
    if (rc < 0)
        return rc;

    switch (family) {
        case AF_INET:
            if ((override) && (override->overlays.v4) && (override->overlays.v4->is_remote_port)) {
                *port = override->overlays.v4->remote_port;
                return 0;
            }
            if ((cfg->global.overlays.v4) && (cfg->global.overlays.v4->is_remote_port)) {
                *port = cfg->global.overlays.v4->remote_port;
                return 0;
            }
            break;
        case AF_INET6:
            if ((override) && (override->overlays.v6) && (override->overlays.v6->is_remote_port)) {
                *port = override->overlays.v6->remote_port;
                return 0;
            }
            if ((cfg->global.overlays.v6) && (cfg->global.overlays.v6->is_remote_port)) {
                *port = cfg->global.overlays.v6->remote_port;
                return 0;
            }
            break;
        case AF_UNSPEC:
            break;
        default:
            return -1;
    }

    *port = HICN_DEFAULT_PORT;
    return 0;
}