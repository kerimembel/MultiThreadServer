#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>


#define MYPORT 50075

void *connection(void *p) {
	FILE *html_file;
	FILE *output_file;
	char recv_buf[1024];
	char send_buf_temp[1024];
	char *send_buf;
	char file_name[100];
	char *file_data;
	char errormsg[69];
	char output[100];
	int outputsize;
	int recv_len;
	int file_size;
	int temp;
	
	recv_len = 0;
	file_size = 0;

	struct tm *ptr;
	time_t local;
	char times[30];
	
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //initialize mutex for this thread
	
	int *connfd_thread = (int *)p;						//type cast the argument for use in this thread

	temp = (recv (*connfd_thread, recv_buf, sizeof(recv_buf), 0));
		if (temp < 0){		//check for error when receiving
		perror("Receive Error");
		exit(errno);
	}
	temp = temp/sizeof(char);
	recv_buf[temp-2] = '\0';
		
	if ((strncmp(recv_buf, "GET ", 4)) != 0) {			//check to make sure that a GET request was made
		strcpy(errormsg, "Invalid Command Entered\nPlease Use The Format: GET <file_name.html>\n");
		send(*connfd_thread, errormsg, 69, 0);
	}
	//Other wise the name of the file is copied character by charcter.
	else {
		recv_len = strlen(recv_buf);
		int i;
		int j = 0;  //remove the forward slash if neccissary 
		for (i=4; i<recv_len ;i++, j++) {
			if ( (recv_buf[i] == '\0') || (recv_buf[i] == '\n') || (recv_buf[i] == ' ') ){ //If the end of the file path is reached, break.
				break;
			}
			else if ( recv_buf[i] == '/') { 		//do nothing except increment i to jump the forward slash
				--j;
			}
			else {
				file_name[j] = recv_buf[i];		//copy the file name character by character
				
			}
		}	
		file_name[j] = '\0';					//add a null terminator to the end of the file name string
		printf("%s", file_name);	

	
	html_file = fopen(file_name, "r");				//open the html file
	if (html_file == NULL) {					//check to make sure fopen worked(if there was a file) 
		send_buf = (char *)malloc(24);
		strcpy(send_buf, "HTTP/1.1 404 Not Found\n\n");		//if there was no file, return 404 message
		send(*connfd_thread, send_buf, 24, 0);
	}
	else {							//If the file did open without errors
		time(&local);					//Get the current time and date
		ptr = localtime(&local);
		strftime(times, 30, "%a, %d %b %Y %X %Z", ptr);

		fseek(html_file, 0, SEEK_END);			//seek to the end of the tfile
		file_size = ftell(html_file);			//get the byte offset of the pointer(the size of the file)
		fseek(html_file, 0, SEEK_SET);			//seek back to the begining of the file
		file_data = (char *)malloc(file_size + 1);	//allocate memmory for the file data
		fread(file_data, 1, (file_size), html_file);	//read the file data into a string
		file_data[file_size] = '\0';

		sprintf(send_buf_temp, "HTTP/1.1 200 OK\nDate: %s\nContent Length: %d\nConnection: close\nContent-Type: text/html\n\n%s\n\n", times, file_size, file_data); //format and create string for output to client
		int a = strlen(send_buf_temp);			//get the length of the string we just created
		send_buf = (char *)malloc(a);			//allocate space for the reply
		strcpy(send_buf, send_buf_temp);		//copy to the correctly sized string
		send(*connfd_thread, send_buf, a, 0);		//send the info to the client
		free(file_data);				//free the allocated memory for file data
		}
	free(send_buf);						//free the allocated memory for the send buffer
	fclose(html_file);					//close the html file
	pthread_mutex_lock(&mutex);				//mutex lock so that the stats file can only be accessed by one thread at a time
	output_file = fopen("stats.txt","a");			//open stats file
	sprintf(output, "File: %s requested at %s\n", file_name, times);	//format string
	outputsize = strlen(output);										
	fwrite(output, outputsize, 1, output_file);					//write to stats file
	fclose(output_file);								//close the file
	pthread_mutex_unlock(&mutex);							//unlock


	}
	
	close(*connfd_thread);								//close the current connection
	fflush(stdout);									//make sure all output is printed
	pthread_exit(NULL);								//exit the pthread with null
}
int main() {
	int sfd;
	int test;
	int id;
	int connfd[100];
	pthread_t threads[100]; //100 posible threads, just to be safe
	int thread_count = 0;
	struct sockaddr_in addr;
	
	if( (sfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Socket Error");
		exit(errno);
	}

	memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MYPORT);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	if( bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Bind Error");
		return (errno);
	}
	
	while(thread_count < 100){					//loop to count the number of connections/threads
	if( listen(sfd, 10) != 0) {
		perror("Listen Error");
		return (errno);
	}


	connfd[thread_count] = accept(sfd, NULL, NULL);
	if (connfd[thread_count] < 0) {
		perror("Accept Error");
		return (errno);
	}
	
	pthread_create(&threads[thread_count], NULL, connection, &connfd[thread_count]);	//create a thread and receive data
	pthread_join(threads[thread_count], NULL);												//join the finished thread and continue
	thread_count++;
}
	close(sfd);						//close the socket
	return 0;
}
