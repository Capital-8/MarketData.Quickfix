/****************************************************************************
** Copyright (c) 2001-2014
**
** This file is part of the QuickFIX FIX Engine
**
** This file may be distributed under the terms of the quickfixengine.org
** license as defined by quickfixengine.org and appearing in the file
** LICENSE included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.quickfixengine.org/LICENSE for licensing information.
**
** Contact ask@quickfixengine.org if any conditions of this licensing are
** not clear to you.
**
****************************************************************************/

#ifdef _MSC_VER
#include "stdafx.h"
#else
#include "config.h"
#endif

#include "FileLog.h"

namespace FIX
{
Log* FileLogFactory::create()
{
  m_globalLogCount++;
  if( m_globalLogCount > 1 ) return m_globalLog;

  try
  {
	bool logMessages = true;
	Dictionary settings = m_settings.get();

	if (settings.has(FILE_LOG_LOG_MESSAGES))
		logMessages = settings.getBool(FILE_LOG_LOG_MESSAGES);

	if ( m_path.size() ) return new FileLog( m_path, logMessages );

    std::string path;
    std::string backupPath;
    path = settings.getString( FILE_LOG_PATH );
    backupPath = path;
    if( settings.has( FILE_LOG_BACKUP_PATH ) )
      backupPath = settings.getString( FILE_LOG_BACKUP_PATH );

    return m_globalLog = new FileLog( path, backupPath, logMessages );
  }
  catch( ConfigError& )
  {
  m_globalLogCount--;
  throw;  
  }
}

Log* FileLogFactory::create( const SessionID& s )
{
  bool logMessages = true;
  Dictionary settings = m_settings.get(s);
  if (settings.has(FILE_LOG_LOG_MESSAGES))
	  logMessages = settings.getBool(FILE_LOG_LOG_MESSAGES);

  if ( m_path.size() && m_backupPath.size() )
    return new FileLog( m_path, m_backupPath, s , logMessages);
  if ( m_path.size() ) 
    return new FileLog( m_path, s , logMessages);

  std::string path;
  std::string backupPath;
  path = settings.getString( FILE_LOG_PATH );
  backupPath = path;
  if( settings.has( FILE_LOG_BACKUP_PATH ) )
    backupPath = settings.getString( FILE_LOG_BACKUP_PATH );

  return new FileLog( path, backupPath, s, logMessages );
}

void FileLogFactory::destroy( Log* pLog )
{
  if( pLog == m_globalLog )
  {
    m_globalLogCount--;
    if( m_globalLogCount == 0 )
    {
      delete pLog;
      m_globalLogCount = 0;
    }  
  }
  else
  {
    delete pLog;
  }
}

FileLog::FileLog( const std::string& path, bool logMessages)
: m_millisecondsInTimeStamp( true ),
m_logMessages(logMessages)
{
  init( path, path, "GLOBAL" );
}

FileLog::FileLog( const std::string& path, const std::string& backupPath, bool logMessages)
: m_millisecondsInTimeStamp( true ),
m_logMessages(logMessages)
{
  init( path, backupPath, "GLOBAL" );
}

FileLog::FileLog( const std::string& path, const SessionID& s, bool logMessages)
: m_millisecondsInTimeStamp( true ),
m_logMessages(logMessages)
{
  init( path, path, generatePrefix(s) );
}

FileLog::FileLog( const std::string& path, const std::string& backupPath, const SessionID& s, bool logMessages)
: m_millisecondsInTimeStamp( true ),
m_logMessages(logMessages)
{
  init( path, backupPath, generatePrefix(s) );
}

std::string FileLog::generatePrefix( const SessionID& s )
{
  const std::string& begin =
    s.getBeginString().getString();
  const std::string& sender =
    s.getSenderCompID().getString();
  const std::string& target =
    s.getTargetCompID().getString();
  const std::string& qualifier =
    s.getSessionQualifier();

  std::string prefix = begin + "-" + sender + "-" + target;
  if( qualifier.size() )
    prefix += "-" + qualifier;

  return prefix;
}

void FileLog::init( std::string path, std::string backupPath, const std::string& prefix )
{  
  file_mkdir( path.c_str() );
  file_mkdir( backupPath.c_str() );

  if ( path.empty() ) path = ".";
  if ( backupPath.empty() ) backupPath = path;

  m_fullPrefix
    = file_appendpath(path, prefix + ".");
  m_fullBackupPrefix
    = file_appendpath(backupPath, prefix + ".");

  if (m_logMessages)
  {
	  m_messagesFileName = m_fullPrefix + "messages.current.log";
	  m_messages.open(m_messagesFileName.c_str(), std::ios::out | std::ios::app);
	  if (!m_messages.is_open()) throw ConfigError("Could not open messages file: " + m_messagesFileName);
  }

  m_eventFileName = m_fullPrefix + "event.current.log";
  m_event.open( m_eventFileName.c_str(), std::ios::out | std::ios::app );
  if ( !m_event.is_open() ) throw ConfigError( "Could not open event file: " + m_eventFileName );
}

FileLog::~FileLog()
{
  if (m_logMessages)
    m_messages.close();

  m_event.close();
}

void FileLog::clear()
{
  if (m_logMessages)
    m_messages.close();

  m_event.close();

  if (m_logMessages)
    m_messages.open( m_messagesFileName.c_str(), std::ios::out | std::ios::trunc );

  m_event.open( m_eventFileName.c_str(), std::ios::out | std::ios::trunc );
}

void FileLog::backup()
{
  if (m_logMessages)
    m_messages.close();

  m_event.close();

  int i = 0;
  while( true )
  {
    std::stringstream messagesFileName;
    std::stringstream eventFileName;
 
    messagesFileName << m_fullBackupPrefix << "messages.backup." << ++i << ".log";
    eventFileName << m_fullBackupPrefix << "event.backup." << i << ".log";

	FILE* messagesLogFile = nullptr;
    if (m_logMessages)
      messagesLogFile = file_fopen(messagesFileName.str().c_str(), "r");

    FILE* eventLogFile = file_fopen( eventFileName.str().c_str(), "r" );

    if (((!m_logMessages) || messagesLogFile == NULL) && eventLogFile == NULL)
    {
      if (m_logMessages)
        file_rename( m_messagesFileName.c_str(), messagesFileName.str().c_str() );

      file_rename( m_eventFileName.c_str(), eventFileName.str().c_str() );

      if (m_logMessages)
        m_messages.open( m_messagesFileName.c_str(), std::ios::out | std::ios::trunc );

      m_event.open( m_eventFileName.c_str(), std::ios::out | std::ios::trunc );
      return;
    }
    
    if( messagesLogFile != NULL ) file_fclose( messagesLogFile );
    if( eventLogFile != NULL ) file_fclose( eventLogFile );
  }
}

} //namespace FIX
