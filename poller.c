/*
 * poller.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>
#include <sys/epoll.h>

#include "config.h"
#include "common.h"
#include "log.h"
#include "poller.h"

cSatipPoller *cSatipPoller::instanceS = NULL;

cSatipPoller *cSatipPoller::GetInstance(void)
{
  if (!instanceS)
     instanceS = new cSatipPoller();
  return instanceS;
}

bool cSatipPoller::Initialize(void)
{
  dbg_funcname("%s", __PRETTY_FUNCTION__);
  if (instanceS)
     instanceS->Activate();
  return true;
}

void cSatipPoller::Destroy(void)
{
  dbg_funcname("%s", __PRETTY_FUNCTION__);
  if (instanceS)
     instanceS->Deactivate();
}

cSatipPoller::cSatipPoller()
: cThread("SATIP poller"),
  mutexM(),
  fdM(epoll_create(eMaxFileDescriptors))
{
  dbg_funcname("%s", __PRETTY_FUNCTION__);
}

cSatipPoller::~cSatipPoller()
{
  dbg_funcname("%s", __PRETTY_FUNCTION__);
  Deactivate();
  cMutexLock MutexLock(&mutexM);
  close(fdM);
  // Free allocated memory
}

void cSatipPoller::Activate(void)
{
  // Start the thread
  Start();
}

void cSatipPoller::Deactivate(void)
{
  dbg_funcname("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  if (Running())
     Cancel(3);
}

void cSatipPoller::Action(void)
{
  dbg_funcname("%s Entering", __PRETTY_FUNCTION__);
  struct epoll_event events[eMaxFileDescriptors];
  uint64_t maxElapsed = 0;
  // Increase priority
  SetPriority(-1);
  // Do the thread loop
  while (Running()) {
        int nfds = epoll_wait(fdM, events, eMaxFileDescriptors, -1);
        ERROR_IF_FUNC((nfds == -1 && errno != EINTR), "epoll_wait() failed", break, ;);
        for (int i = 0; i < nfds; ++i) {
            cSatipPollerIf* poll = reinterpret_cast<cSatipPollerIf *>(events[i].data.ptr);
            uint32_t unexpected = EPOLLRDHUP | EPOLLHUP;
            if (events[i].events & unexpected) {
               dbg_funcname("%s: unexpected peer hangup on fd=%d %s", \
                            __PRETTY_FUNCTION__,                      \
                            events[i].data.fd,                        \
                            poll?*(poll->ToString()):"");
               epoll_ctl(fdM, EPOLL_CTL_DEL, events[i].data.fd, NULL);
               }
            if (poll) {
               uint64_t elapsed;
               cTimeMs processing(0);
               poll->Process();
               elapsed = processing.Elapsed();
               if (elapsed > maxElapsed) {
                  maxElapsed = elapsed;
                  dbg_funcname("%s Processing %s took %" PRIu64 " ms", __PRETTY_FUNCTION__, *(poll->ToString()), maxElapsed);
                  }
               }
           }
        }
  dbg_funcname("%s Exiting", __PRETTY_FUNCTION__);
}

bool cSatipPoller::Register(cSatipPollerIf &pollerP)
{
  dbg_funcname("%s fd=%d", __PRETTY_FUNCTION__, pollerP.GetFd());
  cMutexLock MutexLock(&mutexM);

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
  ev.data.ptr = &pollerP;
  ERROR_IF_RET(epoll_ctl(fdM, EPOLL_CTL_ADD, pollerP.GetFd(), &ev) == -1, "epoll_ctl(EPOLL_CTL_ADD) failed", return false);
  dbg_funcname("%s Added interface fd=%d", __PRETTY_FUNCTION__, pollerP.GetFd());

  return true;
}

bool cSatipPoller::Unregister(cSatipPollerIf &pollerP)
{
  dbg_funcname("%s fd=%d", __PRETTY_FUNCTION__, pollerP.GetFd());
  cMutexLock MutexLock(&mutexM);
  if (epoll_ctl(fdM, EPOLL_CTL_DEL, pollerP.GetFd(), NULL) != 0) {
     if (errno == ENOENT /* No such file or directory */) {
        /* op was EPOLL_CTL_DEL, and fd is not registered with this epoll
         * instance.
         *
         * Our known fd points to an invalid or closed kernel 'file description'.
         * We can do what we want, we lost control on this epoll interest list entry.
         * Repeating EPOLL_CTL_DEL will trigger this message once again.
         *
         * On the other hand, epoll man page says, that epoll will remove invalid
         * file descriptions from the interest list by itself.
         */
        return true;
        }
     if (errno == EBADF /* Bad file number */) {
        /*
         * epfd or fd is not a valid file descriptor.
         * Well, epfd cannot be a bad fd, i guess.
         */
        error("%s failed: Bad file number (EBADF) fd=%d",
              __PRETTY_FUNCTION__, pollerP.GetFd());
        return false;
        }
     else {
        error("epoll_ctl(EPOLL_CTL_DEL) failed: %s (%d)",
              strerror(errno), errno);
        return false;
        }
     }
  else
     dbg_funcname("%s Removed interface fd=%d", __PRETTY_FUNCTION__, pollerP.GetFd());

  return true;
}
