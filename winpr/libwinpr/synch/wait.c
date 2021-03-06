/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>
#include <winpr/synch.h>

#include "synch.h"
#include "../thread/thread.h"
#include <winpr/thread.h>

/**
 * WaitForSingleObject
 * WaitForSingleObjectEx
 * WaitForMultipleObjectsEx
 * SignalObjectAndWait
 */

#ifndef _WIN32

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hHandle, &Type, &Object))
		return WAIT_FAILED;

	if (Type == HANDLE_TYPE_THREAD)
	{
		int status;
		WINPR_THREAD* thread;
		void* thread_status = NULL;

		if (dwMilliseconds != INFINITE)
			printf("WaitForSingleObject: timeout not implemented for thread wait\n");

		thread = (WINPR_THREAD*) Object;

		status = pthread_join(thread->thread, &thread_status);

		if (status != 0)
			printf("WaitForSingleObject: pthread_join failure: %d\n", status);
	}
	if (Type == HANDLE_TYPE_MUTEX)
	{
		if (dwMilliseconds != INFINITE)
			printf("WaitForSingleObject: timeout not implemented for mutex wait\n");

		pthread_mutex_lock((pthread_mutex_t*) Object);
	}
	else if (Type == HANDLE_TYPE_EVENT)
	{
		int status;
		fd_set rfds;
		WINPR_EVENT* event;
		struct timeval timeout;

		event = (WINPR_EVENT*) Object;

		FD_ZERO(&rfds);
		FD_SET(event->pipe_fd[0], &rfds);
		ZeroMemory(&timeout, sizeof(timeout));

		if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
		{
			timeout.tv_usec = dwMilliseconds * 1000;
		}

		status = select(event->pipe_fd[0] + 1, &rfds, 0, 0,
				(dwMilliseconds == INFINITE) ? NULL : &timeout);

		if (status < 0)
			return WAIT_FAILED;

		if (status != 1)
			return WAIT_TIMEOUT;
	}
	else if (Type == HANDLE_TYPE_SEMAPHORE)
	{
		WINPR_SEMAPHORE* semaphore;

		semaphore = (WINPR_SEMAPHORE*) Object;

#ifdef WINPR_PIPE_SEMAPHORE
		if (semaphore->pipe_fd[0] != -1)
		{
			int status;
			int length;
			fd_set rfds;
			struct timeval timeout;

			FD_ZERO(&rfds);
			FD_SET(semaphore->pipe_fd[0], &rfds);
			ZeroMemory(&timeout, sizeof(timeout));

			if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
			{
				timeout.tv_usec = dwMilliseconds * 1000;
			}

			status = select(semaphore->pipe_fd[0] + 1, &rfds, 0, 0,
					(dwMilliseconds == INFINITE) ? NULL : &timeout);

			if (status < 0)
				return WAIT_FAILED;

			if (status != 1)
				return WAIT_TIMEOUT;

			length = read(semaphore->pipe_fd[0], &length, 1);

			if (length != 1)
				return FALSE;
		}
#else

#if defined __APPLE__
		semaphore_wait(*((winpr_sem_t*) semaphore->sem));
#else
		sem_wait((winpr_sem_t*) semaphore->sem);
#endif

#endif
	}

	return WAIT_OBJECT_0;
}

DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable)
{
	return WAIT_OBJECT_0;
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
	int fd = -1;
	int maxfd;
	int index;
	int status;
	fd_set fds;
	ULONG Type;
	PVOID Object;
	struct timeval timeout;

	if (!nCount)
		return WAIT_FAILED;
	maxfd = 0;
	FD_ZERO(&fds);
	ZeroMemory(&timeout, sizeof(timeout));

	if (bWaitAll)
		printf("WaitForMultipleObjects: bWaitAll not yet implemented\n");

	for (index = 0; index < nCount; index++)
	{
		if (!winpr_Handle_GetInfo(lpHandles[index], &Type, &Object))
			return WAIT_FAILED;

		if (Type == HANDLE_TYPE_EVENT)
		{
			fd = ((WINPR_EVENT*) Object)->pipe_fd[0];
		}
		else if (Type == HANDLE_TYPE_SEMAPHORE)
		{
#ifdef WINPR_PIPE_SEMAPHORE
			fd = ((WINPR_SEMAPHORE*) Object)->pipe_fd[0];
#else
			return WAIT_FAILED;
#endif
		}
		else
		{
			return WAIT_FAILED;
		}

		FD_SET(fd, &fds);

		if (fd > maxfd)
			maxfd = fd;
	}

	if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
	{
		timeout.tv_usec = dwMilliseconds * 1000;
	}

	status = select(maxfd + 1, &fds, 0, 0,
			(dwMilliseconds == INFINITE) ? NULL : &timeout);

	if (status < 0)
		return WAIT_FAILED;

	if (status == 0)
		return WAIT_TIMEOUT;

	for (index = 0; index < nCount; index++)
	{
		winpr_Handle_GetInfo(lpHandles[index], &Type, &Object);

		if (Type == HANDLE_TYPE_EVENT)
			fd = ((WINPR_EVENT*) Object)->pipe_fd[0];
		else if (Type == HANDLE_TYPE_SEMAPHORE)
			fd = ((WINPR_SEMAPHORE*) Object)->pipe_fd[0];

		if (FD_ISSET(fd, &fds))
		{
			if (Type == HANDLE_TYPE_SEMAPHORE)
			{
				int length = read(fd, &length, 1);

				if (length != 1)
					return WAIT_FAILED;
			}

			return (WAIT_OBJECT_0 + index);
		}
	}

	return WAIT_FAILED;
}

DWORD WaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable)
{
	return 0;
}

DWORD SignalObjectAndWait(HANDLE hObjectToSignal, HANDLE hObjectToWaitOn, DWORD dwMilliseconds, BOOL bAlertable)
{
	return 0;
}

#endif
