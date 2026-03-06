#ifndef __SSH_CLIENT_OTHER_H__
#define __SSH_CLIENT_OTHER_H__

#include <Arduino.h>

enum class SSHAuthType : uint8_t {
    Password = 0,
    PrivateKeyPath = 1,
    PrivateKeyInline = 2,
};

struct SSHAuth {
    SSHAuthType type = SSHAuthType::Password;
    String password;
    String keyPath;
    String privateKey;
};

struct SSHCommandResult {
    int exitCode = -1;
    String stdoutText;
    String stderrText;
};

struct SSHSession {
    bool connected = false;
    String host;
    uint16_t port = 22;
    String user;
    uint32_t connectedAtMs = 0;
    void *backendHandle = nullptr;
};

// Legal/ethical scope:
// These APIs are only for user-authorized remote administration.
// Do not use for brute-force, password guessing, or unauthorized access.
bool ssh_connect(const String &host, uint16_t port, const String &user, const SSHAuth &auth, SSHSession &outSession);
bool ssh_connect(
    const String &host, uint16_t port, const String &user, const SSHAuth &auth, SSHSession &outSession,
    String *errorOut
);
bool ssh_run_command(SSHSession &session, const String &cmd, SSHCommandResult &outResult);
bool ssh_run_command(
    SSHSession &session, const String &cmd, SSHCommandResult &outResult, uint32_t timeoutMs,
    String *errorOut = nullptr
);
bool ssh_interactive(SSHSession &session);
void ssh_disconnect(SSHSession &session);

bool export_history();
bool export_history(const String &destinationPath);

void ssh_client_menu();

#endif // __SSH_CLIENT_OTHER_H__
