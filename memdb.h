#ifndef _MEMDB_H
#define _MEMDB_H

#include <stdio.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include "memdberr.h"
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#define FPOS( type, field ) ( (int)&(( type *) 0)-> field )
#define FLEN( type, field ) ( sizeof((( type *) 0)-> field ))
#define SetKeyMemDB(p,type,field,attr,sortattr) ( SetTableKey(p,FPOS(type,field),FLEN(type,field),attr,sortattr) )
#define DeclareCursorMemDB(p,type,field,value) ( DeclareTableKey(p,FPOS(type,field),FLEN(type,field),(void*)value) )
#define MEMTRIM(field, ch) {CommonRTrim(field); memset(field + strlen(field), ch, sizeof(field) - strlen(field));}

#define MAX_FETCHKEY_NUM      10
#define MAX_FETCHVALUE_LEN    256
#define MAX_MEMDB_NUM         50

#define FIELD_INT         1
#define FIELD_CHAR        2

#define FIELD_ASC         0   /* Default */
#define FIELD_DES         1

typedef struct
{
    int iKeyOffSet;       /* The Key field offset */
    int iKeyLen;          /* The Key field len */
    char sValue[MAX_FETCHVALUE_LEN];  /* The Value of Fetch Key */
}FETCH_KEY_DEF;

typedef struct
{
    int iIpcKey;       /* The key of ShareMemory and Semphore */
    int iShmId;        /* The Id of ShareMemory */
    int iSemId;        /* The Id of Semphore */
    char *pShmArea;    /* The Point to ShareMemory */
}MEMDATABASE;

typedef struct
{
    long lFetchNum;    /* The Index of Fetch */
    int iFetchKeyNum;  /* The Fetch Key Number */
    FETCH_KEY_DEF stuFetchKeyBuf[MAX_FETCHKEY_NUM];   /* The Declare Fetch Key Define */
}MEMDBFETCH;

MEMDATABASE  gMemDb[MAX_MEMDB_NUM];

int CreateMemDB(MEMDATABASE *pMemDB,char *pTableName,long iIpcKey,long lMaxRecord,int RecordLen, int iDoubleMemBufFlag);
int DropMemDB(MEMDATABASE *pMemDB);
int AttachMemDB(MEMDATABASE *pMemDB,long iIpcKey);
int FetchCursorMemDB(MEMDATABASE *pMemDB, MEMDBFETCH *pMemDbFetch, char *pInBuffer);
int CloseCursorMemDB(MEMDATABASE *pMemDB);
int SelectOneMemDB(MEMDATABASE *pMemDB,char *pInBuffer);
int SelectMemDB(MEMDATABASE *pMemDB,char *pInBuffer);
int UpdateMemDB(MEMDATABASE *pMemDB,char *pInBuffer);
int DeleteMemDB(MEMDATABASE *pMemDB,char *pInBuffer);
int DeleteOneMemDB(MEMDATABASE *pMemDB,char *pInBuffer);
int InsertMemDB(MEMDATABASE *pMemDB,char *pInBuffer);
void TruncateMemDB(MEMDATABASE *pMemDB);
/*int   gMemFunNum1=5;  del by wgj 20120227*/

#endif
