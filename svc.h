#ifndef svc_h
#define svc_h

#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>


#define MAX_PATH_SIZE (261)
#define MAX_COMMAND_SIZE (150)
#define MAX_DEST_SIZE (25)


typedef struct resolution {
    // NOTE: DO NOT MODIFY THIS STRUCT
    char *file_name;
    char *resolved_file;
} resolution;

typedef enum change_type {
  ADDED,
  REMOVED,
  MODIFIED,
  UNCHANGED,
} change_type;

typedef struct change {
  char* file_name;
  enum change_type type;
  int hash;

} change;

typedef struct file {
  char* file_name;
  int hash;
  int prev_hash;

} file;


typedef struct commit {
  char* id;
  char* message;
  char* branch;

  // pointers to parent commits
  struct commit* parent;
  struct commit* parent2;

  // list of changes in the commit
  change* changes;
  size_t n_changes;

  // list of all tracked files
  file* tracked;
  size_t n_tracked;
} commit;

typedef struct branch {
  char* name;
  commit* head;
} branch;

typedef struct helper {
  // list of bramches in the svc
  branch* branches;
  size_t n_branches;
  branch* active_branch;

  // list of file_paths in the staging area
  char** staged;
  size_t n_staged;

  int has_uncommited_changes;

  // pointers to all commits in svc
  commit** all_commits;
  size_t n_commits;

} helper;


void *svc_init(void);

void cleanup(void *helper);

int hash_file(void *helper, char *file_path);

char *svc_commit(void *helper, char *message);

void *get_commit(void *helper, char *commit_id);

char **get_prev_commits(void *helper, void *commit, int *n_prev);

void print_commit(void *helper, char *commit_id);

int svc_branch(void *helper, char *branch_name);

int svc_checkout(void *helper, char *branch_name);

char **list_branches(void *helper, int *n_branches);

int svc_add(void *helper, char *file_name);

int svc_rm(void *helper, char *file_name);

int svc_reset(void *helper, char *commit_id);

char *svc_merge(void *helper, char *branch_name,
  resolution *resolutions, int n_resolutions);

#endif
