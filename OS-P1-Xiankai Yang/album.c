#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#define STRING_LEN 32

char *indent = "===============";

typedef struct {
	char original[STRING_LEN];
	char thumbnail[STRING_LEN];
	char medium_size[STRING_LEN];
	char caption[STRING_LEN];
} Photo;


void initialize(Photo *photo, char *string) {
	strcpy(photo -> original, string);
	string = strrchr(string, '/') + 1;
	strcat(strncpy(photo -> thumbnail, string, strlen(string) - 4), "_thumbnail.jpg");
	strcat(strncpy(photo -> medium_size, string, strlen(string) - 4), "_medium_size.jpg");
}

int input_string(char *buffer, int len) {

  int rc = 0, fetched, lastchar;

  if (NULL == buffer)
    return -1;

  // get the string.  fgets takes in at most 1 character less than
  // the second parameter, in order to leave room for the terminating null.  
  // See the man page for fgets.
  fgets(buffer, len, stdin);
  
  fetched = strlen(buffer);


  // warn the user if we may have left extra chars
  if ( (fetched + 1) >= len) {
    fprintf(stderr, "warning: might have left extra chars on input\n");
    rc = -1;
  }

  // consume a trailing newline
  if (fetched) {
    lastchar = fetched - 1;
    if ('\n' == buffer[lastchar])
      buffer[lastchar] = '\0';
  }

  return rc;
}

int main(int argc, char * argv[]) {

	int i, pid, status;

	int rc;

	char ans[STRING_LEN]; //save the user's response

	Photo *photos;
	
	photos = (Photo *)malloc(sizeof(Photo) * argc);

	for (i = 1; i < argc; i++) {

		initialize(photos + i, argv[i]);

		//Generate a thumbnail version (e.g., very small) of the photo

		printf("%s Generating the thumbnail version of %s ...\n", indent, argv[i]);

		pid = fork();

		if (0 == pid) {

			rc = execlp("convert", "convert", photos[i].original, "-thumbnail", "10%", photos[i].thumbnail, NULL);

			printf("Exec error!!!");
			exit(-1);

		}

		rc = waitpid(pid, &status, 0);

		//Display the thumbnail to the user

		printf("%s Displaying the thumbnail version of %s ...\n", indent, argv[i]);	

		pid = fork();

		if (0 == pid) {

			rc = execlp("display", "display", photos[i].thumbnail, NULL);

			printf("Exec error!!!");
			exit(-1);

		}

		//Ask the user whether or not it should be rotated; if so, do it

		printf("%s Do you want to rotate it 90 degrees? (yes/no) :", indent);
		input_string(ans, STRING_LEN);
		while (strcmp(ans, "yes") != 0 && strcmp(ans, "no") != 0) {
			printf("%s Do you want to rotate it 90 degrees? (yes/no) :", indent);
			input_string(ans, STRING_LEN);
		}

		if (strcmp(ans, "yes") == 0) {

			rc = kill(pid, SIGTERM);

			pid = fork();

			if (0 == pid) {

				rc = execlp("convert", "convert", photos[i].thumbnail, "-rotate", "90", photos[i].thumbnail, NULL);
				
				printf("Exec error!!!");
				exit(-1);

			}
			
			rc = waitpid(pid, &status, 0);

			pid = fork();

			if (0 == pid) {

				rc = execlp("display", "display", photos[i].thumbnail, NULL);
				
				printf("Exec error!!!");
				exit(-1);

			}
		}

		//Ask the user for a caption, and collect it
		
		printf("%s Please input a caption for this picture:", indent);

		input_string(photos[i].caption, STRING_LEN);
		printf("%s The caption is %s\n", indent, photos[i].caption);
	
		//Generate a properly oriented half-size version of the photo
		
		printf("%s Generating the medium size photo...\n", indent);
		
		kill(pid, SIGTERM);	
		
		pid = fork();

		if (0 == pid) {
			
			if (strcmp(ans, "yes") == 0)
				rc = execlp("convert", "convert", photos[i].original, "-resize", "25%", "-rotate", "90", photos[i].medium_size, NULL);
			else
				rc = execlp("convert", "convert", photos[i].original, "-resize", "25%", photos[i].medium_size, NULL);

			printf("Exec error!!!");
			exit(-1);
		}

	}

	FILE *file;

	file = fopen("./index.html", "wt");
	
	if (NULL == file) {
		printf("%s Writing error!!!");
		exit(-1);
  }
	
	fprintf(file, "<html>\n");
	fprintf(file, "<head> <title> Album Index </title> </head>\n");
	fprintf(file, "<body>\n");
	fprintf(file, "<h1> Album Index </h1>\n");
	fprintf(file, "Please click on a thumbnail to view a medium-size image\n");
	
	for (i = 1; i < argc; i++) {
		char buffer[200];
		sprintf(buffer, "<h2> %s </h2><a href=\"%s\"><img src=\"%s\"></a>\n", photos[i].caption, photos[i].medium_size, photos[i].thumbnail);
		fprintf(file, buffer);
	}

	fprintf(file, "</body>\n");
	fprintf(file, "</html>");
	fclose(file);

	return 0;
}
