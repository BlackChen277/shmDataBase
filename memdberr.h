#ifndef _MEMDBERR_H
#define _MEMDBERR_H

#define T_SUCCESS                  0000
#define T_FAIL                     0001

/* Memory DataBase Error Code Define Here */
#define T_MDB_DUPKEY               1701
#define T_MDB_NOTFOUND             1703
#define T_MDB_NOTMATCH             1704
#define T_MDB_LACKSPACE            1705
#define T_MDB_EMAXKEY              1706
#define T_MDB_EMAXFETCHKEY         1707
#define T_MDB_ESHMGET              1711
#define T_MDB_ESHMAT               1712
#define T_MDB_ESHMDT               1713
#define T_MDB_ESEMGET              1715
#define T_MDB_ESEMCTL              1716
#define T_MDB_ESEMRM               1717
#define T_MDB_ESHMRM               1718
#define T_MDB_ESHMLOCK             1721
#define T_MDB_ESHMUNLOCK           1722
#define T_MDB_ENOMEM               1731
#define T_MDB_ENAMELEN             1732

#endif
