# Security Policy

## Supported Versions

Security fixes and integrity improvements are actively applied to the latest release on the `master` branch.

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

---

## Reporting a Vulnerability

If you discover a security vulnerability or critical bug relating to memory safety, corrupted chunk recovery bypass, or potential buffer overflow in potDB, please do **NOT** open a public issue.

Instead, please report it privately via email:

* **Contact:** Jaison ([jaisondz9360@gmail.com](mailto:jaisondz9360@gmail.com))
* **Subject:** `[potDB Security Vulnerability] <Brief Description>`

### What to Include

Please provide:
1. A description of the vulnerability and its potential impact.
2. Steps or a minimal C test case to reproduce the issue.
3. Your operating system, architecture, and compiler versions.
4. Any proposed patch or fix (if available).

### Response Process

1. **Acknowledgment:** You will receive an acknowledgment within 48 hours.
2. **Investigation:** We will investigate and validate the vulnerability.
3. **Fix and Release:** A fix will be developed and verified against unit and edge-case test suites.
4. **Disclosure:** Once the fix is released, credit will be given in the changelog (unless requested to remain anonymous).
