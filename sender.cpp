#include <iostream>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <dirent.h>
#include <sys/file.h>
#include "childfuncs.h"

#define PERMS 0777
#define DURATION 30000

using namespace std;

char* log_file;

//args are 1: checkid 2: myidstring 3:buffsize 4:input directory 5: common 6: log_file
int main(int argc,char* argv[])
{

	char* foreign_id = strdup(argv[1]);
	char* og_id = strdup(argv[2]);
	int buffsize = atoi(argv[3]);
	char* inputdir = strdup(argv[4]);
	char* common_dir = strdup(argv[5]);
	char suffix[6] = ".fifo";
	log_file = strdup(argv[6]);



	char* fifo_path = (char*)calloc(strlen(suffix)+strlen(foreign_id)+strlen(og_id)+strlen(common_dir)+13,sizeof(char));

	strcat(fifo_path,common_dir);
	strcat(fifo_path,"/");
	strcat(fifo_path,"id");
	strcat(fifo_path,og_id);
	strcat(fifo_path,"_to_id");
	strcat(fifo_path,foreign_id);
	strcat(fifo_path,suffix);


	//fifo does not exist.Create it
	if(mkfifo(fifo_path,PERMS) < 0 )
	{	
		if((errno != EEXIST))
		{
			perror("cannot create pipe from sender");
			exit(1);
		}
	}



	int fd = 0;
	if((fd = open(fifo_path,O_WRONLY)) < 0)
	{
		cout << "error opening fifo" << endl;
		exit(2);
	}


	//open input directory and send all files.
	int ret_status = 0;
	ret_status = dir_reader(fifo_path,inputdir,fd,buffsize);

	if(ret_status == 0) //ret_status 0 means everything went smoothly
	{
		short int end = 0;
		write(fd,&end,2);
	}


	if((close(fd)) < 0)
	{
		cout << "error closing fifo" << endl;
		exit(2);
	}

	unlink(fifo_path);
	free(fifo_path);
	free(foreign_id);
	free(og_id);
	free(inputdir);
	free(common_dir);
	free(log_file);

	if(ret_status == -1) //ret_status = -1 denotes a failed transfer
	{
		//kill(getppid(),SIGUSR1);
		exit(ret_status);
	}


	exit(0);

}


/*
*	Recursively called if we find another subdirectory
*	Else send_file
*/
int dir_reader(char* fifo_path,char* newdir,int fd,int buffsize)//for directories inside input
{
	DIR *in_dir;
	struct dirent *input;
	char* inputdir = NULL;
	inputdir = strdup(newdir);
	if((in_dir = opendir(inputdir)) == NULL)
	{
		cout << "Cannot open " << inputdir << endl;
		return -1;
	}
	
	int ret_status = 0;
	char* filename = NULL;
	char* newpath = NULL;	
	int filesindir = 0;

	while((input = readdir(in_dir)) != NULL)
	{
		
		struct stat fstat;
		//skip deleted and current and parent directory
		if(!(strcmp(input->d_name,".")) || !(strcmp(input->d_name,"..") )) continue;
		if(input->d_ino == 0) continue;
		filesindir++;

		filename = (char*)calloc(strlen(input->d_name)+strlen(inputdir)+3,sizeof(char));
		strcat(filename,inputdir);

		if(inputdir[strlen(inputdir)-1] !='/')
			strcat(filename,"/");

		strcat(filename,input->d_name);

		if(stat(filename,&fstat) == -1)
		{
			cout << "Unable to stat file in sender" << filename<<endl;
			return -1;
		}

		if(S_ISDIR(fstat.st_mode))//check for directory
		{
			//If it is a directory call dir_reader with newpath
			newpath = (char*)calloc(strlen(inputdir)+2+strlen(filename),sizeof(char));
			strcpy(newpath,filename);
			ret_status = dir_reader(fifo_path,newpath,fd,buffsize);

			if(ret_status == -1)
			{
				free(newpath);
				free(filename);
				closedir(in_dir);
				free(inputdir);
				return ret_status;
			}
			free(filename);
			free(newpath);
			continue;
		}
		else
		{
			//Send_file
			ret_status = send_file(fifo_path,filename,fd,buffsize);
			
			if(ret_status == -1)
			{
				free(filename);
				closedir(in_dir);
				free(inputdir);
				return ret_status;
			}

			int sneeded = 0;
			char* size_str = NULL;
			sneeded = snprintf(NULL,0,"%ld",fstat.st_size);

			if(sneeded < 1)
			{
				cout << "not enough space for sprintf" << endl;
				exit(1);
			}

			sneeded++;
			size_str = (char*)calloc(sneeded,sizeof(char));

			sprintf(size_str,"%ld",fstat.st_size); 

			//Lock file to avoid concurrent writings
			//Write file sent and bytes sent

			int lf = open(log_file,O_APPEND|O_RDWR,0777);
			flock(lf,LOCK_EX);
			write(lf,"SENT ",strlen("SENT "));
			write(lf,input->d_name,strlen(input->d_name));
			write(lf,"\t",strlen("\t"));
			write(lf,size_str,strlen(size_str));
			write(lf,"\n",strlen("\n"));
			close(lf);
			flock(lf,LOCK_UN);

			free(filename);
			free(size_str);

		}

	}

	if(filesindir == 0)
	{
		char* emptydir = NULL;
		emptydir = (char*)calloc(strlen(inputdir)+2,sizeof(char));
		strcat(emptydir,inputdir);
		strcat(emptydir,"/");
		send_file(fifo_path,emptydir,fd,-1);
		free(emptydir);

	}

	closedir(in_dir);

	free(inputdir);

	return ret_status;
}


/*
*
*	Get file and send it via pipe
*
*/
int send_file(char* fifo_path,char* filename,int fd,int buffsize)
{		
		int nwrite = 0;
		short int fname_length = strlen(filename);

		struct stat fstat;
		if(stat(filename,&fstat) == -1)
		{
			cout << "Unable to stat file in sender" << filename<<endl;
			return -1;
		}
		//Send filename length as 2 bytes
		if((nwrite=write(fd,&fname_length,2)) == -1)
		{
			cout << "Error in writing " << endl;
			return -1;
		}


		//Send filename over pipe
		if((nwrite=write(fd,filename,strlen(filename)+1)) == -1)
		{
			cout << "Error in writing " << endl;
			return -1;
		}

		if(buffsize == -1)
			return 1;

		int fsize = fstat.st_size;

		//Send filesize as 4 bytes
		if((nwrite=write(fd,&fsize,4)) == -1)
		{
			cout << "Error in writing " << endl;
			return -1;
		}

		int fp = open(filename,O_RDONLY);
		if(fp < 0)
		{
			cout << "Error opening file for reading in sender " << endl;
			return -1;
		}


		int tosentbytes = fsize;
		int bytes = 0;

		//While there are still bytes to send
		while(tosentbytes > 0)
		{	
			//Check how many bytes i can send up to buffsize
			if(tosentbytes < buffsize)
				bytes = tosentbytes;
			else
				bytes = buffsize;

			//Alloc bytes space
			char* buffer = NULL;
			buffer = (char*)calloc(bytes+1,sizeof(char));

			//Read from the file
			read(fp,buffer,bytes);

			int sentbytes;
			int nwrite = 0;
			//write on pipe
			for(sentbytes = 0; sentbytes < bytes ; sentbytes+=nwrite)
				nwrite = write(fd,buffer+sentbytes,bytes-sentbytes);

			tosentbytes = tosentbytes - bytes;

			free(buffer);
		}

		close(fp);



	return 0;
}