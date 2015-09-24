#ifndef PROCESSMUTEX_H
#define PROCESSMUTEX_H
#ifdef WIN32
#include <Windows.h>
#endif

#ifdef linux
#include <unistd.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h> 
#include <signal.h>
#include <errno.h>

#endif

class CProcessMutex
{
public:
    /* 默认创建匿名的互斥 */
    CProcessMutex(const char* name = NULL);
    ~CProcessMutex();

    bool Lock(const long long ms=-1);
    bool UnLock();
    bool RemoveMutex();
public:
    int m_errno;

private:

#ifdef WIN32
    void* m_pMutex;
#endif

#ifdef linux
    bool m_locked;
    pid_t m_tid;
    key_t m_semKey;
    int m_sem;
    struct sembuf m_semBuf;
#endif
    char *m_cMutexName;
};

#endif // PROCESSMUTEX_H
