# PS4 Direct PKG — Downloader v1.0 (experimental)

Independent native PS4 homebrew downloader. Runs on console without NovaPS or a Windows proxy.

**Preliminary; needs console hardware testing. Only download packages you have permission to use. This app does NOT register or install PS4 PKGs.**

## How to use
1. Create ASCII/UTF-8 text file at /data/ps4direct_url.txt containing one direct HTTP(S) URL.
2. Run PS4 Direct PKG v1.0 (unique Title ID BREW00977).
3. Review the expected size and storage warning; select YES to download.
4. A native progress bar appears. Cancel to preserve the partial file.
5. On completion, find the PKG under /data/ps4direct_<filename>_<hash>.pkg.

Files are saved to *.part until fully received, with *.state recording the URL hash and expected total. Restart app with the same URL to resume; server must return HTTP 206 with the matching Content-Range start/total. Unexpected 200 on resumption and redirects/errors are rejected without corrupting the partial file.

- Accepts HTTP 200 initial request with Content-Length or HTTP 206 with valid Content-Range.
- Memory use is bounded to a 64 KiB transfer buffer; 16 GiB per-file safety limit.
- Certificate verification is not bypassed. HTTPS support depends on PS4 SSL libraries and destination server.
- The app **does not pre-check available disk space** (filesystem APIs can be unreliable on PS4). Check free space before confirming; a full disk will cause a write failure and preserve the partial file.
- Existing completed files are never overwritten. No auto deletion.
- Downloading a PKG does **not** ensure it is valid or compatible with PS4. Only an authorized homebrew source should be used.
- Range/content verification checks file size and PKG signature, not a cryptographic hash of the entire file. Independent checksum verification remains future work.
- Keyboard input postponed; uses the previously working file input.
- Does not change previous v0.7–v0.9 installations.

Build: GitHub Actions with OpenOrbis v0.5.4.
