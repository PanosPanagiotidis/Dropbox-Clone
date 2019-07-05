#include <iostream>
#include <dirent.h>
#include <iostream>
#include <cerrno>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include "mirror_functions.h"
#include <sys/types.h>
#include <fcntl.h>

#define PERMS 0777

using namespace std;


volatile int keepalive = 1;
int sPid = -1;
child_info** pid_info = NULL;
int cinf_size = 0;

/*
*
*	When receiver has received all files successfully
*	it sends a SIGUSR2 to the parent process.
*
*/

void catch_receiver(int signo,siginfo_t *info,void* context)
{
	sPid = info->si_pid;
	char send_info[] = "Receiver got all files succesfully \n";
	write(1,send_info,strlen(send_info)+1);

	return;
}


/*
*
*	Catches SIGCHLD from child that exited 
*	and releases its resources.
*	No more zombies!
*/

void catch_child(int signo,siginfo_t *info,void* context)
{
	sPid = info->si_pid;
	int status;
	waitpid(sPid,&status,0);
	int es;

	if(WIFEXITED(status)) es = WEXITSTATUS(status);

	if(es != 0)
	{
		char war[] = "Caught fail.Trying again\n";
		write(1,war,strlen(war)+1);
		for(int i = 0 ; i < cinf_size ; i++)
		{
			// cout << sPid << endl;
			if(sPid == pid_info[i]->pid)
			{	
				cout << pid_info[i]->type << endl;
				pid_info[i]->attempt++;
				if(pid_info[i]->attempt > 3)
				{
					char msg[] = "Tried three times.Stopping now...\n";
					write(1,msg,strlen(msg)+1);
					return;
				}

				if(pid_info[i]->type == 's')//If it is a sender fork a sender
				{
					if((pid_info[i]->pid = fork()) == 0 )
					{	write(1,"sender",strlen("sender")+1);
						execl("./sender.out","./sender.out",pid_info[i]->id_foreign,pid_info[i]->og_id
							,pid_info[i]->buffsize,pid_info[i]->input_dir
							,pid_info[i]->common_dir,pid_info[i]->log_file,(char*)0);
						_exit(127);
					}
					else if(pid_info[i]->pid < 0)
					{
						return;
					}
					return;

				}
				else if(pid_info[i]->type == 'r') //if it is a receiver fork a receiver
				{
					if((pid_info[i]->pid = fork()) == 0 )
					{
						execl("./receiver.out","./receiver.out",pid_info[i]->id_foreign,pid_info[i]->og_id
							,pid_info[i]->buffsize,pid_info[i]->mirror_dir
							,pid_info[i]->common_dir,pid_info[i]->log_file,(char*)0);
						_exit(127);
					}
					else if(pid_info[i]->pid < 0)
					{
						return;
					}
					return;
				}

			}
	}
	}

	return;
}

/*
*
*	Catching SIGINT or SIGQUIT 
*	changes global variable keepalive to 0
*	ending the continuous loop of execution
*/

void catch_interrupt(int signo,siginfo_t *info,void* context)
{
	
	keepalive = 0;
	return;
}

void catch_quit(int signo,siginfo_t *info,void* context)
{
	keepalive = 0;
	return;
}


/*
*	Receiving SIGUSR1 from a child means it failed
*	Catch_failure loops through the global array with all
*	process ids.Whether it is a r(eceiver) or a s(ender)
*	It forks another child up to three times before finally
*	giving up.
*/





int main(int argc,char* argv[])
{	
	/*
	*	Install signal handlers for :
	*/

	//SIGUSR2 - reception of files was successful
	struct sigaction rec_action;
	rec_action.sa_sigaction = catch_receiver;
	sigemptyset(&rec_action.sa_mask);
	rec_action.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR2,&rec_action,NULL);

	//SIGCHLD - a child has exited.call waitpid() on sighandler
	struct sigaction chld_action;
	chld_action.sa_sigaction = catch_child;
	sigfillset(&chld_action.sa_mask);
	chld_action.sa_flags = SA_SIGINFO;
	sigaction(SIGCHLD,&chld_action,NULL);

	//SIGINT - terminate our running loop
	struct sigaction int_action;
	int_action.sa_sigaction = catch_interrupt;
	sigemptyset(&int_action.sa_mask);
	int_action.sa_flags = SA_SIGINFO;
	sigaction(SIGINT,&int_action,NULL);

	//SIGQUIT - terminate our running loop
	struct sigaction quit_action;
	quit_action.sa_sigaction = catch_quit;
	sigemptyset(&quit_action.sa_mask);
	quit_action.sa_flags = SA_SIGINFO;
	sigaction(SIGQUIT,&quit_action,NULL);



	int status;

	if(argc != 13)
	{
		cout << "check your args " << endl;
		exit(1);
	}

	char* id_str = NULL;
	char* buffsize = NULL;
	char* mirror_dir = NULL;
	char* input_dir = NULL;
	char* inclog_file = NULL;
	char* common_dir = NULL;

	/*
	*	Get all args as strings:
	*	Common_dir Mirror_dir buffer_size id log_file and input_dir
	*/
	getArgs(argc,argv,&id_str,&common_dir,&input_dir,&mirror_dir,&buffsize,&inclog_file);

	int id=atoi(id_str);


	DIR* input = NULL;
	input = opendir(input_dir);

	if(input == NULL) //check if input_dir exists and exit if it doesnt
	{
		cout << "Folder not found :" << strerror(errno)<< endl;
		exit(1);
	
	}

	closedir(input);

	char* log_file = NULL;
	char log_loc[] = "./logs";
	DIR* log = opendir(log_loc);

	if(log == NULL) //check if folder log exists and create it if it doesnt
	{
		cout << "Log folder not found.Creating..." << endl;
		mkdir(log_loc,PERMS);
	}

	closedir(log);

	if((strstr(inclog_file,"logs/") == NULL) && (strstr(inclog_file,"./logs/") == NULL))
	{
		log_file = (char*)calloc(strlen(log_loc)+strlen("/")+strlen(inclog_file)+1,sizeof(char));
		strcat(log_file,log_loc);
		strcat(log_file,"/");
		strcat(log_file,inclog_file);
	}
	else
	{
		log_file = (char*)calloc(strlen(inclog_file)+1,sizeof(char));
		strcat(log_file,inclog_file);
	}



	DIR* mirror = opendir(mirror_dir);//check mirror dir and if it doesnt exist create it
	if(mirror != NULL)
	{
		cout << "Mirror folder already exists!Exiting..." << endl; //if it exists exit
		exit(1);
	}
	else
	{
		cout << "Mirror folder does not exist.Creating..." << endl;
		status = mkdir(mirror_dir,PERMS);

		if(status == -1)
		{
			cout << "Cannot create folder mirror : " << strerror(errno) << endl;
			exit(1);
		}
	}

	closedir(mirror);

	DIR* common = opendir(common_dir);//check if common dir exists and if not create it
	if(common == NULL)
	{	
		cout << "common_dir does not exist.Creating..." << endl;
		status = mkdir(common_dir,PERMS);

		if(status == -1)
		{
			cout << "Cannot create folder common : " << strerror(errno) << endl;
			exit(1);
		}
	}


	closedir(common);

	FILE* pidfile;

	char* pid_str = NULL;

	int sneeded = 0;
	sneeded = snprintf(NULL,0,"%d",getpid());

	if(sneeded < 1 )
	{
		cout << "Not enough size for sprintf " << endl;
		exit(1);
	}

	sneeded++;
	pid_str = (char*)calloc(sneeded,sizeof(char));

	sprintf(pid_str,"%d",getpid());//write process id to $(id).id file


	char* filepath_buffer = NULL;
	filepath_buffer = (char*)calloc(strlen(common_dir)+1+strlen("/")+strlen(id_str)+1+strlen(".id")+1,sizeof(char));
	
	strcat(filepath_buffer,common_dir);
	strcat(filepath_buffer,"/");
	strcat(filepath_buffer,id_str);
	strcat(filepath_buffer,".id");


	if(access(filepath_buffer,F_OK) != -1)
	{
		cout << "File with id " << id << " already exists.Exiting..." << endl;
		exit(1);
	}

	
	pidfile = fopen(filepath_buffer,"ab+");
	fputs(pid_str,pidfile);
	fclose (pidfile);

	int lf;
	lf = open(log_file,O_CREAT|O_APPEND|O_RDWR,PERMS);

	if(lf < 0)
	{
		cout << "could not open log file " << endl;
		exit(1);
	}

	write(lf,id_str,strlen(id_str)); //Write id first and START keyword,meaning sending and receiving has started
	write(lf,"\n",strlen("\n"));
	write(lf,"START\n",strlen("START\n"));
	close(lf);


	/*
	*	Create an id_info struct keeping all ids that entered the application
	*	Set exists[i] to 0 or 1 whether it left the application or it is still in
	*/
	id_info* existing_ids = (id_info*)malloc(sizeof(id_info));
	existing_ids->new_ids = NULL;
	existing_ids->size = 0;
	existing_ids->exist = NULL;

	while(keepalive == 1)
	{
		//Check after 2 seconds
		sleep(2);

		id_info* sync_ids = NULL;//New ids that we need to sync with

		sync_ids = check_new_ids(&existing_ids,common_dir,id);

		//Check if any id's have been deleted
		check_deleted_ids(&existing_ids,common_dir,id,mirror_dir);

		if(sync_ids == NULL) continue; //if there are no new ids go to next cycle


		char* checkid = NULL; //id to be checked

		for(int i = 0 ; i < sync_ids->size ;i++)//For each new id 
		{
			sneeded = snprintf(NULL,0,"%d",sync_ids->new_ids[i]);

			if(sneeded < 1)
			{
				cout << "not enough space for sprintf"<<endl;
				exit(1);
			}

			sneeded++;
			checkid = (char*)calloc(sneeded,sizeof(char));
			sprintf(checkid,"%d",sync_ids->new_ids[i]);

			for(int j = 0 ;j < 2 ;j++)//Create two children one to send one to receive
			{	

				/*
				*	pid_info is a global var keeping all required info for each child
				*	so that it can restart in case of failure
				*/
				cinf_size++;
				pid_info = (child_info**)realloc(pid_info,sizeof(child_info*)*cinf_size);
				pid_info[cinf_size-1] = (child_info*)malloc(sizeof(child_info));
				pid_info[cinf_size-1]->id_foreign = strdup(checkid);
				pid_info[cinf_size-1]->attempt = 1;
				pid_info[cinf_size-1]->og_id = strdup(id_str);
				pid_info[cinf_size-1]->common_dir = strdup(common_dir);
				pid_info[cinf_size-1]->buffsize = strdup(buffsize);
				pid_info[cinf_size-1]->log_file = strdup(log_file);
				pid_info[cinf_size-1]->input_dir = strdup(input_dir);
				pid_info[cinf_size-1]->mirror_dir = strdup(mirror_dir);

				if(j == 0)
					pid_info[cinf_size-1]->type = 's';
				else
					pid_info[cinf_size-1]->type = 'r';

				
				if((pid_info[cinf_size-1]->pid = fork()) == 0) //CHILD CODE
				{
					if(j == 0)//SENDER
					{
						execl("./sender.out","./sender.out",checkid,id_str,buffsize,input_dir,common_dir,log_file,(char*)0);
						_exit(127);
					}
					if(j == 1)//RECEIVER
					{
						execl("./receiver.out","./receiver.out",checkid,id_str,buffsize,mirror_dir,common_dir,log_file,(char*)0);
						_exit(127);
					}
				}
				else if (pid_info[cinf_size-1]->pid < 0 )
				{
					perror("fork failed\n");
				}

			}

			free(checkid);
			checkid = NULL;

		}

			if(sync_ids != NULL){
				free(sync_ids->new_ids);
				free(sync_ids);
			}

	}
	
	pid_t dpid;

	char* fullcmd = NULL;
	char cmd[] = "rm -r ";
	fullcmd = (char*)calloc(strlen(cmd)+strlen(mirror_dir)+1,sizeof(char));

	strcat(fullcmd,cmd);
	strcat(fullcmd,mirror_dir);

	usleep(500);
	if((dpid = fork()) == 0) //Fork a new child and call rm -r on our mirror directory
	{
		execl("/bin/bash","bash","-c",fullcmd,(char*)0);
		_exit(127);
	}

	free(fullcmd);

	lf = open(log_file,O_CREAT|O_APPEND|O_RDWR,PERMS);//Write END meaning we stopped all processes
	if(lf < 0)
		cout << "could not open file " << endl;
	write(lf,"END \n",strlen("END \n"));
	close(lf);
	cout << "Exiting..."<<endl;
	cout << "Log can be found at " << log_file << endl;

	/*
	*	Free allocated data
	*/

	free(existing_ids->new_ids);
	free(existing_ids->exist);
	free(existing_ids);
	remove(filepath_buffer);
	free(filepath_buffer);

	free(pid_str);
	free(common_dir);
	free(mirror_dir);
	free(buffsize);
	free(inclog_file);
	free(log_file);
	free(input_dir);
	free(id_str);

	for(int i = 0 ;i < cinf_size ;i++)
	{
		free(pid_info[i]->id_foreign);
		free(pid_info[i]->og_id);
		free(pid_info[i]->common_dir);
		free(pid_info[i]->input_dir);
		free(pid_info[i]->log_file);
		free(pid_info[i]->buffsize);
		free(pid_info[i]->mirror_dir);
		free(pid_info[i]);
	}

	free(pid_info);

}