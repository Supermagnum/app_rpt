# AllStarLink Authentication with gr-linux-crypto

This document describes how to enable cryptographic authentication for remote repeater management using the gr-linux-crypto library.

## Overview

The authentication system provides cryptographic verification of remote commands using:
- **Brainpool Elliptic Curve Cryptography** (ECDSA) for digital signatures
- **Linux Kernel Keyring** for secure key storage
- **Timestamp verification** to prevent replay attacks

This ensures that only authorized nodes can execute control operator (COP) commands on your repeater system.

## Prerequisites

### Required Components

1. **gr-linux-crypto library** installed and configured
   - Available from: https://github.com/Supermagnum/gr-linux-crypto
   - Submodule included in: `contrib/gr-linux-crypto/`

2. **Linux kernel with keyring support**
   - Most modern Linux kernels include keyring support
   - Verify with: `ls -la /proc/key-users`
   - Install keyutils package: `apt-get install keyutils` (Debian/Ubuntu) or equivalent

3. **OpenSSL 1.0.2+** (for Brainpool curve support)
   - Verify: `openssl version`
   - Required for ECDSA signature operations

4. **GNU Radio Runtime** (for gr-linux-crypto)
   - Install via package manager or from source

### Build Dependencies

When building app_rpt with authentication support:

```bash
# Required development packages
apt-get install libkeyutils-dev libssl-dev gnuradio-dev cmake

# Or on RHEL/CentOS
yum install keyutils-libs-devel openssl-devel gnuradio-devel cmake
```

## Installation

### 1. Build gr-linux-crypto

The gr-linux-crypto submodule should already be present in `contrib/gr-linux-crypto/`.

```bash
cd contrib/gr-linux-crypto
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

### 2. Build app_rpt with Authentication Support

Enable authentication during compilation:

```bash
# In app_rpt root directory
export CFLAGS="-DHAVE_GRLINUXCRYPTO -I$(pwd)/contrib/gr-linux-crypto/include"
export LDFLAGS="-L$(pwd)/contrib/gr-linux-crypto/build -lgnuradio-linux-crypto -lkeyutils -lcrypto"

# Then build app_rpt normally through Asterisk's build system
# (rpt_install.sh handles this automatically if submodule is present)
```

The build system will automatically:
- Detect the gr-linux-crypto submodule
- Include authentication code if `HAVE_GRLINUXCRYPTO` is defined
- Link against required libraries

### 3. Verify Installation

Check that authentication support is compiled in:

```bash
# Check if authentication symbols are present
nm app_rpt.so | grep rpt_auth

# Should show:
# rpt_auth_load_config
# rpt_auth_verify_command
# parse_cop_command
```

## Configuration

### rpt.conf Setup

Add authentication settings to your `rpt.conf` file:

```ini
[general]
; Authentication mode: disabled, optional, or mandatory
authentication_mode = mandatory

; Maximum clock skew in seconds (default: 300 = 5 minutes)
max_clock_skew = 300

; Elliptic curve for signatures (default: brainpoolP256r1)
; Options: brainpoolP256r1, brainpoolP384r1, brainpoolP512r1
signature_curve = brainpoolP256r1

[node1]
; ... other node settings ...
```

### Authentication Modes

- **`disabled`** (default)
  - Authentication is not performed
  - All commands accepted
  - Use for backward compatibility

- **`optional`**
  - Signed commands are verified
  - Unsigned commands are accepted with a warning
  - Useful for gradual migration

- **`mandatory`**
  - All commands MUST be signed
  - Unsigned commands are rejected
  - Maximum security

### Key Management

Public keys must be stored in the Linux kernel keyring for each authorized node.

#### Adding a Public Key

```bash
# Format: node<ID>-public
# Example for node 2000:

# 1. Extract public key (if you have the private key)
openssl ec -in node2000-private.pem -pubout -outform DER | \
    keyctl padd user node2000-public @u

# 2. Or add directly from file
keyctl padd user node2000-public @u < node2000-public.der

# 3. Verify key was added
keyctl list @u | grep node2000
```

#### Listing Keys

```bash
# List all user keys
keyctl list @u

# Read a specific key
keyctl print <key-id>
```

#### Removing Keys

```bash
# Remove a key
keyctl unlink <key-id> @u

# Or by description
keyctl unlink node2000-public @u
```

### Key Distribution

**Important**: Public keys must be securely distributed out-of-band before authentication can work.

1. **Exchange public keys** between nodes using a secure channel
2. **Verify key fingerprints** to ensure authenticity
3. **Store keys** in kernel keyring before enabling authentication

## Command Format

When authentication is enabled, COP commands must include a cryptographic signature.

### Signed Command Structure

```
cop,<command>,<timestamp>,<signature>,<parameters>
```

Where:
- `command`: COP command number (e.g., `2` for System Enable)
- `timestamp`: Unix timestamp of when command was created
- `signature`: Base64-encoded ECDSA signature (128 bytes)
- `parameters`: Optional command parameters

### Signature Generation

The signature covers:
1. Node ID of sender
2. Command number
3. Timestamp
4. Command parameters

Example signature generation (conceptual):

```python
# Pseudo-code - actual implementation in gr-linux-crypto
message = struct.pack("IIQ", node_id, command, timestamp) + params
signature = brainpool_ecdsa_sign(private_key, message)
signature_b64 = base64.b64encode(signature)
```

## Security Considerations

### Clock Synchronization

Authentication uses timestamps to prevent replay attacks. Ensure:

1. **System clocks are synchronized** using NTP
2. **`max_clock_skew`** is set appropriately (default: 300 seconds)
3. **Network latency** is accounted for

### Key Security

1. **Private keys must be kept secret**
   - Store on secure hardware (TPM, Nitrokey) when possible
   - Use filesystem permissions (600) for key files
   - Never transmit private keys over network

2. **Public key distribution**
   - Exchange via secure channel
   - Verify fingerprints before adding to keyring
   - Rotate keys periodically

3. **Keyring access**
   - User keyring (`@u`) is appropriate for most cases
   - Process keyring (`@p`) for per-process isolation
   - Session keyring (`@s`) for session-scoped keys

### Best Practices

1. **Start with `optional` mode** to test deployment
2. **Monitor logs** for authentication failures
3. **Gradually migrate** to `mandatory` mode
4. **Keep backups** of public keys
5. **Document key assignments** to each node

## Troubleshooting

### Authentication Failures

Check Asterisk logs for authentication messages:

```bash
# View authentication-related log entries
grep -i "authentication\|signature" /var/log/asterisk/full

# Common errors:
# - "Authentication failed for COP X from node Y"
# - "Rejected unsigned command from node X"
# - "Command timestamp out of range"
```

### Common Issues

#### "Authentication failed"

- Verify public key exists: `keyctl list @u | grep node<ID>`
- Check key format (must be DER-encoded)
- Ensure clock synchronization

#### "Timestamp out of range"

- Sync system clock: `ntpdate -s pool.ntp.org`
- Increase `max_clock_skew` if needed
- Check timezone settings

#### "Failed to load public key"

- Verify keyring permissions
- Check key description format: `node<ID>-public`
- Ensure key was added to correct keyring

#### "Unsigned command rejected"

- Verify authentication mode is not `mandatory`
- Check command format includes signature
- Review sending node's signing implementation

### Debug Mode

Enable debug logging for authentication:

```bash
# In Asterisk CLI
rpt set debug 1

# Or in rpt.conf
[general]
debug = yes
```

## Integration with gr-linux-crypto

The authentication system uses the following gr-linux-crypto components:

1. **Brainpool EC Operations**
   - ECDSA signature verification
   - BrainpoolP256r1, P384r1, P512r1 support

2. **Kernel Keyring Integration**
   - Secure key storage
   - Key retrieval for verification

3. **Crypto Helpers**
   - Signature encoding/decoding
   - Message formatting

## API Reference

### Configuration Functions

- `rpt_auth_load_config()` - Load authentication settings from rpt.conf
- `rpt_auth_verify_command()` - Verify signed command
- `parse_cop_command()` - Parse command into signed structure

### Configuration Structure

```c
struct rpt_auth_config {
    enum auth_mode mode;           // AUTH_DISABLED, AUTH_OPTIONAL, AUTH_MANDATORY
    int max_clock_skew;            // Maximum time difference (seconds)
    char signature_curve[32];      // Curve name (e.g., "brainpoolP256r1")
};
```

## Migration Guide

### From Unauthenticated to Authenticated

1. **Prepare keys**
   ```bash
   # Generate key pairs for each authorized node
   # Exchange public keys securely
   # Add to keyring on receiving nodes
   ```

2. **Enable optional mode**
   ```ini
   [general]
   authentication_mode = optional
   ```

3. **Test and verify**
   - Monitor logs for warnings
   - Verify signed commands work
   - Check unsigned commands still accepted

4. **Enable mandatory mode**
   ```ini
   [general]
   authentication_mode = mandatory
   ```

5. **Update all nodes**
   - Ensure all sending nodes sign commands
   - Remove unsigned command support

## Examples

### Example: System Enable (COP 2) with Signature

```bash
# Command format (conceptual - actual format depends on implementation)
cop,2,1704067200,<base64_signature>,

# Where:
# - Command: 2 (System Enable)
# - Timestamp: 1704067200 (Unix time)
# - Signature: Base64-encoded ECDSA signature
```

### Example: Configuration File

```ini
[general]
authentication_mode = mandatory
max_clock_skew = 300
signature_curve = brainpoolP256r1

[node1]
rxchannel = IAX2/node1/user:pass@example.com/rx
txchannel = IAX2/node1/user:pass@example.com/tx
; ... other settings ...
```

## Support

For issues or questions:
- GitHub Issues: https://github.com/AllStarLink/app_rpt/issues
- Documentation: See `contrib/gr-linux-crypto/docs/` for detailed crypto API docs

## See Also

- [gr-linux-crypto README](../contrib/gr-linux-crypto/README.md)
- [Kernel Keyring Documentation](https://www.kernel.org/doc/html/latest/security/keys/core.html)
- [Brainpool Curves RFC 5639](https://tools.ietf.org/html/rfc5639)

