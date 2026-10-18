# PS4 Direct PKG v0.9 — Keyboard diagnostic

This native PS4 diagnostic replaces v0.8's silent keyboard failures with a visible stage code.

Changes:
- Initialize SceUserService using valid initialization parameters (v0.8 passed NULL).
- Query current user and fall back to initial user.
- Use IME system keyboard with a bounded polling loop, and properly wait for NONE/RUNNING/STOPPED transitions.
- Display module/init/status/result errors if URL input fails.
- Test HTTPS URL with a 1024-byte Range request, using native PS4 APIs.
- Keep TLS certificate validation; no SSL bypass.
- No full downloads, installation, or server proxy.
- Uses a distinct title ID BREW00976 so v0.8/v0.7 remain installed.

Stage codes: -1 user service, -2 IME module, -3 IME dialog opening, -4 wait/timeout, -5 getting result, -6 canceled, -7 unsupported characters, -8 URL formatting.

No package installs are attempted. Build via OpenOrbis 0.5.4 in GitHub Actions on the experimental branch. Real PS4 behavior must be confirmed by hardware testing.
