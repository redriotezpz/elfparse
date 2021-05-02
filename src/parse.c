//remove these headers
//
//#include "parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <elf.h>
#include <stdbool.h>

#include <sys/types.h>

struct elf_header{    //elf_header

  Elf64_Ehdr ehdr;    /* elf header */
  ssize_t ehdr_size;    /* size of the elf header */
};

struct phdr_table{    //phdrtab

  Elf64_Phdr *phdr_table;   /* program header table */
  int phdr_entries;    /* no of program headers */
};

struct STRTAB{    //for sections with type strtab 

  int strtab_index;    /* index of section which holds section header name string table */
  int strtab_size;    /* string table length */ 
  int strtab_entries;     /* no of entries in string table, no of strings */
  off_t strtab_offset;    /* string table offset from begining of the file */
  char **strtab_content;   /* string table data */
};

struct shdr_table{    //shdrtab

  Elf64_Shdr *shdr_table;   /* section header table */
 
  /*
   * structures containing each usefull section information
   */
  struct STRTAB shstrtab;   /* string table structure */

  int shdr_entries;  /* no of section headers */
};

typedef struct {    //data

  char *filename;   /* filename of the elf binary */
  FILE *fh;   /* file structure to keep read data */

  /*
   * elf header / header table structures
   */
  struct elf_header elf_header;
  struct phdr_table phdrtab;
  struct shdr_table shdrtab;

} ElfData;

void free_all(ElfData *data){

  /* 
   * shstrtab contains details of section header string table. memeber 
   * strtab_content contains char * pointing to heap where name of every 
   * section is stored 
   */
  for(int i = 0; i < data->shdrtab.shstrtab.strtab_entries; i++){

    free(data->shdrtab.shstrtab.strtab_content[i]);
  }

  /* here we are cleaning char *, whcih are stored in heap */
  free(data->shdrtab.shstrtab.strtab_content);

  /* here we are cleaning up shdr_table (type :Elf64_shdr *) which pointes to an array of Elf64_shdr structures */
  free(data->shdrtab.shdr_table);

  /* here we are cleaning up phdr_table (type :Elf64_phdr *) which points to an array of Elf64_phdr structures */
  free(data->phdrtab.phdr_table);
}

void exit_code(char *msg, int code){

  puts(msg);
  exit(code);
}

char *extrace_info(int index){

  // extract debug into from section and return it to caller;
  return NULL;
}

int get_section_index(struct shdr_table *shdrtab, char *section_name){

  for(int i = 0; i < shdrtab->shstrtab.strtab_entries; i++){

    if(!strcmp(shdrtab->shstrtab.strtab_content[i], section_name))
      return i;
  }
  return -1;
}

char **parse_strings(char *buffer, int entries, int size){

  char **parsed_string = calloc(sizeof(char *), entries);
  if(!parsed_string) return NULL;

  int entry_i = 0;
  for (int i = 0; i < size; ++i){

    if(buffer[i] != '\0'){

      parsed_string[entry_i] = &buffer[i];
      while(buffer[i] != '\0') ++i; 
      ++entry_i;
    }
    else
      while(buffer[i] == '\0') i++;
  }
  return parsed_string;
}

int get_no_of_strings(char *buffer, int size){

  int strings = 0;
  for (int i = 1; i < size; i++){

    if(buffer[i] == '\0')
      ++strings;
    else 
      continue;
  }
  return strings;
}

void print_section_names(ElfData *data){

  /*get the section header table index to get the string table. ehdr.e_shstrndx containts the index of shstrtab table*/
  data->shdrtab.shstrtab.strtab_index = data->elf_header.ehdr.e_shstrndx;

  /*use section header index to access access corresponding section;'s shdr structure and use sh_offset to get the offset of the string table */
  data->shdrtab.shstrtab.strtab_offset = data->shdrtab.shdr_table[data->shdrtab.shstrtab.strtab_index].sh_offset;
  
  /* seek to that offset */
  fseek(data->fh, data->shdrtab.shstrtab.strtab_offset, SEEK_SET);
  
  /* set size of the  string table using sh_size */
  data->shdrtab.shstrtab.strtab_size = data->shdrtab.shdr_table[data->shdrtab.shstrtab.strtab_index].sh_size;

  /* allocate a buffer with size sh_size, then read shstrtab into that buffer */
  char *buffer = malloc(data->shdrtab.shstrtab.strtab_size);
  if(!buffer) exit_code("malloc failed", EXIT_FAILURE);
  
  if(fread(buffer, 1, data->shdrtab.shstrtab.strtab_size, data->fh) < (unsigned long)data->shdrtab.shstrtab.strtab_size) 
    exit_code("fread failed", EXIT_FAILURE);
 
  /* 
   * after reading, we need tp get the exact number of strings in that table. this is also possible using something like 
   * e_shnum, which gives us number of sections. number of sections == no of strings in shstrtab
   */
  data->shdrtab.shstrtab.strtab_entries = get_no_of_strings(buffer, data->shdrtab.shstrtab.strtab_size); 

  printf("%d\t%d\n", data->shdrtab.shstrtab.strtab_size, data->shdrtab.shstrtab.strtab_entries);

  /* read to (char *) buffer , sh_size of bytes from current position (section .strtab offset)) */
  data->shdrtab.shstrtab.strtab_content = parse_strings(buffer, data->shdrtab.shstrtab.strtab_entries, data->shdrtab.shstrtab.strtab_size);

  for (int i = 0; i < data->shdrtab.shstrtab.strtab_entries; i++){

    printf("%d %s\n", i, data->shdrtab.shstrtab.strtab_content[i]);
  }
}

bool check_elf(FILE *fh){

  uint8_t buf[4];
  
  fread(buf, sizeof(uint8_t), 3, fh);
  
  if(buf[0] == EI_MAG0 && buf[1] == EI_MAG1 && buf[2] == EI_MAG2 && buf[3] == EI_MAG3) return true;

  return false;
}

void assign_headers(ElfData *data){

  //there is some error with check_elf function
  //if(!check_elf(data->fh)) exit_code("file is not an elf binary");
  
  /* assigning ehdr with data read from the binary */
  data->elf_header.ehdr_size = sizeof(Elf64_Ehdr);
  if(fread(&data->elf_header.ehdr, data->elf_header.ehdr_size, 1, data->fh) < 1) 
    exit_code("unable to read file", EXIT_FAILURE);
  

  /* assigning phdr with data read from binary using ehdr->e_phoff */ 
  data->phdrtab.phdr_entries = data->elf_header.ehdr.e_phnum;
  
  data->phdrtab.phdr_table = calloc(data->phdrtab.phdr_entries, sizeof(Elf64_Phdr));
  if(!data->phdrtab.phdr_table) exit_code("calloc failed", EXIT_FAILURE);

  fseek(data->fh, data->elf_header.ehdr.e_phoff, SEEK_SET); //seeking file pointer to position where program header table begins

  if(fread(data->phdrtab.phdr_table, sizeof(Elf64_Phdr), data->phdrtab.phdr_entries, data->fh) < (unsigned long)data->phdrtab.phdr_entries) //reading from that location to allocated space in heap
    exit_code("could not read program header table", EXIT_FAILURE);


  /* assigning shdr with data read from binary using ehdr->e_shoff */
  data->shdrtab.shdr_entries = data->elf_header.ehdr.e_shnum;

  data->shdrtab.shdr_table = calloc(data->shdrtab.shdr_entries, sizeof(Elf64_Shdr));
  if(!data->shdrtab.shdr_table) exit_code("calloc failed", EXIT_FAILURE);

  fseek(data->fh, data->elf_header.ehdr.e_shoff, SEEK_SET);

  if(fread(data->shdrtab.shdr_table, sizeof(Elf64_Shdr), data->shdrtab.shdr_entries, data->fh) < (unsigned long)data->shdrtab.shdr_entries)
    exit_code("could not read section header table", EXIT_FAILURE);

}

FILE *open_file(char *filename){

  FILE *local = fopen(filename, "rb");
  if(!local)
    exit_code("coud not open binary file", EXIT_FAILURE);

  return local;
}

int main(void){

  ElfData data;
  data.filename = "dbg";
  printf("%s\n", data.filename);

  data.fh = open_file(data.filename);
  assign_headers(&data);

  print_section_names(&data);
  int index = get_section_index(&data.shdrtab, ".debug_info");

  char *debug_info = extrace_info(index);

  free_all(&data);
  exit_code(NULL, EXIT_SUCCESS);
}
