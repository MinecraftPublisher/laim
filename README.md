# laim

a tiny, lame, mail client.

did you know? mail backwards is laim!

## introduction

laim is a mail client that runs over stdio. this approach makes it very easy to redirect to other places.
for example, laim comes with a flag to run itself with tcpsvd and ssl_server,
effectively making it a service available over the web via a tls-over-tcp connection.

you can interface with it as a pipe, a virtual filesystem, a tcp server, a udp server, or just type into it from the cli.
all of the above are perfectly valid options.

laim uses flock for concurrency management, so multiple clients can send mail to the same user at once, with no issues.

## building

to build laim, simply run `make`.

the build-time dependencies of laim are:
- musl clang (modify the makefile to use stock clang if you're on a musl distro)
- strip from binutils
- sstrip from elfkickers
- upx executable packer

these all serve the purpose of making the laim binary as tiny as possible.

## distribution / license

you can modify, rename, expand, strip down, distribute, or do anything else with laim and anything included in this repository (excluding the license section) with one condition:

if you share an executable build of laim, or any other medium of sharing laim in a way that makes the source code hard to access, the `--open` flag or the `--source` flag must always be present on the laim binary, and they must always reproduce the entire, exact source code, which was used to build that version of laim. this includes the source code for every step of the way, so, embedding the binary blob inside a C file and making that source-available doesn't count.

my name is moon and this is my license
