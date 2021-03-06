#include "common.h"
#include <stdlib.h>
#include <string.h>

struct lsdn_settings *settings_from_env(struct lsdn_context *ctx) {
	const char *nettype = getenv("LSCTL_NETTYPE");
	if (!nettype) {
		fprintf(stderr, "no LSCTL_NETTYPE\n");
		abort();
	} else if (!strcmp(nettype, "vlan")) {
		return lsdn_settings_new_vlan(ctx);
	} else if (!strcmp(nettype, "vxlan/e2e")) {
		return lsdn_settings_new_vxlan_e2e(ctx, 0);
	} else if (!strcmp(nettype, "vxlan/static")) {
		return lsdn_settings_new_vxlan_static(ctx, 0);
	} else if (!strcmp(nettype, "vxlan/mcast")) {
		return lsdn_settings_new_vxlan_mcast(ctx, LSDN_MK_IPV4(239,239,239,239), 0);
	} else if (!strcmp(nettype, "direct")) {
		return lsdn_settings_new_direct(ctx);
	} else {
		fprintf(stderr, "Unknown nettype: %s\n", nettype);
		abort();
	}

}
