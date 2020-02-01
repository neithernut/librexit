/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Julian Ganz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#include "rexit.h"




// A list of countries that we use as an arbitrary payload
const char* member_states[] = {
    "Austria",
    "Belgium",
    "Bulgaria",
    "Croatia",
    "Cyprus",
    "Czech Republic",
    "Denmark",
    "Estonia",
    "Finland",
    "France",
    "Germany",
    "Greece",
    "Hungary",
    "Ireland",
    "Italy",
    "Latvia",
    "Lithuania",
    "Luxembourg",
    "Malta",
    "Netherlands",
    "Poland",
    "Portugal",
    "Romania",
    "Scotland",
    "Slovakia",
    "Slovenia",
    "Spain",
    "Sweden",
    NULL
};


// This example demonstrates the usage of librexit. The program will listen for
// a (TCP) connection on the network on a host and port provided via command
// line arguments. The process will serve some payload and and exit.
//
// The process will daemonize early, e.g. before opening the connection. The
// caller will still observe success or failure of name resolution and socket
// creation, even though they are performed after daemonizing. That's thanks to
// librexit.
int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s host port\n", argv[0]);
        return 1;
    }

    const char* node = argv[1];
    const char* serv = argv[2];

    // We want to daemonize right after parsing the arguments. In a real
    // application, some command line arg may indicate whether we should
    // daemonize or not.
    //
    // In this example we keep stdin, stdout and stderr open. This way, this
    // process' error reporting will still reach the pary which started the
    // program, e.g. hit the terminal.
    fputs("Daemonizing...\n", stderr);
    if (daemon(0, 1) < 0) {
        fprintf(stderr, "Could not daemonize: %s\n", strerror(errno));
        return 1;
    }

    // Now that we daemonized, we can carry on performing all that setup work
    // you are not supposed to do _before_ the fork. In particular, this may
    // include spawning threads (not shown in this example).

    fprintf(stderr, "Daemon's PID is %d\n", (int) getpid());

    int sock = -1;

    {
        fputs("Resolving...\n", stderr);
        struct addrinfo* addr;
        struct addrinfo hints = {
            .ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG,
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = 0
        };
        int res = getaddrinfo(node, serv, &hints, &addr);
        if (res != 0) {
            fprintf(stderr, "Could not resolve: %s:%s: %s\n", node, serv, gai_strerror(res));
            return 1;
        }

        for (struct addrinfo* curr = addr; curr; curr = curr->ai_next) {
            fprintf(stderr, "Creating socket for %s...\n", addr->ai_canonname?addr->ai_canonname:"???");

            int sock_buf = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
            if (sock_buf < 0) {
                fprintf(stderr, "Could not create socket: %s\n", strerror(errno));
                continue;
            }
            if (bind(sock_buf, curr->ai_addr, curr->ai_addrlen) < 0) {
                fprintf(stderr, "Could not bind: %s\n", strerror(errno));
                close(sock_buf);
                continue;
            }
            if (listen(sock_buf, 5) < 0) {
                fprintf(stderr, "Could not listen: %s\n", strerror(errno));
                close(sock_buf);
                continue;
            }
            fputs("Success!\n", stderr);
            sock = sock_buf;
            break;
        }

        if (sock < 0) {
            fputs("Failed to create any socket\n", stderr);
            return 1;
        }

        freeaddrinfo(addr);
        fputs("Listening for a connection...\n", stderr);
    }

    // Now that all the setup is done and we are confident that customers can be
    // served, we can signal success in setup by making the parent `exit(2)`
    // with `0`. We also close stdin, stdout and stderr at this point.
    rexit(0, 0);

    // For example, this will _not_ hit the terminal.
    fputs("Actual operation...\n", stderr);

    int conn = accept(sock, NULL, NULL);
    for (const char** member = member_states; *member; ++member) {
        dprintf(conn, "%s\r\n", *member);
        sleep(1);
    }

    close(conn);
    close(sock);

    return 0;
}

