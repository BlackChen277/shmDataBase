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
	if(argc < 2)
	{
		ret = CreateMemDB(&myMemdb,"studentDB",1001102,100,sizeof(Student), 0);
		if(ret)
		{
			printf("CreateMemDB err");
			return 1;
		}
	}
	else
	{
		ret =  AttachMemDB(&myMemdb, 1001102);
	}
	ret = SetTableKey(&myMemdb,FPOS(Student,id),FLEN(Student,id), FIELD_CHAR, FIELD_ASC);
	printf("offset id[%d],offset name[%d],len id [%d],len name[%d]",FPOS(Student,id),FPOS(Student,name),FLEN(Student,id),FLEN(Student,name));
	printf("set KEY OK");

	Student Sdate[DateNum];
	memset(Sdate,0x00,sizeof(Sdate));
	//init data
	for( i = 0;i < DateNum;i++)
	{
		sprintf(Sdate[i].id,"%d",100+i);
		sprintf(Sdate[i].name,"%s%d","A",i);
		Sdate[i].age = 10+i;
		Sdate[i].math= 15+i;
		Sdate[i].english= 12+i;
		Sdate[i].class= 25+i;
	}

	//²åÈëÊý¾Ý¿â
	printf("Insert Memdb\n");
	for( i = 0;i < DateNum;i++)
	{
		printf("Insert [%d]\n",i);
		ret = InsertMemDB(&myMemdb, &Sdate[i]);
		if (ret && ret != T_MDB_DUPKEY) { 
			return ret; 
		}
	}

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

	printf("id [%s]\n",find1.id);
	printf("name [%s]\n",find1.name);
	printf("math [%d]\n",find1.math);
	printf("english [%d]\n",find1.english);
	Student Insert;
	sprintf(Insert.id,"%d",100+5);
	sprintf(Insert.name,"%s%d","A",5);
	Insert.age = 10+5;
	Insert.math= 15+5;
	Insert.english= 12+5;
	Insert.class= 25+5;
	
	ret = InsertMemDB(&myMemdb,&Insert);
	
	return 0;
}
