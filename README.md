librexit -- remote exit for daemons

When writing a daemon providing some service, one usually wants to daemonize
early, before setting up stuff which may not respond too well to a call to
`fork(2)`. However, this inhibits communicating whether and when the service
actually becomes available. This information is obviously of interest,
especially if we want to start services depending on the current one.

Solutions to this problem in rcs included praying and waiting, regular testing
via a client, hard dependencies on pötterware and notifications over the
infamous DBus. While all of these solutions may work one way or another, they
complicate the calling site, e.g. by making classical rc scripts even more
unwieldy. It would be nice if, instead, the process which we started originally
would just communicate success and failure via its return status, like we'd
expect from non-daemon setup one-shot commands. Thankfully, unlike Northern
Ireland's customs borders, we can figure this one out easily with the help of
some primitive IPC.

This library replaces the `daemon(3)` function and introduces a new function
`rexit`. Like the usual `daemon(3)` function, our variant will `fork(2)` and
call `setsid(2)` in the child process. However, the parent process will _not_
`exit(2)` right away but wait for more information from the child process. It
will ultimately exit in one of the following situations:

 * The child process called `rexit`. In this case, it will `exit(2)` with the
   given status.
 * The child terminated. In this case the child's exit status will be forwarded
   if it could be determined. Otherwise, it will terminate with `EXIT_FAILURE`.
 * The child closed the pipe using for IPC. This is done automatically if the
   child calls one of the `exec*` system calls. In this case, the parent will
   terminate with `EXIT_FAILURE`.

Obviously, `rexit` should be called after all the important setup work is done
right before/after the service effectively becomes available.

