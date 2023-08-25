// Purpose:  This program demonstrates some of the system calls you can use
//           to copy data in block sized chunks from a file, store it in "blocks"
//           and copy the data back in block size chunks into a new file.
//           You are free to use this code in assignment 4 if you wish.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 65536
#define BLOCKS_PER_FILE 1024
#define NUM_FILES 256
#define FIRST_DATA_BLOCK 1001
#define MAX_FILE_SIZE 1048576
#define HIDDEN 0x1
#define READONLY 0x2

unsigned char file_data[NUM_BLOCKS][BLOCK_SIZE];

uint8_t *free_blocks;
uint8_t *free_inodes;

// directory
struct directoryEntry
{
  char filename[64];
  short in_use;
  int32_t inode;
};

struct directoryEntry *directory;

// inode
struct inode
{
  int32_t blocks[BLOCKS_PER_FILE];
  short in_use;
  uint8_t attribute;
  uint32_t file_size;
};
struct inode *inode;

FILE *fp;
char image_name[64];
uint8_t image_open;

#define WHITESPACE " \t\n" // We want to split our command line up into tokens
                           // so we need to define what delimits our tokens.
                           // In this case  white space
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size
#define MAX_NUM_ARGUMENTS 10

int32_t findFreeBlock()
{
  int i;
  for (i = 0; i < NUM_BLOCKS; i++)
  {
    if (free_blocks[i])
    {
      return i + 790;
    }
  }
  return -1;
}

int32_t findFreeInode()
{
  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (free_inodes[i])
    {
      return i;
    }
  }
  return -1;
}

int32_t findFreeInodeBlock(int32_t inode_)
{
  int i;
  for (i = 0; i < BLOCKS_PER_FILE; i++)
  {
    if (inode[inode_].blocks[i] == -1)
    {
      return i;
    }
  }
  return -1;
}

void initialize()
{
  directory = (struct directoryEntry *)&file_data[0][0];
  inode = (struct inode *)&file_data[20][0];
  free_blocks = (uint8_t *)&file_data[277][0];
  free_inodes = (uint8_t *)&file_data[19][0];
  memset(image_name, 0, 64);
  image_open = 0;

  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    directory[i].in_use = 0;
    directory[i].inode = -1;
    free_inodes[i] = 1;

    memset(directory[i].filename, 0, 64);

    int j;

    for (j = 0; j < NUM_BLOCKS; j++)
    {
      inode[i].blocks[j] = -1;
      inode[i].in_use = 0;
      inode[i].attribute = 0;
      inode[i].file_size = 0;
    }
  }
  int j;
  for (j = 0; j < NUM_BLOCKS; j++)
  {
    free_blocks[j] = 1;
  }
}

uint32_t df()
{
  int j;
  int count = 0;
  for (j = FIRST_DATA_BLOCK; j < NUM_BLOCKS; j++)
  {
    if (free_blocks[j])
    {
      count++;
    }
  }
  return count * BLOCK_SIZE;
}

void createfs(char *filename)
{
  fp = fopen(filename, "w");
  strncpy(image_name, filename, strlen(filename));
  memset(file_data, 0, NUM_BLOCKS * BLOCK_SIZE);
  image_open = 1;

  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    directory[i].in_use = 0;
    directory[i].inode = -1;
    free_inodes[i] = 1;
    memset(directory[i].filename, 0, 64);

    int j;
    for (j = 0; j < NUM_BLOCKS; j++)
    {
      inode[i].blocks[j] = -1;
      inode[i].in_use = 0;
      inode[i].attribute = 0;
    }
  }
  int j;
  for (j = 0; j < NUM_BLOCKS; j++)
  {
    free_blocks[j] = 1;
  }
  fclose(fp);
}

void savefs()
{
  if (image_open == 0)
  {
    printf("ERROR: Disk iamge is not open\n");
  }

  fp = fopen(image_name, "w");
  fwrite(&file_data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
  memset(image_name, 0, 64);

  fclose(fp);
}

void openfs(char *filename)
{
  fp = fopen(filename, "r");
  strncpy(image_name, filename, strlen(filename));
  fread(&file_data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
  image_open = 1;
  fclose(fp);
}

void closefs()
{
  if (image_open == 0)
  {
    printf("ERROR: disk image is not open\n");
    return;
  }
  fclose(fp);
  image_open = 0;
  memset(image_name, 0, 64);
}

void list(char *hidden, char *a)
{
  int i;
  int not_found = 1;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use)
    {
      if (!hidden)
      {
        if (inode[directory[i].inode].attribute & HIDDEN)
        {
          continue;
        }
      }
      not_found = 0;

      printf("%s", directory[i].filename);

      if (a && strcmp(a, "-a") == 0)
      {
        printf(" - %02X", inode[directory[i].inode].attribute);
      }
      printf("\n");
    }
  }
  if (not_found)
  {
    printf("ERROR: No files found.\n");
  }
}

// insert shall allow the user to put a new file into the file system.
// The command shall take the form: insert <filename>
// If the filename is too long an error will be returned stating: "insert error: File name too long."
// If there is not enough disk space for the file an error will be returned stating: "insert error: Not enough disk space."
void insert(char *filename)
{
  // verify the filename isn't NULL
  if (filename == NULL)
  {
    printf("ERROR: Filename is NULL\n");
    return;
  }
  // verify the file exists
  struct stat buf;
  int return_value = stat(filename, &buf);
  if (return_value == -1)
  {
    printf("ERROR: File does not exist\n");
    return;
  }
  // verify the file isn't too big
  if (buf.st_size > MAX_FILE_SIZE)
  {
    printf("ERROR: File is too large.\n");
    return;
  }
  // verify there is enough space
  if (buf.st_size > df())
  {
    printf("ERROR: Not enough free disk space. \n");
    return;
  }
  // find an empty directory entry
  int i;
  int directory_entry = -1;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use == 0)
    {
      directory_entry = i;
      break;
    }
  }
  if (directory_entry == -1)
  {
    printf("ERROR: Could not find a free directory entry\n");
    return;
  }

  // find free inodes and place the file
  FILE *ifp = fopen(filename, "r");
  printf("Reading %d bytes from %s\n", (int)buf.st_size, filename);

  int32_t copy_size = buf.st_size;
  int32_t offset = 0;
  int32_t block_index = -1;

  // find a free inode
  int32_t inode_index = findFreeInode();
  if (inode_index == -1)
  {
    printf("ERROR: Can not find a free inode.\n");
    return;
  }

  // place the file info in the directory
  directory[directory_entry].in_use = 1;
  directory[directory_entry].inode = inode_index;
  strncpy(directory[directory_entry].filename, filename, strlen(filename));

  inode[inode_index].file_size = buf.st_size;

  while (copy_size > 0)
  {
    fseek(ifp, offset, SEEK_SET);
    block_index = findFreeBlock();
    if (block_index == -1)
    {
      printf("ERROR: Can not find a free block.\n");
      return;
    }

    int32_t bytes = fread(file_data[block_index], BLOCK_SIZE, 1, ifp);

    int32_t inode_block = findFreeInodeBlock(inode_index);
    inode[inode_index].blocks[inode_block] = block_index;

    if (bytes == 0 && !feof(ifp))
    {
      printf("ERROR: An error occured reading from the input file.\n");
      return;
    }

    clearerr(ifp);
    copy_size -= BLOCK_SIZE;
    offset += BLOCK_SIZE;
    block_index = findFreeBlock();
  }
  fclose(ifp);
}

// The delete command shall allow the user to delete a file from the file system
// If the file does exist in the file system it shall be deleted and all the space available for additional files.
void delete(char *filename)
{
  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && strcmp(directory[i].filename, filename) == 0)
    {
      // found the file, mark blocks as free
      int j;
      for (j = 0; j < BLOCKS_PER_FILE; j++)
      {
        if (inode[directory[i].inode].blocks[j] != -1)
        {
          free_blocks[inode[directory[i].inode].blocks[j] - FIRST_DATA_BLOCK] = 1;
        }
      }

      // mark inode as free
      free_inodes[directory[i].inode] = 1;

      // remove directory entry
      memset(directory[i].filename, 0, 64);
      directory[i].inode = -1;
      directory[i].in_use = 0;

      // update file system image
      fseek(fp, i * sizeof(struct directoryEntry), SEEK_SET);
      fwrite(&directory[i], sizeof(struct directoryEntry), 1, fp);

      fseek(fp, directory[i].inode * sizeof(struct inode), SEEK_SET);
      fwrite(&inode[directory[i].inode], sizeof(struct inode), 1, fp);

      fseek(fp, 20, SEEK_SET);
      fwrite(inode, sizeof(struct inode), NUM_FILES, fp);

      fseek(fp, 277, SEEK_SET);
      fwrite(free_blocks, 1, NUM_BLOCKS, fp);

      fseek(fp, 19, SEEK_SET);
      fwrite(free_inodes, 1, NUM_FILES, fp);

      return;
    }
  }

  // file not found
  printf("File not found\n");
}

// The undelete command shall allow the user to undelete a file that has been deleted from the file system
// If the file does exist in the file system directory and marked deleted it shall be undeleted.
// If the file is not found in the directory then the following shall be printed: "undelete: Can not find the file."
void undelete(char *filename)
{
  // Search the directory for the given filename
  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && strcmp(directory[i].filename, filename) == 0)
    {
      // If the file is not deleted, print an error message
      if (!inode[directory[i].inode].in_use)
      {
        printf("Error: File '%s' has not been deleted.\n", filename);
        return;
      }

      // Allocate a new inode for the recovered file
      int32_t new_inode = findFreeInode();
      if (new_inode == -1)
      {
        printf("Error: No free inodes available.\n");
        return;
      }
      free_inodes[new_inode] = 0;
      directory[i].inode = new_inode;
      inode[new_inode].in_use = 1;
      inode[new_inode].attribute = inode[directory[i].inode].attribute;

      // Allocate new data blocks for the recovered file
      int32_t j;
      int32_t new_block;
      for (j = 0; j < BLOCKS_PER_FILE; j++)
      {
        new_block = findFreeBlock();
        if (new_block == -1)
        {
          printf("Error: No free blocks available.\n");
          return;
        }
        free_blocks[new_block - 790] = 0;
        inode[new_inode].blocks[j] = new_block;
      }

      // Copy the contents from the deleted file's blocks to the new blocks
      int32_t size = inode[directory[i].inode].file_size;
      int32_t num_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
      int32_t offset = 0;
      for (j = 0; j < num_blocks; j++)
      {
        fseek(fp, directory[i].inode * BLOCK_SIZE + offset, SEEK_SET);
        fread(file_data[new_block - 790], BLOCK_SIZE, 1, fp);
        fseek(fp, new_block * BLOCK_SIZE, SEEK_SET);
        fwrite(file_data[new_block - 790], BLOCK_SIZE, 1, fp);
        offset += BLOCK_SIZE;
        new_block++;
      }

      // Update the inode of the recovered file
      inode[new_inode].file_size = size;

      // Mark the recovered file as not deleted
      inode[directory[i].inode].in_use = 0;
      free_inodes[directory[i].inode] = 1;
      directory[i].inode = new_inode;

      printf("File '%s' recovered.\n", filename);
      return;
    }
  }

  // If the filename is not found in the directory, print an error message
  printf("Error: File '%s' not found.\n", filename);
}

void retrieve(char *filename)
{

  if (filename == NULL)
  {
    printf("ERROR: Filename is NULL\n");
    return;
  }

  // find the file's directory entry
  int i;
  int directory_entry = -1;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use == 1 && strcmp(directory[i].filename, filename) == 0)
    {
      directory_entry = i;
      break;
    }
  }
  if (directory_entry == -1)
  {
    printf("ERROR: File not found in directory\n");
    return;
  }

  // retrieve the file's inode and size
  int32_t inode_index = directory[directory_entry].inode;
  int32_t file_size = inode[inode_index].file_size;

  // open a new file for writing
  FILE *ofp = fopen(filename, "w");
  if (ofp == NULL)
  {
    printf("ERROR: Could not open output file\n");
    return;
  }

  // read the file's blocks and write to output file
  int32_t offset = 0;
  int32_t block_index;
  int32_t bytes_to_read;

  while (offset < file_size)
  {
    // determine the block index and bytes to read for this iteration
    block_index = inode[inode_index].blocks[offset / BLOCK_SIZE];
    bytes_to_read = (offset + BLOCK_SIZE <= file_size) ? BLOCK_SIZE : file_size - offset;

    // write the block's data to the output file
    fwrite(file_data[block_index], bytes_to_read, 1, ofp);

    // update the offset for the next iteration
    offset += BLOCK_SIZE;
  }

  // close the output file and print success message
  fclose(ofp);
  printf("File retrieved successfully: %s\n", filename);
}

void attrib(char *attribute, char *filename)
{
  int i;
  int found = 0;

  // Search for the file in the directory
  for (i = 0; i < NUM_FILES; i++)
  {
    if (strcmp(directory[i].filename, filename) == 0)
    {
      found = 1;

      // Set or remove the attribute
      if (attribute[0] == '+')
      {
        inode[directory[i].inode].attribute |= HIDDEN;
      }
      else if (attribute[0] == '-')
      {
        inode[directory[i].inode].attribute &= ~READONLY;
      }
      else
      {
        printf("Invalid attribute\n");
        return;
      }

      printf("Attribute %s set for file %s\n", attribute, filename);
      break;
    }
  }
  if (!found)
  {
    printf("attrib: File not found\n");
  }
}

// read <filename> <starting byte> <number of bytes>
// Print <number of bytes> bytes from the file, in hexadecimal, starting at <starting byte>
void readfs(char *filename, int starting_byte, int num_bytes)
{
  if (!image_open)
  {
    printf("Error: no filesystem image is open\n");
    return;
  }

  // Find the inode corresponding to the given filename
  int i;
  for (i = 0; i < NUM_FILES; i++)
  {
    if (directory[i].in_use && strcmp(directory[i].filename, filename) == 0)
    {
      break;
    }
  }
  if (i == NUM_FILES)
  {
    printf("Error: file not found\n");
    return;
  }

  // Check if the inode is in use
  if (!inode[directory[i].inode].in_use)
  {
    printf("Error: file not found\n");
    return;
  }

  // Calculate the block number and the offset within the block
  int block_num = inode[directory[i].inode].blocks[starting_byte / BLOCK_SIZE];
  int offset = starting_byte % BLOCK_SIZE;

  // Loop through the blocks of the file and read the data
  int bytes_read = 0;
  while (bytes_read < num_bytes && block_num != -1)
  {
    fseek(fp, block_num * BLOCK_SIZE + offset, SEEK_SET);
    int bytes_to_read = BLOCK_SIZE - offset;
    if (num_bytes - bytes_read < bytes_to_read)
    {
      bytes_to_read = num_bytes - bytes_read;
    }
    uint8_t buffer[BLOCK_SIZE];
    fread(buffer, 1, bytes_to_read, fp);

    // Print the requested number of bytes in hexadecimal format
    int j;
    for (j = 0; j < bytes_to_read; j++)
    {
      printf("%02x ", buffer[j]);
    }
    bytes_read += bytes_to_read;

    // Move to the next block
    block_num = inode[directory[i].inode].blocks[block_num];
    offset = 0;
  }
  printf("\n");
}

void encrypt_decrypt(char *filename, char *cipher)
{
  // make the cipher into a type unsinged char
  unsigned char CPHER = (unsigned char)*cipher;

  int directory_entry = -1;
  char *tmpFilename = "crypt.txt";

  // gets the contents of the disk and turns it into a file
  // for easier encryption
  for (int i = 0; i < NUM_FILES; i++)
  {
    if (strcmp(filename, directory[i].filename) == 0)
    {
      directory_entry = i;
      break;
    }
  }
  if (directory_entry == -1)
  {
    printf("Error: File does not exist in directory\n");
  }
  // create a variable to store the location of the idnoe and file size
  int32_t inode_index = directory[directory_entry].inode;
  int32_t file_size = inode[inode_index].file_size;

  // the vaibels to get the file data
  int32_t offset = 0;
  int32_t block_IDX = 0;
  int32_t read_bytes;

  // opens the file for writing
  FILE *fp1, *fp2;
  fp1 = fopen(tmpFilename, "w+");
  // writes to the file
  while (offset < file_size)
  {
    block_IDX = inode[inode_index].blocks[offset / BLOCK_SIZE];
    read_bytes = (offset + BLOCK_SIZE <= file_size) ? BLOCK_SIZE : file_size - offset;

    fwrite(file_data[block_IDX], 1, read_bytes, fp1);

    offset += BLOCK_SIZE;
  }

  fclose(fp1);
  char buf;
  // stores the file before it was changed
  fp1 = fopen(tmpFilename, "r");
  fp2 = fopen(filename, "w");
  if (fp1 == NULL || fp2 == NULL)
  {
    exit(0);
  }
  // gets a character from the file and XOR it and puts it back in the file
  // josh: could not successfull encrypt correctly with the byte by byte menthod
  while ((buf = fgetc(fp1)) != EOF)
  {
    fputc(buf ^ CPHER, fp2);
  }
  fclose(fp1);
  fclose(fp2);
  delete (filename);
  insert(filename);
}

int main(int argc, char *argv[])
{
  char *command_string = (char *)malloc(MAX_COMMAND_SIZE);
  // Allocate memory for the command string and initialize the history array to zero

  fp = NULL;
  initialize();
  while (1)
  {
    printf("mfs> "); // Print out the mfs prompt

    // Read the command from the commandline.  Th
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while (!fgets(command_string, MAX_COMMAND_SIZE, stdin))
      ;

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
    {
      token[i] = NULL;
    }

    int token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr = NULL;

    char *working_string = strdup(command_string);

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while (((argument_ptr = strsep(&working_string, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
      // Copy the token into the array and set it to NULL if it is an empty string
      token[token_count] = strndup(argument_ptr, MAX_COMMAND_SIZE);
      if (strlen(token[token_count]) == 0)
      {
        token[token_count] = NULL;
      }
      token_count++;
    }

    // process the filesystem commands

    if (strcmp("createfs", token[0]) == 0)
    {
      if (token[1] == NULL) // make sure the filename isnt NULL
      {
        printf("ERROR: No Filename specified\n");
        continue;
      }
      createfs(token[1]);
    }
    if (strcmp("savefs", token[0]) == 0)
    {
      savefs();
    }
    if (strcmp("df", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }
      printf("%d bytes free\n", df());
    }
    if (strcmp("open", token[0]) == 0)
    {
      if (token[1] == NULL) // make sure the filename isnt NULL
      {
        printf("ERROR: No Filename specified\n");
        continue;
      }
      openfs(token[1]);
    }

    if (strcmp("close", token[0]) == 0)
    {
      closefs();
    }
    if (strcmp("list", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }
      list(token[1], token[2]);
    }
    if (strcmp("insert", token[0]) == 0) // if the first command is insert
    {
      if (!image_open) // make sure the filename isnt NULL
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }
      if (token[1] == NULL) // make sure the filename isnt NULL
      {
        printf("ERROR: No Filename specified\n");
        continue;
      }
      insert(token[1]); // insert the file (token[0]) into the file system
    }

    if (strcmp("retrieve", token[0]) == 0) // if the first command is retreive
    {
      if (!image_open) // make sure the filename isnt NULL
      {
        printf("ERROR: Disk image is not opened.\n");
        continue;
      }
      if (token[1] == NULL) // make sure the filename isnt NULL
      {
        printf("ERROR: No Filename specified\n");
        continue;
      }

      if (token[2] != NULL)
      {
        *token[1] = rename(token[1], token[2]); // retrieve with new filename
      }
      retrieve(token[1]);
    }

    if (strcmp("read", token[0]) == 0) // if the first command is read
    {
      if (token[1] == NULL) // make sure the filename isnt NULL nor the starting bytes nor file bytes
      {
        printf("ERROR: Filename specified or starting");
        continue;
      }
      else if (token[2] == NULL)
      {
        printf("ERROR: Bytes not specified not specified");
        continue;
      }
      else if (token[3] == NULL)
      {
        printf("ERROR: Bytes from file not specified");
        continue;
      }
      readfs(token[1], atoi(token[2]), atoi(token[3]));
    }

    if (strcmp("delete", token[0]) == 0) // if the first command is delete
    {
      if (token[1] == NULL) // make sure the filename isnt NULL
      {
        printf("ERROR: No Filename specified");
        continue;
      }
      delete (token[1]); // delete the file (token[1])
    }

    if (strcmp("undel", token[0]) == 0) // if the first command is delete
    {
      if (token[1] == NULL) // make sure the filename isnt NULL
      {
        printf("ERROR: No Filename specified");
        continue;
      }
      undelete(token[1]); // undelete the file (token[1])
    }

    if (strcmp("attrib", token[0]) == 0) // if the first command is attrib
    {
      if (token[1] == NULL) // make sure the filename isnt NULL
      {
        printf("ERROR: Attribute not specified");
        continue;
      }
      else if (token[2] == NULL)
      {
        printf("ERROR: No Filename specified");
        continue;
      }
      attrib(token[1], token[2]); // perform the attrib command on the file (token[1])
    }
    if (strcmp("encrypt", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("Error: must open the image first\n");
        continue;
      }
      if (token[1] == NULL)
      {
        printf("Error: need to specify file name: encrypt <filename> <cipher>\n");
        continue;
      }
      if (token[2] == NULL)
      {
        printf("Error: need to provide a cipher: encrypt <filename> <cipher>\n");
        continue;
      }
      encrypt_decrypt(token[1], token[2]);
    }

    if (strcmp("decrypt", token[0]) == 0)
    {
      if (!image_open)
      {
        printf("Error: must open the image first\n");
        continue;
      }
      if (token[1] == NULL)
      {
        printf("Error: need to specify file name: decrypt <filename> <cipher>\n");
        continue;
      }
      if (token[2] == NULL)
      {
        printf("Error: need to provide a cipher: decrypt <filename> <cipher>\n");
        continue;
      }
      encrypt_decrypt(token[1], token[2]);
    }

    if (strcmp("quit", token[0]) == 0)
    {
      exit(0);
    }
    // Free the memory allocated for the tokens.
    for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
    {
      if (token[i] != NULL)
      {
        free(token[i]);
      }
    }

    free(head_ptr); // Free the memory allocated for the working string.
  }

  free(command_string); // Free the memory allocated for the command string.
}
