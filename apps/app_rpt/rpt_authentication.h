/*!
 * \file rpt_authentication.h
 *
 * \brief Authentication bridge for gr-linux-crypto
 *
 * This file provides authentication functionality for app_rpt
 * using the gr-linux-crypto library for cryptographic operations.
 */

#ifndef _RPT_AUTHENTICATION_H
#define _RPT_AUTHENTICATION_H

#include "asterisk.h"

#ifdef HAVE_GRLINUXCRYPTO

/* Authentication modes */
enum auth_mode {
	AUTH_DISABLED = 0,	/*!< Authentication disabled */
	AUTH_OPTIONAL,		/*!< Authentication optional (allow unsigned) */
	AUTH_MANDATORY		/*!< Authentication mandatory (require signature) */
};

/* Signed command structure */
struct signed_command {
	uint32_t node_id;		/*!< Node ID of the sender */
	uint32_t command;		/*!< Command number */
	time_t timestamp;		/*!< Command timestamp */
	unsigned char signature[128];	/*!< ECDSA signature */
	int has_signature;		/*!< Whether signature is present */
	char param[500];		/*!< Command parameters */
};

/* Authentication configuration */
struct rpt_auth_config {
	enum auth_mode mode;		/*!< Authentication mode */
	int max_clock_skew;		/*!< Maximum allowed clock skew (seconds) */
	char signature_curve[32];	/*!< Signature curve name */
};

/*!
 * \brief Load authentication configuration from rpt.conf
 * \param cfg Authentication configuration structure to populate
 * \param config Asterisk config structure
 * \retval 0 on success
 * \retval -1 on error
 */
int rpt_auth_load_config(struct rpt_auth_config *cfg, struct ast_config *config);

/*!
 * \brief Verify a signed command
 * \param cfg Authentication configuration
 * \param cmd Signed command to verify
 * \param remote_node_id Remote node ID (for logging)
 * \retval 0 on successful verification
 * \retval -1 on verification failure
 */
int rpt_auth_verify_command(struct rpt_auth_config *cfg,
			    struct signed_command *cmd,
			    uint32_t remote_node_id);

/*!
 * \brief Parse COP command into signed_command structure
 * \param cmd Signed command structure to populate
 * \param command_num Command number
 * \param param Command parameters
 * \retval 0 on success
 * \retval -1 on error
 */
int parse_cop_command(struct signed_command *cmd, int command_num, char *param);

#else /* !HAVE_GRLINUXCRYPTO */

/* Stub structures when authentication is disabled */
struct rpt_auth_config {
	int mode;	/*!< Always AUTH_DISABLED when gr-linux-crypto not available */
};

/* Stub functions return success when authentication is not available */
static inline int rpt_auth_load_config(struct rpt_auth_config *cfg, struct ast_config *config)
{
	(void)cfg;
	(void)config;
	return 0;
}

static inline int rpt_auth_verify_command(struct rpt_auth_config *cfg,
					  void *cmd,
					  uint32_t remote_node_id)
{
	(void)cfg;
	(void)cmd;
	(void)remote_node_id;
	return 0; /* Always succeed when auth not available */
}

static inline int parse_cop_command(void *cmd, int command_num, char *param)
{
	(void)cmd;
	(void)command_num;
	(void)param;
	return 0; /* Stub function when auth not available */
}

#endif /* HAVE_GRLINUXCRYPTO */

#endif /* _RPT_AUTHENTICATION_H */

