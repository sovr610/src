// TaskManagerConnection.h
// copyright 2000 Verant Interactive
// Author: Justin Randall

#ifndef _TaskManagerConnection_H
#define	_TaskManagerConnection_H

//-----------------------------------------------------------------------

#include "serverUtility/ServerConnection.h"

//-----------------------------------------------------------------------

class TaskManagerConnection : public ServerConnection
{
public:
	TaskManagerConnection(const std::string & remoteAddress, const uint16_t remotePort);
	~TaskManagerConnection();
	void                          onConnectionClosed      ();
	void                          onConnectionOpened      ();

private:
	TaskManagerConnection(const TaskManagerConnection&);
	TaskManagerConnection& operator=(TaskManagerConnection&);
};

//-----------------------------------------------------------------------

#endif	// _TaskManagerConnection_H
