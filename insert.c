#include <stdio.h>
#include "memdb.h"
#include "memdberr.h"



typedef struct MyStruct
{
	char id[10];
	char name[20];
	int age;
	int math;
	int english;
	int class;
}Student;

#define FPOS( type, field ) ( (int)&(( type *) 0)-> field )
#define FLEN( type, field ) ( sizeof((( type *) 0)-> field ))
#define DateNum 10

int main(int argc ,char *argv[])
{
	MEMDATABASE myMemdb;
	int ret;
	int i;
	ret =  AttachMemDB(&myMemdb, 1001102);

	Student find1;
	memset(&find1,0x00,sizeof(find1));
	strcpy(find1.id,"103");
	printf("id [%s]\n",find1.id);
	ret = SelectMemDB(&myMemdb,&find1);
	if(ret != T_SUCCESS)
	{
		printf("err\n");
		printf("%d\n",ret);
		return 1;
	}

	Student insert1;
	memcpy(&insert1,&find1,sizeof(insert1));
	printf("id [%s]\n",find1.id);
	printf("name [%s]\n",find1.name);
	printf("math [%d]\n",find1.math);
	printf("english [%d]\n",find1.english);
	InsertMemDB(&myMemdb, &insert1);

	return 0;
}
