#include "memdb.h"
#include <errno.h>
#include <string.h>

#define HEAD_TABLE_NAME_LEN    64
#define HEAD_RESERVED_LEN      128
#define MDB_IPC_MODE           0660
#define MAX_KEY_NUM            10
#define COMPARE_ALL            0
#define COMPARE_FIRST          1


typedef struct
{
	int iKeyOffSet;    /* key的偏移量,根据这个偏移量在一块内存中来找到key */
	int iKeyLen;       /* key的长度 */
	int iKeyAttr;      /* key的类型,int 类型 或者 char类型 */
	int iKeySortAttr;  /* 排序的方式,是正序排序,还是倒序排序 */
}TABLE_KEY_DEF;

typedef struct
{
	char sTableName[HEAD_TABLE_NAME_LEN]; /* 表的名称*/
	int  iRecordLen;      /* 每条记录的长度 每条记录都是一个结构体 */
	long lMaxRecordNum;   /* 最大数据条数,根据iRecordLen 和LMaxRecordNum 来开辟初始的内存大小 */
	int  iDoubleMemBufFlag; /*是否创建备库1 创建,0 不创建*/
	int  iCurrentMem;	/*当前指向的内存*/
	int  iKeyNum;         /* key的数量 */
	TABLE_KEY_DEF stuTableKeyBuf[MAX_KEY_NUM];   /* key的定义 */
}TABLE_HEAD_DEF;

typedef struct {
	long lRecordNum;      /* 当前记录条数 */
} MEM_HEAD_DEF;

static TABLE_HEAD_DEF stuTableHeadBuf;
static MEM_HEAD_DEF stuMemHeadBuf;

//创建数据库
int CreateMemDB(MEMDATABASE *pMemDb, char *pTableName, long iIpcKey, long lMaxRecord, int RecordLen, int iDoubleMemBufFlag)
{
	int         iRet;
	int         iMultiple = 1;

	memset((char*)&stuTableHeadBuf, 0, sizeof(TABLE_HEAD_DEF));
	memset((char*)&stuMemHeadBuf, 0, sizeof(MEM_HEAD_DEF));
	if (strlen(pTableName) > HEAD_TABLE_NAME_LEN) return T_MDB_ENAMELEN;

	strcpy(stuTableHeadBuf.sTableName, pTableName);
	stuTableHeadBuf.iRecordLen = RecordLen;
	stuTableHeadBuf.lMaxRecordNum = lMaxRecord;
	stuTableHeadBuf.iKeyNum = 0;
	stuTableHeadBuf.iDoubleMemBufFlag = iDoubleMemBufFlag;
	stuTableHeadBuf.iCurrentMem = 0;

	if (pMemDb == NULL) return T_MDB_ENOMEM;
	if (iDoubleMemBufFlag == 1) iMultiple = 2;

	pMemDb->iIpcKey = iIpcKey;
	pMemDb->iShmId = shmget(iIpcKey, 
			sizeof(stuTableHeadBuf) + 
			(stuTableHeadBuf.iRecordLen * stuTableHeadBuf.lMaxRecordNum + 
			 sizeof(MEM_HEAD_DEF)) * iMultiple,
			IPC_CREAT|MDB_IPC_MODE);
	if (pMemDb->iShmId < 0) {
		return T_MDB_ESHMGET;
	}

	pMemDb->pShmArea = (char*)shmat(pMemDb->iShmId, NULL, 0);
	if ((long)pMemDb->pShmArea == - 1) {
		return T_MDB_ESHMAT;
	}
	memcpy(pMemDb->pShmArea, (char*)&stuTableHeadBuf, sizeof(stuTableHeadBuf));

	stuMemHeadBuf.lRecordNum = 0;
	memcpy(pMemDb->pShmArea + sizeof(stuTableHeadBuf), &stuMemHeadBuf, sizeof(stuMemHeadBuf)); 
	if (iDoubleMemBufFlag == 1) 
		memcpy(pMemDb->pShmArea + stuTableHeadBuf.iRecordLen * stuTableHeadBuf.lMaxRecordNum
				+ sizeof(stuTableHeadBuf) + sizeof(stuMemHeadBuf), (char*)&stuMemHeadBuf, sizeof(stuMemHeadBuf));   

	pMemDb->iSemId = semget(iIpcKey, 1, IPC_CREAT|MDB_IPC_MODE);
	if (pMemDb->iSemId < 0) {
		printf("CreateMemDB error, errno=[%d]", errno); 
		return T_MDB_ESEMGET;
	}

	iRet = semctl(pMemDb->iSemId, 0, SETVAL, 0);
	if (iRet < 0) {
		return T_MDB_ESEMCTL;
	}

	return T_SUCCESS;
}

//获取数据库信息头
void GetHead(char *pShmArea)
{
	memcpy((char*)&stuTableHeadBuf, (char*)pShmArea, sizeof(TABLE_HEAD_DEF));
}

//更新数据库信息头stuTableHead
void PutHead(char *pShmArea)
{
	memcpy((char*)pShmArea, (char*)&stuTableHeadBuf, sizeof(TABLE_HEAD_DEF));
}

//获取数据段的信息头
void GetMemHead(char *pShmArea)
{
	if (0 == stuTableHeadBuf.iDoubleMemBufFlag)
		memcpy((char*)&stuMemHeadBuf, pShmArea + sizeof(stuTableHeadBuf), sizeof(stuMemHeadBuf));
	else
		memcpy((char*)&stuMemHeadBuf, (pShmArea + stuTableHeadBuf.iCurrentMem * 
					(stuTableHeadBuf.iRecordLen * stuTableHeadBuf.lMaxRecordNum 
					 + sizeof(stuMemHeadBuf))
					+ sizeof(stuTableHeadBuf)), sizeof(stuMemHeadBuf));
}

//获取备库的MemHeadBuf
void GetUnUseMemHead(char *pShmArea)
{
	if (0 == stuTableHeadBuf.iDoubleMemBufFlag)
		memcpy((char*)&stuMemHeadBuf, pShmArea + sizeof(stuTableHeadBuf), sizeof(stuMemHeadBuf));
	else
		memcpy((char*)&stuMemHeadBuf, (pShmArea + ((stuTableHeadBuf.iCurrentMem == 1 ? 0 : 1) * 
						(stuTableHeadBuf.iRecordLen * stuTableHeadBuf.lMaxRecordNum 
						 + sizeof(stuMemHeadBuf)))
					+ sizeof(stuTableHeadBuf)), 
				sizeof(stuMemHeadBuf));
}

//更新主库的MemHeadBuf
void PutMemHead(char *pShmArea)
{
	if (0 == stuTableHeadBuf.iDoubleMemBufFlag)
		memcpy(pShmArea + sizeof(stuTableHeadBuf), (char*)&stuMemHeadBuf, sizeof(stuMemHeadBuf));
	else
		memcpy((pShmArea + stuTableHeadBuf.iCurrentMem *
					(stuTableHeadBuf.iRecordLen * stuTableHeadBuf.lMaxRecordNum
					 + sizeof(stuMemHeadBuf))
					+ sizeof(stuTableHeadBuf)), (char*)&stuMemHeadBuf, sizeof(stuMemHeadBuf));
}

//更新数据到备库的MemHead
void PutUnUseMemHead(char *pShmArea)
{
	if (0 == stuTableHeadBuf.iDoubleMemBufFlag)
		memcpy(pShmArea + sizeof(stuTableHeadBuf), (char*)&stuMemHeadBuf, sizeof(stuMemHeadBuf));
	else
		memcpy((pShmArea + ((stuTableHeadBuf.iCurrentMem == 1 ? 0 : 1) *
						(stuTableHeadBuf.iRecordLen * stuTableHeadBuf.lMaxRecordNum
						 + sizeof(stuMemHeadBuf)))
					+ sizeof(stuTableHeadBuf)), (char*)&stuMemHeadBuf, sizeof(stuMemHeadBuf));
}

//设置收索关键字key(int,char*)
int SetTableKey(MEMDATABASE *pMemDb, int iKeyOffSet, int iKeyLen, int iKeyAttr, int iKeySortAttr)
{
	int i;
	GetHead(pMemDb->pShmArea);

	for (i = 0; i < stuTableHeadBuf.iKeyNum; i++) {
		if ((stuTableHeadBuf.stuTableKeyBuf[i].iKeyOffSet == iKeyOffSet) &&
				(stuTableHeadBuf.stuTableKeyBuf[i].iKeyLen == iKeyLen)) {
			return T_SUCCESS;
		}
	}

	if (stuTableHeadBuf.iKeyNum >= MAX_KEY_NUM) return T_MDB_EMAXKEY;
	stuTableHeadBuf.stuTableKeyBuf[stuTableHeadBuf.iKeyNum].iKeyOffSet = iKeyOffSet;
	stuTableHeadBuf.stuTableKeyBuf[stuTableHeadBuf.iKeyNum].iKeyLen = iKeyLen;
	stuTableHeadBuf.stuTableKeyBuf[stuTableHeadBuf.iKeyNum].iKeyAttr = iKeyAttr;
	stuTableHeadBuf.stuTableKeyBuf[stuTableHeadBuf.iKeyNum].iKeySortAttr = iKeySortAttr;
	stuTableHeadBuf.iKeyNum++;
	PutHead(pMemDb->pShmArea);

	return T_SUCCESS;
}

//比较key
int CompareKey(MEMDATABASE *pMemDb, char *pShmBuf, char *pInBuf, int nCompareMode, int nNum)
{
	int i, iRet;
	int nSrc, nDest, nCompareNum;
	int sign;

	if (COMPARE_ALL == nCompareMode)
		nCompareNum = stuTableHeadBuf.iKeyNum;
	else 
		nCompareNum = nNum;

	for (i = 0; i < nCompareNum; i++) {
		sign = (stuTableHeadBuf.stuTableKeyBuf[i].iKeySortAttr == FIELD_DES ? -1 : 1);
		switch (stuTableHeadBuf.stuTableKeyBuf[i].iKeyAttr) {
			case FIELD_INT:
				nSrc  = atoi((char*)&pInBuf[stuTableHeadBuf.stuTableKeyBuf[i].iKeyOffSet]);
				nDest = atoi((char*)&pShmBuf[stuTableHeadBuf.stuTableKeyBuf[i].iKeyOffSet]);
				if (nSrc > nDest) 
					iRet = 1;
				else if (nSrc < nDest)
					iRet = -1;
				else 
					iRet = 0;
				break;
			case FIELD_CHAR:
				iRet = memcmp((char*)&pInBuf[stuTableHeadBuf.stuTableKeyBuf[i].iKeyOffSet], 
						(char*)&pShmBuf[stuTableHeadBuf.stuTableKeyBuf[i].iKeyOffSet], 
						stuTableHeadBuf.stuTableKeyBuf[i].iKeyLen);
				break;
		}

		if (iRet > 0) return sign;
		if (iRet < 0) return -sign;
	}
	return 0;
}

//信号量锁
//只有信号量为0才可以从阻塞状态中恢复,并增加一个信号量的值
int ShmLock(int iSemId)
{
	int    iRet;
	struct sembuf stuSemBuf[2];

	stuSemBuf[0].sem_num = 0;
	stuSemBuf[0].sem_op =  0;
	stuSemBuf[0].sem_flg = 0;

	stuSemBuf[1].sem_num = 0;
	stuSemBuf[1].sem_op =  1;
	stuSemBuf[1].sem_flg = SEM_UNDO;
	iRet = semop(iSemId, &stuSemBuf[0], 2);
	return iRet;
}

//解锁，信号量的值减一
int ShmUnLock(int iSemId)
{
	int iRet;
	struct sembuf stuSemBuf;

	stuSemBuf.sem_num = 0;
	stuSemBuf.sem_op = -1;
	stuSemBuf.sem_flg = IPC_NOWAIT|SEM_UNDO;
	iRet = semop(iSemId, &stuSemBuf, 1);
	return iRet;
}

//返回第iNim个数据的POSITION
char * LocateShm(char *pShmArea, int iNum)
{
	if (iNum>stuTableHeadBuf.lMaxRecordNum) {
		return NULL;
	}
	return pShmArea +
		(iNum - 1)*stuTableHeadBuf.iRecordLen;
}

//获取数据库主库的data段首地址
char * LocateUseShm(MEMDATABASE *pMemDb)
{
	char *pShmArea = pMemDb->pShmArea;

	GetHead(pMemDb->pShmArea);

	return pShmArea + stuTableHeadBuf.iCurrentMem * 
		(stuTableHeadBuf.iRecordLen * stuTableHeadBuf.lMaxRecordNum + sizeof(stuMemHeadBuf))
		+ sizeof(stuTableHeadBuf) + sizeof(stuMemHeadBuf);
}

//返回未使用的数据库data段首地址
char * LocateUnUseShm(MEMDATABASE *pMemDb)
{
	char *pShmArea = pMemDb->pShmArea;

	GetHead(pMemDb->pShmArea);
	if (0 == stuTableHeadBuf.iDoubleMemBufFlag)
		return pShmArea + sizeof(stuTableHeadBuf) + sizeof(stuMemHeadBuf);

	return pShmArea + ((stuTableHeadBuf.iCurrentMem == 1 ? 0 : 1) *
			(stuTableHeadBuf.iRecordLen * stuTableHeadBuf.lMaxRecordNum + sizeof(stuMemHeadBuf)))
		+ sizeof(stuTableHeadBuf) + sizeof(stuMemHeadBuf);
}

//根据传入的pInBuf，搜索位置,使用的是二分法查找
int SearchRecord(MEMDATABASE *pMemDb, char *pShmArea, char *pInBuf, int *iPosition)
{
	int iRet, i, iMax, iMin;
	char *pRecordBuf;

	iMin = i = 0;
	iMax = stuMemHeadBuf.lRecordNum - 1;
	while(iMin<=iMax)
	{
		i = (iMax+iMin)/2;
		pRecordBuf = LocateShm(pShmArea, i+1);
		iRet = CompareKey(pMemDb, pRecordBuf, pInBuf, COMPARE_ALL, 0);
		switch(iRet)
		{
			case  0: *iPosition = i+1;
				 return T_SUCCESS;
				 break;
			case  1: iMin = i+1;
				 break;
			case  - 1: iMax = i - 1;
				   break;
		}
	}
	if (iMin>iMax&&i==iMax) i=iMin;
	*iPosition = i+1;

	return T_MDB_NOTFOUND;
}

int SearchFirstRecord(MEMDATABASE *pMemDb, char *pShmArea, char *pInBuf, int *iPosition, int nCompareNum)
{
	int iRet, i, iMax, iMin;
	char *pRecordBuf;
	iMin = i = 0;
	iMax = stuMemHeadBuf.lRecordNum - 1;

	//先使用二分法找到一个相同的数据，然后再查找这个数据前面的一条，如果前一条数据不相同，则当前数据是第一条符合的数据，否则继续往前寻找
	while(iMin<=iMax)
	{
		i = (iMax+iMin)/2;
		pRecordBuf = LocateShm(pShmArea, i+1);
		iRet = CompareKey(pMemDb, pRecordBuf, pInBuf, COMPARE_FIRST, nCompareNum);
		switch(iRet)
		{
			case  0: *iPosition = i+1;
				 while (--i >= 0) {
					 pRecordBuf = LocateShm(pShmArea, i + 1);
					 iRet =  CompareKey(pMemDb, pRecordBuf, pInBuf, COMPARE_FIRST, nCompareNum);
					 if (iRet) {
						 *iPosition = i + 2;
						 return T_SUCCESS;  
					 }              
				 }
				 *iPosition = 1;
				 return T_SUCCESS;
				 break;
			case  1: iMin = i+1;
				 break;
			case  - 1: iMax = i - 1;
				   break;
		}
	}
	if (iMin>iMax&&i==iMax) i=iMin;
	*iPosition = i+1;
	return T_MDB_NOTFOUND;
}

//删除数据库,删除共享内存，删除信号量
int DropMemDb(MEMDATABASE *pMemDb)
{
	int iRet;
	iRet = shmctl(pMemDb->iShmId, IPC_RMID, 0);
	if (iRet<0) 
	{
		return T_MDB_ESHMRM;
	}
	iRet = semctl(pMemDb->iSemId, IPC_RMID, 0);
	if (iRet<0) 
	{
		return T_MDB_ESEMRM;
	}
	return T_SUCCESS;
}

//链接到已存在的数据库
int AttachMemDB(MEMDATABASE *pMemDb, long iIpcKey)
{
	if (pMemDb == NULL) return T_MDB_ENOMEM;
	pMemDb->iIpcKey = iIpcKey;
	pMemDb->iShmId = shmget(iIpcKey, 0, MDB_IPC_MODE);

	if (pMemDb->iShmId < 0) {
		return T_MDB_ESHMGET;
	}

	pMemDb->pShmArea = (char*)shmat(pMemDb->iShmId, NULL, 0);
	if ((long)pMemDb->pShmArea == - 1) {
		return T_MDB_ESHMAT;
	}

	pMemDb->iSemId = semget(iIpcKey, 1, MDB_IPC_MODE);
	if (pMemDb->iSemId < 0) {
		return T_MDB_ESEMGET;
	}
	return T_SUCCESS;
}

//主库备库交换
int SwapMemDB(MEMDATABASE *pMemDb)
{
	GetHead(pMemDb->pShmArea);
	if (0 == stuTableHeadBuf.iDoubleMemBufFlag)
		return T_FAIL;

	stuTableHeadBuf.iCurrentMem = (stuTableHeadBuf.iCurrentMem == 1 ? 0 : 1);
	PutHead(pMemDb->pShmArea);
	return T_SUCCESS;
} 

int DeclareTableKey(MEMDBFETCH *pMemDbFetch, int iKeyOffSet, int iKeyLen, void *pValue)
{
	int i;
	pMemDbFetch->lFetchNum = 1;
	for (i = 0; i < pMemDbFetch->iFetchKeyNum; i++) {
		if ((pMemDbFetch->stuFetchKeyBuf[i].iKeyOffSet == iKeyOffSet)&&
				(pMemDbFetch->stuFetchKeyBuf[i].iKeyLen == iKeyLen)) {
			memcpy(pMemDbFetch->stuFetchKeyBuf[i].sValue, (char*)pValue, iKeyLen);
			return T_SUCCESS;
		}
	}
	if (pMemDbFetch->iFetchKeyNum >= MAX_FETCHKEY_NUM) return T_MDB_EMAXFETCHKEY;
	pMemDbFetch->stuFetchKeyBuf[pMemDbFetch->iFetchKeyNum].iKeyOffSet = iKeyOffSet;
	pMemDbFetch->stuFetchKeyBuf[pMemDbFetch->iFetchKeyNum].iKeyLen = iKeyLen;
	memcpy(pMemDbFetch->stuFetchKeyBuf[pMemDbFetch->iFetchKeyNum].sValue, (char*)pValue, iKeyLen);
	pMemDbFetch->iFetchKeyNum++;
	return T_SUCCESS;
}

//获取数据库中数据的数量
int GetMemDbRecordNum(MEMDATABASE *pMemDb)
{
	GetHead(pMemDb->pShmArea);
	GetMemHead(pMemDb->pShmArea);
	return stuMemHeadBuf.lRecordNum;
}

int CloseCursorMemDB(MEMDATABASE *pMemDb)
{
	return T_SUCCESS;
}

//如果传入的FetchNum 为1，则先查找第一个匹配key的值,然后从第一个匹配到的地方往后查找
int FetchCursorMemDBByIndex(MEMDATABASE *pMemDb, MEMDBFETCH *pMemDbFetch, char *pInBuffer, int nCompareNum)
{
	int  iRet, iPosition;
	char *pRecordBuf;
	char *pShmArea;

	GetHead(pMemDb->pShmArea);
	GetMemHead(pMemDb->pShmArea);
	pShmArea = LocateUseShm(pMemDb);

	if (pMemDbFetch->lFetchNum == 1) {
		iRet = SearchFirstRecord(pMemDb, pShmArea, pInBuffer, &iPosition, nCompareNum);
		if (iRet != T_SUCCESS) {
			return iRet;
		}
		pMemDbFetch->lFetchNum = iPosition;
	}

	while (stuMemHeadBuf.lRecordNum >= pMemDbFetch->lFetchNum) {
		pRecordBuf = LocateShm(pShmArea, pMemDbFetch->lFetchNum++);
		iRet = FetchCondition(pMemDbFetch, pRecordBuf);
		if (iRet != T_SUCCESS) break;
		memcpy((char*)pInBuffer, pRecordBuf, stuTableHeadBuf.iRecordLen);
		return T_SUCCESS;
	}
	return T_MDB_NOTFOUND;
}

//通过FetchKey确定第一个数据,FetchNum由外部提供
int FetchCursorMemDB(MEMDATABASE *pMemDb, MEMDBFETCH *pMemDbFetch, char *pInBuffer)
{
	int iRet;
	char *pRecordBuf;
	char *pShmArea;

	GetHead(pMemDb->pShmArea);
	GetMemHead(pMemDb->pShmArea);
	pShmArea = LocateUseShm(pMemDb);
	while (stuMemHeadBuf.lRecordNum >= pMemDbFetch->lFetchNum) {
		pRecordBuf = LocateShm(pShmArea, pMemDbFetch->lFetchNum++);
		iRet = FetchCondition(pMemDbFetch, pRecordBuf);
		if (iRet != T_SUCCESS) continue;
		memcpy((char*)pInBuffer, pRecordBuf, stuTableHeadBuf.iRecordLen);
		return T_SUCCESS;
	}
	return T_MDB_NOTFOUND;
}

//从后往前找,
int FetchCursorMemDBReverse(MEMDATABASE *pMemDb, MEMDBFETCH *pMemDbFetch, char *pInBuffer)
{
	static int nInit = 1;
	int    iRet;
	char   *pRecordBuf;
	char   *pShmArea;

	GetHead(pMemDb->pShmArea);
	GetMemHead(pMemDb->pShmArea);
	pShmArea = LocateUseShm(pMemDb);

	if (nInit) {
		pMemDbFetch->lFetchNum = stuMemHeadBuf.lRecordNum;
		nInit = 0;
	}

	while (pMemDbFetch->lFetchNum > 0) {
		pRecordBuf = LocateShm(pShmArea, pMemDbFetch->lFetchNum--);
		iRet = FetchCondition(pMemDbFetch, pRecordBuf);
		if (iRet != T_SUCCESS) continue;
		memcpy((char*)pInBuffer, pRecordBuf, stuTableHeadBuf.iRecordLen);
		return T_SUCCESS;
	}
	nInit = 1;
	return T_MDB_NOTFOUND;
}

//比较每一个fetchKey的值
int FetchCondition(MEMDBFETCH *pMemDbFetch, char *pRecordBuf)
{
	int  i;
	for (i = 0; i < pMemDbFetch->iFetchKeyNum; i++) {
		if (memcmp((char*)&pRecordBuf[pMemDbFetch->stuFetchKeyBuf[i].iKeyOffSet], 
					(char*)pMemDbFetch->stuFetchKeyBuf[i].sValue, 
					pMemDbFetch->stuFetchKeyBuf[i].iKeyLen) != 0)
			return T_MDB_NOTMATCH;
	}
	return T_SUCCESS;
}

//获取第一个内容
int SelectOneMemDB(MEMDATABASE *pMemDb, char *pInBuffer)
{
	char *pRecordBuf;
	char *pShmArea;

	GetHead(pMemDb->pShmArea);
	GetMemHead(pMemDb->pShmArea);
	pShmArea = LocateUseShm(pMemDb);
	if (stuMemHeadBuf.lRecordNum > 0) {
		pRecordBuf = LocateShm(pShmArea, 1);
		memcpy((char*)pInBuffer, pRecordBuf, stuTableHeadBuf.iRecordLen);
		return T_SUCCESS;
	} else {
		return T_MDB_NOTFOUND;
	}
}

//根据传入的数字n，获取第n个buffer
int SelectOneMemDBByIndex(MEMDATABASE *pMemDb, int nIndex, char *pInBuffer)
{
	char *pRecordBuf;
	char *pShmArea;
	int  nRecordNum;

	if (nIndex < 0)
		return T_MDB_NOTFOUND;

	nRecordNum = GetMemDbRecordNum(pMemDb);
	if (nRecordNum < 0 || nIndex > nRecordNum) 
		return T_MDB_NOTFOUND;

	pShmArea = LocateUseShm(pMemDb);
	pRecordBuf = LocateShm(pShmArea, nIndex);
	memcpy((char*)pInBuffer, pRecordBuf, stuTableHeadBuf.iRecordLen);
	return T_SUCCESS;
}

//获取传入buff在内存数据库中的位置
int SelectMemDBPos(MEMDATABASE *pMemDb, char *pInBuffer, int *iPos)
{
	int iRet, iPosition;
	char *pRecordBuf;
	char *pShmArea;

	/* if (ShmLock(pMemDb->iSemId)) return T_MDB_ESHMLOCK; */
	GetHead(pMemDb->pShmArea);
	GetMemHead(pMemDb->pShmArea);
	pShmArea = LocateUseShm(pMemDb);

	iRet = SearchRecord(pMemDb, pShmArea, pInBuffer, &iPosition);
	if (iRet != T_SUCCESS) {
		return T_MDB_NOTFOUND;
	}
	*iPos = iPosition;
	return T_SUCCESS;
}

//根据传入的pInBuffer 比较key，获取其他buffer的其他信息
int SelectMemDB(MEMDATABASE *pMemDb, char *pInBuffer)
{
	int iRet, iPosition;
	char *pRecordBuf;
	char *pShmArea;

	GetHead(pMemDb->pShmArea);
	GetMemHead(pMemDb->pShmArea);
	pShmArea = LocateUseShm(pMemDb);

	iRet = SearchRecord(pMemDb, pShmArea, pInBuffer, &iPosition);
	if (iRet != T_SUCCESS) {    
		return T_MDB_NOTFOUND;
	}

	pRecordBuf = LocateShm(pShmArea, iPosition);
	memcpy((char*)pInBuffer, pRecordBuf, stuTableHeadBuf.iRecordLen);
	return T_SUCCESS;
}

//根据key，更新备库的信息
int UpdateMemDB(MEMDATABASE *pMemDb, char *pInBuffer)
{
	int iRet, iPosition;
	char *pRecordBuf;
	char *pShmArea;

	if (ShmLock(pMemDb->iSemId)) return T_MDB_ESHMLOCK;
	GetHead(pMemDb->pShmArea);
	GetUnUseMemHead(pMemDb->pShmArea);
	pShmArea = LocateUnUseShm(pMemDb);

	iRet = SearchRecord(pMemDb, pShmArea, pInBuffer, &iPosition);
	if (iRet != T_SUCCESS) {
		if (ShmUnLock(pMemDb->iSemId)) return T_MDB_ESHMUNLOCK;
		return T_MDB_NOTFOUND;
	}
	pRecordBuf = LocateShm(pShmArea, iPosition);
	memcpy((char*)pRecordBuf, (char*)pInBuffer, stuTableHeadBuf.iRecordLen);
	if (ShmUnLock(pMemDb->iSemId)) return T_MDB_ESHMUNLOCK;
	return T_SUCCESS;
}

//删除备库中的一个数据
int DeleteMemDB(MEMDATABASE *pMemDb, char *pInBuffer)
{
	int iNum, iRet, iPosition;
	char *pRecordBuf;
	char *pShmArea;

	if (ShmLock(pMemDb->iSemId)) return T_MDB_ESHMLOCK;
	GetHead(pMemDb->pShmArea);
	GetUnUseMemHead(pMemDb->pShmArea);
	pShmArea = LocateUnUseShm(pMemDb);

	iRet = SearchRecord(pMemDb, pShmArea, pInBuffer, &iPosition);
	if (iRet != T_SUCCESS) {
		if (ShmUnLock(pMemDb->iSemId)) return T_MDB_ESHMUNLOCK;
		return T_MDB_NOTFOUND;
	}
	pRecordBuf = LocateShm(pShmArea, iPosition);
	memcpy((char*)pInBuffer, (char*)pRecordBuf, stuTableHeadBuf.iRecordLen);
	iNum = stuMemHeadBuf.lRecordNum - iPosition;
	if (iNum > 0) {
		memmove(LocateShm(pShmArea, iPosition), LocateShm(pShmArea, iPosition+1), 
				stuTableHeadBuf.iRecordLen*iNum);
	}
	stuMemHeadBuf.lRecordNum--;
	PutUnUseMemHead(pMemDb->pShmArea);
	if (ShmUnLock(pMemDb->iSemId)) return T_MDB_ESHMUNLOCK;
	return T_SUCCESS;
}

//删除第一个数据
int DeleteOneMemDB(MEMDATABASE *pMemDb, char *pInBuffer)
{
	char *pRecordBuf;
	char *pShmArea;

	if (ShmLock(pMemDb->iSemId)) return T_MDB_ESHMLOCK;
	GetHead(pMemDb->pShmArea);
	GetUnUseMemHead(pMemDb->pShmArea);
	pShmArea = LocateUnUseShm(pMemDb);
	if (stuMemHeadBuf.lRecordNum > 0) {
		pRecordBuf = LocateShm(pShmArea, 1);
		memcpy(pInBuffer, pRecordBuf, stuTableHeadBuf.iRecordLen);
		memcpy(pRecordBuf, LocateShm(pShmArea, 1+1), 
				(stuMemHeadBuf.lRecordNum - 1)*stuTableHeadBuf.iRecordLen);
		stuMemHeadBuf.lRecordNum--;
		PutUnUseMemHead(pMemDb->pShmArea);
	} else {
		if (ShmUnLock(pMemDb->iSemId)) return T_MDB_ESHMUNLOCK;
		return T_MDB_NOTFOUND;
	}
	if (ShmUnLock(pMemDb->iSemId)) return T_MDB_ESHMUNLOCK;
	return T_SUCCESS;
}

//向备库插入一条数据
int InsertMemDB(MEMDATABASE *pMemDb, char *pInBuffer)
{

	int iRet, iNum, iPosition;
	char *pRecordBuf;
	char *pShmArea;
	if (ShmLock(pMemDb->iSemId)) return T_MDB_ESHMLOCK;
	memset(&stuMemHeadBuf,0x00,sizeof(stuMemHeadBuf));
	GetHead(pMemDb->pShmArea);
	GetUnUseMemHead(pMemDb->pShmArea);
	pShmArea = LocateUnUseShm(pMemDb);
	iRet = SearchRecord(pMemDb, pShmArea, pInBuffer, &iPosition);
	if (iRet == T_SUCCESS) {
		if (ShmUnLock(pMemDb->iSemId)) return T_MDB_ESHMUNLOCK;
		return T_MDB_DUPKEY;
	}

	if (stuMemHeadBuf.lRecordNum >= stuTableHeadBuf.lMaxRecordNum) {
		if (ShmUnLock(pMemDb->iSemId)) return T_MDB_ESHMUNLOCK;
		return T_MDB_LACKSPACE;
	}

	stuMemHeadBuf.lRecordNum++;
	iNum = stuMemHeadBuf.lRecordNum - iPosition;
	if (iNum > 0) {
		memmove(LocateShm(pShmArea, iPosition + 1), LocateShm(pShmArea, iPosition), 
				stuTableHeadBuf.iRecordLen * iNum);
	}
	memcpy(LocateShm(pShmArea, iPosition), pInBuffer, stuTableHeadBuf.iRecordLen);
	PutUnUseMemHead(pMemDb->pShmArea);
	if (ShmUnLock(pMemDb->iSemId)) return T_MDB_ESHMUNLOCK;
	return T_SUCCESS;
}

//删除备库中的信息,直接使lRecordNum = 0
void TruncateMemDB(MEMDATABASE *pMemDb)
{
	GetHead(pMemDb->pShmArea);
	GetUnUseMemHead(pMemDb->pShmArea);
	stuMemHeadBuf.lRecordNum = 0;
	PutUnUseMemHead(pMemDb->pShmArea);
}
