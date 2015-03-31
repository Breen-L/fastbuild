// FileIO.cpp
//------------------------------------------------------------------------------

// Includes
//------------------------------------------------------------------------------
#include "Core/PrecompiledHeader.h"

#include "FileIO.h"
#include "FileStream.h"

// Core
#include "Core/FileIO/PathUtils.h"
#include "Core/Process/Thread.h"
#include "Core/Strings/AStackString.h"
#include "Core/Time/Timer.h"

// system
#if defined( __WINDOWS__ )
    #include <windows.h>
#endif
#if defined( __LINUX__ ) || defined( __APPLE__ )
    #include <dirent.h>
    #include <errno.h>
    #include <limits.h>
    #include <stdio.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

// Exists
//------------------------------------------------------------------------------
/*static*/ bool FileIO::FileExists( const char * fileName )
{
#if defined( __WINDOWS__ )
	// see if we can get attributes
	DWORD attributes = GetFileAttributes( fileName );
	if ( attributes == INVALID_FILE_ATTRIBUTES )
	{
		return false;
	}
	return true; // note this might not be file!
#elif defined( __LINUX__ ) || defined( __APPLE__ )
    struct stat st;
    if ( stat( fileName, &st ) == 0 )
    {
        if ( ( st.st_mode & S_IFDIR ) != S_IFDIR )
        {
            return true; // exists and is NOT a folder
        }
    }
    return false;
#else
    #error Unknown platform
#endif
}

// Delete
//------------------------------------------------------------------------------
/*static*/ bool FileIO::FileDelete( const char * fileName )
{
#if defined( __WINDOWS__ )
	BOOL result = DeleteFile( fileName );
	if ( result == FALSE )
	{
		return false; // failed to delete
	}
	return true; // delete ok
#elif defined( __LINUX__ ) || defined( __APPLE__ )
    return ( remove( fileName ) == 0 );
#else
    #error Unknown platform
#endif
}

// Copy
//------------------------------------------------------------------------------
/*static*/ bool FileIO::FileCopy( const char * srcFileName, const char * dstFileName,
							  bool allowOverwrite )
{
#if defined( __WINDOWS__ )
	BOOL failIfDestExists = ( allowOverwrite ? FALSE : TRUE );
	BOOL result = CopyFile( srcFileName, dstFileName, failIfDestExists );
	if ( result == FALSE )
	{
		// even if we allow overwrites, Windows will fail if the dest file
		// was read only, so we have to un-mark the read only status and try again
		if ( ( GetLastError() == ERROR_ACCESS_DENIED ) && ( allowOverwrite ) )
		{
			// see if dst file is read-only
			DWORD dwAttrs = GetFileAttributes( dstFileName );
			if ( dwAttrs == INVALID_FILE_ATTRIBUTES )
			{
				return false; // can't even get the attributes, nothing more we can do
			}
			if ( 0 == ( dwAttrs & FILE_ATTRIBUTE_READONLY ) ) 
			{ 
				return false; // file is not read only, so we don't know what the problem is
			}

			// try to remove read-only flag on dst file
			dwAttrs = ( dwAttrs & ~FILE_ATTRIBUTE_READONLY );
			if ( FALSE == SetFileAttributes( dstFileName, dwAttrs ) )
			{
				return false; // failed to remove read-only flag
			}

			// try to copy again
			result = CopyFile( srcFileName, dstFileName, failIfDestExists );
			return ( result == TRUE );
		}
	}

	return ( result == TRUE );
#elif defined( __APPLE__ )
    return false; // TODO:MAC Implement FileCopy
#elif defined( __LINUX__ )
    return false; // TODO:LINUX Implement FileCopy
#else
    #error Unknown platform
#endif
}

// FileMove
//------------------------------------------------------------------------------
/*static*/ bool FileIO::FileMove( const AString & srcFileName, const AString & dstFileName )
{
#if defined( __WINDOWS__ )
	return ( TRUE == ::MoveFileEx( srcFileName.Get(), dstFileName.Get(), MOVEFILE_REPLACE_EXISTING ) );
#elif defined( __LINUX__ ) || defined( __APPLE__ )
    return ( rename( srcFileName.Get(), dstFileName.Get() ) == 0 );
#else
    #error Unknown platform
#endif
}

// GetFiles
//------------------------------------------------------------------------------
/*static*/ bool FileIO::GetFiles( const AString & path,
								  const AString & wildCard,
							      bool recurse,
								  Array< AString > * results )
{
	ASSERT( results );

	size_t oldSize = results->GetSize();
	if ( recurse )
	{
		// make a copy of the path as it will be modified during recursion
		AStackString< 256 > pathCopy( path );
		PathUtils::EnsureTrailingSlash( pathCopy );
		GetFilesRecurse( pathCopy, wildCard, results );
	}
	else
	{
		GetFilesNoRecurse( path.Get(), wildCard.Get(), results );
	}

	return ( results->GetSize() != oldSize );
}

// GetFilesEx
//------------------------------------------------------------------------------
/*static*/ bool FileIO::GetFilesEx( const AString & path,
								  const AString & wildCard,
								  bool recurse,
								  Array< FileInfo > * results )
{
	ASSERT( results );

	size_t oldSize = results->GetSize();
	if ( recurse )
	{
		// make a copy of the path as it will be modified during recursion
		AStackString< 256 > pathCopy( path );
		PathUtils::EnsureTrailingSlash( pathCopy );
		GetFilesRecurseEx( pathCopy, wildCard, results );
	}
	else
	{
		GetFilesNoRecurseEx( path.Get(), wildCard.Get(), results );
	}

	return ( results->GetSize() != oldSize );
}

// GetFileInfo
//------------------------------------------------------------------------------
/*static*/ bool FileIO::GetFileInfo( const AString & fileName, FileIO::FileInfo & info )
{
    #if defined( __WINDOWS__ )
        WIN32_FILE_ATTRIBUTE_DATA fileAttribs;
        if ( GetFileAttributesEx( fileName.Get(), GetFileExInfoStandard, &fileAttribs ) ) 
        {
            info.m_Name = fileName;
            info.m_Attributes = fileAttribs.dwFileAttributes;
            info.m_LastWriteTime = (uint64_t)fileAttribs.ftLastWriteTime.dwLowDateTime | ( (uint64_t)fileAttribs.ftLastWriteTime.dwHighDateTime << 32 );
            info.m_Size = (uint64_t)fileAttribs.nFileSizeLow | ( (uint64_t)fileAttribs.nFileSizeHigh << 32 );
            return true;
        }
    #elif defined( __APPLE__ )
        ASSERT( false ); // TODO:MAC Implement GetFileInfo
    #elif defined( __LINUX__ )
        ASSERT( false ); // TODO:LINUX Implement GetFileInfo    
    #endif
    return false;
}

// GetCurrentDir
//------------------------------------------------------------------------------
/*static*/ bool FileIO::GetCurrentDir( AString & output )
{
    #if defined( __WINDOWS__ )
        char buffer[ MAX_PATH ];
        DWORD len = GetCurrentDirectory( MAX_PATH, buffer );
        if ( len != 0 )
        {
            output = buffer;
            return true;
        }
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        const size_t bufferSize( PATH_MAX );
        char buffer[ bufferSize ];
        if ( getcwd( buffer, bufferSize ) )
        {
            output = buffer;
            return true;
        }
    #else
        #error Unknown platform
    #endif
    return false;
}

// SetCurrentDir
//------------------------------------------------------------------------------
/*static*/ bool FileIO::SetCurrentDir( const AString & dir )
{
	#if defined( __WINDOWS__ )
		// Windows can have upper or lower case letters in the path for the drive
		// letter.  The case may be important for the user, but setting the current 
		// dir with only a change in case is ignored.
		// To ensure we have the requested case, we have to change dir to another
		// location, and then the location we want.

		// get another valid location to set as the dir
		// (we'll use the windows directory)
		char otherFolder[ 512 ];
		otherFolder[ 0 ] = 0;
		UINT len = ::GetWindowsDirectory( otherFolder, 512 );
		if ( ( len == 0 ) || ( len > 511 ) )
		{
			return false;
		}

		// handle the case where the user actually wants the windows dir
		if ( _stricmp( otherFolder, dir.Get() ) == 0 )
		{
			// use the root of the drive containing the windows dir
			otherFolder[ 3 ] = 0;
		}

		// set "other" dir
		if ( ::SetCurrentDirectory( otherFolder ) == FALSE )
		{
			return false;
		}

		// set the actual directory we want
		if ( ::SetCurrentDirectory( dir.Get() ) == TRUE )
		{
			return true;
		}
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        if ( chdir( dir.Get() ) == 0 )
        {
            return true;
        }
    #else
        #error Unknown platform
    #endif
	return false;
}

// GetTempDir
//------------------------------------------------------------------------------
/*static*/ bool FileIO::GetTempDir( AString & output )
{
    #if defined( __WINDOWS__ )
        char buffer[ MAX_PATH ];
        DWORD len = GetTempPath( MAX_PATH, buffer );
        if ( len != 0 )
        {
            output = buffer;
            return true;
        }
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        output = "/tmp/";
        return true;
    #else
        #error Unknown platform
    #endif
	return false;
}

// DirectoryCreate
//------------------------------------------------------------------------------
/*static*/ bool FileIO::DirectoryCreate( const AString & path )
{
    #if defined( __WINDOWS__ )
        if ( CreateDirectory( path.Get(), nullptr ) )
        {
            return true;
        }

        // it failed - is it because it exists already?
        if ( GetLastError() == ERROR_ALREADY_EXISTS )
        {
            return true;
        }
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        umask( 0 ); // disable default creation mask
        mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; // TODO:LINUX TODO:MAC Check these permissions
        if ( mkdir( path.Get(), mode ) == 0 )
        {
            return true; // created ok
        }
        
        // failed to create - already exists?
        if ( errno == EEXIST )
        {
            return true;
        }
    #else
        #error Unknown platform
    #endif

	// failed, probably missing intermediate folders or an invalid name
	return false;
}

// DirectoryExists
//------------------------------------------------------------------------------
/*static*/ bool FileIO::DirectoryExists( const AString & path )
{
    #if defined( __WINDOWS__ )
        DWORD res = GetFileAttributes( path.Get() );
        if ( ( res != INVALID_FILE_ATTRIBUTES ) &&
             ( ( res & FILE_ATTRIBUTE_DIRECTORY ) != 0 ) )
        {
            return true; // exists and is a folder
        }
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        struct stat st;
        if ( stat( path.Get(), &st ) == 0 )
        {
            if ( ( st.st_mode & S_IFDIR ) != 0 )
            {
                return true; // exists and is folder
            }
        }
    #else
        #error Unknown platform
    #endif
	return false; // doesn't exist, isn't a folder or some other problem
}

//------------------------------------------------------------------------------
/*static*/ bool FileIO::EnsurePathExists( const AString & path )
{
	// if the entire path already exists, nothing is to be done
	if( DirectoryExists( path ) )
	{
		return true;
	}

	// take a copy to locally manipulate
	AStackString<> pathCopy( path );
	PathUtils::FixupFolderPath( pathCopy ); // ensure correct slash type and termination

	// handle UNC paths by skipping leading slashes and machine name
    char * slash = pathCopy.Get();
	#if defined( __WINDOWS__ )
		if ( *slash == NATIVE_SLASH )
		{
			while ( *slash == NATIVE_SLASH ) { ++slash; } // skip leading double slash
			while ( *slash != NATIVE_SLASH ) { ++slash; } // skip machine name
			++slash; // move into first dir name, so next search will find dir name slash
		}
	#endif
	
    #if defined( __LINUX__ ) || defined( __APPLE__ )
        // for full paths, ignore the first slash
        if ( *slash == NATIVE_SLASH )
        {
            ++slash;
        }
    #endif

	slash = pathCopy.Find( NATIVE_SLASH, slash );
	if ( !slash )
	{
		return false;
	}
	do
	{
		// truncate the string to the sub path
		*slash = '\000';
		if ( DirectoryExists( pathCopy ) == false )
		{
			// create this level
			if ( DirectoryCreate( pathCopy ) == false )
			{
				return false; // something went wrong
			}
		}
		*slash = NATIVE_SLASH; // put back the slash
		slash = pathCopy.Find( NATIVE_SLASH, slash + 1 );
	}
	while ( slash );
	return true;
}

// CreateTempPath
//------------------------------------------------------------------------------
/*static*/ bool FileIO::CreateTempPath( const char * tempPrefix, AString & path )
{
    #if defined( __WINDOWS__ )
        // get the system temp path
        char tempPath[ MAX_PATH ];
        DWORD len = GetTempPath( MAX_PATH, tempPath );
        if ( len == 0 )
        {
            return false;
        }	

        // create a temp file in the temp folder
        char tempFile[ MAX_PATH ];
        UINT uniqueVal = GetTempFileName( tempPath,		// LPCTSTR lpPathName
                                          tempPrefix,	// LPCTSTR lpPrefixString
                                          0,			// UINT uUnique,
                                          tempFile );	// LPTSTR lpTempFileName
        if ( uniqueVal == 0 )
        {
            return false;
        }

        path = tempFile;
        return true;
    #elif defined( __APPLE__ )
        return false; // TODO:MAC Implement CreateTempDir
	#elif defined( __LINUX__ )
        return false; // TODO:LINUX Implement CreateTempDir
    #else
        #error Unknown platform
    #endif
}

// GetFileLastWriteTime
//------------------------------------------------------------------------------
/*static*/ uint64_t FileIO::GetFileLastWriteTime( const AString & fileName )
{
    #if defined( __WINDOWS__ )
        WIN32_FILE_ATTRIBUTE_DATA fileAttribs;
        if ( GetFileAttributesEx( fileName.Get(), GetFileExInfoStandard, &fileAttribs ) ) 
        {
            FILETIME ftWrite = fileAttribs.ftLastWriteTime; 
            uint64_t lastWriteTime = (uint64_t)ftWrite.dwLowDateTime | ( (uint64_t)ftWrite.dwHighDateTime << 32 ); 
            return lastWriteTime;
        }
    #elif defined( __APPLE__ )
        struct stat st;
        if ( stat( fileName.Get(), &st ) == 0 )
        {
            return ( ( (uint64_t)st.st_mtimespec.tv_sec * 1000000000ULL ) + (uint64_t)st.st_mtimespec.tv_nsec );
        }
	#elif defined( __LINUX__ )
        struct stat st;
        if ( stat( fileName.Get(), &st ) == 0 )
        {
            return ( ( (uint64_t)st.st_mtim.tv_sec * 1000000000ULL ) + (uint64_t)st.st_mtim.tv_nsec );
        }
    #else
        #error Unknown platform
    #endif
	return 0;
}

// SetFileLastWriteTime
//------------------------------------------------------------------------------
/*static*/ bool FileIO::SetFileLastWriteTime( const AString & fileName, uint64_t fileTime )
{
    #if defined( __WINDOWS__ )
        // open the file
        // TOOD:B Check these args
        HANDLE hFile = CreateFile( fileName.Get(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
        if( hFile == INVALID_HANDLE_VALUE )
        {
            return false;
        }

        // get the file time
        FILETIME ftWrite;
        ftWrite.dwLowDateTime = (uint32_t)( fileTime & 0x00000000FFFFFFFF );
        ftWrite.dwHighDateTime = (uint32_t)( ( fileTime & 0xFFFFFFFF00000000 ) >> 32 );
        if ( !SetFileTime( hFile, nullptr, nullptr, &ftWrite) ) // create, access, write
        {
            CloseHandle( hFile );
            return false;
        }

        // close the file
        CloseHandle( hFile );
    
        return true;
    #elif defined( __APPLE__ )
        return false; // TODO:MAC Implement SetFileLastWriteTime
	#elif defined( __LINUX__ )
        return false; // TODO:LINUX Implement SetFileLastWriteTime
    #else
        #error Unknown platform
    #endif
}

// SetReadOnly
//------------------------------------------------------------------------------
/*static*/ bool FileIO::SetReadOnly( const char * fileName, bool readOnly )
{
    #if defined( __WINDOWS__ )
        // see if dst file is read-only
        DWORD dwAttrs = GetFileAttributes( fileName );
        if ( dwAttrs == INVALID_FILE_ATTRIBUTES )
        {
            return false; // can't even get the attributes, nothing more we can do
        }

        // determine the new attributes
        DWORD dwNewAttrs = ( readOnly ) ? ( dwAttrs | FILE_ATTRIBUTE_READONLY )
                                        : ( dwAttrs & ~FILE_ATTRIBUTE_READONLY );

        // nothing to do if they are the same
        if ( dwNewAttrs == dwAttrs )
        {
            return true;
        }

        // try to set change
        if ( FALSE == SetFileAttributes( fileName, dwNewAttrs ) )
        {
            return false; // failed
        }

        return true;
    #elif defined( __APPLE__ )
        return false; // TODO:MAC Implement SetReadOnly
	#elif defined( __LINUX__ )
        return false; // TODO:LINUX Implement SetReadOnly
    #else
        #error Unknown platform
    #endif
}

// GetReadOnly
//------------------------------------------------------------------------------
/*static*/ bool FileIO::GetReadOnly( const AString & fileName )
{
    #if defined( __WINDOWS__ )
        // see if dst file is read-only
        DWORD dwAttrs = GetFileAttributes( fileName.Get() );
        if ( dwAttrs == INVALID_FILE_ATTRIBUTES )
        {
            return false; // can't even get the attributes, treat as not read only
        }

        // determine the new attributes
		bool readOnly = ( dwAttrs & FILE_ATTRIBUTE_READONLY );
        return readOnly;
    #elif defined( __APPLE__ )
        return false; // TODO:MAC Implement GetReadOnly
	#elif defined( __LINUX__ )
        return false; // TODO:LINUX Implement GetReadOnly
    #else
        #error Unknown platform
    #endif
}

// GetFilesRecurse
//------------------------------------------------------------------------------
/*static*/ void FileIO::GetFilesRecurse( AString & pathCopy, 
										 const AString & wildCard,
										 Array< AString > * results )
{
    const uint32_t baseLength = pathCopy.GetLength();
    
    #if defined( __WINDOWS__ )
        pathCopy += '*'; // don't want to use wildcard to filter folders

        // recurse into directories
        WIN32_FIND_DATA findData;
        HANDLE hFind = FindFirstFileEx( pathCopy.Get(), FindExInfoBasic, &findData, FindExSearchLimitToDirectories, nullptr, 0 );
        if ( hFind == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                // ignore magic '.' and '..' folders
                // (don't need to check length of name, as all names are at least 1 char
                // which means index 0 and 1 are valid to access)
                if ( findData.cFileName[ 0 ] == '.' &&
                    ( ( findData.cFileName[ 1 ] == '.' ) || ( findData.cFileName[ 1 ] == '\000' ) ) )
                {
                    continue;
                }

                pathCopy.SetLength( baseLength );
                pathCopy += findData.cFileName;
                pathCopy += NATIVE_SLASH;
                GetFilesRecurse( pathCopy, wildCard, results );
            }
        }
        while ( FindNextFile( hFind, &findData ) != 0 );
        FindClose( hFind );

        // do files in this directory
        pathCopy.SetLength( baseLength );
        pathCopy += '*';
        hFind = FindFirstFileEx( pathCopy.Get(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, 0 );
        if ( hFind == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                continue;
            }

			if ( PathUtils::IsWildcardMatch( wildCard.Get(), findData.cFileName ) )
			{
				pathCopy.SetLength( baseLength );
				pathCopy += findData.cFileName;
				results->Append( pathCopy );
			}
        }
        while ( FindNextFile( hFind, &findData ) != 0 );

        FindClose( hFind );
        
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        DIR * dir = opendir( pathCopy.Get() );
        if ( dir == nullptr )
        {
            return;
        }
        for ( ;; )
        {
            dirent * entry = readdir( dir );
            if ( entry == nullptr )
            {
                break; // no more entries
            }
            
            // dir?
            if ( ( entry->d_type & DT_DIR ) == DT_DIR )
            {
                // ignore . and ..
                if ( entry->d_name[ 0 ] == '.' )
                {
                    if ( ( entry->d_name[ 1 ] == 0 ) ||
                         ( ( entry->d_name[ 1 ] == '.' ) && ( entry->d_name[ 2 ] == 0 ) ) )
                    {
                        continue;
                    }
                }

                // regular dir
                pathCopy.SetLength( baseLength );
                pathCopy += entry->d_name;
                pathCopy += NATIVE_SLASH;
                GetFilesRecurse( pathCopy, wildCard, results ); 
                continue;
            }
            
            // file - does it match wildcard?
			if ( PathUtils::IsWildcardMatch( wildCard.Get(), entry->d_name ) )
            {
                pathCopy.SetLength( baseLength );
                pathCopy += entry->d_name;
                results->Append( pathCopy );
            }            
        }
        closedir( dir );
    #else
        #error Unknown platform
    #endif
}

// GetFilesNoRecurse
//------------------------------------------------------------------------------
/*static*/ void FileIO::GetFilesNoRecurse( const char * path, 
										   const char * wildCard,
										   Array< AString > * results )
{
    AStackString< 256 > pathCopy( path );
    PathUtils::EnsureTrailingSlash( pathCopy );
    const uint32_t baseLength = pathCopy.GetLength();
    
    #if defined( __WINDOWS__ )
        pathCopy += '*';

        WIN32_FIND_DATA findData;
        //HANDLE hFind = FindFirstFile( pathCopy.Get(), &findData );
        HANDLE hFind = FindFirstFileEx( pathCopy.Get(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, 0 );
        if ( hFind == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                continue;
            }

			if ( PathUtils::IsWildcardMatch( wildCard, findData.cFileName ) )
			{
				pathCopy.SetLength( baseLength );
				pathCopy += findData.cFileName;
				results->Append( pathCopy );
			}
        }
        while ( FindNextFile( hFind, &findData ) != 0 );

        FindClose( hFind );
    
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        DIR * dir = opendir( pathCopy.Get() );
        if ( dir == nullptr )
        {
            return;
        }
        for ( ;; )
        {
            dirent * entry = readdir( dir );
            if ( entry == nullptr )
            {
                break; // no more entries
            }
            
            // dir?
            if ( ( entry->d_type & DT_DIR ) == DT_DIR )
            {
                // ignore dirs
                continue;
            }
            
            // file - does it match wildcard?
			if ( PathUtils::IsWildcardMatch( wildCard, entry->d_name ) )
            {
                pathCopy.SetLength( baseLength );
                pathCopy += entry->d_name;
                results->Append( pathCopy );
            }            
        }
        closedir( dir );
    #else
        #error Unknown platform
    #endif
}


// GetFilesRecurse
//------------------------------------------------------------------------------
/*static*/ void FileIO::GetFilesRecurseEx( AString & pathCopy, 
										 const AString & wildCard,
										 Array< FileInfo > * results )
{
    const uint32_t baseLength = pathCopy.GetLength();
    
    #if defined( __WINDOWS__ )
        pathCopy += '*'; // don't want to use wildcard to filter folders

        // recurse into directories
        WIN32_FIND_DATA findData;
        HANDLE hFind = FindFirstFileEx( pathCopy.Get(), FindExInfoBasic, &findData, FindExSearchLimitToDirectories, nullptr, 0 );
        if ( hFind == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                // ignore magic '.' and '..' folders
                // (don't need to check length of name, as all names are at least 1 char
                // which means index 0 and 1 are valid to access)
                if ( findData.cFileName[ 0 ] == '.' &&
                    ( ( findData.cFileName[ 1 ] == '.' ) || ( findData.cFileName[ 1 ] == '\000' ) ) )
                {
                    continue;
                }

                pathCopy.SetLength( baseLength );
                pathCopy += findData.cFileName;
                pathCopy += NATIVE_SLASH;
                GetFilesRecurseEx( pathCopy, wildCard, results );
            }
        }
        while ( FindNextFile( hFind, &findData ) != 0 );
        FindClose( hFind );

        // do files in this directory
        pathCopy.SetLength( baseLength );
        pathCopy += '*';
        hFind = FindFirstFileEx( pathCopy.Get(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, 0 );
        if ( hFind == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                continue;
            }

			if ( PathUtils::IsWildcardMatch( wildCard.Get(), findData.cFileName ) )
			{
				pathCopy.SetLength( baseLength );
				pathCopy += findData.cFileName;
				if ( results->GetSize() == results->GetCapacity() )
				{
					results->SetCapacity( results->GetSize() * 2 );
				}
				results->SetSize( results->GetSize() + 1 );
				FileInfo & newInfo = results->Top();
				newInfo.m_Name = pathCopy;
				newInfo.m_Attributes = findData.dwFileAttributes;
				newInfo.m_LastWriteTime = (uint64_t)findData.ftLastWriteTime.dwLowDateTime | ( (uint64_t)findData.ftLastWriteTime.dwHighDateTime << 32 );
				newInfo.m_Size = (uint64_t)findData.nFileSizeLow | ( (uint64_t)findData.nFileSizeHigh << 32 );
			}
        }
        while ( FindNextFile( hFind, &findData ) != 0 );

        FindClose( hFind );
        
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        DIR * dir = opendir( pathCopy.Get() );
        if ( dir == nullptr )
        {
            return;
        }
        for ( ;; )
        {
            dirent * entry = readdir( dir );
            if ( entry == nullptr )
            {
                break; // no more entries
            }
            
            // dir?
            if ( ( entry->d_type & DT_DIR ) == DT_DIR )
            {
                // ignore . and ..
                if ( entry->d_name[ 0 ] == '.' )
                {
                    if ( ( entry->d_name[ 1 ] == 0 ) ||
                         ( ( entry->d_name[ 1 ] == '.' ) && ( entry->d_name[ 2 ] == 0 ) ) )
                    {
                        continue;
                    }
                }

                // regular dir
                pathCopy.SetLength( baseLength );
                pathCopy += entry->d_name;
                pathCopy += NATIVE_SLASH;
                GetFilesRecurseEx( pathCopy, wildCard, results ); 
                continue;
            }
            
            // file - does it match wildcard?
			if ( PathUtils::IsWildcardMatch( wildCard.Get(), entry->d_name ) )
            {
                pathCopy.SetLength( baseLength );
                pathCopy += entry->d_name;
                
				if ( results->GetSize() == results->GetCapacity() )
				{
					results->SetCapacity( results->GetSize() * 2 );
				}
                results->SetSize( results->GetSize() + 1 );
                FileInfo & newInfo = results->Top();
                newInfo.m_Name = pathCopy;
                
                // get additional info
                struct stat info;
                VERIFY( stat( pathCopy.Get(), &info ) == 0 );
                newInfo.m_Attributes = info.st_mode;
				#if defined( __APPLE__ )
					newInfo.m_LastWriteTime = 0; // TODO:MAC Implement
				#else
	                newInfo.m_LastWriteTime = ( ( (uint64_t)info.st_mtim.tv_sec * 1000000000ULL ) + (uint64_t)info.st_mtim.tv_nsec );
				#endif
                newInfo.m_Size = info.st_size;
            }            
        }
        closedir( dir );        
    #else
        #error Unknown platform
    #endif
}

// GetFilesNoRecurseEx
//------------------------------------------------------------------------------
/*static*/ void FileIO::GetFilesNoRecurseEx( const char * path, 
										   const char * wildCard,
										   Array< FileInfo > * results )
{
    AStackString< 256 > pathCopy( path );
    PathUtils::EnsureTrailingSlash( pathCopy );
    const uint32_t baseLength = pathCopy.GetLength();
        
    #if defined( __WINDOWS__ )
        pathCopy += '*';

        WIN32_FIND_DATA findData;
        HANDLE hFind = FindFirstFileEx( pathCopy.Get(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, 0 );
        if ( hFind == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                continue;
            }

			if ( PathUtils::IsWildcardMatch( wildCard, findData.cFileName ) )
			{
				pathCopy.SetLength( baseLength );
				pathCopy += findData.cFileName;

				if ( results->GetSize() == results->GetCapacity() )
				{
					results->SetCapacity( results->GetSize() * 2 );
				}
				results->SetSize( results->GetSize() + 1 );
				FileInfo & newInfo = results->Top();
				newInfo.m_Name = pathCopy;
				newInfo.m_Attributes = findData.dwFileAttributes;
				newInfo.m_LastWriteTime = (uint64_t)findData.ftLastWriteTime.dwLowDateTime | ( (uint64_t)findData.ftLastWriteTime.dwHighDateTime << 32 );
				newInfo.m_Size = (uint64_t)findData.nFileSizeLow | ( (uint64_t)findData.nFileSizeHigh << 32 );
			}
        }
        while ( FindNextFile( hFind, &findData ) != 0 );

        FindClose( hFind );
    
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        DIR * dir = opendir( pathCopy.Get() );
        if ( dir == nullptr )
        {
            return;
        }
        for ( ;; )
        {
            dirent * entry = readdir( dir );
            if ( entry == nullptr )
            {
                break; // no more entries
            }
            
            // dir?
            if ( ( entry->d_type & DT_DIR ) == DT_DIR )
            {
                // ingnore dirs
                continue;
            }
            
            // file - does it match wildcard?
			if ( PathUtils::IsWildcardMatch( wildCard, entry->d_name ) )
            {
                pathCopy.SetLength( baseLength );
                pathCopy += entry->d_name;
                
				if ( results->GetSize() == results->GetCapacity() )
				{
					results->SetCapacity( results->GetSize() * 2 );
				}
                results->SetSize( results->GetSize() + 1 );
                FileInfo & newInfo = results->Top();
                newInfo.m_Name = pathCopy;
                
                // get additional info
                struct stat info;
                VERIFY( stat( pathCopy.Get(), &info ) == 0 );
                newInfo.m_Attributes = info.st_mode;
				#if defined( __APPLE__ )
					newInfo.m_LastWriteTime = 0;
				#else
	                newInfo.m_LastWriteTime = ( ( (uint64_t)info.st_mtim.tv_sec * 1000000000ULL ) + (uint64_t)info.st_mtim.tv_nsec );
				#endif
                newInfo.m_Size = info.st_size;
            }            
        }
        closedir( dir );
    #else
        #error Unknown platform
    #endif
}

// WorkAroundForWindowsFilePermissionProblem
//------------------------------------------------------------------------------
#if defined( __WINDOWS__ )
    /*static*/ void FileIO::WorkAroundForWindowsFilePermissionProblem( const AString & fileName )
    {
        // Sometimes after closing a file, subsequent operations on that file will
        // fail.  For example, trying to set the file time, or even another process
        // opening the file.
        //
        // This seems to be a known issue in windows, with multiple potential causes
        // like Virus scanners and possibly the behaviour of the kernel itself.
        //
        // A work-around for this problem is to attempt to open a file we just closed.
        // This will sometimes fail, but if we retry until it succeeds, we avoid the
        // problem on the subsequent operation.
        FileStream f;
        Timer timer;
        while ( f.Open( fileName.Get() ) == false )
        {
            Thread::Sleep( 1 );

            // timeout so we don't get stuck in here forever
            if ( timer.GetElapsed() > 1.0f )
            {
                ASSERT( false && "WorkAroundForWindowsFilePermissionProblem Failed!" );
                return;
            }
        }
        f.Close();
    }
#endif

// FileInfo::IsReadOnly
//------------------------------------------------------------------------------
bool FileIO::FileInfo::IsReadOnly() const
{
    #if defined( __WINDOWS__ )
		return ( ( m_Attributes & FILE_ATTRIBUTE_READONLY ) == FILE_ATTRIBUTE_READONLY );
	#elif defined( __LINUX__ ) || defined( __APPLE__ )
        return ( ( m_Attributes & S_IWUSR ) == 0 );// TODO:LINUX TODO:MAC Is this the correct behaviour?
	#else
        #error Unknown platform
	#endif
}

//------------------------------------------------------------------------------