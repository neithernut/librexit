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
#include "rexit.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


// File descriptor for writing the status
//
// This variable will be set to a file descriptor in `daemon()`. The file
// descriptor will be the write end of a pipe used for transporting the exit
// status.
static int rexit_tunnel = -1;


static void redirect() {
    // Closing stdin, stdout and stderr may have some suboptimal consequences,
    // e.g. if some function tries to report an error by printing to stderr.
    // Hence, it's (apparently) customary to just redirect them to `/dev/null`.
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        // However, we may not be able to open `/dev/null` for some reason. In
        // such a case we should try to close those descriptors as a last
        // resort.
        close(0);
        close(1);
        close(2);
        return;
    }

    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
}


int daemon(
    int nochdir,
    int noclose
) {
    // We use a pipe as a connection between parent and child. The pipe will
    // be used for transporting the exit status (in the regular case) and thus
    // signal the parent when to exit.
    int tunnel[2];
    if (pipe(tunnel) < 0 || fcntl(tunnel[1], F_SETFD, FD_CLOEXEC) < 0)
        return -1;
    rexit_tunnel = tunnel[1];

    const pid_t child = fork();
    if (child < 0)
        return -1;

    if (child > 0) {
        // We are in the parent process.

        // We _really_ want to make sure we don't end up keeping the writing end
        // open while the child drops dead in the background. We don't want to
        // rely, for example, on `read(2)` being interrupted by `SIGCHILD`.
        while (close(tunnel[1]) < 0 && errno == EINTR);

        // The only job left for us now is to wait for a status from the child
        // and exit with that status. The status may reach us either via the
        // tunnel or as exit status of the child process. In the latter case the
        // `read` will (hopefully) return without touching status. Note that we
        // _do_ speculate that the read of a single integer is atomic.
        int status = EXIT_FAILURE;
        if (read(tunnel[0], &status, sizeof(int)) <= 0) {
            if (waitpid(child, &status, WNOHANG) < 0)
                perror("librexit");

            // The status we receive through `waitpid(2)` is generally
            // unsuitable for passing to `exit(2)` directly since it may
            // carry more/other information than the return status. For
            // example, the child process may have returned "abnormally", e.g.
            // not through a call to `exit(2)`. Even if it terminated through
            // `exit(2)`, we still have to extract the actual return status.
            if (WIFEXITED(status))
                status = WEXITSTATUS(status);
            else
                status = EXIT_FAILURE;
        }
        _exit(status);
    }

    // We are in the child process. Do the usual daemon-setup work. Note that
    // we do _not_ report errors to the caller from this point on. The rationale
    // is that we did our main job and the rest is just non-critical convenience
    // functionality. For example, a failure to `chdir(2)` may end up being an
    // annoyance, but it's surely no reason for the process to `abort(3)` with
    // an error. This is somewhat of a limitation of the interface.
    close(tunnel[0]);

    if (setsid() < 0)
        return -1;

    if (nochdir == 0)
        chdir("/");
    if (noclose == 0)
        redirect();

    return 0;
}


int rexit(
    int status,
    int noclose
) {
    // Stuff the exit status into the pipe, which will hopefully make the parent
    // `exit(3)`.
    if (write(rexit_tunnel, &status, sizeof(int)) < 0)
        return -1;
    if (close(rexit_tunnel) < 0)
        return -1;

    if (noclose == 0)
        redirect();

    return 0;
}

