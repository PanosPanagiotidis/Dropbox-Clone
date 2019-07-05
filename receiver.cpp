#include <iostream>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <dirent.h>
#include <poll.h>
#include <sys/file.h>
#include "childfuncs.h"

#define PERMS 0777
#define DURATION 30000

using namespace std;



char* log_file;



int main(int argc,char* argv[])//args are 1: checkid 2:myidstring 3:buffsize 4:mirror directory 5:log file path 
{								// 6 common


	char* foreign_id = strdup(argv[1]);
	char* og_id = strdup(argv[2]);
	int buffsize = atoi(argv[3]);
	char* common_dir = strdup(argv[5]);
	log_file = strdup(argv[6]);
	char suffix[6] = ".fifo";
	char* mirrordir = NULL;
	mirrordir = (char*)calloc(strlen(argv[4])+1+strlen(foreign_id)+2,sizeof(char));
	strcat(mirrordir,argv[4]);
	strcat(mirrordir,"/");
	strcat(mirrordir,foreign_id);
	mkdir(mirrordir,PERMS);


	char* fifo_path = (char*)calloc(strlen(suffix)+strlen(foreign_id)+strlen(og_id)+strlen(common_dir)+13,sizeof(char));

	strcat(fifo_path,common_dir);
	strcat(fifo_path,"/");
	strcat(fifo_path,"id");
	strcat(fifo_path,foreign_id);
	strcat(fifo_path,"_to_id");
	strcat(fifo_path,og_id);
	strcat(fifo_path,suffix);

	


	if(mkfifo(fifo_path,PERMS) < 0 )
	{	
		if((errno != EEXIST))
		{
			perror("cannot create pipe from receiver");
			exit(1);
		}
	}


	int fd = 0;
	if((fd = open(fifo_path,O_RDONLY)) < 0)
	{
		cout << "error opening fifo" << endl;
		exit(2);
	}


	int ret_status = 0;
	ret_status = read_inc_files(fifo_path,mirrordir,fd,buffsize);

	if((close(fd)) < 0)
	{
		cout << "error closing fifo" << endl;
		exit(2);
	}

	unlink(fifo_path);
	free(fifo_path);
	free(foreign_id);
	free(og_id);
	free(mirrordir);
	free(common_dir);
	free(log_file);

	if(ret_status == -1)
	{
		cout << "ret status is -1 " << endl;
		exit(ret_status);
	}

	kill(getppid(),SIGUSR2);
	exit(0);

}

int read_inc_files(char* fifo_path,char* newpath,int fd,int buffsize)
{
	//Wait for the other side to open up to 30 seconds or 30000 milliseconds
	struct pollfd pipefd[1];
	pipefd[0].fd = fd;
	pipefd[0].events = POLLIN;

	int rc;

	int ret_status = 0;
	DIR *m_dir;

	char* mirrordir = NULL;
	mirrordir = strdup(newpath);
	if((m_dir = opendir(mirrordir)) == NULL)
	{
		cout << "Cannot open " << mirrordir << endl;
		return -1;
	}


	char* mirror_path = NULL;


	while(1) //Keep reading until you receiver 00
	{

		mirror_path = (char*)calloc((strlen(mirrordir) + 1),sizeof(char));
		strcat(mirror_path,mirrordir);

		int nread = 0;
		short int incfilenamesize;


		rc = poll(pipefd,1,DURATION);

		if(rc == 0)
		{
			free(mirrordir);
			free(mirror_path);
			closedir(m_dir);
			return (ret_status = -1);
		}


		if((nread=read(fd,&incfilenamesize,2)) == -1)//read filename length over pipe.2 bytes
		{
				cout << "Error in reading " << endl;
				ret_status = -1;
				break;
		}


		if((incfilenamesize == 0)) // if 0 is read exit
			break;

		

		char* incfilename = NULL;
		
		incfilename = (char*)calloc(incfilenamesize+1,sizeof(char));

		rc = poll(pipefd,1,DURATION);
		
		if(rc == 0)
		{
			free(mirrordir);
			free(incfilename);
			free(mirror_path);
			closedir(m_dir);
			return (ret_status = -1);
		}

		if((nread=read(fd,incfilename,incfilenamesize+1)) == -1)//read incfilename over pipe
		{
				cout << "Error in reading " << endl;
				ret_status = -1;
				break;
		}


		/* if last char is / it means we read 0 files in that directory and it is empty */
		if(incfilename[nread-2]=='/')
		{
			char* keep = incfilename;
			char* ret = strchr(incfilename,'/');
			ret++;
			char* token = strtok(ret,"/");
			while(token != NULL)
			{
				mirror_path = (char*)realloc(mirror_path,strlen(mirror_path)+1+strlen("/")+strlen(token));
				strcat(mirror_path,"/");
				strcat(mirror_path,token);
				mkdir(mirror_path,PERMS);
				token = strtok(NULL,"/");
			}
			free(mirror_path);
			free(keep);
			continue;
		}
		

		int dotted = 0;

		if(strchr(incfilename,'.') != NULL)//check if filename starts with dot.
			dotted = 1;



		char* ret = NULL;
		ret = strchr(incfilename,'/');

		ret++;

		if(dotted)
		{
			ret = strchr(ret,'/');
			ret++;
		}

		char* temp = strdup(ret);
		char* token = NULL;
		char* del = temp;

		token = strtok(temp,"/");
		while(token != NULL)//create every sub_directory as you go
		{
			char* keep = strdup(token);
			if((token = strtok(NULL,"/")) == NULL)
			{	
				free(keep);
				break;
			}

			mirror_path = (char*)realloc(mirror_path,strlen(mirror_path)+1+strlen("/")+strlen(keep));
			if(mirror_path == NULL)
			{
				cout << "Realloc failed " << endl;
				return -1;
			}

			strcat(mirror_path,"/");
			strcat(mirror_path,keep);
			mkdir(mirror_path,PERMS);
			free(keep);
		}


		free(del);
		
		

		ret = strrchr(incfilename,'/');
		ret++;

			
		mirror_path = (char*)realloc(mirror_path,strlen(mirror_path)+1+strlen("/")+strlen(ret));
		if(mirror_path == NULL)
		{
			cout << "Realloc failed " << endl;
			return -1;
		}

		strcat(mirror_path,"/");
		strcat(mirror_path,ret);


		int fp;
		fp = open(mirror_path,O_CREAT|O_APPEND|O_RDWR,PERMS);//create file
		if(fp < 0)
		{
			cout << "can not open file in " << mirror_path<< endl;
				ret_status = -1;
				break;
		}
		

		int filesize;

		rc = poll(pipefd,1,DURATION);
		
		if(rc == 0)
		{
			free(mirrordir);
			free(incfilename);
			free(mirror_path);
			closedir(m_dir);			
			return (ret_status = -1);
		}

		if((nread=read(fd,&filesize,4)) == -1)//read the filesize over the pipe
		{
				cout << "Error in reading " << endl;
				ret_status = -1;
				break;
		}

		
		int total = 0;

		int toreceivebytes = filesize;
		int bytes = 0;

		while(toreceivebytes > 0)//while there still bytes left to read
		{	
			if(toreceivebytes < buffsize)//set bytes as max = buffsize or less
				bytes = toreceivebytes;
			else
				bytes = buffsize;


			char* buffer = NULL;
			buffer = (char*)calloc(bytes+1,sizeof(char));

	
			int received = 0;

			nread = 0;
			for(received = 0 ; received < bytes ; received+=nread)//keep reading to fill buffer
			{
				rc = poll(pipefd,1,DURATION);
				
				if(rc == 0)
				{
					free(mirrordir);
					free(incfilename);
					free(mirror_path);
					free(buffer);
					closedir(m_dir);
					return (ret_status = -1);
				}

				nread = read(fd,buffer+received,bytes - received);
				total +=nread;
			}


			write(fp,buffer,bytes);
			free(buffer);

			toreceivebytes = toreceivebytes - bytes;

		}
		close(fp);

		char* size_str=NULL;
		int sneeded = 0;
		sneeded = snprintf(NULL,0,"%d",total);
		if(sneeded < 1)
		{
			cout << "Not enough size allocated" <<endl;
			return (ret_status = -1);
		}

		sneeded++;
		size_str = (char*)malloc(sizeof(char)*sneeded);

		sprintf(size_str,"%d",total);

		int lf = open(log_file,O_APPEND|O_RDWR,PERMS);//write the file receiver and total bytes read
		flock(lf,LOCK_EX);
		write(lf,"RECEIVE ",strlen("RECEIVE "));
		write(lf,ret,strlen(ret));
		write(lf,"\t",strlen("\t"));
		write(lf,size_str,strlen(size_str));
		write(lf,"\n",strlen("\n"));
		close(lf);
		flock(lf,LOCK_UN);

		free(size_str);
		free(incfilename);
		free(mirror_path);
	}

	closedir(m_dir);
	
	if(mirrordir != NULL) free(mirrordir);
	if(mirror_path != NULL) free(mirror_path);

	return ret_status;
}