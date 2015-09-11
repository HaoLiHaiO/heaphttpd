#include <stdio.h>
#include "stgengine.h"

StorageEngine::StorageEngine(const char * host, const char* username, const char* password, const char* database, const char* encoding, const char* private_path, int maxConn)
{
    m_host = host;
	m_username = username;
	m_password = password;
	m_database = database;
	
	m_encoding = encoding;
	m_private_path = private_path;
	
	m_maxConn = maxConn;
	m_realConn = 0;
	m_engine = new stStorageEngine[m_maxConn];
	
	for(int i = 0; i < m_maxConn; i++)
	{
		m_engine[i].storage = new DBStorage(m_encoding.c_str(), m_private_path.c_str());
		if(m_engine[i].storage
            && m_engine[i].storage->Connect(m_host.c_str(), m_username.c_str(), m_password.c_str(), m_database.c_str()) == 0)
		{
            m_engine[i].opened = TRUE;
            m_engine[i].inUse = FALSE;
            m_engine[i].cTime = time(NULL);
            m_engine[i].lTime = m_engine[i].cTime;
            m_engine[i].usedCount = 0;
            m_engine[i].owner = 0;
            m_realConn++;
		}
	}
	pthread_mutex_init(&m_engineMutex, NULL);
}

StorageEngine::~StorageEngine()
{
	pthread_mutex_lock(&m_engineMutex);
	for(int i = 0; i < m_maxConn; i++)
	{
		if(m_engine[i].storage)
		{
			m_engine[i].storage->Close();
			delete m_engine[i].storage;
		}
	}
	if(m_engine)
		delete[] m_engine;
	pthread_mutex_unlock(&m_engineMutex);
	pthread_mutex_destroy(&m_engineMutex);
}

DBStorage* StorageEngine::WaitEngine(int &index)
{
    unsigned int lastMinCount = 0xFFFFFFFF;
	int i = 0;
    index = 0;
	DBStorage* freeOne = NULL;
	pthread_mutex_lock(&m_engineMutex);
	for(i = 0; i < m_maxConn; i++)
	{
		if(m_engine[i].opened == TRUE && m_engine[i].storage != NULL)
		{
            if(lastMinCount > m_engine[i].usedCount)
            {
                lastMinCount = m_engine[i].usedCount;
                index = i;
            }
		}
	}
    
    m_engine[index].inUse = TRUE;
    m_engine[index].lTime = time(NULL);
    m_engine[index].owner = pthread_self();
    m_engine[index].usedCount++;
    freeOne = m_engine[index].storage;
    
	pthread_mutex_unlock(&m_engineMutex);
    
	if(m_engine[index].storage->Ping() < 0)
    {
        printf("Reconnect\n");
        m_engine[index].storage->Close();
        if(m_engine[index].storage->Connect(m_host.c_str(), m_username.c_str(), m_password.c_str(), m_database.c_str()) < 0)
            freeOne = NULL;
    }
	return freeOne;
}

void StorageEngine::PostEngine(int index)
{
	pthread_mutex_lock(&m_engineMutex);
	m_engine[index].lTime = time(NULL);
	m_engine[index].inUse = FALSE;
	m_engine[index].owner = 0;
    m_engine[index].usedCount--;
	pthread_mutex_unlock(&m_engineMutex);
}
	
