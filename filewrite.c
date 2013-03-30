/*
Name: Open_file_and_write.c 
Date: 02/01/05 
Description: Opens file by append example. 
*/ 
#include <stdio.h> 
int main() 
{ 
FILE *file; 
file = fopen("file.txt","a+"); /* apend file (add text to 
a file or create a file if it does not exist.*/ 
fprintf(file,"%s","This is just an example :)"); /*writes*/ 
fclose(file); /*done!*/ 
//getchar(); /* pause and wait for key */ 
return 0; 
}
