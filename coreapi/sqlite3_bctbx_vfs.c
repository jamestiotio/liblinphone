/*
sqlite3_bctbx_vfs.c
Copyright (C) 2016 Belledonne Communications SARL

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef SQLITE_STORAGE_ENABLED

#include "sqlite3_bctbx_vfs.h"
#include <sqlite3.h>

#ifndef _WIN32_WCE
#include <errno.h>
#endif /*_WIN32_WCE*/



/**
 * Closes the file whose file descriptor is stored in the file handle p.
 * @param  p 	sqlite3_file file handle pointer.
 * @return      SQLITE_OK if successful,  SQLITE_IOERR_CLOSE otherwise.
 */
static int sqlite3bctbx_Close(sqlite3_file *p){

	int ret;
	sqlite3_bctbx_file *pFile = (sqlite3_bctbx_file*) p;

	ret = bctbx_file_close(pFile->pbctbx_file);
	if (!ret){
		return SQLITE_OK;
	}
	else{
		free(pFile);
		return SQLITE_IOERR_CLOSE ;
	}
}


/**
 * Read count bytes from the open file given by p, starting at offset and puts them in 
 * the buffer pointed by buf.
 * Calls bctbx_file_read.
 * 
 * @param  p  		sqlite3_file file handle pointer.
 * @param  buf    	buffer to write the read bytes to.
 * @param  count  	number of bytes to read
 * @param  offset 	file offset where to start reading
 * @return 			SQLITE_OK if read bytes equals count,  
 *                  SQLITE_IOERR_SHORT_READ if the number of bytes read is inferior to count
 *                  SQLITE_IOERR_READ if an error occurred. 
 */
static int sqlite3bctbx_Read(sqlite3_file *p, void *buf, int count, sqlite_int64 offset){
	int ret;
	sqlite3_bctbx_file *pFile = (sqlite3_bctbx_file*) p;
	if (pFile){
		ret = bctbx_file_read(pFile->pbctbx_file, buf, count, offset);
		if( ret==count ){
			return SQLITE_OK;
		}
		else if( ret >= 0 ){

			return SQLITE_IOERR_SHORT_READ;
		}
		
		else {
			
			return SQLITE_IOERR_READ;
		}
	}
	return SQLITE_IOERR_READ;
}

/**
 * Writes directly to the open file given through the p argument.
 * Calls bctbx_file_write .
 * @param  p       sqlite3_file file handle pointer.
 * @param  buf     Buffer containing data to write
 * @param  count   Size of data to write in bytes
 * @param  offset  File offset where to write to
 * @return         SQLITE_OK on success, SQLITE_IOERR_WRITE if an error occurred.
 */
static int sqlite3bctbx_Write(sqlite3_file *p, const void *buf, int count, sqlite_int64 offset){
	sqlite3_bctbx_file *pFile = (sqlite3_bctbx_file*) p;
	int ret;
	if (pFile ){
		ret = bctbx_file_write(pFile->pbctbx_file, buf, count, offset);
		if(ret > 0 ) return SQLITE_OK;
		else {
			return SQLITE_IOERR_WRITE;
		}
	}
	return SQLITE_IOERR_WRITE;
}
		

/**
 * Saves the file size associated with the file handle p into the argument pSize.
 * @param  p 	sqlite3_file file handle pointer.
 * @return 		SQLITE_OK if read bytes equals count,  
 *              SQLITE_IOERR_FSTAT if the file size returned is negative 
 *              SQLITE_ERROR if an error occurred. 
 */
static int sqlite3bctbx_FileSize(sqlite3_file *p, sqlite_int64 *pSize){

	int rc;                         /* Return code from fstat() call */
	sqlite3_bctbx_file *pFile = (sqlite3_bctbx_file*) p;
	if (pFile->pbctbx_file){
		rc = bctbx_file_size(pFile->pbctbx_file);
		if( rc < 0 ) {
			return SQLITE_IOERR_FSTAT;
		}
		if (pSize){
			*pSize = rc;
			return SQLITE_OK;
		} 
	}
	return SQLITE_ERROR;


}


/************************ PLACE HOLDER FUNCTIONS ***********************/
/** These functions were implemented to please the SQLite VFS 
implementation. Some of them are just stubs, some do a very limited job. */


/**
 * Returns the device characteristics for the file. Stub function
 * to fill the SQLite VFS function pointer structure .
 * @param  p 	sqlite3_file file handle pointer.
 * @return		value 4096.
 */
static int sqlite3bctbx_DeviceCharacteristics(sqlite3_file *p){
	int rc = 0x00001000;
	return rc;
}

/**
 * Stub function for information and control over the open file.
 * @param  p    sqlite3_file file handle pointer.
 * @param  op   operation
 * @param  pArg unused
 * @return      SQLITE_OK on success, SALITE_NOTFOUND otherwise.
 */
static int sqlite3bctbx_FileControl(sqlite3_file *p, int op, void *pArg){
	if (op == SQLITE_FCNTL_MMAP_SIZE) return SQLITE_OK;
	return SQLITE_NOTFOUND;

}

/**
 * The lock file mechanism is not used with this VFS : checking
 * the reserved lock is always OK.
 * @param  pUnused sqlite3_file file handle pointer.
 * @param  pResOut set to 0 since there is no lock mechanism.
 * @return         SQLITE_OK
 */
static int sqlite3bctbx_nolockCheckReservedLock(sqlite3_file *pUnused, int *pResOut){
	*pResOut = 0;
	return SQLITE_OK;
}

/**
 * The lock file mechanism is not used with this VFS : locking the file 
 * is always OK.
 * @param  pUnused sqlite3_file file handle pointer.
 * @param  unused  unused
 * @return         SQLITE_OK
 */
static int sqlite3bctbx_nolockLock(sqlite3_file *pUnused, int unused){
	return SQLITE_OK;
}

/**
 * The lock file mechanism is not used with this VFS : unlocking the file
  * is always OK.
 * @param  pUnused sqlite3_file file handle pointer.
 * @param  unused  unused
 * @return         SQLITE_OK
 */
static int sqlite3bctbx_nolockUnlock(sqlite3_file *pUnused, int unused){
	return SQLITE_OK;
}


/**
 * Simply sync the file contents given through the file handle p
 * to the persistent media.
 * @param  p 	 sqlite3_file file handle pointer.
 * @param  flags unused
 * @return       SQLITE_OK on success, SLITE_IOERR_FSYNC if an error occurred.
 */
static int sqlite3bctbx_Sync(sqlite3_file *p, int flags){
  sqlite3_bctbx_file *pFile = (sqlite3_bctbx_file*)p;
  int rc;

  rc = fsync(pFile->pbctbx_file->fd);
  return (rc==0 ? SQLITE_OK : SQLITE_IOERR_FSYNC);
}


/**
 * Opens the file fName and populates the structure pointed by p
 * with the necessary io_methods
 * Methods not implemented for version 1 : xTruncate, xSectorSize.
 * Initializes some fields in the p structure, some of which where already
 * initialized by SQLite.
 * @param  pVfs      sqlite3_vfs VFS pointer.
 * @param  fName     filename
 * @param  p         file handle pointer
 * @param  flags     db file access flags 
 * @param  pOutFlags flags used by SQLite
 * @return           SQLITE_CANTOPEN on error, SQLITE_OK on success.
 */
static  int sqlite3bctbx_Open(sqlite3_vfs *pVfs, const char *fName, sqlite3_file *p, int flags, int *pOutFlags ){
	static const sqlite3_io_methods sqlite3_bctbx_io = {
		1,										/* iVersion         Structure version number */
		sqlite3bctbx_Close,                 	/* xClose */
		sqlite3bctbx_Read,                  	/* xRead */
		sqlite3bctbx_Write,                 	/* xWrite */
		0,										/*xTruncate*/
		sqlite3bctbx_Sync,
		sqlite3bctbx_FileSize,                
		sqlite3bctbx_nolockLock,
		sqlite3bctbx_nolockUnlock,
		sqlite3bctbx_nolockCheckReservedLock,
		sqlite3bctbx_FileControl,
		0,										/*xSectorSize*/
		sqlite3bctbx_DeviceCharacteristics,
	};

	sqlite3_bctbx_file * pFile = (sqlite3_bctbx_file*)p; /*File handle sqlite3_bctbx_file*/

	int openFlags = 0;

	/*returns error if filename is empty or file handle not initialized*/
	if (pFile == NULL || fName == NULL){
		return SQLITE_IOERR;
	}

	/* Set flags  to open the file with */
	if( flags&SQLITE_OPEN_EXCLUSIVE ) openFlags  |= O_EXCL;
	if( flags&SQLITE_OPEN_CREATE )    openFlags |= O_CREAT;
	if( flags&SQLITE_OPEN_READONLY )  openFlags |= O_RDONLY;
	if( flags&SQLITE_OPEN_READWRITE ) openFlags |= O_RDWR;

	pFile->pbctbx_file = bctbx_file_create_and_open2(bc_create_vfs(), fName, openFlags);
	if( pFile->pbctbx_file == NULL){
		return SQLITE_CANTOPEN;
	}

	if( pOutFlags ){
    	*pOutFlags = flags;
  	}
	pFile->base.pMethods = &sqlite3_bctbx_io;

	return SQLITE_OK;
}

/**
 * Returns a sqlite3_vfs pointer to the VFS named sqlite3bctbx_vfs 
 * implemented in this file.
 * Methods not implemented:
 *			xDelete 
 *			xAccess 
 *			xFullPathname 
 *			xDlOpen 
 *			xDlError 
 *			xDlSym 
 *			xDlClose 
 *			xRandomness 
 *			xSleep 
 *			xCurrentTime , xCurrentTimeInt64,
 *			xGetLastError
 *			xGetSystemCall
 *			xSetSystemCall
 *			xNextSystemCall
 *			To make the VFS available to SQLite
 * @return  Pointer to bctbx_vfs.
 */

sqlite3_vfs *sqlite3_bctbx_vfs_create(void){
  static sqlite3_vfs bctbx_vfs = {
    1,                            	/* iVersion */
    sizeof(sqlite3_bctbx_file),   	/* szOsFile */
    MAXPATHNAME,                  	/* mxPathname */
    0,                            	/* pNext */
    "sqlite3bctbx_vfs",              /* zName */
    0,                            	/* pAppData */
    sqlite3bctbx_Open,            	/* xOpen */
    0,                   			/* xDelete */
    0,                   			/* xAccess */
    0,             					/* xFullPathname */
    0,                   			/* xDlOpen */
    0,                  			/* xDlError */
    0,                   			/* xDlSym */
    0,                  			/* xDlClose */
    0,               				/* xRandomness */
    0,                    			/* xSleep */
    0,              				/* xCurrentTime */
  };
  return &bctbx_vfs;
}


/**
 * Registers sqlite3bctbx_vfs to SQLite VFS. If makeDefault is 1,
 * the VFS will be used by default.
 * Methods not implemented by sqlite3_bctbx_vfs are initialized to the one 
 * used by the unix-none VFS where all locking file operations are no-ops. 
 * @param  makeDefault  set to 1 to make the newly registered VFS be the default one, set to 0 instead.
 */
void sqlite3_bctbx_vfs_register( int makeDefault){
	sqlite3_vfs* pVfsToUse = sqlite3_bctbx_vfs_create();
	sqlite3_vfs* pDefault = sqlite3_vfs_find("unix-none");

	pVfsToUse->xAccess =  pDefault->xAccess;
	pVfsToUse->xCurrentTime = pDefault->xCurrentTime;

	pVfsToUse->xFullPathname = pDefault->xFullPathname;
	pVfsToUse->xDelete = pDefault->xDelete;
	pVfsToUse->xSleep = pDefault->xSleep;
	pVfsToUse->xRandomness = pDefault->xRandomness;
	pVfsToUse->xGetLastError = pDefault->xGetLastError; /* Not implemented by sqlite3 :place holder */
	/*Functions below should not be a problem sincve we are declaring ourselves
	 in version 1 */
	
	/* used in version 2 */
	pVfsToUse->xCurrentTimeInt64 = pDefault->xCurrentTimeInt64;
	/* used in version 3 */
	pVfsToUse->xGetSystemCall = pDefault->xGetSystemCall;
	pVfsToUse->xSetSystemCall = pDefault->xSetSystemCall;
	pVfsToUse->xNextSystemCall = pDefault->xNextSystemCall;

	sqlite3_vfs_register(pVfsToUse, makeDefault);

}


/**
 * Unregisters sqlite3bctbx_vfs from SQLite.
 */
void sqlite3_bctbx_vfs_unregister(void)
{
	sqlite3_vfs* pVfs = sqlite3_vfs_find("sqlite3bctbx_vfs");
	sqlite3_vfs_unregister(pVfs);
}

#endif


