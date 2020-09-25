#ifndef extra_methods_h
#define extra_methods_h

#include "svc.h"



void free_all_commits(helper* h);

void free_staged(helper* h);

void free_branches(helper* h);

int staged_has_changes(helper* h);

void update_staged(helper* h);

void set_changes(commit* previous, commit* new);

int staged_has_changes(helper* h);

int changes_cmp(const void* c1, const void* c2);

void set_commit_id(commit* commit);

commit* init_commit(helper* h, char* message);

void create_folders(commit* commit);

void create_backup(commit* commit);

void add_commit(helper* h, commit* commit);

void update_branches(helper* h, commit* commit);

int has_uncommited_changes(helper* h);

void restore_files(commit* commit);

char* merge_commit(void *helper, char *message, commit* other_parent);

void merge_update_staged(helper* h, commit* main_commit, commit* other_commit,
                        resolution* resolutions, int* n_resolutions);


#endif
