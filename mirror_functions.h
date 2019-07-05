#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <cerrno>
#include <unistd.h>


typedef struct id_info{
	int* new_ids;
	int size;
	int* exist;
}id_info;

typedef struct child_info{
	pid_t pid;
	char* id_foreign;
	char* og_id;
	char* buffsize;
	char* common_dir;
	char* input_dir;
	char* mirror_dir;
	char* log_file;
	char type;
	int attempt;
}child_info;


id_info* check_new_ids(id_info** ,char* ,int);

int id_exists(id_info** ,int);

void check_deleted_ids(id_info** ,char* ,int ,char* );

void getArgs(int , char*[] ,char** , char** ,char** ,char** ,char** ,char** );