#include "ProcessMutex.h"
#include <errno.h>

#ifdef WIN32

CProcessMutex::CProcessMutex(const char* name)
{
	memset(m_cMutexName, 0 ,sizeof(m_cMutexName));
	int min = strlen(name)>(sizeof(m_cMutexName)-1)?(sizeof(m_cMutexName)-1):strlen(name);
	strncpy(m_cMutexName, name, min);
	m_pMutex = CreateMutex(NULL, false, m_cMutexName);
}

CProcessMutex::~CProcessMutex()
{
	CloseHandle(m_pMutex);
}

bool CProcessMutex::Lock(const long long ms)
{
	//互斥锁创建失败
	if (NULL == m_pMutex)
	{
		return false;
	}
	if(ms<0)
		DWORD nRet = WaitForSingleObject(m_pMutex, INFINITE);
	else
		DWORD nRet=WaitForSingleObject(m_pMutex, ms);
	if (nRet != WAIT_OBJECT_0)
	{
		return false;
	}

	return true;
}

bool CProcessMutex::UnLock()
{
	return ReleaseMutex(m_pMutex);
}

#endif

#ifdef linux
//Get the thread id
inline pid_t gettid()
{
	return syscall(SYS_gettid);
}

unsigned int hashMap(const char *name)
{
    unsigned int hash=0;
    while(*name)
    {
        hash=(*name++)+(hash<<6)+(hash<<16)-hash;
    }
    return (hash&0x7FFFFFFF);
}

CProcessMutex::CProcessMutex(const char* name)
{
	int nameLen = strlen(name);
	m_cMutexName = (char*) malloc(sizeof(char) * nameLen);
	strcpy(m_cMutexName, name);
	m_semKey = (key_t) hashMap(name);
	m_errno=0;
	//if existed
	if ((m_sem = (int) semget(m_semKey, 1, IPC_CREAT | IPC_EXCL | 0660)) == -1)
	{
		m_errno=errno;
		//Get the existed sem
		m_sem = (int) semget(m_semKey, 1, IPC_CREAT | 0660);

		//Check whether it is trash value. semctl(m_sem,0,GETNCNT,0) returns the count of waiting process
		//semctl(m_sem,0,GETPID,9) returns the last process's pid which called semop()
		if ( (semctl(m_sem, 0, GETVAL, 0) != 1) && (kill(semctl(m_sem,0,GETPID,0),0)!=0))
		{
			if (semctl(m_sem, 0, SETVAL, 1) == -1)
			{
				m_errno=errno;
			}
		}
	}
	//If not exist, initial the value
	else
	{
		if (semctl(m_sem, 0, SETVAL, 1) == -1)
			m_errno=errno;
	}
	m_locked = false;
	m_tid=-1;
}

CProcessMutex::~CProcessMutex()
{
	free(m_cMutexName);
	m_cMutexName = NULL;
}

bool CProcessMutex::Lock(const long long ms)
{
	int ret;
	//当前线程不是已经获得锁的线程
	if (!(m_locked == true && m_tid == gettid()))
	{
		m_semBuf.sem_num = 0;
		m_semBuf.sem_op = -1;
		m_semBuf.sem_flg = SEM_UNDO;
		if (ms < 0)
		{
			ret = semop(m_sem, &m_semBuf, 1);
		}
		else
		{
			struct timespec abs_time;
			clock_gettime(CLOCK_REALTIME, &abs_time);
			abs_time.tv_sec += ms / 1000;
			abs_time.tv_nsec += (ms % 1000) * 1000 * 1000;
			abs_time.tv_sec = abs_time.tv_sec + abs_time.tv_nsec / 1000000000;
			abs_time.tv_nsec = abs_time.tv_nsec % 1000000000;
			ret = semtimedop(m_sem, &m_semBuf, 1, &abs_time);
		}
		if (ret != 0)
		{
			m_errno=errno;
			return false;
		}
		//上锁的线程号
		if (m_locked == false)
			m_tid = gettid();
		m_locked = true;
		return true;
	}
	else
		return true;
}

bool CProcessMutex::UnLock()
{
	//获取当前值
	int val = semctl(m_sem, 0, GETVAL, 0);
	if (val != 1)
	{
		if(m_tid == gettid())
		{
			m_semBuf.sem_num = 0;
			m_semBuf.sem_op = 1;
			m_semBuf.sem_flg = SEM_UNDO;
			int ret = semop(m_sem, &m_semBuf, 1);
			if (ret != 0)
			{
				m_errno=errno;
				return false;
			}
		}
		else
		{
			return false;
		}
		
	}
	m_locked = false;
	m_tid=-1;
	return true;
}

bool CProcessMutex::RemoveMutex()
{
	if (semctl(m_sem, NULL, SETVAL) == -1)
	{
		m_errno=errno;
		return false;
	}
	return true;
}
#endif
