CA certificate bundle
=====================

File: certs/cacert.pem
Source: https://curl.se/ca/cacert.pem
Data: Mozilla CA store, 2026-11-05
License: Mozilla Public License 2.0 (MPL-2.0)

The Steam Link firmware ships an old CA store (OpenSSL 1.0.2, 2015) that does
not trust current public roots such as Let's Encrypt's ISRG Root X1/YR. The
app loads this bundle in addition to the firmware roots so modern HTTPS
Jellyfin servers can be verified.
