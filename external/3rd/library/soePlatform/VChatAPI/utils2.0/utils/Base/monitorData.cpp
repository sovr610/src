// #include <varargs.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "monitorData.h"


#ifdef WIN32
#define snprintf _snprintf
#endif


//**********************************************************************************************
//**********************************************************************************************
//**********************************  DO NOT EDIT THIS FILE  ***********************************
//**********************************************************************************************
//**********************************************************************************************


#define ELEMENT_START_VALUE		600
#define BUFFER_SIZE_START		2048
#define BUFFER_RESIZE_VALUE		1024

extern void set_bit(  unsigned char p[], int bit);
extern void unset_bit(unsigned char p[], int bit);
extern int  get_bit(  unsigned char p[], int bit);


// ------------------   Support Routine  -----------------------------
//////////////////////////////////////////////////////////////////////
//  qsort compare routine
//////////////////////////////////////////////////////////////////////

int compar( const void *e1, const void *e2 )
{
MON_ELEMENT *data1,*data2;

		data1 = (MON_ELEMENT *)e1;
		data2 = (MON_ELEMENT *)e2;

#ifdef _WIN32
		return stricmp(data1->label, data2->label);
#else
		return strcasecmp(data1->label,data2->label);
#endif
}


// ------------------   Game Data Object ----------------------------
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CMonitorData::CMonitorData()
{

	m_sequence	= 1;
	m_buffer	= nullptr;
	m_nbuffer	= 0;
	m_count		= 0;
	m_max		= ELEMENT_START_VALUE;
	m_data		= new MON_ELEMENT[m_max];
	for(int x=0;x<m_max;x++) 
	{
		memset(&m_data[x],0,sizeof(MON_ELEMENT));
	}
	resize_buffer(BUFFER_SIZE_START);
}

CMonitorData::~CMonitorData()
{	
int x;

	for(x=0;x<m_count;x++)
	{
		delete m_data[x].discription;
		delete m_data[x].label;
	}

	delete m_buffer; 
	delete [] m_data;
}

void CMonitorData::resize_buffer(int new_size)
{
char *temp;

	if( m_buffer != nullptr )
	{
		temp = m_buffer;
		m_buffer = new char[ new_size];
		memcpy(m_buffer,temp,m_nbuffer);
		m_nbuffer = new_size;
		delete [] temp;
	}
	else
	{
		m_buffer = new char[ new_size];
		m_nbuffer = new_size;
	}
}

void CMonitorData::send(UdpConnection *con,int16_t & sequence,int16_t msg,char *data )
{
int len;
int size;
char *p;

	size = strlen(data)+1;
	p = (char *)malloc(size+6);
	len = 0;
	packShort(p+len, len, msg);
	packShort(p+len, len, m_sequence);
	packShort(p+len, len, size);
	memcpy(   p+len, data, size );
	con->Send(cUdpChannelReliable1, p, size + 6);
	free(p);
	sequence++;
}

bool CMonitorData::processHierarchyRequest( UdpConnection *con, int16_t & sequence )
{
int x,size;
char temp[215];

	if( m_count == 0 )
	{
		send(con,sequence,MON_MSG_REPLY_HIERARCHY,"NOTREADY");
		return 0;
	}

	if( m_sort )
	{
		qsort(m_data,m_count,sizeof(MON_ELEMENT),compar);
		m_sort = 0;
	}

	memset(m_buffer,0, 16);

	size = 0;
	for(x=0;x<m_count;x++)
	{
		snprintf(temp, sizeof(temp), "%s,%X,%d|", m_data[x].label, m_data[x].id, m_data[x].ping);
		size += strlen(temp);
		if( size >= m_nbuffer )
			resize_buffer(size+BUFFER_RESIZE_VALUE);
		strcat(m_buffer,temp);
	}
	send(con,sequence,MON_MSG_REPLY_HIERARCHY,m_buffer);	
	return 1;
}

bool CMonitorData::processElementsRequest(UdpConnection *con, int16_t & sequence, char * data, int /* dataLen */ , long lastUpdateTime )
{
char	tmp[200];
int		x,id;
int     size;
int		count;
char	**list;


	memset(m_buffer,0,16);

	if( !strcmp(data,"-1") )
	{
		size = 0;
		for (x=0;x<m_count;x++)
		{
			snprintf(tmp, sizeof(tmp), "%d %x|", m_data[x].id, m_data[x].value); 
			size += strlen(tmp);
			if( size >= m_nbuffer )
				resize_buffer(size+BUFFER_RESIZE_VALUE);
			strcat(m_buffer,tmp);
		}
		send(con,sequence,MON_MSG_REPLY_ELEMENTS,m_buffer);
		return 1;
	}

	if( !strcmp(data,"-2") )
	{
		size = 0;
		count = 0;
		for (x=0;x<m_count;x++)
		{
			if( lastUpdateTime <= m_data[x].ChangedTime )
			{
				snprintf(tmp, sizeof(tmp), "%d %x|",m_data[x].id,m_data[x].value); 
				size += strlen(tmp);
				if( size >= m_nbuffer )
				resize_buffer(size+BUFFER_RESIZE_VALUE);
				strcat(m_buffer,tmp);
				count++;
			}
		}
		if ( count )
		{
			send(con,sequence,MON_MSG_REPLY_ELEMENTS,m_buffer);
			return 1;
		}
		return 0;
	}

	
	
	list = new char * [ELEMENT_MAX];

	count = parseList( list, data,'|',ELEMENT_MAX);

	memset(m_buffer,0,16);
	
	size = 0;
	for(x=0;x<count;x++)
	{
		id = atoi(list[x]);
		
		for(x=0;x<m_count;x++)
			if( m_data[x].id == id )
			{
				snprintf(tmp, sizeof(tmp), "%d %x|", m_data[x].id, m_data[x].value);
				size += strlen(tmp);
				if( size >= m_nbuffer )
					resize_buffer(size+BUFFER_RESIZE_VALUE);
				strcat(m_buffer,tmp);
			}
	}

	delete [] list;

	if( count > 0 )
		send(con,sequence,MON_MSG_REPLY_ELEMENTS,m_buffer);

	return 0;
}


bool CMonitorData::processDescriptionRequest(UdpConnection *con, int16_t & sequence, char * userData, int dataLen, unsigned char *mark)
{
char line[4096];
char tmp[400];
int	 x,id,size;
int flag;
char *p;

	size = 0;
	memset(m_buffer,0,16);
	strcpy(line,userData);
	p = strtok(line,"|");
	flag = 0;
	if( strstr(p,"next") )
	{
		for(x=0;x<m_count;x++)
		{
			if( get_bit(mark,x) == 0 )
			{
				if( m_data[x].discription != nullptr )
					snprintf(tmp, sizeof(tmp), "%d,%s|", m_data[x].id, m_data[x].discription);
				else
					snprintf(tmp, sizeof(tmp), "%d, |", m_data[x].id);
				set_bit( mark, x );					
				size += strlen(tmp);
				if( size >= m_nbuffer )
					resize_buffer(size+BUFFER_RESIZE_VALUE);
				strcat(m_buffer,tmp);
				send(con,sequence,MON_MSG_REPLY_DESCRIPTION,m_buffer);
				return 1;
			}
		}
	}
	else
	{
		if( p )
		{
			size = 0;
			flag = 0;
			id = atoi(p);
			for(x=0;x<m_count;x++)
			{
				if( flag == 0 && id ==  m_data[x].id )
					flag = 1;

				if( flag )
				{
					flag = 2;

					if( m_data[x].discription != nullptr )
						snprintf(tmp, sizeof(tmp), "%d,%s|", m_data[x].id, m_data[x].discription);
					else
						snprintf(tmp, sizeof(tmp), "%d, |", m_data[x].id);

					set_bit(mark,x);					
					size += strlen(tmp);
					if( size >= m_nbuffer )
						resize_buffer(size+BUFFER_RESIZE_VALUE);
					strcat(m_buffer,tmp);
					if( size >= 2000 || m_data[x].discription )
					{
						send(con,sequence,MON_MSG_REPLY_DESCRIPTION,m_buffer);
						return 1;
					}
				}
			}
		}
	}


	if( flag  == 2 )
		send(con,sequence,MON_MSG_REPLY_DESCRIPTION,m_buffer);
	send(con,sequence,MON_MSG_REPLY_DESCRIPTION,"none");
	return 0;	
	
}

int CMonitorData::add(const char *label, int id, int ping, char *des )
{
int x;

	if( strlen(label) >= 127 )
	{
		printf("MonAPI Error: add() label exceeds length of 127. ->%s\n",label);
		return 0;
	}

	size_t desP1 = (strlen(des) + 1);

	for(x=0;x<m_count;x++)
	{
		if( id == m_data[x].id )
		{
			printf("MonAPI: assign new Id (%d) %s -> %s\n",id,m_data[x].label,label);
			m_data[x].ping = pingValue( ping );
			m_data[x].value = 0;
			m_data[x].id = id;

			//*******  Label  ***********
			delete m_data[x].label;
			m_data[x].label = new char [strlen(label)+1];
			strcpy(m_data[x].label,label);
			
			//*******  Discription ******
			delete [] m_data[x].discription;
			m_data[x].discription = nullptr;
			
			if( des )
			{
				m_data[x].discription = new char[desP1];
				strcpy(m_data[x].discription,des);
			}
			m_sort = 1;
			return 1;
		}

		if( !strcmp(label,m_data[x].label) )
		{
			printf("MonAPI ERROR: Label already found %s  Id: old(%d) new(%d).\n",label,m_data[x].id,id);
			return 0;
		}
	}

	if( m_max == m_count + 1 )
	{
		printf("MonitorObject: max element reached [%d].\n%s\nContact David Taylor (858) 577-3155.\n",m_max,label);
		return 0;
	}
	m_data[m_count].ping = pingValue( ping );
	m_data[m_count].value = 0;
	m_data[m_count].id = id;
	m_data[m_count].label = new char [strlen(label)+1];
	strcpy(m_data[m_count].label,label);
	if( des )
	{
		delete [] m_data[x].discription;
		m_data[x].discription = new char [strlen(des)+1];
		strcpy(m_data[x].discription,des);
	}
	m_count++;
	m_sort = 1;
	return 1;
}

int CMonitorData::setDescription( int Id, const char *Description , int & mode) 
{
int x;
	
	size_t sDescP1 = (strlen(Description) + 1);
	int retVal = -1;

	for(x=0;x<m_count;x++)
	{

		if( Description == nullptr )
		{
			delete [] m_data[x].discription;
			m_data[x].discription = nullptr;
			mode = 0;				
			retVal = x;
		}
		if( m_data[x].discription && !strcmp( m_data[x].discription, Description ) )
			retVal = - 1;

		delete [] m_data[x].discription;
		m_data[x].discription = new char [ sDescP1 ];
		strcpy(m_data[x].discription,Description);
		mode = 1;					
		retVal = x;
	}

	return retVal;
}

int CMonitorData::pingValue(int p)
{
	switch(p)
	{
		case MON_PING_15: return 15;
		case MON_PING_30: return 30; 
		case MON_PING_60: return 60;
		case MON_PING_5MIN: return (60 * 5);
		default:
			printf("MonAPI ERROR: add() function is not using defined constances.\n");
			printf("Please use one of the following:\n"); 
			printf("\tMON_PING_15\n\tMON_PING_30\n\tMON_PING_60\n\tMON_PING_5MIN\n");
			break;
	}
	return 0;
}

void CMonitorData::set(int Id, int value)
{
	for(int x=0;x<m_count;x++)
		if( Id == m_data[x].id )
		{
			if( m_data[x].ChangedTime == 0 ||  m_data[x].value != value )
				m_data[x].ChangedTime = time(nullptr);
			m_data[x].value = value;
			return;
		}
}

void CMonitorData::remove(int Id)
{
int x;

	if( m_count == 1 )
	{
		delete [] m_data[0].label;

		m_data[0].label = 0;
		m_data[0].id = 0;
		m_data[0].value = 0;;
		m_data[0].ping = 0;;
		m_sort	= 1;
		m_count--;
		return;
	}

	for(x=0;x<m_count;x++)
	{
		if( Id == m_data[x].id )
		{
			delete [] m_data[x].label;
			m_data[x].label = 0;
			if( x < m_count -1 )
			{
				memcpy(&m_data[x],&m_data[m_count-1],sizeof(MON_ELEMENT));
			}
			memset(&m_data[m_count-1],0,sizeof(MON_ELEMENT));
			m_sort = 1;
			m_count--;
			return;
		}
	}
}

int CMonitorData::parseList( char **list, char *data, char tok , int max )
{
int count;
int cnt;

	if( data == nullptr )
		return 0;

   list[0] = data;
   count = 1;
	cnt =0;
   while( cnt < max && *data > 0  )
   {
      if( *data == tok )
      {
        *data = 0;
        data++;
		cnt++;
        list[count] = data;
		if( *data )	count++;
      }
	cnt++;
	if( *data )	data++;
	}
   return count;
}


void CMonitorData::dump()
{
	if( m_sort )
	{
		qsort(m_data,m_count,sizeof(MON_ELEMENT),compar);
		m_sort = 0;
	}
	printf("********** Monitor API Dump *******************\n");
	printf("  Version: %d\n",CURRENT_API_VERSION);
	printf("\ncount: %d\n", m_count);
	printf("\t%-40s %8s\t%s\t%s\n","Label","Id","Ping","Value");
	for(int x=0;x<m_count;x++)
	{
		printf("\t%-40s % 8d\t%d\t%d\n",
			m_data[x].label,
			m_data[x].id,
			m_data[x].ping,
			m_data[x].value);
	}
	printf("***********************************************\n");
}


//////////////////////////////////////////////////////////////////////////////
// ------------------   Pack and unpack Routine  -----------------------------
///////////////////////////////////////////////////////////////////////////////

int packString(char *buffer, int & len, char * value)
{
	int size;
	size = strlen(value)+1;
	memcpy(buffer,value, size);
	len += size;
	return size;
}

int packByte(char *buffer, int & len, char value)
{

	buffer[0] = value;
	len++;
	return 1;
}

int packShort(char *buffer, int & len, int16_t value)
{
	char *p;

	p = (char *)&value;

#ifdef  _sun
	buffer[0] = p[1];
	buffer[1] = p[0];
#else
	buffer[0] = p[0];
	buffer[1] = p[1];
#endif


	len += 2;
	return 2;
}

int unpackShort(char *buffer, int & len, int16_t & value)
{
	char *p;
	p = (char *)&value;

#ifdef _sun
	p[1] = buffer[0];
	p[0] = buffer[1];

#else
	p[0] = buffer[0];
	p[1] = buffer[1];
#endif

	len += 2;
	return 2;
}

int unpackByte( char *buffer, int & len, char &value)
{
	value = buffer[0];
	len++;
	return 1;
}


