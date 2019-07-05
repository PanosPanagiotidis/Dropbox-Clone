#include "mirror_functions.h"


/*
*
*	Returns a char array with the new ids
*
*/


using namespace std;

id_info* check_new_ids(id_info** existing_ids,char* idpath,int prs_id)//return struct with new ids to sync with
{
	struct dirent *common;
	DIR *com_dir;

	id_info* inf = (id_info*)malloc(sizeof(id_info));
	inf->size = 0;
	inf->new_ids = NULL;

	char* filename_buff = NULL;


	if((com_dir = opendir(idpath)) == NULL)
	{
		cout << "Folder " << idpath << " not found :" << strerror(errno)<< endl;
		exit(1);
	}

	while((common = readdir(com_dir)) != NULL)
	{
		if(!(strcmp(common->d_name,".")) || !(strcmp(common->d_name,"..") )) continue;//skip current and parent dir
		if(atoi(common->d_name) == prs_id) continue; //skip process id file;
		if(common->d_ino == 0) continue;//skip deleted files
		if(strstr(common->d_name,".fifo")) continue;
		

		struct stat idbuf;
		filename_buff = (char*)calloc(strlen(idpath)+strlen("/")+strlen(common->d_name)+1,sizeof(char));

		strcpy(filename_buff,idpath);
		strcat(filename_buff,"/");
		strcat(filename_buff,common->d_name);

		if(stat(filename_buff,&idbuf) == -1)
		{
			cout << "Unable to stat file " << filename_buff<<endl;
			continue;
		}

		int cur_id = atoi(common->d_name);

		if((id_exists(&(*existing_ids),cur_id) != -1))
		{	
			inf->new_ids = (int*)realloc(inf->new_ids,sizeof(int)*(inf->size+1));
			inf->size++;
			inf->new_ids[inf->size-1] = cur_id;

		}
		
		free(filename_buff);
	}


	closedir(com_dir);
	if(inf->size == 0)
	{
		free(inf);
		return NULL;
	}

	return inf;
}

/*
*	Return -1 if the file exists in existing_ids
*	Return id if it doesnt
*/
int id_exists(id_info** existing_ids,int cur_id)
{

	int size =(*existing_ids)->size;
	for(int i = 0 ;i < size ;i++)
	{
		if((*existing_ids)->new_ids[i] == cur_id)
		{
			return -1;
		}

	}

	if(size == 0)
	{		
			(*existing_ids)->new_ids = (int*)realloc((*existing_ids)->new_ids,sizeof(int)*((*existing_ids)->size+1));
			(*existing_ids)->exist = (int*)realloc((*existing_ids)->exist,sizeof(int)*((*existing_ids)->size+1));
			if( ((*existing_ids)->new_ids == NULL) || ((*existing_ids)->exist == NULL))
			{
				cout << "realloc failed " << endl;
				exit(1);
			}
			(*existing_ids)->size++;
			(*existing_ids)->new_ids[(*existing_ids)->size-1]=cur_id;
			(*existing_ids)->exist[(*existing_ids)->size-1]=1;
			return cur_id;		
	}
	else
	{
			(*existing_ids)->new_ids = (int*)realloc((*existing_ids)->new_ids,sizeof(int)*((*existing_ids)->size+1));
			(*existing_ids)->exist = (int*)realloc((*existing_ids)->exist,sizeof(int)*((*existing_ids)->size+1));
			if( ((*existing_ids)->new_ids == NULL) || ((*existing_ids)->exist == NULL))
			{
				cout << "realloc failed " << endl;
				exit(1);
			}
			(*existing_ids)->size++;
			(*existing_ids)->new_ids[(*existing_ids)->size-1]=cur_id;
			(*existing_ids)->exist[(*existing_ids)->size-1]=1;
			return cur_id;
	}
	return -1;
}

/*
*
*	Check if any id in existing ids has its file removed from common dir
*	Delete its mirror on local
*/

void check_deleted_ids(id_info** existing_ids,char* idpath,int prs_id,char* mirror_path)
{
	struct dirent *common;
	DIR *com_dir;

	for(int i = 0 ;i < (*existing_ids)->size ;i++)
	{
		int erase = 1;


		com_dir = opendir(idpath);
		while((common = readdir(com_dir)) != NULL)
		{
			if(!(strcmp(common->d_name,".")) || !(strcmp(common->d_name,"..") )) continue;
			if(common->d_ino == 0)
			{
				erase = 0;
				continue;
			} 

			int id = atoi(common->d_name);
			erase = 1;
			if(id == (*existing_ids)->new_ids[i])
			{
				erase = 0;
				break;
			} 

		}

		closedir(com_dir);

		if(erase == 1 && ((*existing_ids)->exist[i] == 1))
		{
			(*existing_ids)->exist[i] = 0;
			char* del_file = NULL;
			char* id_str = NULL; 
			int sneeded = snprintf(NULL,0,"%d",(*existing_ids)->new_ids[i]);
			if(sneeded < 1)
			{
				cout << "Not enough space " << endl;
				exit(1);
			}

			sneeded++;
			id_str = (char*)malloc(sizeof(char)*sneeded);
			sprintf(id_str,"%d",(*existing_ids)->new_ids[i]);

			del_file = (char*)calloc(strlen(mirror_path)+1+strlen(id_str)+1,sizeof(char));
			strcat(del_file,mirror_path);
			strcat(del_file,"/");
			strcat(del_file,id_str);

			DIR* mir = NULL;
			mir = opendir(del_file);

			if(mir != NULL)
			{
				pid_t npid;
				char* fullcmd = NULL;
				char cmd[] = "rm -r ";
				fullcmd = (char*)calloc(strlen(cmd)+1+strlen(del_file)+1,sizeof(char));
				strcat(fullcmd,cmd);
				strcat(fullcmd,del_file);

				if((npid = fork()) == 0)
				{
					execl("/bin/bash","bash","-c",fullcmd,(char*)0);
					_exit(127);
				}
				else if (npid < 0)
				{
					cout << "Delete fork failed.Exiting..."<<endl;
					exit(1);
				}

				free(fullcmd);
			}
			closedir(mir);
			free(id_str);
			free(del_file);
		}

	}
	return;
}


void getArgs(int argc, char* argv[],char** id, char** common_dir,char** input_dir,char** mirror_dir,char** buffersize,char** log_file)
{
	for(int i = 0 ; i < argc ; i++)
	{	
		if(!strcmp(argv[i],"-n"))	// id arg
		{	
			*id = strdup(argv[i+1]);
		}
		else if(!strcmp(argv[i],"-c"))	//common_dir arg
		{
			*common_dir = strdup(argv[i+1]);
		}
		else if(!strcmp(argv[i],"-i")) // input_dir arg
		{
			*input_dir = strdup(argv[i+1]);
		}
		else if(!strcmp(argv[i],"-m")) // mirror_dir arg
		{
			*mirror_dir = strdup(argv[i+1]);
		}
		else if(!strcmp(argv[i],"-b")) // buffer size arg
		{
			*buffersize = strdup(argv[i+1]);
		}
		else if(!strcmp(argv[i],"-l")) //log file arg
		{
			*log_file = strdup(argv[i+1]);
		}

	}
}