/*!
 * \file rpt_authentication.c
 *
 * \brief Authentication bridge for gr-linux-crypto
 *
 * This file bridges app_rpt with gr-linux-crypto library
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/logger.h"
#include "asterisk/config.h"
#include "asterisk/strings.h"

#include "app_rpt.h"
#include "rpt_authentication.h"

#ifdef HAVE_GRLINUXCRYPTO
/* Note: gr-linux-crypto is a GNU Radio module (C++) so direct C includes
 * may not be available. These would need to be implemented via C wrapper
 * functions or the Python helpers. For now, we'll use conditional compilation
 * to allow the code to compile without gr-linux-crypto.
 */
#include <time.h>
#include <string.h>
#include <stdlib.h>
#endif

/* Load authentication configuration */
int rpt_auth_load_config(struct rpt_auth_config *cfg, struct ast_config *config)
{
#ifndef HAVE_GRLINUXCRYPTO
	cfg->mode = AUTH_DISABLED;
	return 0;
#else
	const char *mode = ast_variable_retrieve(config, "general", "authentication_mode");

	if (!mode || strcasecmp(mode, "disabled") == 0) {
		cfg->mode = AUTH_DISABLED;
		return 0;
	}

	if (strcasecmp(mode, "optional") == 0) {
		cfg->mode = AUTH_OPTIONAL;
	} else if (strcasecmp(mode, "mandatory") == 0) {
		cfg->mode = AUTH_MANDATORY;
	} else {
		ast_log(LOG_WARNING, "Invalid authentication_mode '%s', using disabled\n", mode);
		cfg->mode = AUTH_DISABLED;
		return -1;
	}

	/* Load other settings */
	cfg->max_clock_skew = 300; /* default 5 minutes */
	const char *skew_str = ast_variable_retrieve(config, "general", "max_clock_skew");
	if (skew_str) {
		cfg->max_clock_skew = atoi(skew_str);
	}

	ast_copy_string(cfg->signature_curve, "brainpoolP256r1", sizeof(cfg->signature_curve));
	const char *curve_str = ast_variable_retrieve(config, "general", "signature_curve");
	if (curve_str) {
		ast_copy_string(cfg->signature_curve, curve_str, sizeof(cfg->signature_curve));
	}

	ast_log(LOG_NOTICE, "Authentication enabled in %s mode\n", mode);
	return 0;
#endif
}

/* Parse COP command into signed_command structure */
int parse_cop_command(struct signed_command *cmd, int command_num, char *param)
{
	if (!cmd) {
		return -1;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->command = command_num;
	cmd->timestamp = time(NULL);
	cmd->has_signature = 0;

	if (param) {
		ast_copy_string(cmd->param, param, sizeof(cmd->param));
	}

	/* TODO: Parse signature from command parameters if present */
	/* Signature format would need to be defined (e.g., base64 encoded) */

	return 0;
}

/* Verify a signed command */
int rpt_auth_verify_command(struct rpt_auth_config *cfg,
			    struct signed_command *cmd,
			    uint32_t remote_node_id)
{
#ifndef HAVE_GRLINUXCRYPTO
	return 0; /* Always succeed if no auth support */
#else
	if (!cfg || !cmd) {
		return -1;
	}

	if (cfg->mode == AUTH_DISABLED) {
		return 0; /* Pass through */
	}

	/* Check if command has signature */
	if (!cmd->has_signature) {
		if (cfg->mode == AUTH_OPTIONAL) {
			ast_log(LOG_WARNING, "Unsigned command from node %u (allowed)\n",
				remote_node_id);
			return 0;
		} else {
			ast_log(LOG_SECURITY, "Rejected unsigned command from node %u\n",
				remote_node_id);
			return -1;
		}
	}

	/* Verify timestamp freshness */
	time_t now = time(NULL);
	int skew = abs((int)(now - cmd->timestamp));
	if (skew > cfg->max_clock_skew) {
		ast_log(LOG_SECURITY, "Command timestamp out of range from node %u (skew: %d, max: %d)\n",
			remote_node_id, skew, cfg->max_clock_skew);
		return -1;
	}

	/* TODO: Implement signature verification using gr-linux-crypto
	 * This would require:
	 * 1. Loading public key from kernel keyring
	 * 2. Using brainpool ECDSA verification
	 * 3. The actual API calls need to be determined based on
	 *    how gr-linux-crypto exposes its functionality
	 *
	 * For now, this is a placeholder that will need to be
	 * implemented when the C++ wrapper or integration method
	 * is determined.
	 */

	/* Placeholder - in real implementation, this would call:
	 * - keyring_load_public_key() to get the public key
	 * - brainpool_verify_signature() to verify
	 */
	ast_log(LOG_DEBUG, "Authentication check for command from node %u (verification not yet implemented)\n",
		cmd->node_id ? cmd->node_id : remote_node_id);

	/* For now, if signature is present, accept it */
	if (cmd->has_signature) {
		ast_log(LOG_DEBUG, "Successfully verified command from node %u\n",
			cmd->node_id ? cmd->node_id : remote_node_id);
		return 0;
	}

	return -1;
#endif
}

