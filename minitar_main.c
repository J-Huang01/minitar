#include <stdio.h>
#include <string.h>
#include "stdbool.h"

#include "file_list.h"
#include "minitar.h"

int main(int argc, char **argv) {
   if (argc < 4) {
      printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
      return 0;
   }

   file_list_t files;
   file_list_init(&files);

   // TODO: Parse command-line arguments and invoke functions from 'minitar.h'
   // to execute archive operations

   if(strcmp(argv[1],"-c") == 0){//Archive Create
      for(int i = 4; i < argc; i++){//populates linked list with file names
         file_list_add(&files,argv[i]);
      }//get curr_node list
      int create_work = create_archive(argv[3],&files);//creat archive
      if(create_work == 1){
         return 1;
      }
      file_list_clear(&files);//list clear
   }

   else if(strcmp(argv[1],"-a") == 0){//Archive Add
      for(int i = 4; i < argc; i++){//populates linked list with file names
         file_list_add(&files,argv[i]);
      }
      int append_work = append_files_to_archive(argv[3], &files);//add curr_node to archive
      if(append_work == 1){
         return 1;
      }
      file_list_clear(&files);//list clear
   }

   else if(strcmp(argv[1],"-t") == 0){//Archive List
      int have_list = get_archive_file_list(argv[3],&files);
      if(have_list == 1){
         return 1;
      }
      node_t *curr_node = files.head;
      while (curr_node != NULL) {
            printf("%s\n", curr_node->name);//Prints out file names
            curr_node = curr_node->next;
      }
      file_list_clear(&files);//cleans up
   }

   else if(strcmp(argv[1],"-u") == 0){//Archive Update
      if(get_archive_file_list(argv[3],&files) == 1){
         return 1;
      }
      bool contain_file = true;
      for(int i = 4; i < argc; i++){
         if(!file_list_contains(&files,argv[i])){
            contain_file = false;
            break;
         }
      }
      if(contain_file){//has all file
         file_list_clear(&files);
         for(int i = 4; i < argc; i++){
            file_list_add(&files,argv[i]);
         }
         append_files_to_archive(argv[3], &files);//append files
      }
      else{
         printf("Error: One or more of the specified files is not already present in archive");
      }
      file_list_clear(&files); //clear list
   }

   else if(strcmp(argv[1],"-x") == 0){//Archive extract
      int extract_work = extract_files_from_archive(argv[3]);
      if(extract_work == 1){
         return 1;
      }
   }

   else{
      printf("operation error");
   }
   //file_list_clear(&files);
   return 0;
}
