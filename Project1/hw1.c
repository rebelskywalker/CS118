#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUF_SIZE 1000000

int main (int argc, char **argv)
{

  
  //Print Usage, note that executable is an arg too
  //So we check for argc less 2 to require a or c to be used
  if (argc < 2)
    {
      printf("Please enter at least one argument");
      exit(2);
    }

  
  //OPtions start with - or --
  //An integer for our option
  int option;
  char htmlstring[100];


  char buf[BUF_SIZE];
  ssize_t nread;
  size_t len2;
  
  //Simple flag to check if mandatory a option used
  int aflag = 0; int cflag = 0;

  const char * http  = "http://"; 
  char host[100];
  char hostonly[100];

  char *path; 
  //As long as we can read an option we report the value
  //We are specifically looking for option a and c, and loop while not -1
  //getopt(int argc, char *const argv[], const char *optstring) where
  //The opstring is chars for options. -1 when no more options, optarg is ptr to value
  //argc is argument count, argc is array in main
  //optind is the index of next element in argv
  //The semi colon represents a value can be taken: html string
  while ((option = getopt(argc, argv, "a:c")) != -1)
    {
      //option is the char value
      switch(option)
	{
	case 'c':
	  cflag = 1;
	  //Test
	  //printf("User entered the c option\n");
	  break;
	case 'a':
	  aflag = 1;
	  //Test
	  //printf("User entered the a option\n");

	  //Copy html string from opt arg, and get size
	  strcpy(htmlstring, optarg); 
	  int len = strlen(htmlstring);
	  
	  //Test Size
	  //printf("%d\n", len);
	  
	  //URL without http
	  strncpy(host, optarg+7, len );

	  //Check the current host var, should contain path, but not http://
	  //printf("host: %s\n", host);

	  //Need to detect if host ended with a slash
	  int sflag = 0;
	  
	  //URL Path
	  path= strchr(host, '/');
	  if (path != NULL)// if there is something inside
	    {
	      //Set Path to string following '/'
	      path = path + 1;
	      sflag = 1;
	      /*
	      if (path[strlen(path)] = '/')
		{
		  printf("HELLOWORLD\n");
		}
	      */
	    }
	  else
	    {
	      //Else there is no path
	      path = "";
	    }
	  //Check path var that we removed from the host
	  //printf("path is: %s\n", path);

	  //Collect the string size for host with path, and remove path + /
	  int len2 = strlen(host);
	  int numtocopy;
	  if (sflag)
	    {
	      numtocopy = len2 - (strlen(path) + 1); 
	    }
	  else
	    {
	      numtocopy = len2 - (strlen(path));
	    }

	  if(path != NULL && path[strlen(path) - 1] == '/')
	    {
	      //Checking path changing for ending in / , change if bad
	      //	      printf("Before path: %s\n", path);
	      path[strlen(path)-1] = '\0';
	      //	      printf("After path: %s\n", path);	      
	    }
	  
	  //Correctly gathers and copies host without path to hostonly
	  //Test size for num bytes to copy
	  //printf("size for numcopy: %d\n", numtocopy);
	  strncpy(hostonly, host, numtocopy);

	  //This checks for the html string containing http:// and
	  //if it has more then just http://
	  if (strlen(htmlstring) > 7 && !strncmp(htmlstring, http, 7));
	  else
	    {
	      printf("Error: Invalid URL Format: %s\n", htmlstring);
	      printf("Exiting code 1");
	      exit(1);
	    }
	  //Test
	  //Checking html argument
	  //printf("Testing HTML argument: %s\n",  htmlstring);
	  //The host name is correctly printing after the http://
	  //and before the first /

	  //Also Check the host only without the path or http://
	  //printf("Host Only: %s\n", hostonly);
	  break;
	//Default case for wrong arguments
	default:
	  printf("Error: Did not use a correct option, or provide arg for -a\n");
	  printf("Exiting code 2 for missing argument");
	  exit(2);
	}//end switch
    }//end while

  //Making sure user enters -a
  if (aflag == 0)
    {
      printf("option a is manadatory!\n");
      exit(2);
    }

  //  printf("host is: %s\n", host);
  //At this point, host is the same as html string.
  char greq[100] = {0};
  int errcode; // for getaddrinfo
  struct addrinfo hints, *result, *rp;
  int sfd;
  size_t nlen;

  //Below code was adapted/edited from documentation page for getaddrinfo
  
  /*Obtain address for host/port */
  // memset(&hints, 0, sizeof(hints));
  //hints.ai_family = AF_UNSPEC;
  //hints.ai_socktype = SOCK_STREAM;
  //hints.ai_flags = 0;
  //hints.ai_protocol = 0;

  /* Testing Values for what is stored in important variables so far*/
  /*
  printf("htmlstring: %s\n", htmlstring);
  printf("path: %s\n", path);
  printf("host: %s\n", host);
  printf("hostonly: %s\n", hostonly);  
  */
  errcode = getaddrinfo (hostonly, "80", NULL, &result);

  	   sprintf(greq, "GET /%s HTTP/1.1\r\nHOST: %s\r\nUser-Agent: http-get 1.0\r\n\r\n", path, hostonly);
	   if (cflag == 1)
	     {
	       //Test
	       //printf("Testing Get Request: \n\n");	       
	       printf("%s", greq);
	       exit(EXIT_SUCCESS);
	     }

  if (errcode != 0)
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
      exit(EXIT_FAILURE);
    }
  else
    {
      //printf("Successful getaddrinfo \n");
    }//this prints so getaddrinfo is successful for html


  /* getaddrinfo() returns a list of address structures.
              Try each address until we successfully connect(2).
              If socket(2) (or connect(2)) fails, we (close the socket
              and) try the next address. */

           for (rp = result; rp != NULL; rp = rp->ai_next) {
               sfd = socket(rp->ai_family, rp->ai_socktype,
                            rp->ai_protocol);
               if (sfd == -1)
                   continue;

               if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
                   break;                  /* Success */

               close(sfd);
           }

           freeaddrinfo(result);           /* No longer needed */

           if (rp == NULL) {               /* No address succeeded */
               fprintf(stderr, "Could not connect\n");
               exit(EXIT_FAILURE);
           }


	   //Testing moving the get req, before the getaddrinfo

	   
	   nlen = strlen(greq);
  	   if(write(sfd, greq, nlen) != nlen)
	     {
	       fprintf(stderr, "partial failed write\n");
	       exit(EXIT_FAILURE);
	     }

	   /*
	   //piazza code
	   struct timeval timeout;
	   timeout.tv_sec = 5;
	   timeout.tv_usec = 0;

	   while (1)
	     {
	       fd_set readfds;
	       FD_SET(sfd, &readfds);
	       if (select(sfd + 1, &readfds, NULL, NULL, &timeout) > 0)

		 {
		   nread = read(sfd, buf, BUF_SIZE);
		   if (nread == -1)
		     {
		       perror("read");
		       exit(EXIT_FAILURE);

		     }

		 }
	       else
		 {
		   break;
		 }
	     }

	     //piazza code
	     */
	   
	    
	   nread = read(sfd, buf, BUF_SIZE);
	   if (nread == -1)
	     {
	       perror("read");
	       exit(EXIT_FAILURE);
	     }
	   

	     
	   //From doc page
	   //	   printf("Received %zd bytes: %s\n", nread, buf);
	   printf("%s\n", buf);
	   //End of execution test for earlier prints and checks
	   //printf("We should have printed by now\n\n");
	   close(sfd);
	   exit(EXIT_SUCCESS);
  
  
}//end main
