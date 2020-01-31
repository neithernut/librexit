#ifndef LIBREXIT_H_
#define LIBREXIT_H_


/**
 * Remotely trigger the parent process to exit with a given status
 *
 * After a call to librexit's version of `daemon()`, this function will cause
 * the parent to `exit(2)` with the given `status`. If `noclose` is zero, stdin,
 * stdout and stderr will be redirected, stopping output from reaching the
 * terminal. Errors occuring during during redirection will not be reported to
 * the caller.
 *
 * @returns `0` on success and `-1` on error, in which case `errno` will be set
 *          accordingly
 */
int rexit(
    int status, //< exit status
    int noclose //< indication of whether to close stdin, stdout and stderr
);


#endif
