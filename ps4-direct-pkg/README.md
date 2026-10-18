# PS4 Direct PKG diagnostic v0.8

Native PlayStation 4 application without dependence on NovaPSRPI or a PC server.

- On-screen URL entry using the PS4 IME dialog; file fallback: `/data/ps4direct_url.txt`.
- Uses native sceHttp/sceSsl/sceNet and requests only bytes 0–1023.
- Requires HTTP 206 and the PS4 PKG magic header. Reports HTTP status and short error codes.
- No full PKG download, no installer, no copying of media.
- Distinct Title ID **BREW00975** so verified v0.7 (**BREW00974**) remains available.
- This is experimental. Some signed/redirecting HTTPS hosts may fail due to server or PS4 TLS behavior. No TLS verification bypass.
- **Test first using only a direct URL for a homebrew PKG distributed with permission.**
