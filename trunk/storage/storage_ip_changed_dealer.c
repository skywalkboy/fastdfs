/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/statvfs.h>
#include <sys/param.h>
#include "fdfs_define.h"
#include "logger.h"
#include "fdfs_global.h"
#include "sockopt.h"
#include "shared_func.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_ip_changed_dealer.h"

static int storage_report_storage_ip_addr()
{
	TrackerServerInfo *pGlobalServer;
	TrackerServerInfo *pTServer;
	TrackerServerInfo *pTServerEnd;
	TrackerServerInfo trackerServer;
	char tracker_client_ip[IP_ADDRESS_SIZE];
	int success_count;
	int result;
	int i;

	success_count = 0;
	pTServer = &trackerServer;
	pTServerEnd = g_tracker_group.servers + g_tracker_group.server_count;

	while (success_count == 0 && g_continue_flag)
	{
	for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd; \
			pGlobalServer++)
	{
		memcpy(pTServer, pGlobalServer, sizeof(TrackerServerInfo));
		for (i=0; i < 3; i++)
		{
			pTServer->sock = socket(AF_INET, SOCK_STREAM, 0);
			if(pTServer->sock < 0)
			{
				result = errno != 0 ? errno : EPERM;
				logError("file: "__FILE__", line: %d, " \
					"socket create failed, errno: %d, " \
					"error info: %s.", \
					__LINE__, result, strerror(result));
				sleep(5);
				break;
			}

			if (g_client_bind_addr && *g_bind_addr != '\0')
			{
				socketBind(pTServer->sock, g_bind_addr, 0);
			}

			if ((result=connectserverbyip(pTServer->sock, \
				pTServer->ip_addr, pTServer->port)) == 0)
			{
				break;
			}

			close(pTServer->sock);
			pTServer->sock = -1;
			sleep(5);
		}

		if (pTServer->sock < 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"connect to tracker server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pTServer->ip_addr, pTServer->port, \
				result, strerror(result));

			continue;
		}

		getSockIpaddr(pTServer->sock,tracker_client_ip,IP_ADDRESS_SIZE);
		if (*g_tracker_client_ip == '\0')
		{
			strcpy(g_tracker_client_ip, tracker_client_ip);
		}
		else if (strcmp(tracker_client_ip, g_tracker_client_ip) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"as a client of tracker server %s:%d, " \
				"my ip: %s != client ip: %s of other " \
				"tracker client", __LINE__, \
				pTServer->ip_addr, pTServer->port, \
				tracker_client_ip, g_tracker_client_ip);

			close(pTServer->sock);
			return EINVAL;
		}

		fdfs_quit(pTServer);
		close(pTServer->sock);
		success_count++;
	}
	}

	if (*g_last_storage_ip == '\0')
	{
		return storage_write_to_sync_ini_file();
	}
	else if (strcmp(g_tracker_client_ip, g_last_storage_ip) == 0)
	{
		return 0;
	}

	success_count = 0;
	while (success_count == 0 && g_continue_flag)
	{
	for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd; \
			pGlobalServer++)
	{
		memcpy(pTServer, pGlobalServer, sizeof(TrackerServerInfo));
		for (i=0; i < 3; i++)
		{
			pTServer->sock = socket(AF_INET, SOCK_STREAM, 0);
			if(pTServer->sock < 0)
			{
				result = errno != 0 ? errno : EPERM;
				logError("file: "__FILE__", line: %d, " \
					"socket create failed, errno: %d, " \
					"error info: %s.", \
					__LINE__, result, strerror(result));
				sleep(5);
				break;
			}

			if (g_client_bind_addr && *g_bind_addr != '\0')
			{
				socketBind(pTServer->sock, g_bind_addr, 0);
			}

			if ((result=connectserverbyip(pTServer->sock, \
				pTServer->ip_addr, pTServer->port)) == 0)
			{
				break;
			}

			close(pTServer->sock);
			pTServer->sock = -1;
			sleep(5);
		}

		if (pTServer->sock < 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"connect to tracker server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pTServer->ip_addr, pTServer->port, \
				result, strerror(result));

			continue;
		}

		if (tcpsetnonblockopt(pTServer->sock) != 0)
		{
			close(pTServer->sock);
			continue;
		}

		if (tracker_report_join(pTServer, g_sync_old_done) != 0)
		{
			close(pTServer->sock);
			continue;
		}

		//to do report ip addr changed

		fdfs_quit(pTServer);
		close(pTServer->sock);
		success_count++;
	}
	}

	if (!g_continue_flag)
	{
		return EINTR;
	}

	return storage_write_to_sync_ini_file();
}

int storage_check_ip_changed()
{
	int result;

	if ((result=storage_report_storage_ip_addr()) != 0)
	{
		return result;
	}

	return 0;
}

